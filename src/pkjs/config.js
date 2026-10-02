// Einstellungsseite (Clay) – erscheint in der Pebble-App unter „Einstellungen“.
// Die Seite wird in der eingestellten Sprache erzeugt (nach dem Umstellen
// erscheint sie beim nächsten Öffnen in der neuen Sprache).

// Links im Bereich "Unterstützen". Leer lassen, um den jeweiligen Link auszublenden.
var SUPPORT_URL = 'https://buymeacoffee.com/michaelrunge';               // TODO: eigenen Namen eintragen
var PROJECT_URL = 'https://github.com/Zeichenfolge/paperless-pebble'; // TODO: eigenes Repository

var TEXT = {
  de: {
    intro: 'Zeigt deine Paperless-ngx-Inbox auf der Uhr.',
    general: 'Allgemein',
    language: 'Sprache',
    server: 'Server',
    url: 'Server-URL',
    token: 'API-Token',
    tokenHelp: 'In Paperless: Profil (oben rechts) → „Mein Profil“ → API-Auth-Token erzeugen und hier einfügen.',
    tokenPlaceholder: 'Token einfügen',
    inbox: 'Inbox',
    inboxTag: 'Inbox-Tag',
    inboxTagHelp: 'Name des Tags, der neue Dokumente markiert (z. B. „Neu“ oder „Inbox“). ' +
      'Er wird beim Erledigen entfernt. Mehrere Tags mit Komma trennen. ' +
      'Leer lassen = die in Paperless als „Posteingangs-Tag“ markierten Tags verwenden.',
    inboxTagPlaceholder: 'automatisch',
    max: 'Max. Dokumente auf der Uhr',
    image: 'Bildansicht',
    split: 'Bild aufteilen',
    splitHelp: 'Wie die Seite beim Hineinzoomen aufgeteilt wird. ' +
      '„Nicht teilen“: ganze Seite – UP/DOWN scrollt, SELECT wechselt zwischen ↕ und ↔ ' +
      '(auf der Pebble Time 2 auch per Finger verschiebbar).',
    splitVertical: 'Vertikal (senkrechte Streifen)',
    splitHorizontal: 'Horizontal (waagerechte Streifen)',
    splitNone: 'Nicht teilen (frei scrollen)',
    save: 'Speichern',
    support: 'Unterstützen',
    supportText: 'Paperless für Pebble ist kostenlos und Open Source. ' +
      'Wenn dir die App gefällt, freue ich mich über einen Kaffee. ☕',
    supportHint: 'Bitte vorher speichern – der Link öffnet sich in diesem Fenster.',
    project: 'Fehler gefunden oder eine Idee? Auf GitHub kannst du sie melden.'
  },
  en: {
    intro: 'Shows your Paperless-ngx inbox on your watch.',
    general: 'General',
    language: 'Language',
    server: 'Server',
    url: 'Server URL',
    token: 'API token',
    tokenHelp: 'In Paperless: profile (top right) → "My Profile" → generate an API auth token and paste it here.',
    tokenPlaceholder: 'Paste token',
    inbox: 'Inbox',
    inboxTag: 'Inbox tag',
    inboxTagHelp: 'Name of the tag that marks new documents (e.g. "Inbox" or "New"). ' +
      'It is removed when you mark a document as done. Separate several tags with commas. ' +
      'Leave empty to use the tags marked as "inbox tag" in Paperless.',
    inboxTagPlaceholder: 'automatic',
    max: 'Max. documents on the watch',
    image: 'Image view',
    split: 'Split image',
    splitHelp: 'How the page is divided when you zoom in. ' +
      '"Don\'t split": whole page – UP/DOWN scrolls, SELECT switches between ↕ and ↔ ' +
      '(on Pebble Time 2 you can also drag with your finger).',
    splitVertical: 'Vertical (columns)',
    splitHorizontal: 'Horizontal (rows)',
    splitNone: 'Don\'t split (free scrolling)',
    save: 'Save',
    support: 'Support',
    supportText: 'Paperless for Pebble is free and open source. ' +
      'If you enjoy it, I\'d be happy about a coffee. ☕',
    supportHint: 'Please save first – the link opens in this window.',
    project: 'Found a bug or have an idea? Report it on GitHub.'
  }
};

function button(url, label) {
  return '<a href="' + url + '" style="display:block;margin:12px 0 4px;padding:12px;border-radius:8px;' +
    'background:#ffdd00;color:#000;font-weight:bold;text-align:center;text-decoration:none;">' + label + '</a>';
}

// Bereich "Unterstützen" – steht unter dem Speichern-Knopf, damit niemand
// ungespeicherte Einstellungen verliert, wenn er den Link öffnet.
function supportSection(t) {
  var items = [{ type: 'heading', defaultValue: t.support }];
  if (SUPPORT_URL) {
    items.push({ type: 'text', defaultValue: t.supportText + button(SUPPORT_URL, '☕ Buy me a coffee') });
    items.push({ type: 'text', defaultValue: '<span style="font-size:0.8em;opacity:0.7">' + t.supportHint + '</span>' });
  }
  if (PROJECT_URL) {
    items.push({ type: 'text', defaultValue: t.project + ' <a href="' + PROJECT_URL + '">GitHub</a>' });
  }
  return items.length > 1 ? [{ type: 'section', items: items }] : [];
}

module.exports = function (lang) {
  var t = TEXT[lang] || TEXT.de;
  return [
    { type: 'heading', defaultValue: 'Paperless' },
    { type: 'text', defaultValue: t.intro },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.general },
        {
          type: 'select',
          messageKey: 'CfgLang',
          label: t.language,
          defaultValue: 'de',
          options: [
            { label: 'Deutsch', value: 'de' },
            { label: 'English', value: 'en' }
          ]
        }
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.server },
        {
          type: 'input',
          messageKey: 'CfgUrl',
          label: t.url,
          defaultValue: '',
          attributes: {
            placeholder: 'https://paperless.example.com',
            type: 'url',
            autocapitalize: 'off',
            autocorrect: 'off'
          }
        },
        {
          type: 'input',
          messageKey: 'CfgToken',
          label: t.token,
          description: t.tokenHelp,
          defaultValue: '',
          attributes: {
            placeholder: t.tokenPlaceholder,
            type: 'text',
            autocapitalize: 'off',
            autocorrect: 'off'
          }
        }
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.inbox },
        {
          type: 'input',
          messageKey: 'CfgInboxTag',
          label: t.inboxTag,
          description: t.inboxTagHelp,
          defaultValue: '',
          attributes: {
            placeholder: t.inboxTagPlaceholder,
            type: 'text',
            autocapitalize: 'off',
            autocorrect: 'off'
          }
        },
        {
          type: 'slider',
          messageKey: 'CfgMax',
          label: t.max,
          defaultValue: 15,
          min: 5,
          max: 20,
          step: 1
        }
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.image },
        {
          type: 'radiogroup',
          messageKey: 'CfgSplit',
          label: t.split,
          description: t.splitHelp,
          defaultValue: '0',
          options: [
            { label: t.splitVertical, value: '0' },
            { label: t.splitHorizontal, value: '1' },
            { label: t.splitNone, value: '2' }
          ]
        }
      ]
    },
    { type: 'submit', defaultValue: t.save }
  ].concat(supportSection(t));
};
