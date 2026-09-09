#pragma once
#include <lvgl.h>

// The device's visual tokens, in one place.
//
// The screen is a 2.0" 240x320 panel at 200 PPI, so 1 mm is 7.87 px. Every size
// below was chosen against a finger, not by eye: the numbers in millimetres are
// the reason each one is what it is. See docs/ONBOARDING.md.
namespace theme {

// ---- colour ----------------------------------------------------------------
// A physical LCD in a workshop: dark ground, one warm accent, and semantic
// colours kept separate from it so "selected" never reads as "connected".
// PURE black, not a near-black.
//
// #07080A looks like black in a hex editor and is not one on this panel: at
// 5/6/5 it quantises to r=0 g=2 b=1 - three tiny, UNEQUAL values sitting at the
// very bottom of an IPS gamma curve, which is exactly where a panel's response
// stops being linear. The device rendered a blue-cast ground from a colour LVGL
// had resolved correctly, measured at 0x0041 with full opacity.
//
// Pure black has nothing to quantise and nothing to skew. It is also what dark
// consumer interfaces use anyway.
constexpr uint32_t BG        = 0x000000;   // screen ground
constexpr uint32_t HEADER    = 0x000000;   // the ground - the rule below is the bar
// A card is its OUTLINE. The inside is the screen's own black.
//
// Chosen by looking at the panel, not at a screenshot: six fill/border pairs
// were drawn on the glass at once and this is the one that separates. It is
// also the only one that cannot go wrong. Every other candidate is a dark grey
// a few counts above black, and that is the bottom of the IPS gamma curve
// where the panel stops being linear - the same region that once turned this
// interface's ground blue on the glass while the buffer held plain black, and
// which turned a near-black card into a pale blue slab. Black has nothing to
// skew: the ground already renders correctly, and the card now uses it.
//
// So the border does the whole job, and it is a neutral grey rather than the
// blue-grey it was - that one sat close enough to the ground to read as part
// of it, which left a row's edge to guesswork.
constexpr uint32_t LINE      = 0x3A4046;   // rules and row outlines
constexpr uint32_t SURFACE   = 0x000000;   // rows, cells, buttons
constexpr uint32_t TEXT      = 0xFFFFFF;
constexpr uint32_t TEXT_DIM  = 0x7C8590;
constexpr uint32_t ACCENT    = 0xF2C744;   // selection, focus, progress
constexpr uint32_t OK        = 0x3FA85E;   // reachable, success
constexpr uint32_t DANGER    = 0xE0483C;   // unreachable, destructive
// Orange is not a weaker red, it is a different category: it interrupts what
// is on screen without destroying anything. Restarting is orange, a factory
// reset is red. Without the distinction everything consequential turns red
// and red stops meaning anything.
constexpr uint32_t WARN      = 0xE8821E;   // interrupts, destroys nothing
// Working on it. The state that stops a device announcing a problem it is in
// the middle of solving - without it, every boot showed "no account" for the
// seconds before the token arrived, which is a support call.
constexpr uint32_t BUSY      = 0x2F7FFF;
constexpr uint32_t GO_BG     = 0x1E5B33;   // confirm button
constexpr uint32_t NO_BG     = 0x5A2320;   // destructive button
constexpr uint32_t WARN_BG   = 0x6B3D12;   // interrupts, destroys nothing

// ---- geometry, in device pixels --------------------------------------------
constexpr lv_coord_t SCREEN_W   = 240;
constexpr lv_coord_t SCREEN_H   = 320;
constexpr lv_coord_t HEADER_H   = 44;   // 5.6 mm - the bar is a touch target too
constexpr lv_coord_t ROW_H      = 48;   // 6.1 mm - smallest reliable finger row
constexpr lv_coord_t BUTTON_H   = 52;   // 6.6 mm - the most consequential taps
constexpr lv_coord_t ICON_HIT_W = 52;   // gear / chevron hit width
constexpr lv_coord_t GAP        = 6;
constexpr lv_coord_t PAD        = 8;
constexpr lv_coord_t RADIUS     = 9;
constexpr lv_coord_t CELL_H     = 92;   // slot cell, 11.7 mm tall

void init();                 // build the shared styles; call once after lv_init
lv_style_t* rowStyle();

// Make a scrollable container's scrollbar visible.
//
// Every list here calls lv_obj_remove_style_all, which removes the theme's
// scrollbar style along with everything else - so LV_SCROLLBAR_MODE_AUTO was
// set on lists that then drew nothing at all, and a list of eleven printers
// gave no sign that it went past the fifth. This paints one and leaves it on:
// with a touch screen and no wheel, "there is more below" has to be visible
// before the finger moves, not after.
void scrollbar(lv_obj_t* obj);
lv_style_t* rowPressedStyle();
lv_style_t* headerStyle();
lv_style_t* screenStyle();

}  // namespace theme
