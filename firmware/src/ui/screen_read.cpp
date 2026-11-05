#include "screen_read.h"
#include "frame.h"
#include "fonts.h"
#include "icons.h"
#include "theme.h"
#include "lvgl_port.h"
#include "../i18n.h"
#include "i18n.h"
#include <lvgl.h>
#include <Arduino.h>

namespace {

volatile bool s_back = false;
void onBack() { s_back = true; }

// What is on screen, so a tag held on the pad does not rebuild the screen sixty
// times a second. The product id and the remaining quantity together are enough
// to tell one spool from another and to notice the scale updating this one.
uint32_t s_sig = 0;

// A value row: the word on the left, the number on the right, both white.
//
// The Settings rows dim their labels because the value is what you came for
// there. Here the pair IS the content - "Nozzle 215-230" is one fact in two
// halves - and a dim half reads as decoration.
//
// `note`, when given, is a small dim word after the value - where the value came
// from, not part of it.
void factRow(lv_obj_t* parent, const char* key, const char* value,
             const char* note = nullptr) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(row, 4, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    // An LVGL container is clickable from birth and a click does not bubble.
    // These rows cover most of the screen, and the screen is one big back
    // button - see showTag.
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* k = lv_label_create(row);
    lv_label_set_text(k, key);
    lv_obj_set_style_text_font(k, &font_ui_14, 0);
    lv_obj_set_style_text_color(k, lv_color_hex(theme::TEXT), 0);

    lv_obj_t* v = lv_label_create(row);
    lv_label_set_text(v, value);
    lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(v, 150, 0);
    lv_obj_set_style_text_font(v, &font_ui_bold_16, 0);
    lv_obj_set_style_text_color(v, lv_color_hex(theme::TEXT), 0);

    if (note) {
        // The key takes the slack, so value and note stay together on the right.
        lv_obj_set_flex_grow(k, 1);
        lv_obj_t* n = lv_label_create(row);
        lv_label_set_text(n, note);
        lv_obj_set_style_text_font(n, &font_ui_12, 0);
        lv_obj_set_style_text_color(n, lv_color_hex(theme::TEXT_DIM), 0);
        lv_obj_set_style_pad_left(n, 5, 0);
    }
}

}  // namespace

namespace screen_read {

void invalidate() { s_sig = 0; }
bool takeBack()   { bool v = s_back; s_back = false; return v; }

void showWaiting() {
    lvgl_port::Lock lvglGuard;   // LVGL is not reentrant - see lvgl_port.h
    // A constant that no tag signature can collide with.
    const uint32_t WAITING = 0x57414954u;
    if (s_sig == WAITING) return;
    s_sig = WAITING;

    lv_obj_t* body = frame::build(i18n::T(S_READ_MODE), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    // The reader's own wave in a ring, the same shape the tester uses while it
    // waits: this device has one picture for "here is the state of the thing
    // you came to look at", and a bare glyph floating in the middle would be a
    // second idea for the same job.
    lv_obj_t* ring = lv_obj_create(body);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 96, 96);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(theme::ACCENT), 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_opa(ring, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(icons::build(ring, icons::NFC, theme::ACCENT, 220));

    lv_obj_t* put = frame::caption(i18n::T(S_READ_PUT_SPOOL), theme::TEXT);
    lv_obj_set_style_text_font(put, &font_ui_16, 0);
    lv_obj_set_style_pad_top(put, 20, 0);

    lv_obj_t* where = frame::caption(i18n::T(S_READ_ON_READER), theme::TEXT_DIM);
    lv_obj_set_style_text_font(where, &font_ui_12, 0);
    lv_obj_set_style_pad_top(where, 4, 0);
}

void showTag(const TagInfo& tag, bool fromCloud, int cloudGrams, bool cloudPending) {
    lvgl_port::Lock lvglGuard;   // LVGL is not reentrant - see lvgl_port.h
    const uint32_t sig = 0xA5000000u ^ tag.idProduct ^ (tag.available << 3)
                       ^ ((uint32_t)tag.signature << 24)
                       ^ (fromCloud ? 0x00800000u ^ ((uint32_t)(cloudGrams + 2) << 5)
                                      ^ (cloudPending ? 0x00400000u : 0u) : 0u);
    if (sig == s_sig) return;
    s_sig = sig;

    lv_obj_t* body = frame::build(i18n::T(S_READ_MODE), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    // The whole screen goes back, not just the chevron. Somebody holding a
    // spool in one hand has read what they came for; making them find a 56 px
    // target in the corner to leave is a tax on the one hand they have free.
    //
    // The handler is on the screen AND the containers over it: a click does
    // not bubble in LVGL, and the body covers everything under the header.
    auto backCb = [](lv_event_t*) { s_back = true; };
    lv_obj_add_flag(frame::screen(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(frame::screen(), backCb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(body, backCb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(frame::header(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(frame::header(), backCb, LV_EVENT_CLICKED, nullptr);

    // Genuine, in the header rather than in the body: it is a property of the
    // chip, not a field of the spool, and putting it among the temperatures
    // would give it a weight it does not deserve until it is WRONG.
    if (tag.signature == TagInfo::SIG_VALID || tag.signature == TagInfo::SIG_INVALID) {
        const bool good = tag.signature == TagInfo::SIG_VALID;
        lv_obj_t* mark = lv_label_create(frame::header());
        // Not certified is a fact about the chip, not a fault: plain grey
        // text, no warning sign and no red - most spools in the world carry
        // no signature at all, and none of them is an error.
        if (good) lv_label_set_text_fmt(mark, "%s %s", LV_SYMBOL_OK, i18n::T(S_SIG_VALID));
        else      lv_label_set_text(mark, i18n::T(S_SIG_INVALID));
        lv_obj_set_style_text_font(mark, &font_ui_12, 0);
        lv_obj_set_style_text_color(mark, lv_color_hex(good ? theme::OK : theme::TEXT_DIM), 0);
        lv_obj_align(mark, LV_ALIGN_RIGHT_MID, -10, 0);
    }

    // The colour, as the spool's own disc. It is what a person recognises
    // before reading a word of the screen.
    //
    // 60 px rather than the 68 it started at: with drying on the screen there
    // are four value rows under it, and on 320 px the last of them - what is
    // left on the spool - fell off the bottom. The disc is the one element
    // that reads the same a little smaller.
    lv_obj_t* disc = lv_obj_create(body);
    lv_obj_remove_style_all(disc);
    lv_obj_set_size(disc, 60, 60);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, lv_color_make(tag.r, tag.g, tag.b), 0);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(disc, lv_color_hex(theme::LINE), 0);
    lv_obj_set_style_border_width(disc, 2, 0);
    lv_obj_set_style_border_opa(disc, LV_OPA_COVER, 0);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* mat = lv_label_create(body);
    lv_label_set_text(mat, tag.material.length() ? tag.material.c_str() : "?");
    lv_label_set_long_mode(mat, LV_LABEL_LONG_DOT);
    lv_obj_set_width(mat, theme::SCREEN_W - 2 * theme::PAD - 6);
    lv_obj_set_style_text_align(mat, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(mat, &font_ui_20, 0);
    lv_obj_set_style_text_color(mat, lv_color_hex(theme::TEXT), 0);
    lv_obj_set_style_pad_top(mat, 6, 0);

    // Brand, finish and diameter on one line: three short facts that belong
    // together and that nobody reads one at a time.
    String sub = tag.brand;
    if (tag.aspect1Label.length() && tag.aspect1Label != "-") {
        if (sub.length()) sub += " · ";
        sub += tag.aspect1Label;
    }
    if (tag.diameterLabel.length() && tag.diameterLabel != "-") {
        if (sub.length()) sub += " · ";
        sub += tag.diameterLabel;
    }
    lv_obj_t* who = lv_label_create(body);
    lv_label_set_text(who, sub.c_str());
    lv_label_set_long_mode(who, LV_LABEL_LONG_DOT);
    lv_obj_set_width(who, theme::SCREEN_W - 2 * theme::PAD - 6);
    lv_obj_set_style_text_align(who, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(who, &font_ui_14, 0);
    lv_obj_set_style_text_color(who, lv_color_hex(theme::TEXT_DIM), 0);
    lv_obj_set_style_pad_bottom(who, 10, 0);

    char buf[32];
    if (tag.nozMin || tag.nozMax) {
        snprintf(buf, sizeof(buf), "%u - %u °C", tag.nozMin, tag.nozMax);
        factRow(body, i18n::T(S_NOZZLE), buf);
    }
    if (tag.bedMin || tag.bedMax) {
        snprintf(buf, sizeof(buf), "%u - %u °C", tag.bedMin, tag.bedMax);
        factRow(body, i18n::T(S_BED), buf);
    }
    // How to dry it, on one line: the temperature and the hours are never read
    // apart, and two rows for one instruction would push what is left on the
    // spool off the bottom of a 320 px screen.
    if (tag.dryTemp || tag.dryHours) {
        snprintf(buf, sizeof(buf), "%u °C · %u h", tag.dryTemp, tag.dryHours);
        factRow(body, i18n::T(S_DRYING), buf);
    }
    // What is left, from the source the user chose, named after the value so a
    // figure from the chip is never taken for the account's, or the reverse.
    //
    // From the account the row is always there: "..." while the answer is on
    // its way, "-" when the account has no figure. From the chip it is shown
    // only when a scale has written one - a spool nobody weighed reads 0, and
    // "0 g" on a full spool is worse than no line at all.
    if (fromCloud) {
        if (cloudPending)        snprintf(buf, sizeof(buf), "...");
        else if (cloudGrams < 0) snprintf(buf, sizeof(buf), "-");
        else                     snprintf(buf, sizeof(buf), "%d g", cloudGrams);
        factRow(body, i18n::T(S_REMAINING), buf, i18n::T(S_SRC_CLOUD));
    } else if (tag.available) {
        snprintf(buf, sizeof(buf), "%lu %s", (unsigned long)tag.available,
                 tag.unitLabel.length() ? tag.unitLabel.c_str() : "g");
        factRow(body, i18n::T(S_REMAINING), buf, i18n::T(S_SRC_CHIP));
    }
}

}  // namespace screen_read
