// Demo-Modus: Server-URL "demo" zeigt erfundene Beispieldaten, ganz ohne Paperless-Server.
// Gut zum Ausprobieren und für Screenshots (keine echten Dokumente).
// Beantwortet dieselben API-Anfragen wie Paperless, nur im Speicher des Handys.
// Änderungen (Bearbeiten, Erledigt) gelten bis zum Neustart der App.
// Bewusst ES5 (Pebble-SDK nutzt webpack 1; iOS führt PebbleKit JS in JavaScriptCore aus).

// Vorschaubild: erfundene Beispielrechnung (WebP, 500 x 708)
var THUMB_B64 = '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';

var DATA = {
  de: {
    docs: [
      { id: 101, title: 'Rechnung 2026-0815', correspondent: 1, document_type: 1, created: '2026-09-28', tags: [10, 20, 21] },
      { id: 102, title: 'Stromabrechnung September', correspondent: 2, document_type: 1, created: '2026-09-25', tags: [10, 22] },
      { id: 103, title: 'Kfz-Versicherung Verlängerung', correspondent: 3, document_type: 2, created: '2026-09-20', tags: [10, 23] }
    ],
    correspondents: { 1: 'ACME Raketenbedarf', 2: 'Stadtwerke Musterstadt', 3: 'Sicher Fahren Versicherung', 4: 'Beispielbank' },
    document_types: { 1: 'Rechnung', 2: 'Vertrag', 3: 'Brief' },
    tags: { 10: 'Inbox', 20: 'Einkauf', 21: 'Mail', 22: 'Haushalt', 23: 'Auto', 24: 'Steuern' }
  },
  en: {
    docs: [
      { id: 101, title: 'Invoice 2026-0815', correspondent: 1, document_type: 1, created: '2026-09-28', tags: [10, 20, 21] },
      { id: 102, title: 'Electricity bill September', correspondent: 2, document_type: 1, created: '2026-09-25', tags: [10, 22] },
      { id: 103, title: 'Car insurance renewal', correspondent: 3, document_type: 2, created: '2026-09-20', tags: [10, 23] }
    ],
    correspondents: { 1: 'ACME Rocket Supplies', 2: 'City Utilities', 3: 'Safe Drive Insurance', 4: 'Example Bank' },
    document_types: { 1: 'Invoice', 2: 'Contract', 3: 'Letter' },
    tags: { 10: 'Inbox', 20: 'Shopping', 21: 'Mail', 22: 'Household', 23: 'Car', 24: 'Taxes' }
  }
};
var INBOX_TAG = 10;
var SUGGESTIONS = { correspondents: [1], tags: [24], document_types: [1], dates: ['2026-09-28'] };

var state = null;  // Kopie der Daten für die laufende Sitzung
var stateLang = null;
var thumbBytes = null;

function isDemo(url) {
  return /^(https?:\/\/)?demo\/?$/i.test(String(url || '').trim());
}

function copy(o) {
  return JSON.parse(JSON.stringify(o));
}

function getState(lang) {
  lang = lang === 'en' ? 'en' : 'de';
  if (!state || stateLang !== lang) {
    state = copy(DATA[lang]);
    stateLang = lang;
  }
  return state;
}

// Base64 -> Uint8Array (iOS PebbleKit JS hat kein atob)
function decodeBase64(s) {
  var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  var lookup = {};
  for (var i = 0; i < chars.length; i++) lookup[chars.charAt(i)] = i;
  s = s.replace(/[^A-Za-z0-9+\/]/g, '');
  var out = new Uint8Array(Math.floor(s.length * 3 / 4));
  var buf = 0, bits = 0, n = 0;
  for (var j = 0; j < s.length; j++) {
    buf = (buf << 6) | lookup[s.charAt(j)];
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out[n++] = (buf >> bits) & 0xFF;
    }
  }
  return n === out.length ? out : out.subarray(0, n);
}

function nameList(map, ids) {
  var results = [];
  for (var id in map) {
    if (!ids || ids.indexOf(parseInt(id, 10)) >= 0) results.push({ id: parseInt(id, 10), name: map[id] });
  }
  return { count: results.length, results: results };
}

function query(path) {
  var q = {};
  var i = path.indexOf('?');
  if (i < 0) return q;
  path.substr(i + 1).split('&').forEach(function (kv) {
    var p = kv.split('=');
    q[decodeURIComponent(p[0])] = decodeURIComponent(p[1] || '');
  });
  return q;
}

function findDoc(st, id) {
  for (var i = 0; i < st.docs.length; i++) {
    if (st.docs[i].id === id) return st.docs[i];
  }
  return null;
}

// Liefert [status, daten, binär?] für eine API-Anfrage
function route(method, path, body, lang) {
  var st = getState(lang);
  var p = path.split('?')[0];
  var q = query(path);
  var ids = q.id__in ? q.id__in.split(',').map(function (x) { return parseInt(x, 10); }) : null;
  var m;

  if (p === '/api/tags/' && q.is_inbox_tag) {
    return [200, { results: [{ id: INBOX_TAG, name: st.tags[INBOX_TAG], is_inbox_tag: true }] }];
  }
  if (p === '/api/tags/' && q.name__iexact) {
    var wanted = q.name__iexact.toLowerCase();
    for (var t in st.tags) {
      if (st.tags[t].toLowerCase() === wanted) return [200, { results: [{ id: parseInt(t, 10), name: st.tags[t] }] }];
    }
    return [200, { results: [] }];
  }
  if (p === '/api/tags/') return [200, nameList(st.tags, ids)];
  if (p === '/api/correspondents/') return [200, nameList(st.correspondents, ids)];
  if (p === '/api/document_types/') return [200, nameList(st.document_types, ids)];

  if (p === '/api/documents/' && method === 'GET') {
    var tagIds = (q.tags__id__in || '').split(',').map(function (x) { return parseInt(x, 10); });
    var list = st.docs.filter(function (d) {
      return d.tags.some(function (x) { return tagIds.indexOf(x) >= 0; });
    });
    return [200, { count: list.length, results: copy(list) }];
  }
  if ((m = /^\/api\/documents\/(\d+)\/thumb\/$/.exec(p))) {
    if (!thumbBytes) thumbBytes = decodeBase64(THUMB_B64);
    return [200, thumbBytes, true];
  }
  if ((m = /^\/api\/documents\/(\d+)\/suggestions\/$/.exec(p))) {
    return [200, copy(SUGGESTIONS)];
  }
  if ((m = /^\/api\/documents\/(\d+)\/$/.exec(p))) {
    var doc = findDoc(st, parseInt(m[1], 10));
    if (!doc) return [404, {}];
    if (method === 'PATCH' && body) {
      var changes = typeof body === 'string' ? JSON.parse(body) : body;
      for (var k in changes) {
        if (k === 'created_date') doc.created = changes[k];
        else doc[k] = changes[k];
      }
    }
    return [200, copy(doc)];
  }
  if (p === '/api/documents/bulk_edit/' && method === 'POST') {
    var b = typeof body === 'string' ? JSON.parse(body) : body;
    var remove = (b.parameters && b.parameters.remove_tags) || [];
    (b.documents || []).forEach(function (id) {
      var d = findDoc(st, id);
      if (d) d.tags = d.tags.filter(function (x) { return remove.indexOf(x) < 0; });
    });
    return [200, { result: 'OK' }];
  }
  return [404, {}];
}

// Gleiche Schnittstelle wie request() in index.js
function request(method, path, body, binary, lang, onSuccess, onError) {
  setTimeout(function () {
    var r;
    try {
      r = route(method, path, body, lang);
    } catch (e) {
      onError('Demo: ' + e, 500);
      return;
    }
    if (r[0] >= 200 && r[0] < 300) {
      onSuccess(r[1]);
    } else {
      onError('Demo: ' + r[0], r[0]);
    }
  }, 150);  // kleine Verzögerung wie bei einem echten Server
}

module.exports = {
  isDemo: isDemo,
  request: request
};
