#include "screen_scan.h"
#include "fonts.h"
#include "frame.h"
#include "theme.h"
#include "lvgl_port.h"
#include "i18n.h"
#include <lvgl.h>
#include <stdio.h>

namespace {
volatile bool s_cancel = false, s_dismiss = false;
enum Which { NONE, SCAN, REVIEW, RESULT } s_which = NONE;
uint32_t s_sig = 0;

void onCancel()  { s_cancel = true; }
void onDismiss() { s_dismiss = true; }

uint32_t hashStr(const char* s, uint32_t h = 2166136261u) {
    for (; s && *s; s++) h = h * 16777619u ^ (uint8_t)*s;
    return h;
}

// A colour swatch: the disc on the grid, larger, because on the confirm screen
// the colour is the main content rather than an index.
lv_obj_t* swatch(lv_obj_t* parent, uint8_t r, uint8_t g, uint8_t b, lv_coord_t d) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, d, d);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_make(r, g, b), 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(0x4A535F), 0);
    return o;
}
}  // namespace

namespace screen_scan {

void invalidate() { s_which = NONE; s_sig = 0; }

// No status dots on any of these three screens. Getting here is already the
// proof: the slot grid is only reachable through a printer that answered, and
// the tap that opened this screen came off that grid. A pair of green dots
// repeating it spends the top of the panel saying what the user just did.
void showScan(const char* slotLabel) {
    lvgl_port::Lock lvglGuard;   // LVGL is not reentrant - see lvgl_port.h
    uint32_t sig = hashStr(slotLabel);
    if (s_which == SCAN && sig == s_sig) return;
    s_which = SCAN; s_sig = sig;

    // The slot's name alone, exactly as the receipt at the end of the flow
    // titles itself. Those two screens are the same moment seen twice - before
    // and after - and a header that says "Slot B2" on one and "B2" on the
    // other reads as having moved somewhere else in between.
    lv_obj_t* body = frame::build(slotLabel, onCancel);

    lv_obj_t* sp = lv_spinner_create(body, 1400, 55);
    lv_obj_set_size(sp, 104, 104);
    lv_obj_set_style_arc_color(sp, lv_color_hex(0x1E2530), LV_PART_MAIN);
    lv_obj_set_style_arc_color(sp, lv_color_hex(theme::ACCENT), LV_PART_INDICATOR);

    frame::caption(i18n::T(S_BRING_TAG), theme::TEXT, &font_ui_14);
    frame::caption(i18n::T(S_TO_READER), theme::TEXT, &font_ui_14);

    // Tone 0, not the destructive red. Cancelling a scan throws nothing away -
    // it wore the same colour as Sign out and Factory reset, which teaches
    // people to hesitate over the one button on this screen that is harmless.
    //
    // It stays for the whole of this screen's life, including the write that
    // follows the read. Taking it away the instant the chip was caught was
    // tried and thrown out: the write is short, so the button vanished a
    // fraction of a second before the screen changed anyway - a flicker on the
    // way out, which is exactly the kind of movement that makes a device feel
    // unsteady. Nothing moves now.
    frame::button(body, i18n::T(S_CANCEL), 0, onCancel);
}

// How long the success screen stays up on its own, and the bar that shows it
// draining. The countdown is visible because an screen that vanishes without
// warning, while somebody is reading where to put the spool, is a screen that
// took the instruction away mid-sentence.
static const uint32_t RESULT_MS = 5000;
static lv_obj_t* s_timerFill = nullptr;   // width driven by msLeft, not rebuilt

void showResult(const char* slotLabel, bool ok, const char* message,
                const TagInfo& tag, uint32_t msLeft, const char* warning) {
    lvgl_port::Lock lvglGuard;   // LVGL is not reentrant - see lvgl_port.h
    // msLeft is NOT in the signature: it changes on every pass, and rebuilding
    // this screen sixty times a second is how a device stops answering taps.
    uint32_t sig = hashStr(message) ^ (uint32_t)ok ^ hashStr(slotLabel)
                 ^ hashStr(tag.material.c_str()) ^ ((uint32_t)tag.signature << 24)
                 ^ (warning ? hashStr(warning) * 31u : 0u);
    if (s_which == RESULT && sig == s_sig) {
        if (s_timerFill) {
            const lv_coord_t full = theme::SCREEN_W - 2 * theme::PAD;
            lv_coord_t w = (lv_coord_t)((uint32_t)full * msLeft / RESULT_MS);
            lv_obj_set_width(s_timerFill, w < 0 ? 0 : w);
        }
        return;
    }
    s_which = RESULT; s_sig = sig;
    s_timerFill = nullptr;

    lv_obj_t* body = frame::build(slotLabel, onDismiss);
    lv_obj_set_style_pad_row(body, 6, 0);

    // A tap ANYWHERE takes the countdown to zero and closes, not only the
    // chevron. The screen is a receipt with one instruction on it; asking
    // someone to find a target on it is asking them to solve a puzzle to
    // dismiss a message.
    //
    // The handler goes on the screen AND the containers above it, because an
    // LVGL container is clickable from birth and a click does not bubble: the
    // body covers everything under the header, so a handler on the screen
    // alone would only ever fire on the few pixels the body does not reach.
    auto dismissCb = [](lv_event_t*) { s_dismiss = true; };
    lv_obj_add_flag(frame::screen(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(frame::screen(), dismissCb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(body, dismissCb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(frame::header(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(frame::header(), dismissCb, LV_EVENT_CLICKED, nullptr);

    // Genuine or not, in the header as on the reader screen. The verification
    // runs while this screen is up (reader.h), so the mark arrives a moment
    // after the screen does; a brand that signs its spools wants it seen at
    // the moment the spool is used, not only on the reader screen.
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


    if (!ok) {
        // A failure keeps the plain shape: what went wrong, and that the screen
        // waits. There is no next step to give - the spool did not go anywhere.
        lv_obj_t* icon = lv_label_create(body);
        lv_label_set_text(icon, LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_font(icon, &font_ui_24, 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(theme::DANGER), 0);
        frame::bigLabel(message, theme::TEXT);
        // And it means it: the caption says "tap to continue", and until this
        // handler was installed for the failure too, only the chevron answered.
        frame::caption(i18n::T(S_TAP_BACK), theme::TEXT_DIM);
        return;
    }

    // The screen has no spare height: a warning line under the spool pushes the
    // countdown off the bottom, and a bar that drains out of sight is a screen
    // that closes without warning. So a warning takes the tick's place - the
    // title and the green band still say it went through.
    if (!warning || !*warning) {
        lv_obj_t* ring = lv_obj_create(body);
        lv_obj_remove_style_all(ring);
        lv_obj_set_size(ring, 52, 52);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(ring, lv_color_hex(theme::OK), 0);
        lv_obj_set_style_bg_opa(ring, 40, 0);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* tick = lv_label_create(ring);
        lv_label_set_text(tick, LV_SYMBOL_OK);
        lv_obj_set_style_text_font(tick, &font_ui_24, 0);
        lv_obj_set_style_text_color(tick, lv_color_hex(theme::OK), 0);
        lv_obj_center(tick);
    }

    lv_obj_t* title = lv_label_create(body);
    lv_label_set_text(title, i18n::T(S_SENT_TO_PRINTER));
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(title, theme::SCREEN_W - 2 * theme::PAD);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(title, &font_ui_bold_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(theme::TEXT), 0);

    // What was sent, and nothing about what the slot held before: the question
    // this screen answers is "did my spool go through", not "what changed".
    lv_obj_t* card = lv_obj_create(body);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(card, 9, 0);
    lv_obj_set_style_pad_column(card, 10, 0);
    lv_obj_set_style_radius(card, theme::RADIUS, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(theme::LINE), 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
    swatch(card, tag.r, tag.g, tag.b, 28);

    lv_obj_t* col = lv_obj_create(card);
    lv_obj_remove_style_all(col);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* mat = lv_label_create(col);
    lv_label_set_text(mat, tag.material.length() ? tag.material.c_str() : "?");
    lv_label_set_long_mode(mat, LV_LABEL_LONG_DOT);
    lv_obj_set_width(mat, LV_PCT(100));
    lv_obj_set_style_text_font(mat, &font_ui_bold_16, 0);
    lv_obj_set_style_text_color(mat, lv_color_hex(theme::TEXT), 0);

    String sub = tag.brand;
    if (tag.diameterLabel.length() && tag.diameterLabel != "-") {
        if (sub.length()) sub += " \xC2\xB7 ";
        sub += tag.diameterLabel;
    }
    lv_obj_t* who = lv_label_create(col);
    lv_label_set_text(who, sub.c_str());
    lv_label_set_long_mode(who, LV_LABEL_LONG_DOT);
    lv_obj_set_width(who, LV_PCT(100));
    lv_obj_set_style_text_font(who, &font_ui_12, 0);
    lv_obj_set_style_text_color(who, lv_color_hex(theme::TEXT_DIM), 0);

    // Directly under the spool it is about, before the instruction: the spool
    // still goes in the slot, so this must not read as a failure - the warning
    // colour, not the danger one.
    if (warning && *warning) {
        lv_obj_t* w = lv_label_create(body);
        lv_label_set_text_fmt(w, "%s %s", LV_SYMBOL_WARNING, warning);
        lv_label_set_long_mode(w, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(w, LV_PCT(100));
        lv_obj_set_style_text_align(w, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(w, &font_ui_12, 0);
        lv_obj_set_style_text_color(w, lv_color_hex(theme::WARN), 0);
        lv_obj_clear_flag(w, LV_OBJ_FLAG_CLICKABLE);
    }

    // Pushes the instruction to the bottom of the screen, where the thumb is.
    lv_obj_t* spacer = lv_obj_create(body);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_width(spacer, 1);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_CLICKABLE);

    // ALWAYS two lines and always the same height, whatever the slot is called.
    // A block that grows a line when a printer names its slots "AMS2-4" and
    // shrinks again on "1" makes the screen jump between two spools, and the
    // eye reads that as two different screens.
    lv_obj_t* band = lv_obj_create(body);
    lv_obj_remove_style_all(band);
    lv_obj_set_width(band, LV_PCT(100));
    lv_obj_set_height(band, 68);
    lv_obj_set_style_radius(band, theme::RADIUS, 0);
    lv_obj_set_style_bg_color(band, lv_color_hex(theme::GO_BG), 0);
    lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(band, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(band, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(band, 6, 0);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* l1 = lv_label_create(band);
    lv_label_set_text(l1, i18n::T(S_INSERT_IN));
    lv_label_set_long_mode(l1, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l1, LV_PCT(100));
    lv_obj_set_style_text_align(l1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(l1, &font_ui_14, 0);
    lv_obj_set_style_text_color(l1, lv_color_hex(theme::TEXT), 0);

    // The slot name on the second line, on its own: it is the one word the
    // user has to carry to the machine.
    lv_obj_t* l2 = lv_label_create(band);
    lv_label_set_text(l2, slotLabel);
    lv_label_set_long_mode(l2, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l2, LV_PCT(100));
    lv_obj_set_style_text_align(l2, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(l2, &font_ui_20, 0);
    lv_obj_set_style_text_color(l2, lv_color_hex(theme::TEXT), 0);

    // The timer, as a bar that drains. Four pixels: it says "this is going to
    // close" without competing with the instruction above it.
    lv_obj_t* track = lv_obj_create(body);
    lv_obj_remove_style_all(track);
    lv_obj_set_size(track, LV_PCT(100), 4);
    lv_obj_set_style_radius(track, 2, 0);
    lv_obj_set_style_bg_color(track, lv_color_hex(0x1E2530), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);

    // Built at the width the caller asks for, not at full: the screen can be
    // rebuilt mid-countdown - a language change, a redraw after a preview - and
    // a bar that jumps back to full is a timer that lies.
    const lv_coord_t fullW = theme::SCREEN_W - 2 * theme::PAD;
    lv_coord_t startW = (lv_coord_t)((uint32_t)fullW * (msLeft > RESULT_MS ? RESULT_MS : msLeft)
                                     / RESULT_MS);
    s_timerFill = lv_obj_create(track);
    lv_obj_remove_style_all(s_timerFill);
    lv_obj_set_size(s_timerFill, startW, 4);
    lv_obj_set_style_radius(s_timerFill, 2, 0);
    lv_obj_set_style_bg_color(s_timerFill, lv_color_hex(theme::OK), 0);
    lv_obj_set_style_bg_opa(s_timerFill, LV_OPA_COVER, 0);
    lv_obj_align(s_timerFill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_clear_flag(s_timerFill, LV_OBJ_FLAG_CLICKABLE);

    frame::caption(i18n::T(S_TAP_BACK), theme::TEXT_DIM);
}

bool takeCancel()  { bool v = s_cancel;  s_cancel = false;  return v; }
bool takeDismiss() { bool v = s_dismiss; s_dismiss = false; return v; }

}  // namespace screen_scan
