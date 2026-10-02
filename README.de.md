# Paperless für Pebble

Deine [Paperless-ngx](https://docs.paperless-ngx.com/)-Inbox am Handgelenk.
Neue Dokumente durchsehen, den Scan anschauen, Korrespondent, Dokumenttyp, Tags und
Datum korrigieren und Dokumente als erledigt markieren – direkt auf der Pebble.

![Screenshots](docs/screenshots.png)

*Screenshots aus dem Emulator mit erfundenen Beispieldaten.*

[English version](README.md) · [Änderungen](CHANGELOG.md) · [Mitmachen](CONTRIBUTING.md)

Wenn dir die App gefällt, freue ich mich über einen [Kaffee](https://buymeacoffee.com/michaelrunge) ☕ –
kostenlos und Open Source bleibt sie so oder so.

## Funktionen

- **Inbox** – die neuesten Dokumente mit deinem Inbox-Tag, mit Korrespondent und Datum
- **Dokumentbild** – die erste Seite in 4 Graustufen, dazu ein Zoom, in dem man Text lesen kann.
  Die Seite lässt sich in Spalten oder Zeilen teilen oder frei scrollen.
  Auf der Pebble Time 2: antippen zum Zoomen, mit dem Finger verschieben.
- **Bearbeiten** – Korrespondent, Dokumenttyp, Tags und Datum; die Vorschläge von Paperless stehen oben
- **Erledigt** – entfernt den Inbox-Tag, das nächste Dokument rückt nach
- **Deutsch und Englisch**
- **Privat** – die App spricht nur mit *deinem* Paperless-Server. Sonst nichts, kein Tracking.

## Voraussetzungen

- Eine Pebble. Gemacht für die **Pebble Time 2**; Pebble Time und Pebble 2 gehen auch (kleinerer Bildschirm).
- Die Pebble-App auf iPhone oder Android
- Ein Paperless-ngx-Server, den das Handy erreicht, und ein API-Token

## Installation

1. `paperless-pebble.pbw` aus dem [neuesten Release](../../releases/latest) laden und
   mit der Pebble-App öffnen – **oder** selbst bauen (siehe unten).
2. In der Pebble-App **Paperless → Einstellungen** öffnen und eintragen:
   - Server-URL, z. B. `https://paperless.example.de`
   - API-Token (Paperless: Profil oben rechts → *Mein Profil* → API-Auth-Token)
   - optional den Namen deines Inbox-Tags (leer = die in Paperless als Posteingangs-Tag markierten Tags)

**Nur mal ausprobieren?** Als Server-URL `demo` eintragen – dann zeigt die App erfundene
Beispieldokumente, ganz ohne Server und Token.

## Bedienung

| Wo | Taste | Aktion |
|---|---|---|
| Inbox | UP / DOWN | blättern |
| | SELECT | Dokument öffnen |
| | SELECT lang | neu laden |
| Dokument | SELECT | Aktionen: Bild ansehen, Korrespondent, Dokumenttyp, Tags, Datum, Erledigt |
| | SELECT lang | sofort erledigt |
| Aktionen | BACK nach einer Aktion | zurück zu den Aktionen |
| Bild-Übersicht | UP / DOWN | roten Rahmen verschieben |
| | SELECT | in den Rahmen zoomen |
| Zoom | UP / DOWN | scrollen |
| | SELECT | nächste Spalte/Zeile bzw. ↕/↔ wechseln (nicht teilen) |
| | BACK | zurück zur Übersicht |

**Bild aufteilen** (Einstellung): *Vertikal* – Spalten, hoch/runter scrollen ·
*Horizontal* – Zeilen, links/rechts scrollen · *Nicht teilen* – ganze Seite, in beide Richtungen.

## Selbst bauen

```bash
# Linux / macOS / Windows (WSL) – siehe https://developer.repebble.com/sdk/
uv tool install pebble-tool --python 3.13
pebble sdk install latest

pebble build
pebble install --emulator emery     # Emulator
pebble install --cloudpebble        # Uhr (Dev Connect in der Pebble-App einschalten)
```

Aufbau des Projekts und technische Details: siehe [README.md](README.md#how-it-works).

## Wie die App entstanden ist

Die App wurde mit Hilfe von KI (Claude von Anthropic) entwickelt: Code,
Bildverarbeitung und Dokumentation sind gemeinsam mit der KI entstanden,
wurden gebaut, auf einer echten Pebble Time 2 getestet und von mir geprüft.
Issues und Pull Requests bearbeite ich selbst.

## Lizenz

[MIT](LICENSE) © 2026 Michael Runge. Fremder Code: siehe [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Kein offizielles Projekt von Paperless-ngx oder Core Devices.
