/*
 * Paperless für Pebble – Inbox-Liste, Detailansicht, Nachrichten
 */
#include "paperless.h"

#define PERSIST_KEY_SPLIT 2

static Window *s_main_window;
static MenuLayer *s_menu_layer;

static Window *s_detail_window;
static ScrollLayer *s_scroll_layer;
static TextLayer *s_detail_title_layer;
static TextLayer *s_detail_body_layer;
static TextLayer *s_detail_footer_layer;
static char s_detail_body[220];
static char s_detail_footer[48];

static Doc s_docs[MAX_DOCS];
static int s_num_docs;
static int s_total;
static bool s_loading = true;
static bool s_has_error;
static char s_status[96];
static char s_header[40];

static int32_t s_detail_doc_id;      // Dokument in der Detailansicht (0 = keins)
static int32_t s_pending_done_id;    // wartet auf Bestätigung "erledigt"
static int s_split = SPLIT_VERTICAL;

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------

void copy_utf8(char *dst, const char *src, size_t size) {
  if (!src) { dst[0] = '\0'; return; }
  size_t len = strlen(src);
  if (len < size) {
    memcpy(dst, src, len + 1);
    return;
  }
  size_t cut = size - 1;
  while (cut > 0 && (((unsigned char)src[cut]) & 0xC0) == 0x80) {
    cut--;  // nicht mitten in einem UTF-8-Zeichen abschneiden
  }
  memcpy(dst, src, cut);
  dst[cut] = '\0';
}

void copy_tuple(DictionaryIterator *iter, uint32_t key, char *dst, size_t size) {
  Tuple *t = dict_find(iter, key);
  copy_utf8(dst, (t && t->type == TUPLE_CSTRING) ? t->value->cstring : "", size);
}

int32_t tuple_int(DictionaryIterator *iter, uint32_t key, int32_t fallback) {
  Tuple *t = dict_find(iter, key);
  if (!t) return fallback;
  if (t->type == TUPLE_INT || t->type == TUPLE_UINT) {
    switch (t->length) {
      case 1: return t->type == TUPLE_INT ? t->value->int8 : t->value->uint8;
      case 2: return t->type == TUPLE_INT ? t->value->int16 : t->value->uint16;
      default: return t->value->int32;
    }
  }
  return fallback;
}

Doc *doc_find(int32_t id) {
  for (int i = 0; i < s_num_docs; i++) {
    if (s_docs[i].id == id) return &s_docs[i];
  }
  return NULL;
}

int settings_split(void) {
  return s_split;
}

static void prv_set_status(const char *text, bool is_error) {
  copy_utf8(s_status, text, sizeof(s_status));
  s_has_error = is_error;
}

static void prv_reload_menu(void) {
  if (s_menu_layer) menu_layer_reload_data(s_menu_layer);
}

DictionaryIterator *comm_begin(int32_t command) {
  DictionaryIterator *out;
  AppMessageResult res = app_message_outbox_begin(&out);
  if (res != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "outbox_begin failed: %d", (int)res);
    return NULL;
  }
  dict_write_int32(out, MESSAGE_KEY_Command, command);
  return out;
}

void comm_send(void) {
  app_message_outbox_send();
}

static void prv_refresh(void) {
  if (!comm_begin(CMD_REFRESH)) {
    prv_set_status(tr(STR_PHONE_UNREACHABLE), true);
    prv_reload_menu();
    return;
  }
  comm_send();
  s_loading = true;
  prv_set_status(tr(STR_LOADING_INBOX), false);
  prv_reload_menu();
}

static void prv_fill_doc(Doc *d, DictionaryIterator *iter) {
  copy_tuple(iter, MESSAGE_KEY_Title, d->title, sizeof(d->title));
  copy_tuple(iter, MESSAGE_KEY_Subtitle, d->subtitle, sizeof(d->subtitle));
  copy_tuple(iter, MESSAGE_KEY_Date, d->date, sizeof(d->date));
  copy_tuple(iter, MESSAGE_KEY_Correspondent, d->correspondent, sizeof(d->correspondent));
  copy_tuple(iter, MESSAGE_KEY_DocType, d->doc_type, sizeof(d->doc_type));
  copy_tuple(iter, MESSAGE_KEY_Tags, d->tags, sizeof(d->tags));
}

// ---------------------------------------------------------------------------
// Detailansicht
// ---------------------------------------------------------------------------

static void prv_detail_set_footer(const char *text) {
  copy_utf8(s_detail_footer, text, sizeof(s_detail_footer));
  if (s_detail_footer_layer) text_layer_set_text(s_detail_footer_layer, s_detail_footer);
}

bool doc_mark_done(int32_t doc_id) {
  if (!doc_id || s_pending_done_id) return false;
  DictionaryIterator *out = comm_begin(CMD_DONE);
  if (!out) return false;
  dict_write_int32(out, MESSAGE_KEY_DocId, doc_id);
  comm_send();
  s_pending_done_id = doc_id;
  return true;
}

static void prv_detail_fill(void) {
  Doc *d = doc_find(s_detail_doc_id);
  // s_scroll_layer existiert nur, solange die Detailansicht geladen ist
  // (window_is_loaded() ist während des Ladens noch false).
  if (!d || !s_scroll_layer) return;

  snprintf(s_detail_body, sizeof(s_detail_body), "%s\n%s%s%s%s%s",
           d->correspondent[0] ? d->correspondent : tr(STR_NO_CORRESPONDENT),
           d->doc_type[0] ? d->doc_type : tr(STR_NO_DOC_TYPE),
           d->date[0] ? tr(STR_DATE_PREFIX) : "", d->date,
           d->tags[0] ? tr(STR_TAGS_PREFIX) : "", d->tags);
  text_layer_set_text(s_detail_title_layer, d->title);
  text_layer_set_text(s_detail_body_layer, s_detail_body);

  Layer *root = window_get_root_layer(s_detail_window);
  GRect bounds = layer_get_bounds(root);
  const int16_t inset = PBL_IF_ROUND_ELSE(18, 6);
  const int16_t width = bounds.size.w - 2 * inset;

  layer_set_frame(text_layer_get_layer(s_detail_title_layer), GRect(inset, 4, width, 2000));
  GSize title_size = text_layer_get_content_size(s_detail_title_layer);
  title_size.h += 6;
  layer_set_frame(text_layer_get_layer(s_detail_title_layer), GRect(inset, 4, width, title_size.h));

  layer_set_frame(text_layer_get_layer(s_detail_body_layer),
                  GRect(inset, 4 + title_size.h, width, 2000));
  GSize body_size = text_layer_get_content_size(s_detail_body_layer);
  body_size.h += 8;
  layer_set_frame(text_layer_get_layer(s_detail_body_layer),
                  GRect(inset, 4 + title_size.h, width, body_size.h));

  scroll_layer_set_content_size(s_scroll_layer,
                                GSize(bounds.size.w, 4 + title_size.h + body_size.h + 8));
  if (!s_pending_done_id) prv_detail_set_footer(tr(STR_FOOTER_ACTIONS));
}

static void prv_detail_select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_pending_done_id) return;
  actions_window_push(s_detail_doc_id);
}

static void prv_detail_select_long(ClickRecognizerRef recognizer, void *context) {
  // Abkürzung: lange drücken = erledigt
  if (doc_mark_done(s_detail_doc_id)) {
    vibes_short_pulse();
    prv_detail_set_footer(tr(STR_MARKING_DONE));
  } else if (!s_pending_done_id) {
    prv_detail_set_footer(tr(STR_PHONE_UNREACHABLE));
  }
}

static void prv_detail_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_detail_select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 600, prv_detail_select_long, NULL);
}

static void prv_detail_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  const int16_t footer_h = PBL_IF_ROUND_ELSE(36, 26);

  window_set_background_color(window, GColorWhite);

  s_scroll_layer = scroll_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h - footer_h));
  scroll_layer_set_click_config_onto_window(s_scroll_layer, window);
  scroll_layer_set_callbacks(s_scroll_layer, (ScrollLayerCallbacks) {
    .click_config_provider = prv_detail_click_config,
  });
  scroll_layer_set_shadow_hidden(s_scroll_layer, true);

  s_detail_title_layer = text_layer_create(GRect(0, 0, bounds.size.w, 30));
  text_layer_set_font(s_detail_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_detail_title_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_overflow_mode(s_detail_title_layer, GTextOverflowModeWordWrap);
  text_layer_set_background_color(s_detail_title_layer, GColorClear);
  scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_detail_title_layer));

  s_detail_body_layer = text_layer_create(GRect(0, 30, bounds.size.w, 30));
  text_layer_set_font(s_detail_body_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_detail_body_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_overflow_mode(s_detail_body_layer, GTextOverflowModeWordWrap);
  text_layer_set_background_color(s_detail_body_layer, GColorClear);
  scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_detail_body_layer));

  layer_add_child(root, scroll_layer_get_layer(s_scroll_layer));

  s_detail_footer_layer = text_layer_create(GRect(0, bounds.size.h - footer_h, bounds.size.w, footer_h));
  text_layer_set_font(s_detail_footer_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_detail_footer_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_detail_footer_layer, PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorBlack));
  text_layer_set_text_color(s_detail_footer_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(s_detail_footer_layer));

  prv_detail_fill();
  scroll_layer_set_content_offset(s_scroll_layer, GPointZero, false);
}

static void prv_detail_window_unload(Window *window) {
  text_layer_destroy(s_detail_footer_layer);
  text_layer_destroy(s_detail_body_layer);
  text_layer_destroy(s_detail_title_layer);
  scroll_layer_destroy(s_scroll_layer);
  s_detail_footer_layer = NULL;
  s_detail_body_layer = NULL;
  s_detail_title_layer = NULL;
  s_scroll_layer = NULL;
  s_detail_doc_id = 0;
}

static void prv_show_detail(int index) {
  s_detail_doc_id = s_docs[index].id;
  window_stack_push(s_detail_window, true);
}

// ---------------------------------------------------------------------------
// Inbox-Liste
// ---------------------------------------------------------------------------

static uint16_t prv_menu_num_sections(MenuLayer *menu, void *ctx) {
  return 1;
}

static uint16_t prv_menu_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  return s_num_docs > 0 ? s_num_docs : 1;  // sonst eine Statuszeile
}

static int16_t prv_menu_header_height(MenuLayer *menu, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_menu_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  if (s_has_error) {
    copy_utf8(s_header, tr(STR_HDR_ERROR), sizeof(s_header));
  } else if (s_loading && s_num_docs == 0) {
    copy_utf8(s_header, tr(STR_HDR_INBOX), sizeof(s_header));
  } else if (s_total == 1) {
    copy_utf8(s_header, tr(STR_HDR_COUNT_ONE), sizeof(s_header));
  } else {
    snprintf(s_header, sizeof(s_header), tr(STR_HDR_COUNT_N), s_total);
  }
  menu_cell_basic_header_draw(ctx, cell, s_header);
}

static int16_t prv_menu_cell_height(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  if (s_num_docs == 0) return PBL_IF_ROUND_ELSE(90, 80);
  return PBL_IF_ROUND_ELSE(menu_layer_is_index_selected(menu, idx) ? 60 : 36, 50);
}

static void prv_menu_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  if (s_num_docs == 0) {
    GRect b = layer_get_bounds(cell);
    graphics_draw_text(ctx, s_status, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       grect_inset(b, GEdgeInsets(6, 8)), GTextOverflowModeWordWrap,
                       GTextAlignmentCenter, NULL);
    return;
  }
  Doc *d = &s_docs[idx->row];
  menu_cell_basic_draw(ctx, cell, d->title, d->subtitle, NULL);
}

static void prv_menu_select(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  if (s_num_docs == 0) {
    prv_refresh();  // Statuszeile: SELECT = erneut versuchen
    return;
  }
  prv_show_detail(idx->row);
}

static void prv_menu_select_long(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  vibes_short_pulse();
  prv_refresh();
}

static void prv_main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu_layer = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_sections = prv_menu_num_sections,
    .get_num_rows = prv_menu_num_rows,
    .get_header_height = prv_menu_header_height,
    .draw_header = prv_menu_draw_header,
    .get_cell_height = prv_menu_cell_height,
    .draw_row = prv_menu_draw_row,
    .select_click = prv_menu_select,
    .select_long_click = prv_menu_select_long,
  });
  menu_layer_set_normal_colors(s_menu_layer, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu_layer,
                                  PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorBlack), GColorWhite);
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));
}

static void prv_main_window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  s_menu_layer = NULL;
}

// ---------------------------------------------------------------------------
// AppMessage
// ---------------------------------------------------------------------------

static void prv_remove_doc(int32_t doc_id) {
  for (int i = 0; i < s_num_docs; i++) {
    if (s_docs[i].id == doc_id) {
      for (int j = i; j < s_num_docs - 1; j++) s_docs[j] = s_docs[j + 1];
      s_num_docs--;
      if (s_total > 0) s_total--;
      break;
    }
  }
  if (s_num_docs == 0) {
    prv_set_status(s_total > 0 ? tr(STR_LOADING_MORE) : tr(STR_INBOX_EMPTY), false);
  }
}

static bool prv_window_open(Window *w) {
  return w && window_stack_contains_window(w);
}

static void prv_apply_config(DictionaryIterator *iter) {
  Language old_lang = i18n_language();
  i18n_set_language((Language)tuple_int(iter, MESSAGE_KEY_Lang, old_lang));
  int split = tuple_int(iter, MESSAGE_KEY_Split, s_split);
  if (split < SPLIT_VERTICAL || split > SPLIT_NONE) split = SPLIT_VERTICAL;
  if (split != s_split) {
    s_split = split;
    persist_write_int(PERSIST_KEY_SPLIT, split);
  }
  if (old_lang != i18n_language()) {
    // Texte, die schon angezeigt werden, neu setzen
    if (!s_has_error && s_num_docs == 0) {
      prv_set_status(s_loading ? tr(STR_LOADING_INBOX) : tr(STR_INBOX_EMPTY), false);
    }
    prv_detail_fill();
  }
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  int32_t type = tuple_int(iter, MESSAGE_KEY_Type, 0);

  if (image_window_handle_message(type, iter)) return;
  if (option_window_handle_message(type, iter)) return;

  switch (type) {
    case MSG_CONFIG:
      prv_apply_config(iter);
      break;
    case MSG_LIST_BEGIN: {
      s_total = tuple_int(iter, MESSAGE_KEY_Total, 0);
      s_loading = true;
      s_has_error = false;
      s_num_docs = 0;
      prv_set_status(s_total == 0 ? tr(STR_INBOX_EMPTY) : tr(STR_LOADING_DOCS), false);
      break;
    }
    case MSG_ITEM: {
      int32_t i = tuple_int(iter, MESSAGE_KEY_Index, -1);
      if (i < 0 || i >= MAX_DOCS) break;
      Doc *d = &s_docs[i];
      d->id = tuple_int(iter, MESSAGE_KEY_DocId, 0);
      prv_fill_doc(d, iter);
      if (i + 1 > s_num_docs) s_num_docs = i + 1;
      if (d->id == s_detail_doc_id) prv_detail_fill();
      break;
    }
    case MSG_LIST_END:
      s_loading = false;
      s_has_error = false;
      // Geöffnetes Dokument ist nicht mehr in der Inbox -> schließen
      if (s_detail_doc_id && !doc_find(s_detail_doc_id) && !s_pending_done_id &&
          prv_window_open(s_detail_window) && !option_window_is_open() && !actions_window_is_open()) {
        window_stack_remove(s_detail_window, false);
      }
      break;
    case MSG_DOC_UPDATE: {
      Doc *d = doc_find(tuple_int(iter, MESSAGE_KEY_DocId, 0));
      if (d) {
        prv_fill_doc(d, iter);
        if (d->id == s_detail_doc_id) prv_detail_fill();
      }
      break;
    }
    case MSG_ERROR: {
      char text[96];
      copy_tuple(iter, MESSAGE_KEY_Text, text, sizeof(text));
      s_loading = false;
      if (option_window_is_open()) {
        option_window_show_error(text);
      } else if (s_pending_done_id) {
        s_pending_done_id = 0;
        vibes_double_pulse();
        if (actions_window_is_open()) {
          actions_window_set_status(text);
        } else {
          prv_detail_set_footer(tr(STR_ERROR_RETRY));
        }
      } else {
        prv_set_status(text, true);
      }
      break;
    }
    case MSG_STATUS: {
      char text[96];
      copy_tuple(iter, MESSAGE_KEY_Text, text, sizeof(text));
      prv_set_status(text, false);
      break;
    }
    case MSG_DONE: {
      int32_t id = tuple_int(iter, MESSAGE_KEY_DocId, 0);
      bool was_open = id && (s_pending_done_id == id || s_detail_doc_id == id);
      if (s_pending_done_id == id) s_pending_done_id = 0;
      prv_remove_doc(id);
      vibes_short_pulse();
      if (was_open) {
        // Zurück zur Liste: Aktionen und Detailansicht schließen
        actions_window_close(false);
        if (prv_window_open(s_detail_window)) window_stack_remove(s_detail_window, true);
      }
      break;
    }
    default:
      break;
  }
  prv_reload_menu();
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Message dropped: %d", (int)reason);
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Outbox failed: %d", (int)reason);
  if (option_window_is_open()) {
    option_window_show_error(tr(STR_PHONE_UNREACHABLE));
  } else if (s_pending_done_id) {
    s_pending_done_id = 0;
    if (actions_window_is_open()) {
      actions_window_set_status(tr(STR_SEND_FAILED));
    } else {
      prv_detail_set_footer(tr(STR_SEND_FAILED));
    }
  } else {
    prv_set_status(tr(STR_PHONE_UNREACHABLE), true);
  }
  prv_reload_menu();
}

// ---------------------------------------------------------------------------
// App-Lebenszyklus
// ---------------------------------------------------------------------------

static void prv_init(void) {
  i18n_init();
  if (persist_exists(PERSIST_KEY_SPLIT)) {
    s_split = persist_read_int(PERSIST_KEY_SPLIT);
    if (s_split < SPLIT_VERTICAL || s_split > SPLIT_NONE) s_split = SPLIT_VERTICAL;
  }
  copy_utf8(s_status, tr(STR_CONNECTING), sizeof(s_status));

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = prv_main_window_load,
    .unload = prv_main_window_unload,
  });

  s_detail_window = window_create();
  window_set_window_handlers(s_detail_window, (WindowHandlers) {
    .load = prv_detail_window_load,
    .unload = prv_detail_window_unload,
  });

  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_failed(prv_outbox_failed);
  // Bilddaten kommen in Stücken von bis zu 2 KB
  uint32_t inbox = app_message_inbox_size_maximum();
  app_message_open(inbox > 4096 ? 4096 : inbox, 256);

  window_stack_push(s_main_window, true);
  // Das Laden startet das Handy selbst, sobald PebbleKit JS bereit ist.
}

static void prv_deinit(void) {
  image_window_destroy();
  option_window_destroy();
  actions_window_close(false);
  window_destroy(s_detail_window);
  window_destroy(s_main_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
