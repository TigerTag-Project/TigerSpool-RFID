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

// The box is 20x21 and the glyph is bottom-anchored inside it. Both numbers
// are measured against a panel, not derived: the clip heights below fall in
// the GAPS between the glyph's three pieces - outer arc, inner arc, dot -
// rather than across one of them. A five-level scale was tried on the
// TigerScale first and it cut through the outer arc, leaving its apex dim
// while its shoulders were lit; the wave read as chopped off at the top.
static const int WIFI_W = 20, WIFI_H = 21;
static const uint8_t WIFI_CLIP_H[4] = { 0, 5, 10, WIFI_H };

int wifiLevelFromRssi(int rssi) {
    if (rssi > -40)  rssi = -40;
    if (rssi < -100) rssi = -100;
    int lv = (rssi + 100) * 3 / 60;
    return (lv < 0) ? 0 : (lv > 3 ? 3 : lv);
}

lv_obj_t* wifiWave(lv_obj_t* parent) {
    lv_obj_t* wrap = piece(parent, 0, 0, WIFI_W, WIFI_H);

    lv_obj_t* dim = lv_label_create(wrap);          // child 0
    lv_label_set_text(dim, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(dim, &font_ui_16, 0);
    lv_obj_align(dim, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t* clip = lv_obj_create(wrap);           // child 1
    lv_obj_remove_style_all(clip);
    lv_obj_set_size(clip, WIFI_W, WIFI_H);
    lv_obj_align(clip, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(clip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(clip, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* lit = lv_label_create(clip);
    lv_label_set_text(lit, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(lit, &font_ui_16, 0);
    lv_obj_align(lit, LV_ALIGN_BOTTOM_MID, 0, 0);
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
    lv_obj_align(lit, LV_ALIGN_BOTTOM_MID, 0, 0);
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
    case SCREEN:  return symbol(parent, TT_SYMBOL_SUN, c);
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
