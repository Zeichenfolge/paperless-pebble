// PebbleKit JS – läuft auf dem Handy in der Pebble-App.
// Spricht mit der Paperless-ngx-REST-API und schickt alles fertig aufbereitet an die Uhr.
// Bewusst ES5 (Pebble-SDK nutzt webpack 1; iOS führt PebbleKit JS in JavaScriptCore aus).

var Clay = require('pebble-clay');
var clayConfig = require('./config');
var webp = require('./webp');
var imaging = require('./image');
var demo = require('./demo');
var clay = null;  // wird beim Öffnen der Einstellungen in der aktuellen Sprache erzeugt

var SETTINGS_KEY = 'paperless-settings';

// Muss zu src/c/paperless.h passen
var MSG = {
  LIST_BEGIN: 1, ITEM: 2, LIST_END: 3, ERROR: 4, DONE: 5, STATUS: 6,
  IMG_BEGIN: 7, IMG_DATA: 8, IMG_END: 9, IMG_ERROR: 10,
  OPT_BEGIN: 11, OPT_ITEM: 12, OPT_END: 13, DOC_UPDATE: 14, SAVED: 15, IMG_STATUS: 16,
  CONFIG: 17
};
var CMD = { REFRESH: 1, DONE: 2, IMAGE: 3, IMAGE_CANCEL: 4, OPTIONS: 5, SET: 6 };
var FIELD = { CORRESPONDENT: 1, DOC_TYPE: 2, TAGS: 3, DATE: 4 };
var FLAG = { SELECTED: 1, SUGGESTED: 2 };

// Texte, die das Handy an die Uhr schickt
var TEXT = {
  de: {
    setup: 'Bitte in der Pebble-App unter Einstellungen URL und API-Token eintragen – oder „demo“ zum Ausprobieren.',
    invalidResponse: 'Ungültige Antwort vom Server',
    tokenInvalid: 'Token ungültig oder keine Rechte (%s)',
    notFound: 'Nicht gefunden (404) – URL prüfen',
    serverError: 'Serverfehler %s',
    unreachable: 'Server nicht erreichbar',
    timeout: 'Zeitüberschreitung',
    noInboxTag: 'Kein Inbox-Tag gefunden. In den Einstellungen einen Tag-Namen eintragen.',
    tagNotFound: 'Tag „%s“ nicht gefunden',
    loadingThumb: 'Lade Vorschaubild...',
    processing: 'Bild wird aufbereitet...',
    noBinary: 'Bild-Download vom Handy nicht unterstützt',
    badFormat: 'Vorschau-Format nicht unterstützt',
    decodeError: 'Bild konnte nicht gelesen werden',
    imageError: 'Bildfehler',
    none: '– Keiner –',
    document: 'Dokument %s'
  },
  en: {
    setup: 'Please enter the URL and API token in the Pebble app settings – or "demo" to try it out.',
    invalidResponse: 'Invalid response from server',
    tokenInvalid: 'Invalid token or no permission (%s)',
    notFound: 'Not found (404) – check URL',
    serverError: 'Server error %s',
    unreachable: 'Server not reachable',
    timeout: 'Timeout',
    noInboxTag: 'No inbox tag found. Enter a tag name in the settings.',
    tagNotFound: 'Tag "%s" not found',
    loadingThumb: 'Loading preview...',
    processing: 'Preparing image...',
    noBinary: 'Image download not supported on this phone',
    badFormat: 'Preview format not supported',
    decodeError: 'Could not read image',
    imageError: 'Image error',
    none: '– None –',
    document: 'Document %s'
  }
};

function T(key, arg) {
  var table = TEXT[loadSettings().lang] || TEXT.de;
  var text = table[key] || TEXT.de[key] || key;
  return arg === undefined ? text : text.replace('%s', arg);
}

var CHUNK = 1800;     // Bytes Bilddaten pro Nachricht
var MAX_OPTS = 30;    // Einträge in Auswahllisten

var inboxTagIds = null;                                       // IDs der Inbox-Tags
var names = { correspondents: {}, document_types: {}, tags: {} }; // id -> Name
var docs = {};                                                // id -> Dokument (API-Objekt)
var loading = false;
var imgCache = null;                                          // { docId, width, height, gray }
var imgSeq = -1;                                              // aktuelle Bildanfrage der Uhr

// ---------------------------------------------------------------------------
// Einstellungen
// ---------------------------------------------------------------------------

function loadSettings() {
  var s = {};
  try {
    s = JSON.parse(localStorage.getItem(SETTINGS_KEY)) || {};
  } catch (e) {
    s = {};
  }
  return {
    url: normalizeUrl(s.url || ''),
    token: String(s.token || '').trim(),
    max: Math.max(1, Math.min(20, parseInt(s.max, 10) || 15)),
    inboxTag: String(s.inboxTag || '').trim(),
    lang: s.lang === 'en' ? 'en' : 'de',
    split: [0, 1, 2].indexOf(parseInt(s.split, 10)) >= 0 ? parseInt(s.split, 10) : 0
  };
}

function normalizeUrl(url) {
  url = String(url || '').trim().replace(/\/+$/, '');
  if (url && !/^https?:\/\//i.test(url)) url = 'https://' + url;
  return url;
}

function unwrap(v) {
  return (v !== null && typeof v === 'object' && 'value' in v) ? v.value : v;
}

function resetCaches() {
  inboxTagIds = null;
  names = { correspondents: {}, document_types: {}, tags: {} };
  docs = {};
  imgCache = null;
}

Pebble.addEventListener('showConfiguration', function () {
  clay = new Clay(clayConfig(loadSettings().lang), null, { autoHandleEvents: false });
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  if (!clay) clay = new Clay(clayConfig(loadSettings().lang), null, { autoHandleEvents: false });
  var raw = clay.getSettings(e.response, false);
  localStorage.setItem(SETTINGS_KEY, JSON.stringify({
    url: unwrap(raw.CfgUrl),
    token: unwrap(raw.CfgToken),
    max: unwrap(raw.CfgMax),
    inboxTag: unwrap(raw.CfgInboxTag),
    lang: unwrap(raw.CfgLang),
    split: unwrap(raw.CfgSplit)
  }));
  resetCaches();
  sendConfig();
  fetchInbox();
});

// ---------------------------------------------------------------------------
// Nachrichten an die Uhr – Warteschlange, immer nur eine Nachricht unterwegs
// ---------------------------------------------------------------------------

var queue = [];
var sending = false;

function send(msg) {
  queue.push({ msg: msg, tries: 0 });
  pump();
}

function pump() {
  if (sending || queue.length === 0) return;
  sending = true;
  var item = queue[0];
  Pebble.sendAppMessage(item.msg, function () {
    queue.shift();
    sending = false;
    pump();
  }, function () {
    item.tries++;
    sending = false;
    if (item.tries >= 3) {
      console.log('Nachricht verworfen, Typ ' + item.msg.Type);
      queue.shift();
    }
    setTimeout(pump, 300);
  });
}

function isImageMsg(m) {
  return m.Type === MSG.IMG_BEGIN || m.Type === MSG.IMG_DATA || m.Type === MSG.IMG_END ||
    m.Type === MSG.IMG_ERROR || m.Type === MSG.IMG_STATUS;
}

// Noch nicht gesendete Bilddaten verwerfen (die gerade laufende Nachricht bleibt)
function purgeImageMessages() {
  var kept = [];
  for (var i = 0; i < queue.length; i++) {
    if ((sending && i === 0) || !isImageMsg(queue[i].msg)) kept.push(queue[i]);
  }
  queue = kept;
}

function sendError(text) {
  console.log('Fehler: ' + text);
  send({ Type: MSG.ERROR, Text: text });
}

// Sprache und Bild-Aufteilung an die Uhr (wird dort gespeichert)
function sendConfig() {
  var cfg = loadSettings();
  send({ Type: MSG.CONFIG, Lang: cfg.lang === 'en' ? 1 : 0, Split: cfg.split });
}

function sendImageStatus(seq, text, isError) {
  send({ Type: isError ? MSG.IMG_ERROR : MSG.IMG_STATUS, Seq: seq, Text: text });
}

// ---------------------------------------------------------------------------
// Paperless-API
// ---------------------------------------------------------------------------

function request(method, path, body, binary, onSuccess, onError) {
  var cfg = loadSettings();
  if (demo.isDemo(cfg.url)) {
    demo.request(method, path, body, binary, cfg.lang, onSuccess, onError);
    return;
  }
  var xhr = new XMLHttpRequest();
  xhr.open(method, cfg.url + path, true);
  xhr.setRequestHeader('Authorization', 'Token ' + cfg.token);
  xhr.setRequestHeader('Accept', binary ? '*/*' : 'application/json');
  if (body) xhr.setRequestHeader('Content-Type', 'application/json');
  if (binary) xhr.responseType = 'arraybuffer';
  xhr.timeout = 20000;
  xhr.onload = function () {
    var st = xhr.status;
    if (st >= 200 && st < 300) {
      if (binary) {
        onSuccess(xhr.response);
        return;
      }
      var data;
      try {
        data = xhr.responseText ? JSON.parse(xhr.responseText) : {};
      } catch (e) {
        onError(T('invalidResponse'), st);
        return;
      }
      onSuccess(data);
    } else if (st === 401 || st === 403) {
      onError(T('tokenInvalid', st), st);
    } else if (st === 404) {
      onError(T('notFound'), st);
    } else {
      onError(T('serverError', st), st);
    }
  };
  xhr.onerror = function () { onError(T('unreachable'), 0); };
  xhr.ontimeout = function () { onError(T('timeout'), 0); };
  xhr.send(body ? JSON.stringify(body) : null);
}

function apiGet(path, ok, fail) { request('GET', path, null, false, ok, fail); }

// Fehlende Namen (Korrespondenten, Dokumenttypen, Tags) nachladen.
// Nicht kritisch: bei Fehlern bleiben die Namen leer.
function resolveNames(kind, ids, cb) {
  var missing = [];
  for (var i = 0; i < ids.length; i++) {
    var id = ids[i];
    if (id !== null && id !== undefined && !(id in names[kind]) && missing.indexOf(id) < 0) {
      missing.push(id);
    }
  }
  if (missing.length === 0) {
    cb();
    return;
  }
  apiGet('/api/' + kind + '/?page_size=100&id__in=' + missing.join(','), function (data) {
    (data.results || []).forEach(function (r) { names[kind][r.id] = r.name; });
    cb();
  }, function () { cb(); });
}

function resolveDocNames(list, cb) {
  var corr = [], types = [], tags = [];
  list.forEach(function (d) {
    corr.push(d.correspondent);
    types.push(d.document_type);
    (d.tags || []).forEach(function (t) { tags.push(t); });
  });
  resolveNames('correspondents', corr, function () {
    resolveNames('document_types', types, function () {
      resolveNames('tags', tags, cb);
    });
  });
}

function ensureInboxTags(cb, onError) {
  if (inboxTagIds && inboxTagIds.length) {
    cb(inboxTagIds);
    return;
  }
  var wanted = loadSettings().inboxTag.split(',')
    .map(function (s) { return s.trim(); })
    .filter(function (s) { return s.length > 0; });

  if (wanted.length === 0) {
    // Automatisch: in Paperless als Posteingangs-Tag markierte Tags
    apiGet('/api/tags/?page_size=100&is_inbox_tag=true', function (data) {
      var ids = [];
      (data.results || []).forEach(function (t) {
        if (t.is_inbox_tag) {
          ids.push(t.id);
          names.tags[t.id] = t.name;
        }
      });
      if (ids.length === 0) {
        onError(T('noInboxTag'));
        return;
      }
      inboxTagIds = ids;
      cb(ids);
    }, onError);
    return;
  }

  // Namen aus den Einstellungen nachschlagen (Groß-/Kleinschreibung egal)
  var ids = [], notFound = [];
  (function next(i) {
    if (i >= wanted.length) {
      if (notFound.length) {
        onError(T('tagNotFound', notFound.join(', ')));
        return;
      }
      inboxTagIds = ids;
      cb(ids);
      return;
    }
    apiGet('/api/tags/?name__iexact=' + encodeURIComponent(wanted[i]), function (data) {
      var t = (data.results || [])[0];
      if (t) {
        ids.push(t.id);
        names.tags[t.id] = t.name;
      } else {
        notFound.push(wanted[i]);
      }
      next(i + 1);
    }, onError);
  })(0);
}

function isInboxTag(id) {
  return inboxTagIds && inboxTagIds.indexOf(id) >= 0;
}

function formatDate(created) {
  if (!created) return '';
  var m = /^(\d{4})-(\d{2})-(\d{2})/.exec(created);
  if (!m) return String(created).substr(0, 10);
  // Deutsch: 18.07.2026 – Englisch: 2026-07-18 (eindeutig in jedem Land)
  return loadSettings().lang === 'en' ? (m[1] + '-' + m[2] + '-' + m[3]) : (m[3] + '.' + m[2] + '.' + m[1]);
}

function isoDate(d) {
  var c = d.created_date || d.created || '';
  var m = /^(\d{4}-\d{2}-\d{2})/.exec(c);
  return m ? m[1] : '';
}

// Felder für die Uhr (MSG.ITEM und MSG.DOC_UPDATE)
function summary(d) {
  var corr = d.correspondent !== null && d.correspondent !== undefined ? (names.correspondents[d.correspondent] || '') : '';
  var type = d.document_type !== null && d.document_type !== undefined ? (names.document_types[d.document_type] || '') : '';
  var date = formatDate(isoDate(d));
  var tagList = (d.tags || [])
    .filter(function (t) { return !isInboxTag(t); })
    .map(function (t) { return names.tags[t]; })
    .filter(function (n) { return !!n; })
    .join(', ');
  return {
    Title: d.title || T('document', d.id),
    Subtitle: corr ? (corr + ' · ' + date) : date,
    Date: date,
    Correspondent: corr,
    DocType: type,
    Tags: tagList
  };
}

function getDoc(docId, cb, onError) {
  if (docs[docId]) {
    cb(docs[docId]);
    return;
  }
  apiGet('/api/documents/' + docId + '/', function (d) {
    docs[d.id] = d;
    cb(d);
  }, onError);
}

// ---------------------------------------------------------------------------
// Inbox
// ---------------------------------------------------------------------------

function fetchInbox() {
  var cfg = loadSettings();
  if (!cfg.url || (!cfg.token && !demo.isDemo(cfg.url))) {
    sendError(T('setup'));
    return;
  }
  if (loading) return;
  loading = true;

  var fail = function (msg) {
    loading = false;
    sendError(msg);
  };

  ensureInboxTags(function (ids) {
    var query = '/api/documents/?ordering=-added' +
      '&page_size=' + cfg.max +
      '&tags__id__in=' + ids.join(',') +
      '&fields=id,title,correspondent,document_type,created,created_date,tags';

    apiGet(query, function (data) {
      var list = data.results || [];
      list.forEach(function (d) { docs[d.id] = d; });
      resolveDocNames(list, function () {
        loading = false;
        send({ Type: MSG.LIST_BEGIN, Total: data.count || list.length, Num: list.length });
        list.forEach(function (d, i) {
          var m = summary(d);
          m.Type = MSG.ITEM;
          m.Index = i;
          m.DocId = d.id;
          send(m);
        });
        send({ Type: MSG.LIST_END });
      });
    }, fail);
  }, fail);
}

// Erledigt = Inbox-Tag(s) entfernen
function markDone(docId) {
  ensureInboxTags(function (ids) {
    request('POST', '/api/documents/bulk_edit/', {
      documents: [docId],
      method: 'modify_tags',
      parameters: { add_tags: [], remove_tags: ids }
    }, false, function () {
      delete docs[docId];
      send({ Type: MSG.DONE, DocId: docId });
      setTimeout(fetchInbox, 500);  // nachrückende Dokumente laden
    }, sendError);
  }, sendError);
}

// ---------------------------------------------------------------------------
// Bild
// ---------------------------------------------------------------------------

function toBytes(resp) {
  if (!resp) return null;
  if (resp instanceof Uint8Array) return resp;  // iOS liefert direkt ein Uint8Array
  return new Uint8Array(resp);                  // ArrayBuffer (Android, Emulator)
}

function isWebP(b) {
  return b.length > 12 && b[0] === 0x52 && b[1] === 0x49 && b[2] === 0x46 && b[3] === 0x46 &&
    b[8] === 0x57 && b[9] === 0x45 && b[10] === 0x42 && b[11] === 0x50;
}

function getImage(docId, seq, cb) {
  if (imgCache && imgCache.docId === docId) {
    cb(imgCache);
    return;
  }
  sendImageStatus(seq, T('loadingThumb'));
  request('GET', '/api/documents/' + docId + '/thumb/', null, true, function (resp) {
    if (seq !== imgSeq) return;
    var bytes = toBytes(resp);
    if (!bytes || bytes.length === 0) {
      sendImageStatus(seq, T('noBinary'), true);
      return;
    }
    if (!isWebP(bytes)) {
      sendImageStatus(seq, T('badFormat'), true);
      return;
    }
    sendImageStatus(seq, T('processing'));
    // Kurz warten, damit die Statusmeldung vor dem (langsamen) Dekodieren rausgeht
    setTimeout(function () {
      if (seq !== imgSeq) return;
      try {
        var t0 = Date.now();
        var img = webp.decodeWebPGray(bytes);
        console.log('WebP ' + img.width + 'x' + img.height + ' dekodiert in ' + (Date.now() - t0) + ' ms');
        imgCache = { docId: docId, width: img.width, height: img.height, gray: img.gray };
        cb(imgCache);
      } catch (e) {
        console.log('Dekodierfehler: ' + e);
        sendImageStatus(seq, T('decodeError'), true);
      }
    }, 100);
  }, function (msg) {
    if (seq === imgSeq) sendImageStatus(seq, msg, true);
  });
}

function toArray(u8) {
  var a = new Array(u8.length);
  for (var i = 0; i < u8.length; i++) a[i] = u8[i];
  return a;
}

function handleImage(p) {
  var seq = p.Seq;
  var docId = p.DocId;
  var mode = p.ImgMode || 0;
  imgSeq = seq;
  purgeImageMessages();

  getImage(docId, seq, function (img) {
    if (seq !== imgSeq) return;
    var r;
    try {
      r = imaging.render(img, {
        mode: mode,
        part: p.ImgCol || 0,
        split: p.Split !== undefined ? p.Split : loadSettings().split,
        screenW: p.Width || 200,
        screenH: p.Height || 228,
        maxBytes: p.MaxBytes || 30000
      });
    } catch (e) {
      console.log('Renderfehler: ' + e);
      sendImageStatus(seq, T('imageError'), true);
      return;
    }
    send({
      Type: MSG.IMG_BEGIN, Seq: seq, DocId: docId, ImgMode: mode, ImgCol: r.part, Split: r.split,
      Width: r.width, Height: r.height, Num: r.parts, Step: r.step, SrcW: r.srcW, SrcH: r.srcH
    });
    for (var off = 0; off < r.data.length; off += CHUNK) {
      send({ Type: MSG.IMG_DATA, Seq: seq, Offset: off, Data: toArray(r.data.subarray(off, off + CHUNK)) });
    }
    send({ Type: MSG.IMG_END, Seq: seq });
  });
}

// ---------------------------------------------------------------------------
// Bearbeiten: Auswahllisten
// ---------------------------------------------------------------------------

function getSuggestions(docId, cb) {
  apiGet('/api/documents/' + docId + '/suggestions/', function (s) {
    cb(s || {});
  }, function () {
    cb({});  // Vorschläge sind optional
  });
}

function sendOptions(docId, field, opts) {
  opts = opts.slice(0, MAX_OPTS);
  send({ Type: MSG.OPT_BEGIN, DocId: docId, Field: field, Num: opts.length });
  opts.forEach(function (o, i) {
    send({ Type: MSG.OPT_ITEM, Index: i, OptId: o.id, Title: o.name, Flags: o.flags });
  });
  send({ Type: MSG.OPT_END });
}

// Vorschläge zuerst, dann aktuelle Werte, dann die meistgenutzten Einträge
function buildList(kind, suggested, selected, exclude, withNone, cb) {
  apiGet('/api/' + kind + '/?ordering=-document_count&page_size=40', function (data) {
    var top = [];
    (data.results || []).forEach(function (r) {
      names[kind][r.id] = r.name;
      top.push(r.id);
    });
    resolveNames(kind, suggested.concat(selected), function () {
      var order = [], seen = {};
      suggested.concat(selected, top).forEach(function (id) {
        if (id === null || id === undefined || seen[id] || exclude.indexOf(id) >= 0) return;
        if (!(id in names[kind])) return;
        seen[id] = true;
        order.push(id);
      });
      var opts = order.map(function (id) {
        return {
          id: id,
          name: names[kind][id],
          flags: (selected.indexOf(id) >= 0 ? FLAG.SELECTED : 0) | (suggested.indexOf(id) >= 0 ? FLAG.SUGGESTED : 0)
        };
      });
      if (withNone) {
        opts = opts.slice(0, MAX_OPTS - 1);
        opts.push({ id: 0, name: T('none'), flags: selected.length === 0 ? FLAG.SELECTED : 0 });
      }
      cb(opts);
    });
  }, sendError);
}

function dateOption(iso, flags) {
  return {
    id: parseInt(iso.replace(/-/g, ''), 10),
    name: formatDate(iso),
    flags: flags
  };
}

function handleOptions(docId, field) {
  getDoc(docId, function (doc) {
    getSuggestions(docId, function (sug) {
      var done = function (opts) { sendOptions(docId, field, opts); };
      var one = function (v) { return v === null || v === undefined ? [] : [v]; };

      if (field === FIELD.CORRESPONDENT) {
        buildList('correspondents', sug.correspondents || [], one(doc.correspondent), [], true, done);
      } else if (field === FIELD.DOC_TYPE) {
        buildList('document_types', sug.document_types || [], one(doc.document_type), [], true, done);
      } else if (field === FIELD.TAGS) {
        ensureInboxTags(function (inbox) {
          buildList('tags', sug.tags || [], doc.tags || [], inbox, false, done);
        }, sendError);
      } else if (field === FIELD.DATE) {
        var current = isoDate(doc);
        var today = new Date();
        var todayIso = today.getFullYear() + '-' + ('0' + (today.getMonth() + 1)).slice(-2) + '-' +
          ('0' + today.getDate()).slice(-2);
        var opts = [], seen = {};
        var add = function (iso, flags) {
          if (!iso || seen[iso]) return;
          seen[iso] = true;
          opts.push(dateOption(iso, flags));
        };
        if (current) add(current, FLAG.SELECTED | ((sug.dates || []).indexOf(current) >= 0 ? FLAG.SUGGESTED : 0));
        (sug.dates || []).forEach(function (d) { add(String(d).substr(0, 10), FLAG.SUGGESTED); });
        add(todayIso, 0);
        done(opts);
      }
    });
  }, sendError);
}

// ---------------------------------------------------------------------------
// Bearbeiten: Speichern
// ---------------------------------------------------------------------------

function afterSave(docId, field, updated) {
  docs[docId] = updated;
  resolveDocNames([updated], function () {
    var m = summary(updated);
    m.Type = MSG.DOC_UPDATE;
    m.DocId = docId;
    send(m);
    send({ Type: MSG.SAVED, DocId: docId, Field: field });
  });
}

function patchDoc(docId, body, ok, fail) {
  request('PATCH', '/api/documents/' + docId + '/', body, false, ok, fail);
}

function handleSet(docId, field, optId, flags) {
  getDoc(docId, function (doc) {
    var body;
    if (field === FIELD.CORRESPONDENT) {
      body = { correspondent: optId || null };
    } else if (field === FIELD.DOC_TYPE) {
      body = { document_type: optId || null };
    } else if (field === FIELD.TAGS) {
      var tags = (doc.tags || []).slice();
      var idx = tags.indexOf(optId);
      if ((flags & FLAG.SELECTED) && idx < 0) tags.push(optId);
      if (!(flags & FLAG.SELECTED) && idx >= 0) tags.splice(idx, 1);
      body = { tags: tags };
    } else if (field === FIELD.DATE) {
      var s = String(optId);
      var iso = s.substr(0, 4) + '-' + s.substr(4, 2) + '-' + s.substr(6, 2);
      // Neuere Paperless-Versionen: "created" ist ein Datum. Ältere: "created_date".
      patchDoc(docId, { created: iso }, function (u) { afterSave(docId, field, u); }, function (msg, status) {
        if (status === 400) {
          patchDoc(docId, { created_date: iso }, function (u) { afterSave(docId, field, u); }, sendError);
        } else {
          sendError(msg);
        }
      });
      return;
    } else {
      return;
    }
    patchDoc(docId, body, function (u) { afterSave(docId, field, u); }, sendError);
  }, sendError);
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

Pebble.addEventListener('ready', function () {
  console.log('Paperless PebbleKit JS bereit');
  sendConfig();
  fetchInbox();
});

Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload || {};
  switch (p.Command) {
    case CMD.REFRESH:
      fetchInbox();
      break;
    case CMD.DONE:
      if (p.DocId) markDone(p.DocId);
      break;
    case CMD.IMAGE:
      if (p.DocId) handleImage(p);
      break;
    case CMD.IMAGE_CANCEL:
      imgSeq = -1;
      purgeImageMessages();
      break;
    case CMD.OPTIONS:
      if (p.DocId) handleOptions(p.DocId, p.Field);
      break;
    case CMD.SET:
      if (p.DocId) handleSet(p.DocId, p.Field, p.OptId, p.Flags || 0);
      break;
  }
});
