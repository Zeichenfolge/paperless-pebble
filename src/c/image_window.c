/*
 * Paperless für Pebble – Bildansicht
 *
 * Übersicht: ganze erste Seite verkleinert, ein roter Rahmen zeigt den Ausschnitt.
 * Zoom:      Ausschnitt in voller Auflösung des Vorschaubildes. Wie die Seite
 *            aufgeteilt wird, legt die Einstellung "Bild aufteilen" fest:
 *
 *   Vertikal   – senkrechte Streifen.  UP/DOWN scrollt hoch/runter, SELECT = nächster Streifen
 *   Horizontal – waagerechte Streifen. UP/DOWN scrollt links/rechts, SELECT = nächster Streifen
 *   Nicht      – ganze Seite.          UP/DOWN scrollt, SELECT wechselt ↕/↔
 *
 * In der Übersicht wählt UP/DOWN den Ausschnitt, SELECT zoomt hinein.
 * Im Zoom führt BACK (oder SELECT lang) zurück zur Übersicht.
 * Auf Uhren mit Touchscreen (Pebble Time 2): Tippen in der Übersicht zoomt an
 * diese Stelle, im Zoom kann man mit dem Finger verschieben.
 *
 * Das Handy schickt das Bild fertig skaliert und auf 4 Graustufen gerastert
 * (2 Bit pro Pixel, erstes Pixel in den obersten Bits). Es kommt von oben nach
 * unten in Stücken an und wird schon während des Ladens gezeichnet.
 */
#include "paperless.h"

#define MEMORY_RESERVE 9000   // Bytes, die für den Rest der App frei bleiben sollen
#define TOUCH_SLOP 6

typedef struct {
  GBitmap *bitmap;
  int16_t width;
  int16_t height;
  int16_t rows_loaded;
  int8_t part;       // nur Zoom: welcher Streifen
  bool complete;
} ImageBuffer;

static Window *s_window;
static Layer *s_canvas;

static int32_t s_doc_id;
static int s_split;
static int s_mode;
static int s_part;             // gewählter Streifen (vertikal/horizontal)
static int s_parts = 1;        // Anzahl Streifen
static int s_step;             // Abstand der Streifen im Zoom-Bild (Pixel)
static int s_src_w, s_src_h;   // Größe des ganzen Zoom-Bildes
static int16_t s_scroll_x, s_scroll_y;
static bool s_axis_x;          // "Nicht teilen": UP/DOWN scrollt waagerecht

static ImageBuffer s_page;
static ImageBuffer s_zoom;

static int32_t s_seq;          // laufende Anfrage-Nummer
static int s_req_mode;         // Modus der laufenden Anfrage
static bool s_loading;
static char s_msg[64];

static AppTimer *s_retry_timer;
static int s_retry_count;

static GColor s_palette[4];

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------

static GSize prv_screen(void) {
  return layer_get_bounds(window_get_root_layer(s_window)).size;
}

static void prv_buffer_free(ImageBuffer *b) {
  if (b->bitmap) gbitmap_destroy(b->bitmap);
  memset(b, 0, sizeof(*b));
}

static ImageBuffer *prv_buffer_for(int mode) {
  return mode == IMG_MODE_ZOOM ? &s_zoom : &s_page;
}

static void prv_set_msg(const char *text) {
  copy_utf8(s_msg, text, sizeof(s_msg));
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static int16_t prv_clamp(int v, int lo, int hi) {
  if (hi < lo) hi = lo;
  return (int16_t)(v < lo ? lo : (v > hi ? hi : v));
}

// Scrollbereich im Zoom-Bild (s_src_w x s_src_h) minus Bildschirm.
// Bei Streifen ist nur eine Richtung scrollbar.
static void prv_clamp_scroll(void) {
  GSize scr = prv_screen();
  s_scroll_x = prv_clamp(s_scroll_x, 0, s_src_w - scr.w);
  s_scroll_y = prv_clamp(s_scroll_y, 0, s_src_h - scr.h);
  if (s_split == SPLIT_VERTICAL) s_scroll_x = 0;
  if (s_split == SPLIT_HORIZONTAL) s_scroll_y = 0;
}

// ---------------------------------------------------------------------------
// Anfragen ans Handy
// ---------------------------------------------------------------------------

static void prv_send_request(void *unused);

static void prv_request(int mode) {
  s_seq++;
  s_req_mode = mode;
  s_loading = true;
  s_retry_count = 0;
  prv_set_msg(tr(mode == IMG_MODE_ZOOM ? STR_IMG_LOADING_PART : STR_IMG_LOADING));
  prv_send_request(NULL);
}

static void prv_send_request(void *unused) {
  s_retry_timer = NULL;
  if (!s_window) return;
  DictionaryIterator *out = comm_begin(CMD_IMAGE);
  if (!out) {
    // Ausgang gerade belegt (z. B. vorheriger Abbruch) -> kurz warten
    if (++s_retry_count <= 10) {
      s_retry_timer = app_timer_register(200, prv_send_request, NULL);
    } else {
      s_loading = false;
      prv_set_msg(tr(STR_PHONE_UNREACHABLE));
    }
    return;
  }
  GSize scr = prv_screen();
  int32_t free_bytes = (int32_t)heap_bytes_free() - MEMORY_RESERVE;
  if (free_bytes < 4000) free_bytes = 4000;
  dict_write_int32(out, MESSAGE_KEY_DocId, s_doc_id);
  dict_write_int32(out, MESSAGE_KEY_ImgMode, s_req_mode);
  dict_write_int32(out, MESSAGE_KEY_ImgCol, s_part);
  dict_write_int32(out, MESSAGE_KEY_Seq, s_seq);
  dict_write_int32(out, MESSAGE_KEY_Width, scr.w);
  dict_write_int32(out, MESSAGE_KEY_Height, scr.h);
  dict_write_int32(out, MESSAGE_KEY_Split, s_split);
  dict_write_int32(out, MESSAGE_KEY_MaxBytes, free_bytes);
  comm_send();
}

static void prv_cancel(void) {
  if (!s_loading) return;
  s_loading = false;
  s_seq++;  // spätere Daten der alten Anfrage ignorieren
  DictionaryIterator *out = comm_begin(CMD_IMAGE_CANCEL);
  if (out) comm_send();
}

static void prv_show_zoom(void) {
  s_mode = IMG_MODE_ZOOM;
  if (s_zoom.bitmap && s_zoom.complete && (s_split == SPLIT_NONE || s_zoom.part == s_part)) {
    prv_clamp_scroll();
    layer_mark_dirty(s_canvas);
    return;
  }
  // Speicher für den neuen Ausschnitt frei machen, damit er so groß wie möglich sein kann
  prv_buffer_free(&s_zoom);
  if (s_split == SPLIT_NONE) prv_buffer_free(&s_page);  // ganze Seite braucht viel Platz
  prv_request(IMG_MODE_ZOOM);
  layer_mark_dirty(s_canvas);
}

static void prv_show_page(void) {
  s_mode = IMG_MODE_PAGE;
  if (s_loading && s_req_mode == IMG_MODE_ZOOM) prv_cancel();
  if (!s_page.bitmap || !s_page.complete) {
    if (s_split == SPLIT_NONE) prv_buffer_free(&s_zoom);  // Platz für die Übersicht
    prv_request(IMG_MODE_PAGE);
  }
  layer_mark_dirty(s_canvas);
}

// ---------------------------------------------------------------------------
// Zeichnen
// ---------------------------------------------------------------------------

static void prv_draw_center_text(GContext *ctx, GRect bounds, const char *text) {
  GRect box = GRect(10, bounds.size.h / 2 - 40, bounds.size.w - 20, 80);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), box,
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void prv_draw_progress(GContext *ctx, GRect bounds, ImageBuffer *b) {
  if (!s_loading && !s_msg[0]) return;
  const int16_t h = 24;
  GRect bar = GRect(0, bounds.size.h - h, bounds.size.w, h);
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorBlack));
  graphics_fill_rect(ctx, bar, 0, GCornerNone);
  if (s_loading && b->bitmap && b->height > 0) {
    int16_t w = (int16_t)((int32_t)bounds.size.w * b->rows_loaded / b->height);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorDarkGray));
    graphics_fill_rect(ctx, GRect(0, bar.origin.y, w, h), 0, GCornerNone);
  }
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_msg[0] ? s_msg : tr(STR_LOADING), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(4, bar.origin.y - 1, bounds.size.w - 8, h), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

// Lage der Übersicht auf dem Bildschirm
static GRect prv_page_rect(GRect bounds) {
  return GRect((bounds.size.w - s_page.width) / 2, (bounds.size.h - s_page.height) / 2,
               s_page.width, s_page.height);
}

// Roter Rahmen: welcher Teil wird im Zoom gezeigt?
static void prv_draw_frame(GContext *ctx, GRect bounds, GRect page) {
  if (s_src_w <= 0 || s_src_h <= 0) return;
  GSize scr = bounds.size;
  int32_t fx = 0, fy = 0, fw = s_src_w, fh = s_src_h;  // in Zoom-Koordinaten
  if (s_split == SPLIT_VERTICAL) {
    if (s_parts <= 1) return;
    fx = s_part * s_step;
    fw = scr.w;
  } else if (s_split == SPLIT_HORIZONTAL) {
    if (s_parts <= 1) return;
    fy = s_part * s_step;
    fh = scr.h;
  } else {
    fx = s_scroll_x;
    fy = s_scroll_y;
    fw = scr.w < s_src_w ? scr.w : s_src_w;
    fh = scr.h < s_src_h ? scr.h : s_src_h;
  }
  GRect r = GRect(page.origin.x + (int16_t)(fx * page.size.w / s_src_w),
                  page.origin.y + (int16_t)(fy * page.size.h / s_src_h),
                  (int16_t)(fw * page.size.w / s_src_w),
                  (int16_t)(fh * page.size.h / s_src_h));
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorBlack));
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_rect(ctx, r);
}

// Pfeil-Symbol für "Nicht teilen": zeigt die aktuelle Scrollrichtung
static void prv_draw_axis_icon(GContext *ctx, GRect box) {
  GPoint c = grect_center_point(&box);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);
  if (s_axis_x) {
    graphics_draw_line(ctx, GPoint(c.x - 9, c.y), GPoint(c.x + 9, c.y));
    graphics_draw_line(ctx, GPoint(c.x - 9, c.y), GPoint(c.x - 5, c.y - 4));
    graphics_draw_line(ctx, GPoint(c.x - 9, c.y), GPoint(c.x - 5, c.y + 4));
    graphics_draw_line(ctx, GPoint(c.x + 9, c.y), GPoint(c.x + 5, c.y - 4));
    graphics_draw_line(ctx, GPoint(c.x + 9, c.y), GPoint(c.x + 5, c.y + 4));
  } else {
    graphics_draw_line(ctx, GPoint(c.x, c.y - 7), GPoint(c.x, c.y + 7));
    graphics_draw_line(ctx, GPoint(c.x, c.y - 7), GPoint(c.x - 4, c.y - 3));
    graphics_draw_line(ctx, GPoint(c.x, c.y - 7), GPoint(c.x + 4, c.y - 3));
    graphics_draw_line(ctx, GPoint(c.x, c.y + 7), GPoint(c.x - 4, c.y + 3));
    graphics_draw_line(ctx, GPoint(c.x, c.y + 7), GPoint(c.x + 4, c.y + 3));
  }
}

static void prv_draw_scrollbars(GContext *ctx, GRect bounds, ImageBuffer *b) {
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorBlack));
  if (b->height > bounds.size.h) {
    int16_t track = bounds.size.h;
    int16_t len = (int16_t)((int32_t)track * bounds.size.h / b->height);
    int16_t max = b->height - bounds.size.h;
    int16_t pos = (int16_t)((int32_t)(track - len) * s_scroll_y / max);
    graphics_fill_rect(ctx, GRect(bounds.size.w - 4, pos, 4, len), 2, GCornersAll);
  }
  if (b->width > bounds.size.w) {
    int16_t track = bounds.size.w;
    int16_t len = (int16_t)((int32_t)track * bounds.size.w / b->width);
    int16_t max = b->width - bounds.size.w;
    int16_t pos = (int16_t)((int32_t)(track - len) * s_scroll_x / max);
    graphics_fill_rect(ctx, GRect(pos, bounds.size.h - 4, len, 4), 2, GCornersAll);
  }
}

static void prv_canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  ImageBuffer *b = prv_buffer_for(s_mode);
  bool other_part = s_mode == IMG_MODE_ZOOM && s_split != SPLIT_NONE && b->bitmap && b->part != s_part;
  if (!b->bitmap || other_part) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    prv_draw_center_text(ctx, bounds, s_msg[0] ? s_msg : tr(STR_LOADING));
    return;
  }

  if (s_mode == IMG_MODE_PAGE) {
    GRect r = prv_page_rect(bounds);
    graphics_draw_bitmap_in_rect(ctx, b->bitmap, r);
    prv_draw_frame(ctx, bounds, r);
  } else {
    graphics_draw_bitmap_in_rect(ctx, b->bitmap, GRect(-s_scroll_x, -s_scroll_y, b->width, b->height));
    prv_draw_scrollbars(ctx, bounds, b);

    // Anzeige oben rechts: Streifen "2/3" oder Scrollrichtung
    if (s_split == SPLIT_NONE || s_parts > 1) {
      GRect box = GRect(bounds.size.w - 42, 3, 34, 22);
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorBlack));
      graphics_fill_rect(ctx, box, 4, GCornersAll);
      if (s_split == SPLIT_NONE) {
        prv_draw_axis_icon(ctx, box);
      } else {
        char label[24];
        snprintf(label, sizeof(label), "%d/%d", s_part + 1, s_parts);
        graphics_context_set_text_color(ctx, GColorWhite);
        graphics_draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                           GRect(box.origin.x, box.origin.y + 2, box.size.w, box.size.h),
                           GTextOverflowModeFill, GTextAlignmentCenter, NULL);
      }
    }
  }
  prv_draw_progress(ctx, bounds, b);
}

// ---------------------------------------------------------------------------
// Bedienung
// ---------------------------------------------------------------------------

static void prv_scroll_by(int dx, int dy) {
  s_scroll_x += dx;
  s_scroll_y += dy;
  prv_clamp_scroll();
  layer_mark_dirty(s_canvas);
}

static void prv_move(int dir) {  // dir: -1 = UP, +1 = DOWN
  GSize scr = prv_screen();
  if (s_mode == IMG_MODE_PAGE) {
    if (s_split == SPLIT_NONE) {
      // Rahmen um eine Bildschirmhöhe verschieben, am Ende wieder oben anfangen
      int max = s_src_h - scr.h;
      if (dir > 0 && s_scroll_y >= max) s_scroll_y = 0;
      else if (dir < 0 && s_scroll_y <= 0) s_scroll_y = max;
      else s_scroll_y += dir * scr.h * 2 / 3;
      prv_clamp_scroll();
    } else if (s_parts > 1) {
      s_part = (s_part + s_parts + dir) % s_parts;
    }
    layer_mark_dirty(s_canvas);
    return;
  }
  bool horizontal = s_split == SPLIT_HORIZONTAL || (s_split == SPLIT_NONE && s_axis_x);
  if (horizontal) {
    prv_scroll_by(dir * scr.w * 2 / 5, 0);
  } else {
    prv_scroll_by(0, dir * scr.h / 3);
  }
}

static void prv_up(ClickRecognizerRef r, void *ctx) { prv_move(-1); }
static void prv_down(ClickRecognizerRef r, void *ctx) { prv_move(1); }

static void prv_select(ClickRecognizerRef r, void *ctx) {
  if (s_mode == IMG_MODE_PAGE) {
    if (!s_page.complete) return;
    prv_show_zoom();
  } else if (s_split == SPLIT_NONE) {
    s_axis_x = !s_axis_x;  // Scrollrichtung wechseln
    layer_mark_dirty(s_canvas);
  } else if (s_parts > 1) {
    if (s_loading) prv_cancel();
    s_part = (s_part + 1) % s_parts;
    prv_show_zoom();
  }
}

static void prv_select_long(ClickRecognizerRef r, void *ctx) {
  if (s_mode == IMG_MODE_ZOOM) prv_show_page();
}

static void prv_back(ClickRecognizerRef r, void *ctx) {
  if (s_mode == IMG_MODE_ZOOM) {
    prv_show_page();
  } else {
    window_stack_remove(s_window, true);
  }
}

static void prv_click_config(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 120, prv_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 120, prv_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_long_click_subscribe(BUTTON_ID_SELECT, 600, prv_select_long, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back);
}

#if defined(PBL_TOUCH)
// --- Touchscreen (Pebble Time 2) ---

static bool s_touch_down;
static bool s_touch_moved;
static int16_t s_touch_start_x, s_touch_start_y, s_touch_last_x, s_touch_last_y;

// Tippen in der Übersicht: an diese Stelle zoomen
static void prv_tap_page(GPoint pt) {
  if (!s_page.complete || s_src_w <= 0 || s_page.width <= 0) return;
  GRect page = prv_page_rect(layer_get_bounds(s_canvas));
  GSize scr = prv_screen();
  int zx = (pt.x - page.origin.x) * s_src_w / page.size.w;
  int zy = (pt.y - page.origin.y) * s_src_h / page.size.h;
  if (s_split == SPLIT_VERTICAL && s_parts > 1 && s_step > 0) {
    s_part = prv_clamp((zx - scr.w / 2 + s_step / 2) / s_step, 0, s_parts - 1);
    s_scroll_y = zy - scr.h / 2;
  } else if (s_split == SPLIT_HORIZONTAL && s_parts > 1 && s_step > 0) {
    s_part = prv_clamp((zy - scr.h / 2 + s_step / 2) / s_step, 0, s_parts - 1);
    s_scroll_x = zx - scr.w / 2;
  } else {
    s_scroll_x = zx - scr.w / 2;
    s_scroll_y = zy - scr.h / 2;
  }
  prv_clamp_scroll();
  prv_show_zoom();
}

static void prv_touch_handler(const TouchEvent *e, void *context) {
  if (!s_canvas) return;
  switch (e->type) {
    case TouchEvent_Touchdown:
      s_touch_down = true;
      s_touch_moved = false;
      s_touch_start_x = s_touch_last_x = e->x;
      s_touch_start_y = s_touch_last_y = e->y;
      break;
    case TouchEvent_PositionUpdate: {
      if (!s_touch_down) break;
      int dx = e->x - s_touch_last_x;
      int dy = e->y - s_touch_last_y;
      if (!s_touch_moved) {
        int mx = e->x - s_touch_start_x, my = e->y - s_touch_start_y;
        if (mx < 0) mx = -mx;
        if (my < 0) my = -my;
        s_touch_moved = mx > TOUCH_SLOP || my > TOUCH_SLOP;
      }
      if (s_touch_moved && s_mode == IMG_MODE_ZOOM) {
        prv_scroll_by(-dx, -dy);  // Bild folgt dem Finger
      }
      s_touch_last_x = e->x;
      s_touch_last_y = e->y;
      break;
    }
    case TouchEvent_Liftoff:
      if (s_touch_down && !s_touch_moved && s_mode == IMG_MODE_PAGE) {
        prv_tap_page(GPoint(e->x, e->y));
      }
      s_touch_down = false;
      break;
  }
}
#endif

// ---------------------------------------------------------------------------
// Fenster
// ---------------------------------------------------------------------------

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, prv_canvas_update);
  layer_add_child(root, s_canvas);
#if defined(PBL_TOUCH)
  if (touch_service_is_enabled()) {
    touch_service_subscribe(prv_touch_handler, NULL);
  }
#endif
}

static void prv_window_unload(Window *window) {
#if defined(PBL_TOUCH)
  touch_service_unsubscribe();
#endif
  if (s_retry_timer) {
    app_timer_cancel(s_retry_timer);
    s_retry_timer = NULL;
  }
  prv_cancel();
  layer_destroy(s_canvas);
  s_canvas = NULL;
  prv_buffer_free(&s_page);
  prv_buffer_free(&s_zoom);
  window_destroy(s_window);
  s_window = NULL;
}

void image_window_push(int32_t doc_id) {
  if (s_window || !doc_id) return;
  s_palette[0] = GColorBlack;
  s_palette[1] = GColorDarkGray;
  s_palette[2] = GColorLightGray;
  s_palette[3] = GColorWhite;

  s_doc_id = doc_id;
  s_split = settings_split();
  s_mode = IMG_MODE_PAGE;
  s_part = 0;
  s_parts = 1;
  s_step = 0;
  s_src_w = s_src_h = 0;
  s_scroll_x = s_scroll_y = 0;
  s_axis_x = false;
  s_msg[0] = '\0';

  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_click_config_provider(s_window, prv_click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
  prv_request(IMG_MODE_PAGE);
}

void image_window_destroy(void) {
  if (s_window) window_stack_remove(s_window, false);
}

// ---------------------------------------------------------------------------
// Nachrichten vom Handy
// ---------------------------------------------------------------------------

static void prv_begin(DictionaryIterator *iter) {
  int mode = tuple_int(iter, MESSAGE_KEY_ImgMode, IMG_MODE_PAGE);
  int16_t w = tuple_int(iter, MESSAGE_KEY_Width, 0);
  int16_t h = tuple_int(iter, MESSAGE_KEY_Height, 0);
  s_parts = tuple_int(iter, MESSAGE_KEY_Num, 1);
  s_step = tuple_int(iter, MESSAGE_KEY_Step, 0);
  int new_w = tuple_int(iter, MESSAGE_KEY_SrcW, 0);
  int new_h = tuple_int(iter, MESSAGE_KEY_SrcH, 0);
  // Das Handy kann das Zoom-Bild je nach freiem Speicher kleiner rechnen:
  // dann die Scrollposition mitskalieren
  if (s_src_w > 0 && new_w > 0 && new_w != s_src_w) s_scroll_x = s_scroll_x * new_w / s_src_w;
  if (s_src_h > 0 && new_h > 0 && new_h != s_src_h) s_scroll_y = s_scroll_y * new_h / s_src_h;
  s_src_w = new_w;
  s_src_h = new_h;
  if (s_parts < 1) s_parts = 1;
  if (s_part >= s_parts) s_part = 0;

  ImageBuffer *b = prv_buffer_for(mode);
  ImageBuffer *other = mode == IMG_MODE_ZOOM ? &s_page : &s_zoom;
  prv_buffer_free(b);
  if (w <= 0 || h <= 0) {
    s_loading = false;
    prv_set_msg(tr(STR_IMG_INVALID));
    return;
  }
  b->bitmap = gbitmap_create_blank_with_palette(GSize(w, h), GBitmapFormat2BitPalette, s_palette, false);
  if (!b->bitmap && other->bitmap) {
    prv_buffer_free(other);  // zweiter Versuch mit mehr Platz
    b->bitmap = gbitmap_create_blank_with_palette(GSize(w, h), GBitmapFormat2BitPalette, s_palette, false);
  }
  if (!b->bitmap) {
    s_loading = false;
    prv_set_msg(tr(STR_IMG_NO_MEMORY));
    return;
  }
  b->width = w;
  b->height = h;
  b->part = tuple_int(iter, MESSAGE_KEY_ImgCol, 0);
  memset(gbitmap_get_data(b->bitmap), 0xFF, (size_t)gbitmap_get_bytes_per_row(b->bitmap) * h);  // weiß
  prv_clamp_scroll();
  prv_set_msg("");
}

static void prv_data(DictionaryIterator *iter) {
  ImageBuffer *b = prv_buffer_for(s_req_mode);
  Tuple *t = dict_find(iter, MESSAGE_KEY_Data);
  if (!b->bitmap || !t || t->type != TUPLE_BYTE_ARRAY) return;

  uint32_t pos = (uint32_t)tuple_int(iter, MESSAGE_KEY_Offset, 0);
  const uint8_t *src = t->value->data;
  uint32_t len = t->length;
  const uint32_t packed_row = (uint32_t)(b->width + 3) / 4;
  const uint16_t stride = gbitmap_get_bytes_per_row(b->bitmap);
  const uint32_t total = packed_row * (uint32_t)b->height;
  uint8_t *dst = gbitmap_get_data(b->bitmap);

  while (len > 0 && pos < total) {
    uint32_t row = pos / packed_row;
    uint32_t in_row = pos % packed_row;
    uint32_t n = packed_row - in_row;
    if (n > len) n = len;
    memcpy(dst + row * stride + in_row, src, n);
    src += n;
    pos += n;
    len -= n;
  }
  b->rows_loaded = (int16_t)(pos / packed_row);
}

bool image_window_handle_message(int32_t type, DictionaryIterator *iter) {
  if (type < MSG_IMG_BEGIN || (type > MSG_IMG_ERROR && type != MSG_IMG_STATUS)) return false;
  if (!s_window || tuple_int(iter, MESSAGE_KEY_Seq, -1) != s_seq) return true;  // veraltet

  switch (type) {
    case MSG_IMG_BEGIN:
      prv_begin(iter);
      break;
    case MSG_IMG_DATA:
      prv_data(iter);
      break;
    case MSG_IMG_END: {
      ImageBuffer *b = prv_buffer_for(s_req_mode);
      b->complete = b->bitmap != NULL;
      b->rows_loaded = b->height;
      s_loading = false;
      prv_set_msg("");
      break;
    }
    case MSG_IMG_ERROR:
    case MSG_IMG_STATUS: {
      char text[64];
      copy_tuple(iter, MESSAGE_KEY_Text, text, sizeof(text));
      if (type == MSG_IMG_ERROR) {
        s_loading = false;
        vibes_double_pulse();
      }
      prv_set_msg(text);
      break;
    }
  }
  if (s_canvas) layer_mark_dirty(s_canvas);
  return true;
}
