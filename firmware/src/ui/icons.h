// Row icons, drawn rather than imported.
//
// There is no icon font and no icon set. Everything here is a handful of bare
// lv_obj primitives - a border and no fill for an outline, a fill and no border
// for a solid - assembled inside one 22x22 box. That box is what gets aligned;
// the strokes inside it never are. Change the box and everything follows.
//
// The reason it is done this way rather than with a generated font: it costs
// about a kilobyte of code and no data at all, it recolours perfectly because
// the colour is an argument rather than a style on a glyph, it carries no
// licence obligation, and there is no generated file for a guard to police.
// The TigerScale reached the same conclusion; the recipes below come from it.
//
// Three mechanisms live side by side on purpose: draw what LVGL does not have,
// use LV_SYMBOL_* for what it does. The menu looks of a piece because every
// icon lands in the same box under the same colour rule, not because they all
// come from the same place.
#pragma once
#include <lvgl.h>

// A glyph the built-in symbol set does not carry. It is the TigerScale's own
// sun, which is why it is a glyph and not a drawing: primitives are
// axis-aligned, so the eight pointed rays of Font Awesome's sun cannot be
// assembled from rectangles at all. Drawn, it is a different icon however
// carefully it is drawn.
//
// It used to need a face of its own. It rides in the ordinary UI faces now -
// lv_font_conv takes several --font in one call, each with its own range, so
// one more codepoint costs no second face and no extra link in the fallback
// chain. See scripts/make-ui-font.sh.
#define TT_SYMBOL_SUN "\xEF\x86\x85"   /* U+F185 */

namespace icons {

enum Id {
    NONE = 0,
    PRINTER,     // drawn - a body, the sheet above it, the sheet below
    WIFI,        // LV_SYMBOL
    USER,        // drawn - LVGL has no person, and an envelope says "messages"
    SCREEN,      // drawn - a sun, for brightness and sleep
    GLOBE,       // drawn - LVGL has no globe, and a keyboard is not a language
    UPDATE,      // LV_SYMBOL
    RESTART,     // LV_SYMBOL
    ERASE,       // LV_SYMBOL
};

// Builds the icon into a 22x22 box parented to `parent`. `colour` is applied to
// every stroke. Returns the box, or nullptr for NONE.
// `scale` in percent of the 22 px box. A row gives an icon a whole 48 px line
// to itself and 22 reads there; a header puts it beside 16 px glyphs where the
// same drawing collapses into a blob, so it needs room rather than a different
// picture - the point of using the same glyph is that it is the same glyph.
lv_obj_t* build(lv_obj_t* parent, Id id, uint32_t colour, int scale = 100);

// Recolour an icon already on screen. A row whose state changes - Wi-Fi lost,
// an update arriving - repaints its glyph instead of rebuilding the row that
// holds it, and therefore instead of rebuilding the list that holds the row.
void tint(lv_obj_t* box, uint32_t colour);

// Wi-Fi strength, and it is the TigerScale's own icon rather than a copy of it.
//
// Two LV_SYMBOL_WIFI labels stacked: a dimmed one at 50% showing the whole
// glyph, and a lit one inside a container whose HEIGHT is the signal level.
// The arcs that light up are not objects at all - they are what a horizontal
// mask lets through of a single glyph. That is the whole trick, and it is why
// three arcs drawn with lv_arc could never match: this displays the original
// shape instead of reproducing it.
//
// The box is deliberately larger than the glyph. At 16x17 the top arc was
// clipped and the icon read as a broken drawing; the clip heights are measured
// from the BOTTOM and everything is bottom-anchored, so a taller box reveals
// more without moving a single threshold.
lv_obj_t* wifiWave(lv_obj_t* parent);

// level 0..3, and `connected` is a separate statement from a bad signal: a
// disconnected device paints the whole glyph solid red, where one bar lights
// the dot in green over dimmed arcs. "No network" and "dreadful network" have
// to be told apart at a glance, which they cannot be if one of them is just
// zero bars. Written into the labels already on screen.
void setSignal(lv_obj_t* box, int level, bool connected);

// 0..3 from dBm. Integer truncation on purpose - it is what fixes the bands 20
// dBm apart. Taken verbatim from the TigerScale so that one network is
// described identically by both products; changing it here alone would have
// two devices on the same shelf reporting different strengths, which is worse
// than two different drawings.
int wifiLevelFromRssi(int rssi);

constexpr lv_coord_t BOX = 22;

}  // namespace icons
