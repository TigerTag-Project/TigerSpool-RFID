#include "frame.h"
#include "fonts.h"
#include "theme.h"

namespace {
lv_obj_t* s_screen = nullptr;
lv_obj_t* s_header = nullptr;
lv_obj_t* s_body   = nullptr;
lv_obj_t* s_dots[3] = { nullptr, nullptr, nullptr };
frame::Callback s_onBack = nullptr;

void backCb(lv_event_t*)  { if (s_onBack) s_onBack(); }

void clickCb(lv_event_t* e) {
    auto fn = (frame::Callback)lv_event_get_user_data(e);
    if (fn) fn();
}

lv_obj_t* dot(lv_obj_t* parent) {
    lv_obj_t* d = lv_obj_create(parent);
    lv_obj_remove_style_all(d);
    lv_obj_set_size(d, 9, 9);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(d, lv_color_hex(theme::TEXT_DIM), 0);
    return d;
}
}  // namespace

namespace frame {

lv_obj_t* build(const char* title, Callback onBack) {
    s_onBack = onBack;

    lv_obj_t* old = s_screen;
    s_screen = lv_obj_create(nullptr);
    lv_obj_add_style(s_screen, theme::screenStyle(), 0);
    // Local properties, not just the shared style: a local property is the
    // highest-precedence source in LVGL's cascade, so the ground is this colour
    // whatever a theme has to say about it.
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(theme::BG), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    s_header = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_header);
    lv_obj_add_style(s_header, theme::headerStyle(), 0);
    lv_obj_set_size(s_header, theme::SCREEN_W, theme::HEADER_H);
    lv_obj_align(s_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(s_header, LV_OBJ_FLAG_SCROLLABLE);

    lv_coord_t titleX = 9;
    if (onBack) {
        // 56 x 44 of hit area for a 24 px glyph. The chevron carries no word:
        // "back" cost 40 px of a 240 px bar and said nothing the arrow does not.
        lv_obj_t* back = lv_btn_create(s_header);
        lv_obj_remove_style_all(back);
        lv_obj_set_size(back, 56, theme::HEADER_H);
        lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_add_event_cb(back, backCb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* g = lv_label_create(back);
        lv_label_set_text(g, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_font(g, &font_ui_24, 0);
        lv_obj_center(g);
        titleX = 56;
    }

    // No title means NO LABEL. Created and then handed a null string, an LVGL
    // label keeps the placeholder it ships with, and the word "Text" appeared
    // in the header of the one screen that asks for no header at all - the
    // firmware download, where the user is being told not to unplug the box.
    if (title) {
        lv_obj_t* t = lv_label_create(s_header);
        lv_label_set_text(t, title);
        lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
        lv_obj_set_width(t, theme::SCREEN_W - titleX - 60);
        // ONE line, always. LV_LABEL_LONG_DOT wraps first and only then dots at
        // the bottom of the box, so with a content-sized height a long title
        // grew the label to two lines, pushed itself out of a 44 px header and
        // took the rule under it off the screen. A header is a fixed thing; a
        // title too long for it is truncated, not accommodated.
        lv_obj_set_height(t, lv_font_get_line_height(&font_ui_bold_16));
        lv_obj_set_style_text_font(t, &font_ui_bold_16, 0);
        lv_obj_align(t, LV_ALIGN_LEFT_MID, titleX, 0);
    }

    for (int i = 0; i < 3; i++) {
        s_dots[i] = dot(s_header);
        lv_obj_align(s_dots[i], LV_ALIGN_RIGHT_MID, -8 - (2 - i) * 14, 0);
        lv_obj_add_flag(s_dots[i], LV_OBJ_FLAG_HIDDEN);
    }

    s_body = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_body);
    lv_obj_set_size(s_body, theme::SCREEN_W, theme::SCREEN_H - theme::HEADER_H);
    lv_obj_align(s_body, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(s_body, theme::PAD, 0);
    lv_obj_set_style_pad_row(s_body, theme::GAP, 0);
    lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_body, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(s_body, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_body, LV_OBJ_FLAG_SCROLLABLE);

    lv_scr_load(s_screen);
    // Freeing the previous screen only after the new one is loaded: deleting a
    // screen LVGL is still showing takes the whole UI down with it.
    if (old) lv_obj_del(old);
    return s_body;
}

lv_obj_t* screen() { return s_screen; }
lv_obj_t* header() { return s_header; }
lv_obj_t* body()   { return s_body; }

void setDots(int syncing, int wifiUp, int readerUp) {
    const int  vals[3]    = { syncing, wifiUp, readerUp };
    const uint32_t on[3]  = { theme::OK, theme::OK, theme::OK };
    for (int i = 0; i < 3; i++) {
        if (!s_dots[i]) continue;
        if (vals[i] < 0) { lv_obj_add_flag(s_dots[i], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(s_dots[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(s_dots[i],
            lv_color_hex(vals[i] ? on[i] : theme::DANGER), 0);
    }
}

lv_obj_t* caption(const char* text, uint32_t colour, const lv_font_t* font) {
    lv_obj_t* l = lv_label_create(s_body);
    lv_label_set_text(l, text);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, theme::SCREEN_W - 2 * theme::PAD - 6);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
    lv_obj_set_style_text_font(l, font ? font : &font_ui_12, 0);
    return l;
}

lv_obj_t* bigLabel(const char* text, uint32_t colour) {
    lv_obj_t* l = lv_label_create(s_body);
    lv_label_set_text(l, text);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, theme::SCREEN_W - 2 * theme::PAD);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
    lv_obj_set_style_text_font(l, &font_ui_20, 0);
    return l;
}

lv_obj_t* button(lv_obj_t* parent, const char* text, int tone, Callback onClick) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, theme::rowStyle(), 0);
    lv_obj_add_style(b, theme::rowPressedStyle(), LV_STATE_PRESSED);
    lv_obj_set_size(b, LV_PCT(100), theme::BUTTON_H);
    if (tone == 1) lv_obj_set_style_bg_color(b, lv_color_hex(theme::GO_BG), 0);
    if (tone == 2) lv_obj_set_style_bg_color(b, lv_color_hex(theme::NO_BG), 0);
    // 3: it interrupts and destroys nothing - the palette's own definition of
    // WARN, and what the Restart row in the menu already says. A restart button
    // in the green of an install claimed the wrong thing about itself.
    if (tone == 3) lv_obj_set_style_bg_color(b, lv_color_hex(theme::WARN_BG), 0);
    lv_obj_add_event_cb(b, clickCb, LV_EVENT_CLICKED, (void*)onClick);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    // 20, not 14 - and Medium, not bold. A button is 52 px tall because it is
    // the most consequential tap on the screen, and its label was set in the
    // size used for a row's secondary value, so it read as one. Size is what
    // was missing; weight was tried here and taken back out. The bold face is
    // for titles, and a screen where the title and every button are both bold
    // has no hierarchy left to express.
    lv_obj_set_style_text_font(l, &font_ui_20, 0);
    lv_obj_center(l);
    return b;
}

lv_obj_t* row(lv_obj_t* parent, const char* label, const char* value,
              bool chevron, lv_event_cb_t cb, void* userData,
              icons::Id icon, uint32_t iconColour) {
    lv_obj_t* r = lv_btn_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_add_style(r, theme::rowStyle(), 0);
    lv_obj_add_style(r, theme::rowPressedStyle(), LV_STATE_PRESSED);
    lv_obj_set_size(r, LV_PCT(100), theme::ROW_H);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, 6, 0);
    if (cb) lv_obj_add_event_cb(r, cb, LV_EVENT_CLICKED, userData);

    // A fixed 22 px column, so every label on a menu starts at the same x
    // whatever sits beside it. A ragged left edge down a list of eight rows is
    // more distracting than no icons at all.
    if (icon != icons::NONE)
        icons::build(r, icon, iconColour ? iconColour : theme::TEXT);

    lv_obj_t* l = lv_label_create(r);
    lv_label_set_text(l, label);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(l, 1);
    lv_obj_set_style_min_width(l, icon != icons::NONE ? 66 : 84, 0);
    // A row's LABEL is set like a title: 16 and SemiBold, not 14 Medium.
    //
    // It is the thing a person reads to decide where to go, on a 48 px row they
    // are about to press, on a panel held at arm's length. At 14 Medium it was
    // the same weight and nearly the same size as the dim value beside it, and
    // the two competed. The value stays 14 and dim, and that difference is now
    // what tells them apart.
    lv_obj_set_style_text_font(l, &font_ui_bold_16, 0);

    if (value && *value) {
        // The value gets a ceiling and truncates; the label never does. An
        // email address is longer than the row, and letting it take the space
        // it asks for wrapped "Account" onto two lines - the one word on the
        // row that has to stay readable.
        lv_obj_t* v = lv_label_create(r);
        lv_label_set_text(v, value);
        lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
        // The icon column and the full-size chevron together cost about 55 px
        // of a 240 px row, and both come out of the value - which truncates by
        // design - rather than out of the chevron, which is the only mark on
        // the row that says it opens something.
        lv_obj_set_style_max_width(v, icon != icons::NONE ? 74 : 100, 0);
        lv_obj_set_style_text_color(v, lv_color_hex(theme::TEXT_DIM), 0);
        lv_obj_set_style_text_font(v, &font_ui_12, 0);
    }
    if (chevron) {
        // Half the height of the row, which is what it is on the scale. At 12
        // px it was a punctuation mark; the chevron is the only thing on the
        // row that says it opens something, so it has to carry.
        lv_obj_t* c = lv_label_create(r);
        lv_label_set_text(c, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(c, lv_color_hex(theme::TEXT_DIM), 0);
        lv_obj_set_style_text_font(c, &font_ui_24, 0);
    }
    return r;
}

}  // namespace frame
