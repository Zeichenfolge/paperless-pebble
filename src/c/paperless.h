/*
 * Paperless für Pebble – gemeinsame Definitionen
 *
 * Die Uhr spricht nie direkt mit dem Server: PebbleKit JS auf dem Handy
 * (src/pkjs/index.js) ruft die Paperless-API auf und schickt die Ergebnisse
 * per AppMessage. Die Nummern unten müssen zu src/pkjs/index.js passen.
 */
#pragma once
#include <pebble.h>
#include "i18n.h"

#define MAX_DOCS 20

// Nachrichten Handy -> Uhr (Key "Type")
enum {
  MSG_LIST_BEGIN = 1,   // Total, Num
  MSG_ITEM       = 2,   // Index, DocId, Title, Subtitle, Date, Correspondent, DocType, Tags
  MSG_LIST_END   = 3,
  MSG_ERROR      = 4,   // Text
  MSG_DONE       = 5,   // DocId: Inbox-Tag entfernt
  MSG_STATUS     = 6,   // Text
  MSG_IMG_BEGIN  = 7,   // Seq, DocId, ImgMode, ImgCol, Width, Height, Split, Num, Step, SrcW, SrcH
  MSG_IMG_DATA   = 8,   // Seq, Offset, Data
  MSG_IMG_END    = 9,   // Seq
  MSG_IMG_ERROR  = 10,  // Seq, Text
  MSG_OPT_BEGIN  = 11,  // DocId, Field, Num
  MSG_OPT_ITEM   = 12,  // Index, OptId, Title, Flags
  MSG_OPT_END    = 13,
  MSG_DOC_UPDATE = 14,  // DocId + Felder wie MSG_ITEM
  MSG_SAVED      = 15,  // DocId, Field
  MSG_IMG_STATUS = 16,  // Seq, Text
  MSG_CONFIG     = 17,  // Lang, Split
};

// Befehle Uhr -> Handy (Key "Command")
enum {
  CMD_REFRESH      = 1,
  CMD_DONE         = 2,  // DocId
  CMD_IMAGE        = 3,  // DocId, ImgMode, ImgCol, Seq, Width, Height, Split, MaxBytes
  CMD_IMAGE_CANCEL = 4,
  CMD_OPTIONS      = 5,  // DocId, Field
  CMD_SET          = 6,  // DocId, Field, OptId, Flags
};

// Bearbeitbare Felder
enum {
  FIELD_CORRESPONDENT = 1,
  FIELD_DOC_TYPE      = 2,
  FIELD_TAGS          = 3,
  FIELD_DATE          = 4,
};

// Bildmodi
enum {
  IMG_MODE_PAGE = 0,  // ganze Seite
  IMG_MODE_ZOOM = 1,  // Ausschnitt in voller Auflösung
};

// Bild-Aufteilung (Einstellung)
enum {
  SPLIT_VERTICAL   = 0,  // senkrechte Streifen (Spalten)
  SPLIT_HORIZONTAL = 1,  // waagerechte Streifen (Zeilen)
  SPLIT_NONE       = 2,  // ganze Seite, frei scrollen
};

// Flags für Auswahl-Optionen
#define OPT_FLAG_SELECTED  1
#define OPT_FLAG_SUGGESTED 2

typedef struct {
  int32_t id;
  char title[72];
  char subtitle[48];
  char date[12];
  char correspondent[40];
  char doc_type[32];
  char tags[72];
} Doc;

// --- main.c --------------------------------------------------------------
Doc *doc_find(int32_t id);
// Startet eine Nachricht an das Handy. Gibt NULL zurück, wenn das gerade nicht geht.
DictionaryIterator *comm_begin(int32_t command);
void comm_send(void);
void copy_utf8(char *dst, const char *src, size_t size);
void copy_tuple(DictionaryIterator *iter, uint32_t key, char *dst, size_t size);
int32_t tuple_int(DictionaryIterator *iter, uint32_t key, int32_t fallback);
int settings_split(void);
// Schickt "erledigt" für das Dokument; Ergebnis kommt als MSG_DONE/MSG_ERROR
bool doc_mark_done(int32_t doc_id);

// --- actions_window.c ----------------------------------------------------
void actions_window_push(int32_t doc_id);
bool actions_window_is_open(void);
void actions_window_set_status(const char *text);
void actions_window_close(bool animated);

// --- image_window.c ------------------------------------------------------
void image_window_push(int32_t doc_id);
// true, wenn die Nachricht für das Bildfenster war
bool image_window_handle_message(int32_t type, DictionaryIterator *iter);
void image_window_destroy(void);

// --- option_window.c -----------------------------------------------------
void option_window_push(int32_t doc_id, int field);
bool option_window_handle_message(int32_t type, DictionaryIterator *iter);
void option_window_show_error(const char *text);
bool option_window_is_open(void);
void option_window_destroy(void);
