# Appstore listing

Texts and assets for the Pebble Appstore. Copy the parts you need when
`pebble publish` asks for them, or paste them into the dashboard
(https://appstore-api.repebble.com/dashboard).

## Name

Paperless

## Short description (asked by `pebble publish`)

Your Paperless-ngx inbox on your watch: view scans, edit tags & correspondent, mark documents as done.

## Long description

**English**

Your Paperless-ngx inbox on your wrist.

- See the newest documents in your inbox with correspondent and date
- Look at the scan: whole page or zoomed in until the text is readable
  (split into columns, rows, or scroll freely – tap and drag on Pebble Time 2)
- Change correspondent, document type, tags and date – Paperless' own suggestions come first
- Mark documents as done: the inbox tag is removed and the next one moves up
- German and English

Private: the app only talks to your own Paperless-ngx server. No tracking, no other servers.

Setup: in the app settings enter your server URL and an API token
(Paperless: profile → My Profile → API auth token).
Just curious? Enter "demo" as server URL to try it with sample data.

Free and open source (MIT), built with AI assistance:
https://github.com/Zeichenfolge/paperless-pebble

**Deutsch**

Deine Paperless-ngx-Inbox am Handgelenk.

- Die neuesten Dokumente deiner Inbox mit Korrespondent und Datum
- Den Scan ansehen: ganze Seite oder so weit gezoomt, dass man den Text lesen kann
  (in Spalten oder Zeilen geteilt oder frei scrollen – auf der Pebble Time 2 per Finger)
- Korrespondent, Dokumenttyp, Tags und Datum ändern – die Vorschläge von Paperless stehen oben
- Dokumente erledigen: der Inbox-Tag wird entfernt, das nächste rückt nach
- Deutsch und Englisch

Privat: Die App spricht nur mit deinem eigenen Paperless-ngx-Server. Kein Tracking, keine anderen Server.

Einrichten: in den Einstellungen Server-URL und API-Token eintragen
(Paperless: Profil → Mein Profil → API-Auth-Token).
Nur neugierig? „demo“ als Server-URL eintragen und mit Beispieldaten ausprobieren.

Kostenlos und Open Source (MIT), entwickelt mit Hilfe von KI:
https://github.com/Zeichenfolge/paperless-pebble

## Category

Tools & Utilities (or Productivity, if offered)

## Source URL

https://github.com/Zeichenfolge/paperless-pebble

## Icons

- `store/icon-small-80.png` (80 × 80)
- `store/icon-large-144.png` (144 × 144)

## Screenshots

File names must start with the platform (`emery_…`, `basalt_…`).
All screenshots use the built-in demo mode (fictional data).

- `store/screenshots/basalt_*.png` – Pebble Time, from the emulator
- `store/screenshots/emery_*.png` – Pebble Time 2: take them on your watch in demo mode, see below

### Taking Pebble Time 2 screenshots

1. Pebble app → Paperless → Settings: server URL `demo`, language English → Save
2. With Dev Connect enabled, open the screen you want on the watch and run, e.g.:

```bash
pebble screenshot --cloudpebble --no-open store/screenshots/emery_1_inbox.png
```
