/*
 * Paperless für Pebble – Aktionen zu einem Dokument
 *
 * Eigenes Fenster (statt des System-Aktionsmenüs), damit man nach einer
 * Aktion mit BACK wieder hier landet und direkt die nächste wählen kann.
 * Die Icons werden mit einfachen Grafikbefehlen gezeichnet.
 */
#include "paperless.h"

enum {
  ACT_IMAGE = 0,
  ACT_CORRESPONDENT,
  ACT_DOC_TYPE,
  ACT_TAGS,
  ACT_DATE,
  ACT_DONE,
  ACT_COUNT
};

static Window *s_window;
static MenuLayer *s_menu;
static int32_t s_doc_id;
static bool s_busy;            // "erledigt" läuft
static char s_status[64];

static const StrId s_labels[ACT_COUNT] = {
  STR_ACT_IMAGE, STR_ACT_CORRESPONDENT, STR_ACT_DOC_TYPE, STR_ACT_TAGS, STR_ACT_DATE, STR_ACT_DONE,
};

// ---------------------------------------------------------------------------
// Icons (22 x 22)
// ---------------------------------------------------------------------------

static void prv_line(GContext *ctx, int x0, int y0, int x1, int y1) {
  graphics_draw_line(ctx, GPoint(x0, y0), GPoint(x1, y1));
}

static void prv_draw_icon(GContext *ctx, int action, GPoint o, bool hl) {
  GColor fg = hl ? GColorWhite : GColorBlack;
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_fill_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, 2);
  const int x = o.x, y = o.y;

  switch (action) {
    case ACT_IMAGE:  // Bild: Rahmen, Sonne, Berg
      graphics_draw_round_rect(ctx, GRect(x + 1, y + 3, 20, 16), 2);
      graphics_fill_circle(ctx, GPoint(x + 15, y + 8), 2);
      prv_line(ctx, x + 3, y + 17, x + 9, y + 10);
      prv_line(ctx, x + 9, y + 10, x + 14, y + 17);
      prv_line(ctx, x + 12, y + 14, x + 15, y + 12);
      prv_line(ctx, x + 15, y + 12, x + 19, y + 17);
      break;
    case ACT_CORRESPONDENT:  // Person
      graphics_fill_circle(ctx, GPoint(x + 11, y + 6), 5);
      graphics_fill_rect(ctx, GRect(x + 3, y + 13, 16, 9), 6, GCornersTop);
      break;
    case ACT_DOC_TYPE:  // Dokument mit Zeilen und Eselsohr
      prv_line(ctx, x + 4, y + 1, x + 13, y + 1);
      prv_line(ctx, x + 13, y + 1, x + 18, y + 6);
      prv_line(ctx, x + 18, y + 6, x + 18, y + 21);
      prv_line(ctx, x + 18, y + 21, x + 4, y + 21);
      prv_line(ctx, x + 4, y + 21, x + 4, y + 1);
      prv_line(ctx, x + 7, y + 10, x + 15, y + 10);
      prv_line(ctx, x + 7, y + 14, x + 15, y + 14);
      prv_line(ctx, x + 7, y + 18, x + 12, y + 18);
      break;
    case ACT_TAGS:  // Etikett mit Loch
      prv_line(ctx, x + 1, y + 11, x + 8, y + 3);
      prv_line(ctx, x + 8, y + 3, x + 20, y + 3);
      prv_line(ctx, x + 20, y + 3, x + 20, y + 19);
      prv_line(ctx, x + 20, y + 19, x + 8, y + 19);
      prv_line(ctx, x + 8, y + 19, x + 1, y + 11);
      graphics_fill_circle(ctx, GPoint(x + 8, y + 11), 2);
      break;
    case ACT_DATE:  // Kalender
      graphics_draw_rect(ctx, GRect(x + 2, y + 4, 19, 17));
      graphics_fill_rect(ctx, GRect(x + 2, y + 4, 19, 5), 0, GCornerNone);
      prv_line(ctx, x + 7, y + 1, x + 7, y + 5);
      prv_line(ctx, x + 15, y + 1, x + 15, y + 5);
      for (int r = 0; r < 2; r++) {
        for (int c = 0; c < 3; c++) {
          graphics_fill_rect(ctx, GRect(x + 5 + c * 5, y + 11 + r * 5, 3, 3), 0, GCornerNone);
        }
      }
      break;
    case ACT_DONE: {  // Grüner Kreis mit Haken
      GColor circle = hl ? GColorWhite : PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorBlack);
      GColor check = hl ? PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorBlack) : GColorWhite;
      graphics_context_set_fill_color(ctx, circle);
      graphics_fill_circle(ctx, GPoint(x + 11, y + 11), 11);
      graphics_context_set_stroke_color(ctx, check);
      graphics_context_set_stroke_width(ctx, 3);
      prv_line(ctx, x + 5, y + 11, x + 9, y + 16);
      prv_line(ctx, x + 9, y + 16, x + 17, y + 6);
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Menü
// ---------------------------------------------------------------------------

static uint16_t prv_num_rows(MenuLayer *m, uint16_t section, void *ctx) {
  return ACT_COUNT;
}

static int16_t prv_header_height(MenuLayer *m, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  menu_cell_basic_header_draw(ctx, cell, s_status[0] ? s_status : tr(STR_HDR_ACTIONS));
}

static int16_t prv_cell_height(MenuLayer *m, MenuIndex *idx, void *ctx) {
  return idx->row == ACT_DONE ? 50 : 42;
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  const int icon_x = PBL_IF_ROUND_ELSE(20, 8);
  const int text_x = icon_x + 30;
  const int text_w = b.size.w - text_x - 4;
  bool done = idx->row == ACT_DONE;

  prv_draw_icon(ctx, idx->row, GPoint(icon_x, (b.size.h - 22) / 2), hl);

  graphics_context_set_text_color(ctx, hl ? GColorWhite : GColorBlack);
  int title_y = done ? 0 : (b.size.h - 30) / 2;
  graphics_draw_text(ctx, tr(s_labels[idx->row]), fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(text_x, title_y, text_w, 28), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  if (done) {
    graphics_draw_text(ctx, tr(STR_ACT_DONE_SUB), fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(text_x, 26, text_w, 18), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
  }
}

static void prv_select(MenuLayer *m, MenuIndex *idx, void *ctx) {
  if (s_busy) return;
  s_status[0] = '\0';
  switch (idx->row) {
    case ACT_IMAGE:         image_window_push(s_doc_id); break;
    case ACT_CORRESPONDENT: option_window_push(s_doc_id, FIELD_CORRESPONDENT); break;
    case ACT_DOC_TYPE:      option_window_push(s_doc_id, FIELD_DOC_TYPE); break;
    case ACT_TAGS:          option_window_push(s_doc_id, FIELD_TAGS); break;
    case ACT_DATE:          option_window_push(s_doc_id, FIELD_DATE); break;
    case ACT_DONE:
      if (doc_mark_done(s_doc_id)) {
        s_busy = true;
        actions_window_set_status(tr(STR_MARKING_DONE));
      } else {
        actions_window_set_status(tr(STR_PHONE_UNREACHABLE));
      }
      break;
  }
  menu_layer_reload_data(s_menu);
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

void actions_window_push(int32_t doc_id) {
  if (s_window || !doc_id) return;
  s_doc_id = doc_id;
  s_busy = false;
  s_status[0] = '\0';
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}

bool actions_window_is_open(void) {
  return s_window != NULL;
}

void actions_window_set_status(const char *text) {
  if (!s_window) return;
  copy_utf8(s_status, text, sizeof(s_status));
  // Fehler beim Erledigen: wieder bedienbar machen
  if (strcmp(text, tr(STR_MARKING_DONE)) != 0) s_busy = false;
  if (s_menu) menu_layer_reload_data(s_menu);
}

void actions_window_close(bool animated) {
  if (s_window) window_stack_remove(s_window, animated);
}
