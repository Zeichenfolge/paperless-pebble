/*
 * Paperless für Pebble – Auswahlliste zum Bearbeiten
 * (Korrespondent, Dokumenttyp, Tags, Datum)
 *
 * Das Handy schickt Vorschläge von Paperless zuerst, danach die am häufigsten
 * verwendeten Einträge. Einfachauswahl: SELECT speichert und schließt.
 * Tags: SELECT schaltet einen Tag an/aus (wird sofort gespeichert).
 */
#include "paperless.h"

#define MAX_OPTS 32

typedef struct {
  int32_t id;
  char name[36];
  uint8_t flags;
} Option;

static Window *s_window;
static MenuLayer *s_menu;

static int32_t s_doc_id;
static int s_field;
static Option s_opts[MAX_OPTS];
static int s_num;
static bool s_loaded;
static bool s_saving;
static int s_toggle_index = -1;   // Tags: welche Zeile gerade gespeichert wird
static char s_status[64];
static char s_header[48];

static const char *prv_field_name(int field) {
  switch (field) {
    case FIELD_CORRESPONDENT: return tr(STR_ACT_CORRESPONDENT);
    case FIELD_DOC_TYPE:      return tr(STR_ACT_DOC_TYPE);
    case FIELD_TAGS:          return tr(STR_ACT_TAGS);
    case FIELD_DATE:          return tr(STR_ACT_DATE);
    default:                  return "";
  }
}

static void prv_set_status(const char *text) {
  copy_utf8(s_status, text, sizeof(s_status));
  if (s_menu) menu_layer_reload_data(s_menu);
}

bool option_window_is_open(void) {
  return s_window != NULL;
}

void option_window_show_error(const char *text) {
  if (!s_window) return;
  // Fehlgeschlagenes Umschalten eines Tags rückgängig machen
  if (s_toggle_index >= 0 && s_toggle_index < s_num) {
    s_opts[s_toggle_index].flags ^= OPT_FLAG_SELECTED;
  }
  s_toggle_index = -1;
  s_saving = false;
  vibes_double_pulse();
  prv_set_status(text);
}

// ---------------------------------------------------------------------------
// Menü
// ---------------------------------------------------------------------------

static uint16_t prv_num_rows(MenuLayer *m, uint16_t section, void *ctx) {
  return s_num > 0 ? s_num : 1;
}

static int16_t prv_header_height(MenuLayer *m, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  if (s_status[0] && s_num > 0) {
    copy_utf8(s_header, s_status, sizeof(s_header));
  } else {
    copy_utf8(s_header, prv_field_name(s_field), sizeof(s_header));
  }
  menu_cell_basic_header_draw(ctx, cell, s_header);
}

static int16_t prv_cell_height(MenuLayer *m, MenuIndex *idx, void *ctx) {
  return s_num == 0 ? 80 : 44;
}

static void prv_draw_checkbox(GContext *ctx, GRect cell, bool checked, bool highlighted) {
  GRect box = GRect(cell.size.w - 24, (cell.size.h - 16) / 2, 16, 16);
  GColor fg = highlighted ? GColorWhite : GColorBlack;
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_rect(ctx, box);
  if (checked) {
    graphics_context_set_fill_color(ctx, fg);
    graphics_fill_rect(ctx, grect_inset(box, GEdgeInsets(4)), 0, GCornerNone);
  }
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  GRect b = layer_get_bounds(cell);
  if (s_num == 0) {
    graphics_draw_text(ctx, s_status[0] ? s_status : tr(STR_LOADING), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       grect_inset(b, GEdgeInsets(6, 8)), GTextOverflowModeWordWrap,
                       GTextAlignmentCenter, NULL);
    return;
  }
  Option *o = &s_opts[idx->row];
  bool selected = o->flags & OPT_FLAG_SELECTED;
  bool suggested = o->flags & OPT_FLAG_SUGGESTED;
  const char *sub = NULL;
  if (s_field == FIELD_TAGS) {
    sub = suggested ? tr(STR_OPT_SUGGESTED) : NULL;
  } else if (selected && suggested) {
    sub = tr(STR_OPT_CURRENT_SUGGESTED);
  } else if (selected) {
    sub = tr(STR_OPT_CURRENT);
  } else if (suggested) {
    sub = tr(STR_OPT_SUGGESTED);
  }

  if (s_field == FIELD_TAGS) {
    // Selbst zeichnen, damit rechts Platz für die Checkbox bleibt
    bool hl = menu_cell_layer_is_highlighted(cell);
    graphics_context_set_text_color(ctx, hl ? GColorWhite : GColorBlack);
    int16_t text_w = b.size.w - 36;
    graphics_draw_text(ctx, o->name, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(6, sub ? -2 : 6, text_w, 28), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
    if (sub) {
      graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                         GRect(6, 24, text_w, 18), GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentLeft, NULL);
    }
    prv_draw_checkbox(ctx, b, selected, hl);
  } else {
    menu_cell_basic_draw(ctx, cell, o->name, sub, NULL);
  }
}

static void prv_select(MenuLayer *m, MenuIndex *idx, void *ctx) {
  if (s_num == 0 || s_saving || idx->row >= s_num) return;
  Option *o = &s_opts[idx->row];

  uint8_t new_flags = OPT_FLAG_SELECTED;
  if (s_field == FIELD_TAGS) {
    new_flags = (o->flags & OPT_FLAG_SELECTED) ? 0 : OPT_FLAG_SELECTED;
  } else if (o->flags & OPT_FLAG_SELECTED) {
    window_stack_remove(s_window, true);  // unverändert
    return;
  }

  DictionaryIterator *out = comm_begin(CMD_SET);
  if (!out) {
    prv_set_status(tr(STR_PHONE_UNREACHABLE));
    return;
  }
  dict_write_int32(out, MESSAGE_KEY_DocId, s_doc_id);
  dict_write_int32(out, MESSAGE_KEY_Field, s_field);
  dict_write_int32(out, MESSAGE_KEY_OptId, o->id);
  dict_write_int32(out, MESSAGE_KEY_Flags, new_flags);
  comm_send();

  s_saving = true;
  if (s_field == FIELD_TAGS) {
    o->flags ^= OPT_FLAG_SELECTED;  // sofort anzeigen, bei Fehler zurücknehmen
    s_toggle_index = idx->row;
  }
  prv_set_status(tr(STR_SAVING));
}

// ---------------------------------------------------------------------------
// Fenster
// ---------------------------------------------------------------------------

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_num_rows,
    .get_header_height = prv_header_height,
    .draw_header = prv_draw_header,
    .get_cell_height = prv_cell_height,
    .draw_row = prv_draw_row,
    .select_click = prv_select,
  });
  menu_layer_set_normal_colors(s_menu, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu, PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorBlack), GColorWhite);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void prv_window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void option_window_push(int32_t doc_id, int field) {
  if (s_window || !doc_id) return;
  s_doc_id = doc_id;
  s_field = field;
  s_num = 0;
  s_loaded = false;
  s_saving = false;
  s_toggle_index = -1;
  copy_utf8(s_status, tr(STR_OPT_LOADING), sizeof(s_status));

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  DictionaryIterator *out = comm_begin(CMD_OPTIONS);
  if (!out) {
    prv_set_status(tr(STR_PHONE_UNREACHABLE));
    return;
  }
  dict_write_int32(out, MESSAGE_KEY_DocId, doc_id);
  dict_write_int32(out, MESSAGE_KEY_Field, field);
  comm_send();
}

void option_window_destroy(void) {
  if (s_window) window_stack_remove(s_window, false);
}

// ---------------------------------------------------------------------------
// Nachrichten vom Handy
// ---------------------------------------------------------------------------

bool option_window_handle_message(int32_t type, DictionaryIterator *iter) {
  if (type != MSG_OPT_BEGIN && type != MSG_OPT_ITEM && type != MSG_OPT_END && type != MSG_SAVED) {
    return false;
  }
  if (!s_window) return true;

  switch (type) {
    case MSG_OPT_BEGIN:
      if (tuple_int(iter, MESSAGE_KEY_DocId, 0) != s_doc_id ||
          tuple_int(iter, MESSAGE_KEY_Field, 0) != s_field) {
        return true;
      }
      s_num = 0;
      break;
    case MSG_OPT_ITEM: {
      int32_t i = tuple_int(iter, MESSAGE_KEY_Index, -1);
      if (i < 0 || i >= MAX_OPTS) break;
      Option *o = &s_opts[i];
      o->id = tuple_int(iter, MESSAGE_KEY_OptId, 0);
      o->flags = (uint8_t)tuple_int(iter, MESSAGE_KEY_Flags, 0);
      copy_tuple(iter, MESSAGE_KEY_Title, o->name, sizeof(o->name));
      if (i + 1 > s_num) s_num = i + 1;
      break;
    }
    case MSG_OPT_END: {
      s_loaded = true;
      s_status[0] = '\0';
      if (s_num == 0) copy_utf8(s_status, tr(STR_NO_ENTRIES), sizeof(s_status));
      // Auf den aktuellen Eintrag springen (bei Tags: oben bleiben)
      if (s_field != FIELD_TAGS && s_menu) {
        for (int i = 0; i < s_num; i++) {
          if (s_opts[i].flags & OPT_FLAG_SELECTED) {
            menu_layer_set_selected_index(s_menu, (MenuIndex) { .section = 0, .row = i },
                                          MenuRowAlignCenter, false);
            break;
          }
        }
      }
      break;
    }
    case MSG_SAVED:
      if (tuple_int(iter, MESSAGE_KEY_DocId, 0) != s_doc_id) return true;
      s_saving = false;
      s_toggle_index = -1;
      vibes_short_pulse();
      if (s_field == FIELD_TAGS) {
        copy_utf8(s_status, tr(STR_SAVED), sizeof(s_status));
      } else {
        window_stack_remove(s_window, true);
        return true;
      }
      break;
  }
  if (s_menu) menu_layer_reload_data(s_menu);
  return true;
}
