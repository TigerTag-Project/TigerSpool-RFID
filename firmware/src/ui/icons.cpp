#include "icons.h"
#include "fonts.h"
#include "theme.h"

namespace icons {
namespace {

// Every primitive needs the same three things, and each of them is a bug if it
// is left out. remove_style_all, or LVGL's theme arrives with a grey fill on
// what was meant to be an outline. Clearing SCROLLABLE and CLICKABLE, or every
// stroke becomes an object that swallows the press meant for the row - and the
// row stops responding where it is touched, which reads as a dead menu entry.
lv_obj_t* piece(lv_obj_t* p, int x, int y, int w, int h) {
    lv_obj_t* o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

// An outline: border, no fill. The stroke stays 2 px whatever the box, because
// a 1 px border disappears at this pixel density and a border does not scale
// with the shape it draws.
void outline(lv_obj_t* p, int x, int y, int w, int h, int r, uint32_t c) {
    lv_obj_t* o = piece(p, x, y, w, h);
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(c), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
}

void ring(lv_obj_t* p, int x, int y, int d, uint32_t c) {
    outline(p, x, y, d, d, LV_RADIUS_CIRCLE, c);
}

// A solid: fill, no border.
void bar(lv_obj_t* p, int x, int y, int w, int h, int r, uint32_t c) {
    lv_obj_t* o = piece(p, x, y, w, h);
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(c), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
}

void disc(lv_obj_t* p, int x, int y, int d, uint32_t c) {
    bar(p, x, y, d, d, LV_RADIUS_CIRCLE, c);
}

// The same glyph, turned.
//
// LVGL rotates IMAGES, not labels - a font glyph drawn as text has no angle to
// set. So the glyph is drawn once into a canvas, and the canvas, being an
// image, is rotated.
//
// This exists so the NFC row can carry the Wi-Fi wave itself rather than a
// hand-drawn lookalike. Three arcs and a dot drawn from primitives came close
// and were not the same picture: different stroke, different spacing, and the
// eye reads two icons that are nearly identical as a mistake rather than as a
// family.
//
// One canvas is alive at a time - a single row uses this - so the pixel buffer
// is shared. It is 22 x 22 at two bytes plus an alpha byte per pixel, which is
// about 1.5 KB, and it lives in static memory rather than being allocated and
// freed on every screen build.
lv_obj_t* turnedSymbol(lv_obj_t* parent, const char* glyph, uint32_t colour,
                       int16_t tenthsOfADegree, int scale = 100) {
    // Two sizes, because there are two uses: 22 px on a menu row, and a big one
    // for a screen whose whole subject is the reader. The buffer is sized for
    // the larger of them and the canvas is told how much of it to use.
    const bool big = (scale >= 150);
    const lv_coord_t side = big ? BOX * 2 : BOX;
    const lv_font_t* face = big ? &font_ui_24 : &font_ui_16;
    static uint8_t buf[LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(BOX * 2, BOX * 2)];

    // The box is larger than the canvas in the big case, because the canvas is
    // then ZOOMED and a parent clips what its child draws outside it. 24 px is
    // the largest face compiled in, so the only way to a bigger wave is to
    // scale the drawn one - acceptable here, where the shape is three strokes
    // and a dot rather than a letterform full of detail.
    lv_obj_t* box = piece(parent, 0, 0, big ? BOX * 3 : side, big ? BOX * 3 : side);
    lv_obj_t* cv = lv_canvas_create(box);
    lv_canvas_set_buffer(cv, buf, side, side, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_canvas_fill_bg(cv, lv_color_black(), LV_OPA_TRANSP);

    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font  = face;
    d.color = lv_color_hex(colour);
    d.align = LV_TEXT_ALIGN_CENTER;
    const lv_coord_t h = lv_font_get_line_height(face);
    lv_canvas_draw_text(cv, 0, (side - h) / 2, side, &d, glyph);

    lv_img_set_pivot(cv, side / 2, side / 2);
    lv_img_set_angle(cv, tenthsOfADegree);
    if (big) {
        lv_img_set_antialias(cv, true);
        lv_img_set_zoom(cv, 410);        // 256 is 1:1, so this is 1.6x
    }
    lv_obj_center(cv);
    return box;
}

lv_obj_t* symbol(lv_obj_t* parent, const char* glyph, uint32_t colour,
                 const lv_font_t* face = &font_ui_16) {
    lv_obj_t* box = piece(parent, 0, 0, BOX, BOX);
    lv_obj_t* g = lv_label_create(box);
    lv_label_set_text(g, glyph);
    lv_obj_set_style_text_font(g, face, 0);
    lv_obj_set_style_text_color(g, lv_color_hex(colour), 0);
    lv_obj_center(g);
    return box;
}

}  // namespace

// The wave at 24 px, not 16: beside the account bust and the gear, both 21 px
// tall on the panel, a 16 px wave was 14 px - a third smaller, and the three
// read as two icons and a leftover. At 24 its ink is 22 rows.
//
// The box is the glyph's own box, 30 x 23, and the glyph fills it exactly:
// font_ui_24 draws this glyph from row 3 of a 31-row line and leaves 5 empty
// rows under it, so the label is dropped 5 px below the box's bottom edge.
//
// The clip boxes are cut in the GAPS of that bitmap, read from font_ui_24.c
// row by row rather than guessed:
//   rows 17-22        the dot            row 16 is empty
//   rows  9-15        the inner arc      columns 5-24
//   rows  1-9         the outer arc      its tips reach row 9, at columns 1-3
//                                        and 26-28 - beside the inner arc's top
// So one level is the bottom 7 rows, and two levels are the bottom 14 rows but
// only 22 columns wide: the width is what keeps the outer arc's tips out. A
// five-level scale was tried on the TigerScale and cut through an arc; this
// one never crosses ink.
static const int WIFI_W = 30, WIFI_H = 23, WIFI_DROP = 5;
static const uint8_t WIFI_CLIP_H[4] = { 0, 7, 14, WIFI_H };
static const uint8_t WIFI_CLIP_W = 22;

int wifiLevelFromRssi(int rssi) {
    if (rssi >= -60) return 3;
    if (rssi >= -70) return 2;
    if (rssi >= -80) return 1;
    return 0;
}

lv_obj_t* wifiWave(lv_obj_t* parent) {
    lv_obj_t* wrap = piece(parent, 0, 0, WIFI_W, WIFI_H);

    lv_obj_t* dim = lv_label_create(wrap);          // child 0
    lv_label_set_text(dim, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(dim, &font_ui_24, 0);
    lv_obj_align(dim, LV_ALIGN_BOTTOM_MID, 0, WIFI_DROP);

    lv_obj_t* clip = lv_obj_create(wrap);           // child 1
    lv_obj_remove_style_all(clip);
    lv_obj_set_size(clip, WIFI_CLIP_W, WIFI_H);
    lv_obj_align(clip, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(clip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(clip, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* lit = lv_label_create(clip);
    lv_label_set_text(lit, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(lit, &font_ui_24, 0);
    lv_obj_align(lit, LV_ALIGN_BOTTOM_MID, 0, WIFI_DROP);
    return wrap;
}

void setSignal(lv_obj_t* box, int level, bool connected) {
    if (!box || lv_obj_get_child_cnt(box) < 2) return;
    lv_obj_t* dim  = lv_obj_get_child(box, 0);
    lv_obj_t* clip = lv_obj_get_child(box, 1);
    lv_obj_t* lit  = lv_obj_get_child(clip, 0);
    if (!lit) return;

    if (!connected) {
        lv_obj_set_style_text_color(dim, lv_color_hex(theme::DANGER), 0);
        lv_obj_set_style_text_opa(dim, LV_OPA_COVER, 0);
        lv_obj_add_flag(clip, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (level < 0) level = 0;
    if (level > 3) level = 3;

    // Full signal is ONE uniform glyph, not a stack. A clip box can only cover
    // the ink it reaches, so at maximum the apex of the outer arc stayed on the
    // dimmed copy and the wave read as cut off at the top. There is nothing to
    // dim at full strength.
    if (level >= 3) {
        lv_obj_set_style_text_color(dim, lv_color_hex(theme::OK), 0);
        lv_obj_set_style_text_opa(dim, LV_OPA_COVER, 0);
        lv_obj_add_flag(clip, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_set_style_text_color(dim, lv_color_hex(theme::OK), 0);
    // 50%, not the 30% iOS uses: 30 reads on a black status bar and vanishes on
    // the grey cards of a network picker. 50 reads on both.
    lv_obj_set_style_text_opa(dim, LV_OPA_50, 0);
    lv_obj_set_style_text_color(lit, lv_color_hex(theme::OK), 0);
    lv_obj_set_style_text_opa(lit, LV_OPA_COVER, 0);

    if (level == 0) { lv_obj_add_flag(clip, LV_OBJ_FLAG_HIDDEN); return; }

    lv_obj_clear_flag(clip, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(clip, WIFI_CLIP_H[level]);
    // LVGL does not re-run alignment after a size change. Without these two the
    // clip box re-centres and the lit copy drifts off the dimmed one - two
    // waves a few pixels apart, visible, ugly, and a symptom that looks nothing
    // like its cause.
    lv_obj_align(clip, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_align(lit, LV_ALIGN_BOTTOM_MID, 0, WIFI_DROP);
}

void tint(lv_obj_t* box, uint32_t colour) {
    if (!box) return;
    const lv_color_t c = lv_color_hex(colour);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(box); i++) {
        lv_obj_t* k = lv_obj_get_child(box, i);
        if (lv_obj_check_type(k, &lv_label_class)) {
            lv_obj_set_style_text_color(k, c, 0);           // a symbol or a glyph
        } else {
            // A drawn stroke is either an outline or a solid; setting both is
            // harmless because only one of them is visible on any given piece.
            lv_obj_set_style_border_color(k, c, 0);
            lv_obj_set_style_bg_color(k, c, 0);
        }
    }
}

lv_obj_t* build(lv_obj_t* parent, Id id, uint32_t c, int scale) {
    const auto S = [scale](int v) { return v * scale / 100; };
    switch (id) {
    case WIFI:    return symbol(parent, LV_SYMBOL_WIFI, c);
    case UPDATE:  return symbol(parent, LV_SYMBOL_DOWNLOAD, c);
    case RESTART: return symbol(parent, LV_SYMBOL_REFRESH, c);
    case ERASE:   return symbol(parent, LV_SYMBOL_TRASH, c);
    // Text, so it tints through text_color like any other glyph - unlike the
    // drawn icons below, which tint through border_color or bg_color. No face
    // of its own any more: the sun is in the UI faces alongside the letters.
    // The sun at 20, not 16, and it is not an exception for its own sake.
    //
    // The drawn icons fill the 22 px box - the globe's sphere is a ring from
    // 1 to 20 - while a glyph set at 16 px occupies about sixteen of it. Beside
    // the Language row's globe, the Display row's sun read as a smaller icon
    // for no reason a user could name. Matching the drawn diameter is what puts
    // the two on the same footing.
    case SCREEN:  return symbol(parent, TT_SYMBOL_SUN, c, &font_ui_20);
    // The Wi-Fi wave, a quarter turn to the right: the field leaves towards
    // the spool rather than upwards. Same glyph as the Wi-Fi row, so the two
    // read as the same idea pointed two ways.
    case NFC:     return turnedSymbol(parent, LV_SYMBOL_WIFI, c, 900, scale);
    case NONE:    return nullptr;
    default:      break;
    }

    lv_obj_t* box = piece(parent, 0, 0, S(BOX), S(BOX));

    switch (id) {
    case USER:
        // Two solid discs and no outline at all. The shoulders disc runs from
        // y=13 to y=30 inside a box that stops at 22, and the eight clipped
        // pixels are the whole mechanism: a circle cut off at the bottom reads
        // as a pair of shoulders. Let it overflow and you get a snowman.
        //
        // The head follows the box; the shoulders must keep overflowing by
        // about a third of their diameter, or the cut rises and the bust
        // becomes a half-circle.
        disc(box, S(7), S(1), S(8), c);
        disc(box, S(2), S(13), S(17), c);
        break;

    case GLOBE:
        // Three strokes: the sphere, the equator, one meridian. That is enough
        // for the eye to finish it as a globe. These are the TigerScale's own
        // coordinates for a 22 px box, taken as given rather than scaled from
        // its 26 px ones - a 2 px stroke does not scale with the shape it
        // draws, so scaled coordinates come out wrong.
        ring(box, S(1), S(1), S(19), c);
        bar(box, S(1), S(10), S(19), 2, 0, c);
        outline(box, S(7), S(1), S(8), S(19), 4, c);
        break;

    case PRINTER:
        // The sheet going in, the body, the sheet coming out. An earlier
        // revision widened the body to twenty and fattened the output tray to
        // 12x7, on the reasoning that one form should dominate. On the glass
        // it did not read better - the heavy block at the bottom took over the
        // icon and the printer stopped looking like a printer. Reverted to the
        // first version, which was balanced.
        bar(box, S(6), S(0), S(10), S(5), 1, c);
        outline(box, S(2), S(6), S(18), S(10), 2, c);
        bar(box, S(6), S(17), S(10), S(5), 1, c);
        break;

    default:
        break;
    }
    return box;
}

}  // namespace icons
