#include "screen_home.h"
#include "fonts.h"
#include "icons.h"
#include "theme.h"
#include "../i18n.h"
#include "i18n.h"
#include <lvgl.h>
#include <Arduino.h>

namespace {

lv_obj_t* s_screen   = nullptr;
lv_obj_t* s_list     = nullptr;
lv_obj_t* s_account  = nullptr;
lv_obj_t* s_wifi     = nullptr;

// The level lives in icons::wifiLevelFromRssi now - the TigerScale's own
// arithmetic, so one network is described identically by both products. The
// portal's picker was moved onto it too; see `bars()` in net/portal_page.h.
bool      s_active   = false;
int       s_tapped   = -1;
bool      s_settings = false;

void onRow(lv_event_t* e)      { s_tapped   = (int)(intptr_t)lv_event_get_user_data(e); }
void onSettings(lv_event_t*)   { s_settings = true; }

// A status dot: 9 px, and colour is the only thing that changes. Green means
// the printer answered on its control port recently; grey means it did not.
lv_obj_t* makeDot(lv_obj_t* parent, uint32_t colour) {
    lv_obj_t* d = lv_obj_create(parent);
    lv_obj_remove_style_all(d);
    lv_obj_set_size(d, 9, 9);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(d, lv_color_hex(colour), 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    return d;
}

void buildScreen() {
    s_screen = lv_obj_create(nullptr);
    lv_obj_add_style(s_screen, theme::screenStyle(), 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ---- header: title, status dots, gear -----------------------------------
    lv_obj_t* header = lv_obj_create(s_screen);
    lv_obj_remove_style_all(header);
    lv_obj_add_style(header, theme::headerStyle(), 0);
    lv_obj_set_size(header, theme::SCREEN_W, theme::HEADER_H);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, i18n::T(S_PRINTER));
    lv_obj_set_style_text_font(title, &font_ui_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(theme::TEXT), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 9, 0);

    // Account, then Wi-Fi, then the gear. Two things this screen depends on
    // and cannot show you otherwise: the printers come from the account, and
    // they are reached over Wi-Fi. The same person glyph the Account row in
    // Settings uses, so the two are recognisably the same subject.
    s_account = icons::build(header, icons::USER, theme::OK);
    lv_obj_align(s_account, LV_ALIGN_RIGHT_MID, -theme::ICON_HIT_W - 32, 0);

    s_wifi = icons::wifiWave(header);
    lv_obj_align(s_wifi, LV_ALIGN_RIGHT_MID, -theme::ICON_HIT_W - 4, 0);

    // The gear's hit area is 52 x 44 (6.6 x 5.6 mm) even though the glyph is
    // small. Sizing a target to its icon is how a 2 mm button happens.
    lv_obj_t* gear = lv_btn_create(header);
    lv_obj_remove_style_all(gear);
    lv_obj_set_size(gear, theme::ICON_HIT_W, theme::HEADER_H);
    lv_obj_align(gear, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(gear, onSettings, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* gearIcon = lv_label_create(gear);
    lv_label_set_text(gearIcon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(gearIcon, &font_ui_20, 0);
    lv_obj_set_style_text_color(gearIcon, lv_color_hex(theme::TEXT), 0);
    lv_obj_center(gearIcon);

    // ---- the list ------------------------------------------------------------
    // A flex column inside a scrollable container: LVGL handles the drag, the
    // momentum and the "a tap that moved is not a tap" rule that had to be
    // written by hand against raw touch coordinates.
    s_list = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, theme::SCREEN_W, theme::SCREEN_H - theme::HEADER_H);
    lv_obj_align(s_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_list, theme::PAD, 0);
    lv_obj_set_style_pad_row(s_list, theme::GAP, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
}

}  // namespace

namespace screen_home {

// A cheap signature of everything on screen. show() is called from the main
// loop, and rebuilding the list on every one of those calls destroys each row
// under the finger that is pressing it - taps never land, the CPU does nothing
// else, and the screen looks frozen while the device is perfectly healthy.
static uint32_t signature(const PrinterCfg* printers, int count,
                          int selected, const bool* online, bool syncing,
                          int wifiRssi, int account) {
    uint32_t h = 2166136261u ^ (uint32_t)selected ^ ((uint32_t)syncing << 16)
               ^ ((uint32_t)icons::wifiLevelFromRssi(wifiRssi) << 24)
               ^ ((uint32_t)account << 12);
    for (int i = 0; i < count; i++) {
        h = h * 16777619u ^ (uint32_t)printers[i].type;
        h = h * 16777619u ^ (uint32_t)printers[i].visible;
        h = h * 16777619u ^ (uint32_t)(online && online[i]);
        for (const char* p = printers[i].name.c_str(); *p; p++)
            h = h * 16777619u ^ (uint8_t)*p;
    }
    return h;
}

void show(const PrinterCfg* printers, int count,
          int selected, const bool* online, bool syncing, int wifiRssi,
          int account) {
    if (!s_screen) buildScreen();

    static uint32_t lastSig = 0;
    static bool     everBuilt = false;
    uint32_t sig = signature(printers, count, selected, online, syncing, wifiRssi, account);
    if (everBuilt && s_active && sig == lastSig) return;
    lastSig = sig; everBuilt = true;

    lv_obj_clean(s_list);
    int shown = 0, configured = 0;
    for (int i = 0; i < count; i++) {
        if (printers[i].type == PT_NONE) continue;
        configured++;
        // Hidden in Settings -> Printers. This screen used to filter on type
        // alone and showed everything regardless, which made the picker look
        // like it did nothing.
        if (!printers[i].visible) continue;
        shown++;

        lv_obj_t* row = lv_btn_create(s_list);
        lv_obj_remove_style_all(row);
        lv_obj_add_style(row, theme::rowStyle(), 0);
        lv_obj_add_style(row, theme::rowPressedStyle(), LV_STATE_PRESSED);
        lv_obj_set_size(row, LV_PCT(100), theme::ROW_H);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_event_cb(row, onRow, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        if (i == selected)
            lv_obj_set_style_outline_width(row, 2, 0),
            lv_obj_set_style_outline_color(row, lv_color_hex(theme::ACCENT), 0);

        lv_obj_t* name = lv_label_create(row);
        lv_label_set_text(name, printers[i].name.c_str());
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(name, 1);
        lv_obj_set_style_text_font(name, &font_ui_14, 0);

        makeDot(row, (online && online[i]) ? theme::OK : theme::DANGER);
    }

    if (!shown) {
        // Two different situations that look identical on an empty list: the
        // account has no printers, or they are all hidden. Sending someone to
        // Tiger Studio when the answer is one tap away in Settings is the kind
        // of wrong advice that costs an evening.
        lv_obj_t* empty = lv_label_create(s_list);
        lv_label_set_text(empty, configured ? i18n::T(S_ALL_HIDDEN)
                                            : i18n::T(S_NO_PRINTERS));
        lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(empty, theme::SCREEN_W - 2 * theme::PAD - 6);
        lv_obj_set_style_text_color(empty, lv_color_hex(theme::TEXT_DIM), 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
    }

    // Green reachable, orange signed in but unreachable - which on this device
    // means the internet is down, and is the case worth seeing - red not
    // signed in at all.
    // Red no account to be connected to, blue linked and working on it, orange
    // linked but the last exchange failed - the one a user can act on - green
    // the last exchange succeeded. Green is a claim about the exchange, not
    // about holding a token: a device whose network died stops claiming to be
    // fine instead of waiting half an hour for its token to expire.
    static const uint32_t ACCT_COLOUR[4] = {
        theme::DANGER, theme::BUSY, theme::WARN, theme::OK };
    icons::tint(s_account, ACCT_COLOUR[account < 0 ? 0 : (account > 3 ? 3 : account)]);

    // Length, not colour. The glyph used to go red, orange, then green as the
    // signal improved, which made a perfectly usable -70 dBm look like a fault
    // - orange means "something needs your attention" everywhere else on this
    // device, and a slightly weaker signal does not. The rule this settles, and
    // the TigerScale has been following it all along: colour carries a STATE
    // (green connected, red no network), length carries a QUANTITY. Nobody has
    // to wonder whether a yellow means "middling" or "look out".
    icons::setSignal(s_wifi, icons::wifiLevelFromRssi(wifiRssi), wifiRssi != 0);


    if (!s_active) {
        lv_scr_load(s_screen);
        lv_obj_invalidate(s_screen);   // full repaint: a legacy screen drew last
        s_active = true;
    }
}

bool active() { return s_active; }
void leave()  { s_active = false; }

int  takeTappedPrinter() { int v = s_tapped; s_tapped = -1; return v; }
bool takeSettingsTap()   { bool v = s_settings; s_settings = false; return v; }

}  // namespace screen_home
