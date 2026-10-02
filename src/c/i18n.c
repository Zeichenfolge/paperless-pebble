/*
 * Paperless für Pebble – Texte in Deutsch und Englisch
 * Die Sprache kommt aus den Einstellungen (Handy) und wird auf der Uhr
 * gespeichert, damit schon der Startbildschirm in der richtigen Sprache ist.
 */
#include "i18n.h"

#define PERSIST_KEY_LANG 1

static Language s_lang = LANG_DE;

static const char *const s_de[STR_COUNT] = {
  [STR_CONNECTING]            = "Verbinde mit Handy...",
  [STR_PHONE_UNREACHABLE]     = "Handy nicht erreichbar",
  [STR_LOADING_INBOX]         = "Lade Inbox...",
  [STR_LOADING_DOCS]          = "Lade Dokumente...",
  [STR_INBOX_EMPTY]           = "Inbox ist leer. Gut gemacht!",
  [STR_LOADING_MORE]          = "Lade weitere...",
  [STR_HDR_ERROR]             = "Paperless - Fehler",
  [STR_HDR_INBOX]             = "Paperless Inbox",
  [STR_HDR_COUNT_ONE]         = "Inbox: 1 Dokument",
  [STR_HDR_COUNT_N]           = "Inbox: %d Dokumente",
  [STR_NO_CORRESPONDENT]      = "Kein Korrespondent",
  [STR_NO_DOC_TYPE]           = "Kein Dokumenttyp",
  [STR_DATE_PREFIX]           = "\nDatum: ",
  [STR_TAGS_PREFIX]           = "\nTags: ",
  [STR_FOOTER_ACTIONS]        = "SELECT: Aktionen",
  [STR_MARKING_DONE]          = "Wird erledigt...",
  [STR_ERROR_RETRY]           = "Fehler - nochmal?",
  [STR_SEND_FAILED]           = "Senden fehlgeschlagen",
  [STR_HDR_ACTIONS]           = "Aktionen",
  [STR_ACT_IMAGE]             = "Bild ansehen",
  [STR_ACT_CORRESPONDENT]     = "Korrespondent",
  [STR_ACT_DOC_TYPE]          = "Dokumenttyp",
  [STR_ACT_TAGS]              = "Tags",
  [STR_ACT_DATE]              = "Datum",
  [STR_ACT_DONE]              = "Erledigt",
  [STR_ACT_DONE_SUB]          = "Aus Inbox entfernen",
  [STR_IMG_LOADING]           = "Lade Bild...",
  [STR_IMG_LOADING_PART]      = "Lade Ausschnitt...",
  [STR_IMG_INVALID]           = "Ungültiges Bild",
  [STR_IMG_NO_MEMORY]         = "Zu wenig Speicher",
  [STR_LOADING]               = "Lade...",
  [STR_OPT_LOADING]           = "Lade Vorschläge...",
  [STR_OPT_CURRENT]           = "Aktuell",
  [STR_OPT_SUGGESTED]         = "Vorschlag",
  [STR_OPT_CURRENT_SUGGESTED] = "Aktuell + Vorschlag",
  [STR_SAVING]                = "Speichere...",
  [STR_SAVED]                 = "Gespeichert",
  [STR_NO_ENTRIES]            = "Keine Einträge gefunden",
};

static const char *const s_en[STR_COUNT] = {
  [STR_CONNECTING]            = "Connecting to phone...",
  [STR_PHONE_UNREACHABLE]     = "Phone not reachable",
  [STR_LOADING_INBOX]         = "Loading inbox...",
  [STR_LOADING_DOCS]          = "Loading documents...",
  [STR_INBOX_EMPTY]           = "Inbox is empty. Well done!",
  [STR_LOADING_MORE]          = "Loading more...",
  [STR_HDR_ERROR]             = "Paperless - Error",
  [STR_HDR_INBOX]             = "Paperless Inbox",
  [STR_HDR_COUNT_ONE]         = "Inbox: 1 document",
  [STR_HDR_COUNT_N]           = "Inbox: %d documents",
  [STR_NO_CORRESPONDENT]      = "No correspondent",
  [STR_NO_DOC_TYPE]           = "No document type",
  [STR_DATE_PREFIX]           = "\nDate: ",
  [STR_TAGS_PREFIX]           = "\nTags: ",
  [STR_FOOTER_ACTIONS]        = "SELECT: Actions",
  [STR_MARKING_DONE]          = "Marking as done...",
  [STR_ERROR_RETRY]           = "Error - try again?",
  [STR_SEND_FAILED]           = "Sending failed",
  [STR_HDR_ACTIONS]           = "Actions",
  [STR_ACT_IMAGE]             = "View image",
  [STR_ACT_CORRESPONDENT]     = "Correspondent",
  [STR_ACT_DOC_TYPE]          = "Document type",
  [STR_ACT_TAGS]              = "Tags",
  [STR_ACT_DATE]              = "Date",
  [STR_ACT_DONE]              = "Done",
  [STR_ACT_DONE_SUB]          = "Remove from inbox",
  [STR_IMG_LOADING]           = "Loading image...",
  [STR_IMG_LOADING_PART]      = "Loading section...",
  [STR_IMG_INVALID]           = "Invalid image",
  [STR_IMG_NO_MEMORY]         = "Not enough memory",
  [STR_LOADING]               = "Loading...",
  [STR_OPT_LOADING]           = "Loading suggestions...",
  [STR_OPT_CURRENT]           = "Current",
  [STR_OPT_SUGGESTED]         = "Suggestion",
  [STR_OPT_CURRENT_SUGGESTED] = "Current + suggestion",
  [STR_SAVING]                = "Saving...",
  [STR_SAVED]                 = "Saved",
  [STR_NO_ENTRIES]            = "No entries found",
};

void i18n_init(void) {
  if (persist_exists(PERSIST_KEY_LANG)) {
    s_lang = persist_read_int(PERSIST_KEY_LANG) == LANG_EN ? LANG_EN : LANG_DE;
  }
}

void i18n_set_language(Language lang) {
  lang = lang == LANG_EN ? LANG_EN : LANG_DE;
  if (lang != s_lang) {
    s_lang = lang;
    persist_write_int(PERSIST_KEY_LANG, lang);
  }
}

Language i18n_language(void) {
  return s_lang;
}

const char *tr(StrId id) {
  if (id >= STR_COUNT) return "";
  const char *s = (s_lang == LANG_EN ? s_en : s_de)[id];
  return s ? s : "";
}
