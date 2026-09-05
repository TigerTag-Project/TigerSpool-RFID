#include "icons.h"

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
                 const lv_font_t* face = &lv_font_montserrat_16) {
    lv_obj_t* box = piece(parent, 0, 0, BOX, BOX);
    lv_obj_t* g = lv_label_create(box);
    lv_label_set_text(g, glyph);
    lv_obj_set_style_text_font(g, face, 0);
    lv_obj_set_style_text_color(g, lv_color_hex(colour), 0);
    lv_obj_center(g);
    return box;
}

}  // namespace

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

lv_obj_t* build(lv_obj_t* parent, Id id, uint32_t c) {
    switch (id) {
    case WIFI:    return symbol(parent, LV_SYMBOL_WIFI, c);
    case UPDATE:  return symbol(parent, LV_SYMBOL_DOWNLOAD, c);
    case RESTART: return symbol(parent, LV_SYMBOL_REFRESH, c);
    case ERASE:   return symbol(parent, LV_SYMBOL_TRASH, c);
    // Text, so it tints through text_color like any other glyph - unlike the
    // drawn icons below, which tint through border_color or bg_color.
    case SCREEN:  return symbol(parent, TT_SYMBOL_SUN, c, &font_icons_16);
    case NONE:    return nullptr;
    default:      break;
    }

    lv_obj_t* box = piece(parent, 0, 0, BOX, BOX);

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
        disc(box, 7, 1, 8, c);
        disc(box, 2, 13, 17, c);
        break;

    case GLOBE:
        // Three strokes: the sphere, the equator, one meridian. That is enough
        // for the eye to finish it as a globe. These are the TigerScale's own
        // coordinates for a 22 px box, taken as given rather than scaled from
        // its 26 px ones - a 2 px stroke does not scale with the shape it
        // draws, so scaled coordinates come out wrong.
        ring(box, 1, 1, 19, c);
        bar(box, 1, 10, 19, 2, 0, c);
        outline(box, 7, 1, 8, 19, 4, c);
        break;

    case PRINTER:
        // The sheet going in, the body, the sheet coming out. An earlier
        // revision widened the body to twenty and fattened the output tray to
        // 12x7, on the reasoning that one form should dominate. On the glass
        // it did not read better - the heavy block at the bottom took over the
        // icon and the printer stopped looking like a printer. Reverted to the
        // first version, which was balanced.
        bar(box, 6, 0, 10, 5, 1, c);
        outline(box, 2, 6, 18, 10, 2, c);
        bar(box, 6, 17, 10, 5, 1, c);
        break;

    default:
        break;
    }
    return box;
}

}  // namespace icons
