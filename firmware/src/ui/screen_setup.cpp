#include "screen_setup.h"
#include "theme.h"
#include "i18n.h"
#include <Arduino.h>

namespace {

lv_obj_t* s_screen = nullptr;
lv_obj_t* s_body   = nullptr;
bool      s_active = false;
int       s_lang   = -1;
bool      s_pair   = false;

void onLang(lv_event_t* e) { s_lang = (int)(intptr_t)lv_event_get_user_data(e); }
void onPair(lv_event_t*)   { s_pair = true; }
int  s_choice = -1;
bool s_back = false;
void onBack(lv_event_t*) { s_back = true; }

// A floating chevron for the header-less screens. 56 x 44 of hit area for a
// 20 px glyph, the same target as everywhere else on this device.
// Back is the whole top strip, not a chevron.
//
// A 56 x 44 icon was too small to hit reliably - which is the same mistake as
// sizing a target to its glyph, made again after saying not to. The strip is
// the full 240 px wide and 56 tall, carries the word next to the arrow, and
// fires on PRESS rather than on release: a back control that waits for the
// finger to lift feels broken on a screen this size, because a slight drag
// between press and release cancels it.
void addBack() {
    if (s_body) lv_obj_set_style_pad_top(s_body, 56, 0);

    lv_obj_t* b = lv_btn_create(s_screen);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, theme::SCREEN_W, 56);
    lv_obj_align(b, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(b, 8, 0);
    lv_obj_set_style_pad_column(b, 4, 0);
    // No pressed state: firing on PRESS means the screen is already gone by the
    // time a highlight could be seen. Feedback that never renders is a frame of
    // work per touch for nothing.
    lv_obj_add_event_cb(b, onBack, LV_EVENT_PRESSED, nullptr);

    lv_obj_t* g = lv_label_create(b);
    lv_label_set_text(g, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(g, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g, lv_color_hex(theme::TEXT_DIM), 0);

    lv_obj_t* w = lv_label_create(b);
    lv_label_set_text(w, i18n::T(S_BACK));
    lv_obj_set_style_text_font(w, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(w, lv_color_hex(theme::TEXT_DIM), 0);
}
void onChoice(lv_event_t* e) { s_choice = (int)(intptr_t)lv_event_get_user_data(e); }

// One screen object reused across all three steps: building and destroying a
// screen per step would flash the panel between them, and the whole point of
// this sequence is that it feels like one continuous thing.
// The header the last frame() built, so a screen can hang one more control in
// it without frame() having to know about that control.
lv_obj_t* s_setupHeader = nullptr;

lv_obj_t* frame(const char* title, bool withBack = false) {
    if (!s_screen) {
        s_screen = lv_obj_create(nullptr);
        lv_obj_add_style(s_screen, theme::screenStyle(), 0);
    // Local properties, not just the shared style: a local property is the
    // highest-precedence source in LVGL's cascade, so the ground is this colour
    // whatever a theme has to say about it.
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(theme::BG), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
        lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_clean(s_screen);

    // A null title means no header at all. On a screen whose whole content is
    // "scan this square", a bar repeating "Wi-Fi setup" tells nobody anything
    // they cannot already see - and the 44 px it costs are exactly what the
    // layout needs to breathe.
    if (!title) {
        s_setupHeader = nullptr;      // headerless screens have nowhere to hang one
        s_body = lv_obj_create(s_screen);
        lv_obj_remove_style_all(s_body);
        lv_obj_set_size(s_body, theme::SCREEN_W, theme::SCREEN_H);
        lv_obj_align(s_body, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_pad_all(s_body, theme::PAD, 0);
        lv_obj_set_style_pad_row(s_body, 0, 0);
        lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(s_body, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(s_body, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(s_body, LV_OBJ_FLAG_SCROLLABLE);
        if (!s_active) { lv_scr_load(s_screen); s_active = true; }
        lv_obj_invalidate(s_screen);
        return s_body;
    }

    lv_obj_t* header = s_setupHeader = lv_obj_create(s_screen);
    lv_obj_remove_style_all(header);
    lv_obj_add_style(header, theme::headerStyle(), 0);
    lv_obj_set_size(header, theme::SCREEN_W, theme::HEADER_H);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // The chevron shares the header rather than floating over it, so a titled
    // screen keeps one bar instead of two stacked strips.
    lv_coord_t titleX = 9;
    if (withBack) {
        lv_obj_t* b = lv_btn_create(header);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, 56, theme::HEADER_H);
        lv_obj_align(b, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_add_event_cb(b, onBack, LV_EVENT_PRESSED, nullptr);
        lv_obj_t* g = lv_label_create(b);
        lv_label_set_text(g, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_font(g, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(g, lv_color_hex(theme::TEXT_DIM), 0);
        lv_obj_center(g);
        titleX = 56;
    }

    lv_obj_t* t = lv_label_create(header);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, titleX, 0);

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

    if (!s_active) { lv_scr_load(s_screen); s_active = true; }
    lv_obj_invalidate(s_screen);

    // What LVGL actually resolved for the screen it is about to show, and for
    // the body drawn on top of it. If these are right and the glass is wrong,
    // the fault is in the panel path, not the styling.
    {
        lv_color_t sc = lv_obj_get_style_bg_color(s_screen, LV_PART_MAIN);
        lv_opa_t   so = lv_obj_get_style_bg_opa(s_screen, LV_PART_MAIN);
        lv_color_t bc = lv_obj_get_style_bg_color(s_body, LV_PART_MAIN);
        lv_opa_t   bo = lv_obj_get_style_bg_opa(s_body, LV_PART_MAIN);
        Serial.printf("[ui] setup screen bg=0x%04X(r%u g%u b%u) opa=%u | "
                      "body bg=0x%04X(r%u g%u b%u) opa=%u\n",
                      sc.full, sc.ch.red, sc.ch.green, sc.ch.blue, so,
                      bc.full, bc.ch.red, bc.ch.green, bc.ch.blue, bo);
    }
    return s_body;
}

lv_obj_t* caption(const char* text, uint32_t colour = theme::TEXT_DIM) {
    lv_obj_t* l = lv_label_create(s_body);
    lv_label_set_text(l, text);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, theme::SCREEN_W - 2 * theme::PAD - 8);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    return l;
}

// The Google mark, drawn rather than shipped as an image.
//
// It is four arcs and a bar, in Google's own brand colours - the button has to
// be recognisable at a glance or it is just another grey row, and an image
// asset for five shapes would cost more flash than the shapes do.
//
// LVGL angles start at 3 o'clock and run clockwise. The bar is the blue
// segment's crossbar and has to sit on the vertical centre, or the G reads as
// a C.
lv_obj_t* googleMark(lv_obj_t* parent, lv_coord_t d) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, d, d);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    struct Seg { uint16_t from, to; uint32_t colour; };
    static const Seg segs[4] = {
        { 336,  62, 0x4285F4 },   // blue   - right
        {  62, 158, 0x34A853 },   // green  - bottom
        { 158, 216, 0xFBBC05 },   // yellow - left
        { 216, 336, 0xEA4335 },   // red    - top
    };
    const lv_coord_t stroke = d / 5;
    for (int i = 0; i < 4; i++) {
        lv_obj_t* a = lv_arc_create(box);
        lv_obj_remove_style(a, nullptr, LV_PART_KNOB);
        lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(a, d, d);
        lv_obj_center(a);
        lv_arc_set_bg_angles(a, segs[i].from, segs[i].to);
        lv_obj_set_style_arc_color(a, lv_color_hex(segs[i].colour), LV_PART_MAIN);
        lv_obj_set_style_arc_width(a, stroke, LV_PART_MAIN);
        lv_obj_set_style_arc_width(a, 0, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
    }

    lv_obj_t* bar = lv_obj_create(box);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, d / 2 - stroke / 2, stroke);
    lv_obj_align(bar, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x4285F4), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    return box;
}

// A QR big enough to scan from arm's length. 132 px is 16.8 mm; below about
// 15 mm a phone has to be held close enough that the screen glares.
lv_obj_t* qr(const char* payload, lv_coord_t size = 132) {
    lv_obj_t* q = lv_qrcode_create(s_body, size,
                                   lv_color_black(), lv_color_white());
    lv_qrcode_update(q, payload, strlen(payload));
    lv_obj_set_style_border_width(q, 5, 0);          // quiet zone
    lv_obj_set_style_border_color(q, lv_color_white(), 0);
    return q;
}

}  // namespace

namespace screen_setup {

static bool s_langBuilt = false;
bool s_rotate = false;
void onRotatePress(lv_event_t*) { s_rotate = true; }

void showLanguage(bool force, bool withBack) {
    if (force) { s_active = false; }
    // Leaving without choosing has to be possible. Reached from Settings, this
    // screen had no exit at all: the only way out was to pick a language,
    // including for someone who opened it to check which one was set.
    static bool lastBack = false;
    if (lastBack != withBack) { s_langBuilt = false; lastBack = withBack; }
    // Built once. frame() clears and rebuilds, and this screen is called from
    // the main loop, so without this guard the list would be torn down and
    // rebuilt every iteration - which cancels the scroll and eats the taps.
    if (s_langBuilt && s_active) return;
    s_langBuilt = true;

    // From Settings the title is the row that opened it, not the first-boot
    // question: "Language" matches what the user tapped, and the long form does
    // not fit beside a back chevron - it was clipped mid-word.
    lv_obj_t* body = frame(i18n::T(withBack ? S_LANGUAGE : S_CHOOSE_LANG), withBack);

    // First boot only. Reached from Settings there is a proper control under
    // Display, and a second way to do the same thing on a screen that already
    // has a back chevron is clutter.
    if (!withBack && s_setupHeader) {
        lv_obj_t* r = lv_btn_create(s_setupHeader);
        lv_obj_remove_style_all(r);
        lv_obj_set_size(r, 52, theme::HEADER_H);
        lv_obj_align(r, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(r, onRotatePress, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* g = lv_label_create(r);
        lv_label_set_text(g, LV_SYMBOL_LOOP);
        lv_obj_set_style_text_font(g, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(g, lv_color_hex(theme::TEXT_DIM), 0);
        lv_obj_center(g);
    }

    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

    for (int i = 0; i < LANG_N; i++) {
        lv_obj_t* row = lv_btn_create(body);
        lv_obj_remove_style_all(row);
        lv_obj_add_style(row, theme::rowStyle(), 0);
        lv_obj_add_style(row, theme::rowPressedStyle(), LV_STATE_PRESSED);
        lv_obj_set_size(row, LV_PCT(100), theme::ROW_H);
        lv_obj_add_event_cb(row, onLang, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t* l = lv_label_create(row);
        // Every language is written in itself. A user looking for Portugues
        // should not have to recognise the English word "Portuguese" first.
        lv_label_set_text(l, i18n::name((Lang)i));
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_center(l);
    }
}

int takeLanguage() { int v = s_lang; s_lang = -1; return v; }

void showWifi(const char* apSsid, const char* apPass) {
    frame(nullptr);            // no header: the QR explains itself

    // Two instructions, one per route, each sitting with the thing it describes:
    // the scan line above the QR, the join line above the network name. Reading
    // "scan this" after the square has gone past is reading it too late.
    //
    // Both are set at the same size as the network name. This screen is read at
    // arm's length on a 2.0" panel; anything smaller is decoration.
    lv_obj_t* scan = lv_label_create(s_body);
    lv_label_set_text(scan, i18n::T(S_AP_JOIN));
    lv_label_set_long_mode(scan, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(scan, theme::SCREEN_W - 2 * theme::PAD - 6);
    lv_obj_set_style_text_align(scan, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(scan, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(scan, lv_color_hex(theme::TEXT), 0);
    lv_obj_set_style_pad_bottom(scan, 20, 0);   // air before the square

    // The standard Wi-Fi join format, which both phone cameras recognise
    // natively - no app, and no SSID read off a small screen and typed.
    // T:WPA and the key, so a camera joins an encrypted network without
    // anyone reading a password off a 2" panel. The manual route below still
    // needs it printed, which is why it is on the screen as well.
    char payload[128];
    snprintf(payload, sizeof(payload), "WIFI:T:WPA;S:%s;P:%s;;", apSsid, apPass);
    qr(payload);

    // The fallback route, for a camera that will not scan.
    lv_obj_t* orJoin = lv_label_create(s_body);
    lv_obj_set_style_pad_top(orJoin, 22, 0);    // and after it
    lv_label_set_text(orJoin, i18n::T(S_OR_JOIN));
    lv_obj_set_style_text_font(orJoin, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(orJoin, lv_color_hex(theme::TEXT_DIM), 0);

    lv_obj_t* ssid = lv_label_create(s_body);
    lv_obj_set_style_pad_top(ssid, 4, 0);
    lv_label_set_text(ssid, apSsid);
    lv_obj_set_style_text_font(ssid, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ssid, lv_color_hex(theme::ACCENT), 0);

    // The key, for the manual route. The QR carries it too, so this is only
    // read by someone whose camera would not scan - but that someone has no
    // other way in, and an access point they cannot join is worse than an open
    // one.
    lv_obj_t* pass = lv_label_create(s_body);
    lv_obj_set_style_pad_top(pass, 2, 0);
    lv_label_set_text(pass, apPass);
    lv_obj_set_style_text_font(pass, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(pass, lv_color_hex(theme::ACCENT), 0);

    // No "waiting for a phone", and no count of connected devices. Neither
    // tells the user anything they can act on: the screen already says what to
    // do, and how many phones happen to be attached is the device's business,
    // not theirs.
}

void showWifiConnecting(const char* ssid, int secondsLeft) {
    frame("Wi-Fi");
    lv_obj_t* sp = lv_spinner_create(s_body, 1200, 60);
    lv_obj_set_size(sp, 72, 72);
    lv_obj_set_style_arc_color(sp, lv_color_hex(theme::ACCENT), LV_PART_INDICATOR);

    lv_obj_t* n = lv_label_create(s_body);
    lv_label_set_text(n, ssid);
    lv_obj_set_style_text_font(n, &lv_font_montserrat_16, 0);

    char t[32];
    snprintf(t, sizeof(t), "%s  %ds", i18n::T(S_CONNECTING), secondsLeft);
    caption(t);
}

void showWifiFailed(const char* ssid) {
    frame("Wi-Fi");
    lv_obj_t* x = lv_label_create(s_body);
    lv_label_set_text(x, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(x, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(x, lv_color_hex(theme::DANGER), 0);

    lv_obj_t* n = lv_label_create(s_body);
    lv_label_set_text(n, ssid);
    lv_obj_set_style_text_font(n, &lv_font_montserrat_16, 0);

    // Naming the likely cause is the whole difference between a message a user
    // can act on and one that sends them to a forum.
    caption(i18n::T(S_WIFI_BAD_PASSWORD), theme::TEXT);
}

void showSignInChoice() {
    static bool built = false;
    if (built && s_active) return;
    built = true;

    frame(nullptr);

    // Two lines, and the break is in the translation rather than left to
    // wrapping: every language gets to split where its own grammar allows,
    // instead of wherever 240 px happens to fall.
    lv_obj_t* t = lv_label_create(s_body);
    lv_label_set_text(t, i18n::T(S_SIGN_IN));
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(t, 4, 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(theme::TEXT), 0);
    // One rhythm for the whole screen: title to first button, and button to
    // button, are the same 28 px.
    lv_obj_set_style_pad_row(s_body, 28, 0);

    // Email first: it is the route most people already have, and Google is the
    // one that needs the QR dance.
    //
    // 32 px between them. Two 52 px targets on a 30 mm wide screen need real
    // separation, not a hairline: a thumb aimed at one otherwise lands on the
    // other, and this is the screen where the wrong route leads somewhere that
    // cannot serve you.
    const char* labels[2] = { i18n::T(S_WITH_EMAIL), i18n::T(S_WITH_GOOGLE) };
    for (int i = 0; i < 2; i++) {
        lv_obj_t* b = lv_btn_create(s_body);
        lv_obj_remove_style_all(b);
        lv_obj_add_style(b, theme::rowStyle(), 0);
        lv_obj_add_style(b, theme::rowPressedStyle(), LV_STATE_PRESSED);
        lv_obj_set_size(b, LV_PCT(100), theme::BUTTON_H);
        // No per-button spacing: the gap comes from the parent's row spacing,
        // set below. Padding here is INSIDE the button and pushed the label to
        // the top of a 52 px box instead of separating two boxes - which is
        // what the first screenshot of this screen showed. LVGL 8 has no
        // margin, so the container owns the rhythm.
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(b, 10, 0);
        lv_obj_add_event_cb(b, onChoice, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        if (i == 1) googleMark(b, 20);

        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, labels[i]);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    }
}

int takeSignInChoice() { int v = s_choice; s_choice = -1; return v; }

void showPortalReady(const char* url) {
    frame(nullptr);            // no header: the QR explains itself

    lv_obj_t* scan = lv_label_create(s_body);
    lv_label_set_text(scan, i18n::T(S_AP_JOIN));
    lv_label_set_long_mode(scan, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(scan, theme::SCREEN_W - 2 * theme::PAD - 6);
    lv_obj_set_style_text_align(scan, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(scan, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(scan, lv_color_hex(theme::TEXT), 0);
    lv_obj_set_style_pad_bottom(scan, 20, 0);

    qr(url, 132);

    lv_obj_t* orOpen = lv_label_create(s_body);
    lv_obj_set_style_pad_top(orOpen, 22, 0);
    lv_label_set_text(orOpen, i18n::T(S_OR_OPEN));
    lv_obj_set_style_text_font(orOpen, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(orOpen, lv_color_hex(theme::TEXT_DIM), 0);

    lv_obj_t* a = lv_label_create(s_body);
    lv_obj_set_style_pad_top(a, 4, 0);
    lv_label_set_text(a, url);
    lv_obj_set_style_text_font(a, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(a, lv_color_hex(theme::ACCENT), 0);
}

void showEmailPairing(const char* deviceUrl) {
    static String lastUrl;
    if (s_active && lastUrl == deviceUrl) return;
    lastUrl = deviceUrl;

    // Same shape as every other QR screen on this device.
    frame(nullptr);
    addBack();

    lv_obj_t* scan = lv_label_create(s_body);
    lv_label_set_text(scan, i18n::T(S_AP_JOIN));
    lv_obj_set_style_text_font(scan, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(scan, lv_color_hex(theme::TEXT), 0);
    lv_obj_set_style_pad_bottom(scan, 20, 0);

    qr(deviceUrl, 124);

    lv_obj_t* u = lv_label_create(s_body);
    lv_obj_set_style_pad_top(u, 22, 0);
    lv_label_set_text(u, i18n::T(S_OR_JOIN));
    lv_obj_set_style_text_font(u, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(u, lv_color_hex(theme::TEXT_DIM), 0);

    lv_obj_t* a = lv_label_create(s_body);
    lv_obj_set_style_pad_top(a, 4, 0);
    lv_label_set_text(a, deviceUrl);
    lv_obj_set_style_text_font(a, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(a, lv_color_hex(theme::ACCENT), 0);
}

void showAccountIntro() {
    // Built once. frame() tears down and rebuilds, and this is called from the
    // main loop - without the guard the button is destroyed under the finger
    // that is pressing it and the tap never lands.
    static bool built = false;
    if (built && s_active) return;
    built = true;

    frame(i18n::T(S_TT_ACCOUNT), false);
    lv_obj_t* icon = lv_label_create(s_body);
    lv_label_set_text(icon, LV_SYMBOL_DOWNLOAD);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(theme::ACCENT), 0);

    caption(i18n::T(S_ACCOUNT_WHY), theme::TEXT);

    lv_obj_t* b = lv_btn_create(s_body);
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, theme::rowStyle(), 0);
    lv_obj_add_style(b, theme::rowPressedStyle(), LV_STATE_PRESSED);
    lv_obj_set_size(b, LV_PCT(100), theme::BUTTON_H);
    lv_obj_set_style_bg_color(b, lv_color_hex(theme::GO_BG), 0);
    lv_obj_add_event_cb(b, onPair, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, i18n::T(S_LINK_ACCOUNT));
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
}

void showBusy(const char* text, bool withBack) {
    static String lastText;
    static bool lastBack = false;
    if (s_active && lastText == text && lastBack == withBack) return;
    lastText = text; lastBack = withBack;

    frame(nullptr);
    if (withBack) {
        addBack();
        lv_obj_set_style_pad_bottom(s_body, 56, 0);
    }

    lv_obj_t* sp = lv_spinner_create(s_body, 1300, 60);
    lv_obj_set_size(sp, 76, 76);
    lv_obj_set_style_arc_color(sp, lv_color_hex(0x1E2530), LV_PART_MAIN);
    lv_obj_set_style_arc_color(sp, lv_color_hex(theme::ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_pad_bottom(sp, 22, 0);

    lv_obj_t* t = lv_label_create(s_body);
    lv_label_set_text(t, text);
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(t, theme::SCREEN_W - 2 * theme::PAD - 6);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(theme::TEXT), 0);
}

void showPreparing() {
    // "Waiting", not "Importing printers": nothing is being imported here. The
    // device is asking the cloud for a pairing code.
    showBusy(i18n::T(S_WAITING), true);
}

void showPreparingOld() {
    static bool built = false;
    if (built && s_active) return;
    built = true;

    frame(nullptr);
    addBack();
    // addBack() reserves 40 px at the top for the chevron, which on a screen
    // whose whole content is a spinner and one word pushes everything visibly
    // off centre. Matching it at the bottom makes the free space symmetric
    // again, so the pair really does sit in the middle of the glass.
    lv_obj_set_style_pad_bottom(s_body, 56, 0);

    lv_obj_t* sp = lv_spinner_create(s_body, 1300, 60);
    lv_obj_set_size(sp, 76, 76);
    lv_obj_set_style_arc_color(sp, lv_color_hex(0x1E2530), LV_PART_MAIN);
    lv_obj_set_style_arc_color(sp, lv_color_hex(theme::ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_pad_bottom(sp, 22, 0);

    // "Waiting", not "Importing printers": nothing is being imported here. The
    // device is asking the cloud for a pairing code, and naming the wrong
    // operation is how a user learns not to trust what the screen says.
    lv_obj_t* t = lv_label_create(s_body);
    lv_label_set_text(t, i18n::T(S_WAITING));
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(theme::TEXT), 0);
}

void showPairing(const char* verifyUrl, const char* code, int secondsLeft) {
    // Only the countdown changes, and only once a second. Re-encoding the QR
    // sixty times a second would be the most expensive thing the device does,
    // for a picture that never changes.
    static int lastShown = -1;
    static String lastCode;
    if (s_active && secondsLeft == lastShown && lastCode == code) return;
    lastShown = secondsLeft; lastCode = code;

    // Same shape as the Wi-Fi screen: no header, the instruction above the
    // square, the fallback below it. Someone who has just scanned one QR to get
    // onto the network meets the same page twice, which is the point.
    frame(nullptr);
    addBack();

    lv_obj_t* scan = lv_label_create(s_body);
    lv_label_set_text(scan, i18n::T(S_AP_JOIN));
    lv_obj_set_style_text_font(scan, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(scan, lv_color_hex(theme::TEXT), 0);
    lv_obj_set_style_pad_bottom(scan, 20, 0);

    qr(verifyUrl, 124);

    // The fallback route: where to go, then what to type when you get there.
    lv_obj_t* url = lv_label_create(s_body);
    lv_obj_set_style_pad_top(url, 22, 0);
    lv_label_set_text(url, i18n::T(S_SCAN_TO_LINK));
    lv_label_set_long_mode(url, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(url, theme::SCREEN_W - 2 * theme::PAD - 6);
    lv_obj_set_style_text_align(url, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(url, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(url, lv_color_hex(theme::TEXT_DIM), 0);

    lv_obj_t* c = lv_label_create(s_body);
    lv_obj_set_style_pad_top(c, 4, 0);
    lv_label_set_text(c, code);
    lv_obj_set_style_text_font(c, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(c, lv_color_hex(theme::ACCENT), 0);
    lv_obj_set_style_text_letter_space(c, 2, 0);

    // The code expires. Small, at the bottom, because it only matters if you
    // have been standing there a while.
    char t[16];
    snprintf(t, sizeof(t), "%d:%02d", secondsLeft / 60, secondsLeft % 60);
    lv_obj_t* left = lv_label_create(s_body);
    lv_obj_set_style_pad_top(left, 10, 0);
    lv_label_set_text(left, t);
    lv_obj_set_style_text_font(left, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(left, lv_color_hex(theme::TEXT_DIM), 0);
}

void showPairFailed(const char* reason) {
    // Built once per failure. Without a way out this screen is a dead end, and
    // a dead end on the last step of setup means a factory reset.
    static bool built = false;
    static String lastReason;
    if (built && s_active && lastReason == reason) return;
    built = true; lastReason = reason;

    frame(i18n::T(S_TT_ACCOUNT), false);
    lv_obj_t* x = lv_label_create(s_body);
    lv_label_set_text(x, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(x, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(x, lv_color_hex(theme::DANGER), 0);
    caption(reason, theme::TEXT);

    lv_obj_t* b = lv_btn_create(s_body);
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, theme::rowStyle(), 0);
    lv_obj_add_style(b, theme::rowPressedStyle(), LV_STATE_PRESSED);
    lv_obj_set_size(b, LV_PCT(100), theme::BUTTON_H);
    lv_obj_add_event_cb(b, onPair, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, i18n::T(S_LINK_ACCOUNT));
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
}

bool takeStartPairing() { bool v = s_pair; s_pair = false; return v; }
bool takeBack()          { bool v = s_back; s_back = false; return v; }

void hide()   { s_active = false; }   // next show() rebuilds
bool active() { return s_active; }

bool takeRotate() { bool v = s_rotate; s_rotate = false; return v; }

}  // namespace screen_setup
