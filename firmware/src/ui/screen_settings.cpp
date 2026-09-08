#include "screen_settings.h"
#include "fonts.h"

// U+00B0. One of the three characters outside ASCII the font carries,
// spelled as bytes so the source itself stays ASCII.
#define LV_DEG "\xC2\xB0"
#include "frame.h"
#include "theme.h"
#include "i18n.h"
#include "version.h"
#include "../net/ota.h"
#include <lvgl.h>

namespace {
screen_settings::Entry s_entry = screen_settings::E_NONE;
bool s_back = false;
int  s_toggled = -1;
uint32_t s_viewSig = 0;

// WHICH screen the signature belongs to, and it is not a nicety.
//
// Every view in this file shared one `s_viewSig` and each folded a distinct
// constant into its own hash - 0xA0000000 for Wi-Fi, 0xB1000000 for the NFC
// tester, and so on. That is not a namespace: the constant is XORed with a
// hash of the content, so one screen's signature can land on another's. When
// it does, the second screen takes its "nothing changed" path and writes into
// the widget pointers the FIRST one cached - which LVGL destroyed when the
// screen was rebuilt.
//
// It crashed exactly there, and the assert named the real cause:
//   assert failed: heap_caps_free ... "free() target pointer is outside heap"
//   lv_label_set_text <- screen_settings::showWifi
// after reading a tag and walking back through Settings. The tester hashes the
// tag's product id, so which spool you scanned decided whether you got a
// collision - a crash that depended on the contents of a chip.
//
// The owner is the function's own address. Two different screens cannot share
// one, and adding a screen cannot forget to pick a unique constant.
const void* s_viewOwner = nullptr;

inline bool sameView(const void* owner, uint32_t sig) {
    return s_viewOwner == owner && s_viewSig == sig;
}
inline void claimView(const void* owner, uint32_t sig) {
    s_viewOwner = owner; s_viewSig = sig;
}
uint32_t s_menuSig = 0;
uint32_t s_pickSig = 0;

void onEntry(lv_event_t* e) {
    s_entry = (screen_settings::Entry)(intptr_t)lv_event_get_user_data(e);
}
void onBack()  { s_back = true; }
bool s_reload = false;
lv_obj_t* s_reloadIcon = nullptr;
lv_obj_t* s_reloadSpin = nullptr;
void onReload(lv_event_t*) { s_reload = true; }

// Which of the two is showing. Written into widgets that are already there:
// the header is never rebuilt to change this.
void setReloadBusy(bool busy) {
    if (!s_reloadIcon || !s_reloadSpin) return;
    if (busy) {
        lv_obj_add_flag(s_reloadIcon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_reloadSpin, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_reloadIcon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_reloadSpin, LV_OBJ_FLAG_HIDDEN);
    }
}
void onCheck()   { ota::checkAsync(); }
void onInstall() { ota::applyAsync(); }
// Flips the switch on the spot, then reports the tap.
//
// The screen used to be rebuilt to show the new state, and rebuilding a
// scrolled list throws away where it was scrolled to - press a toggle six
// printers down and the view jumped back to the top. The switch is the only
// thing on the row that changed, so it is the only thing that changes.
void onToggle(lv_event_t* e) {
    s_toggled = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t* row = lv_event_get_target(e);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(row); i++) {
        lv_obj_t* c = lv_obj_get_child(row, i);
        if (!lv_obj_check_type(c, &lv_switch_class)) continue;
        if (lv_obj_has_state(c, LV_STATE_CHECKED)) lv_obj_clear_state(c, LV_STATE_CHECKED);
        else                                       lv_obj_add_state(c, LV_STATE_CHECKED);
        break;
    }
}

uint32_t hashOf(const char* s, uint32_t h = 2166136261u) {
    for (; s && *s; s++) h = h * 16777619u ^ (uint8_t)*s;
    return h;
}
}  // namespace

namespace screen_settings {

void invalidate() {
    s_menuSig = 0; s_pickSig = 0;
    // And drop the ownership claim. Leaving a screen means its widgets are
    // about to be destroyed, so no later call has any business writing into
    // the pointers it cached - clearing the owner is what makes that true
    // rather than merely likely.
    s_viewOwner = nullptr; s_viewSig = 0;
}

// The four rows whose value and colour a background sync can change. The menu
// itself never changes: eight rows, same order, always. So it is built once and
// these are written into - a sync landing while somebody is scrolled down used
// to rebuild the list under them and send them back to the top.
lv_obj_t* s_mVal[4]  = { nullptr, nullptr, nullptr, nullptr };
lv_obj_t* s_mIcon[4] = { nullptr, nullptr, nullptr, nullptr };
// The screen those pointers belong to. Building any other screen frees them,
// and writing into freed LVGL objects is a crash rather than a glitch - so the
// update path proves it is still looking at its own screen before it writes.
lv_obj_t* s_menuScreen = nullptr;

void showMenu(const MenuState& st) {
    char printersVal[16];
    snprintf(printersVal, sizeof(printersVal), "%d/%d",
             st.visiblePrinters, st.totalPrinters);
    const char* upVal = (st.updateWaiting && st.latest && *st.latest)
                      ? st.latest : TIGERSPOOL_FW_VERSION;
    const char* vals[4] = { printersVal, st.network, st.account, upVal };
    const uint32_t tints[4] = {
        st.totalPrinters ? theme::TEXT : theme::DANGER,
        st.wifiUp        ? theme::OK   : theme::DANGER,
        st.signedIn      ? theme::OK   : theme::DANGER,
        st.updateWaiting ? theme::WARN : theme::TEXT,
    };

    if (s_menuSig == 0x4D454E55u && s_menuScreen == frame::screen()) {
        for (int i = 0; i < 4; i++) {
            if (s_mVal[i])  lv_label_set_text(s_mVal[i], vals[i] ? vals[i] : "");
            if (s_mIcon[i]) icons::tint(s_mIcon[i], tints[i]);
        }
        return;
    }
    s_menuSig = 0x4D454E55u;
    for (int i = 0; i < 4; i++) s_mVal[i] = s_mIcon[i] = nullptr;

    lv_obj_t* body = frame::build(i18n::T(S_SETTINGS), onBack);
    s_menuScreen = frame::screen();
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    theme::scrollbar(body);

    // The icon carries the state, the label stays white.
    //
    // An icon is plain white unless it holds something worth seeing without
    // reading the row: a binary state, something waiting, or a consequence.
    // Colour every row and no row stands out - which is the whole point. In
    // the healthy case exactly three are tinted, and each of the other five
    // says something real when it lights up.
    struct Row { Entry id; const char* label; const char* value;
                 icons::Id icon; uint32_t tint; };
    const Row rows[] = {
        // Red on an account with no printers in it: the single most common
        // thing wrong with a new device, and until now you had to open the row
        // to find out.
        { E_PRINTERS, i18n::T(S_PRINTER),    vals[0], icons::PRINTER, tints[0] },
        { E_WIFI,     "Wi-Fi",               vals[1], icons::WIFI,    tints[1] },
        { E_ACCOUNT,  i18n::T(S_TT_ACCOUNT), vals[2], icons::USER,    tints[2] },
        { E_SCREEN,   i18n::T(S_SCREEN),     "",
          icons::SCREEN,  0 },
        { E_LANGUAGE, i18n::T(S_LANGUAGE),   i18n::name(i18n::current()),
          icons::GLOBE,   0 },
        { E_READER,   i18n::T(S_READER),   "",      icons::SCREEN,  0 },
        { E_UPDATE,   i18n::T(S_UPDATE),    vals[3], icons::UPDATE,  tints[3] },
        { E_RESTART,  i18n::T(S_RESTART),    "",
          icons::RESTART, theme::WARN },
        { E_FACTORY,  i18n::T(S_FACTORY),    "",
          icons::ERASE,   theme::DANGER },
    };
    for (auto& r : rows) {
        lv_obj_t* row = frame::row(body, r.label, r.value, true, onEntry,
                                   (void*)(intptr_t)r.id, r.icon, r.tint);

        // Keep the four that a sync can change. Children of a row with an icon
        // and a value are: icon box, label, value, chevron.
        const int slot = (r.id == E_PRINTERS) ? 0 : (r.id == E_WIFI) ? 1
                       : (r.id == E_ACCOUNT)  ? 2 : (r.id == E_UPDATE) ? 3 : -1;
        if (slot >= 0) {
            s_mIcon[slot] = lv_obj_get_child(row, 0);
            s_mVal[slot]  = lv_obj_get_child(row, 2);
        }

        if (r.id == E_FACTORY) {
            // The single entry that cannot be undone is the one place the
            // label is tinted too. Its icon alone would put it on the same
            // footing as Restart, and the two are not the same kind of thing.
            lv_obj_t* label = lv_obj_get_child(row, 1);
            lv_obj_set_style_text_color(label, lv_color_hex(theme::DANGER), 0);
        }
    }
}

Entry takeEntry() { Entry v = s_entry; s_entry = E_NONE; return v; }
bool  takeBack()  { bool v = s_back; s_back = false; return v; }
bool  takeReload(){ bool v = s_reload; s_reload = false; return v; }

void showPrinters(const PrinterCfg* printers, int count, bool syncing) {
    // Deliberately NOT hashing `visible`. It changes on every toggle, and a
    // changed signature means a rebuilt screen, and a rebuilt list has lost
    // its scroll position - which is how pressing a switch sent the view back
    // to the top. The switch shows its own new state; see onToggle. What is in
    // the signature is what only a sync can change: which printers exist and
    // what they are called.
    uint32_t sig = 2166136261u;
    for (int i = 0; i < count; i++) {
        if (printers[i].type == PT_NONE) continue;
        sig = hashOf(printers[i].name.c_str(), sig);
    }
    if (sig == s_pickSig) {
        // The one thing that changes without a rebuild. The button is its own
        // progress indicator: a control that does something invisible for
        // fifteen seconds gets pressed again, and again.
        setReloadBusy(syncing);
        return;
    }
    s_pickSig = sig;
    s_reloadIcon = s_reloadSpin = nullptr;

    lv_obj_t* body = frame::build(i18n::T(S_PRINTER), onBack);

    // Ask the account again, now. This list refreshes itself every five
    // minutes, which suits a box on a shelf and is no use to somebody who has
    // just added a printer in Tiger Studio and is standing in front of the
    // device. This is also the screen where a missing printer is noticed, so
    // it is where the button belongs.
    lv_obj_t* reload = lv_btn_create(frame::header());
    lv_obj_remove_style_all(reload);
    lv_obj_set_size(reload, theme::ICON_HIT_W, theme::HEADER_H);
    lv_obj_align(reload, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(reload, onReload, LV_EVENT_CLICKED, nullptr);
    s_reloadIcon = lv_label_create(reload);
    lv_label_set_text(s_reloadIcon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(s_reloadIcon, &font_ui_20, 0);
    lv_obj_set_style_text_color(s_reloadIcon, lv_color_hex(theme::TEXT), 0);
    lv_obj_center(s_reloadIcon);

    // A turning ring for the waiting state, in the same button.
    //
    // The glyph itself cannot turn: LVGL rotates images, not labels, and a
    // refresh arrow drawn as text has no angle to set. So the two swap - the
    // arrow when there is nothing happening, an arc that actually moves while
    // the account is being read. A colour change alone is a still picture, and
    // a still picture is what makes someone press the button a second time.
    s_reloadSpin = lv_spinner_create(reload, 900, 60);
    lv_obj_set_size(s_reloadSpin, 22, 22);
    lv_obj_center(s_reloadSpin);
    lv_obj_set_style_arc_width(s_reloadSpin, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_reloadSpin, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_reloadSpin, lv_color_hex(theme::LINE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_reloadSpin, lv_color_hex(theme::WARN), LV_PART_INDICATOR);
    setReloadBusy(syncing);

    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    theme::scrollbar(body);

    int shown = 0;
    for (int i = 0; i < count; i++) {
        if (printers[i].type == PT_NONE) continue;
        shown++;

        lv_obj_t* row = lv_obj_create(body);
        lv_obj_remove_style_all(row);
        lv_obj_add_style(row, theme::rowStyle(), 0);
        lv_obj_set_size(row, LV_PCT(100), theme::ROW_H);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 8, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* name = lv_label_create(row);
        lv_label_set_text(name, printers[i].name.c_str());
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(name, 1);
        lv_obj_set_style_text_font(name, &font_ui_14, 0);

        // The switch is the control, and the whole row is its target: a 40 px
        // switch on a 240 px row is a small thing to aim at when the row it
        // sits in is already the obvious place to press.
        lv_obj_t* sw = lv_switch_create(row);
        lv_obj_set_size(sw, 44, 24);
        lv_obj_clear_flag(sw, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(sw, lv_color_hex(0x2A313B), LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, lv_color_hex(theme::ACCENT),
                                  LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (printers[i].visible) lv_obj_add_state(sw, LV_STATE_CHECKED);

        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, onToggle, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    if (!shown) frame::caption(i18n::T(S_NO_PRINTERS), theme::TEXT_DIM);
}

int takeToggled() { int v = s_toggled; s_toggled = -1; return v; }

}  // namespace screen_settings

// ===========================================================================
//  The remaining settings views.
// ===========================================================================
namespace {
screen_settings::Action s_action = screen_settings::A_NONE;
int  s_newBright = -1;
int  s_newSleep  = -1;
int  s_newRot    = screen_settings::ROT_NONE;


// Widgets kept from the last build, so a value that changes can be written
// into the screen instead of rebuilding it. A rebuild throws away the scroll
// position, the focus and any animation in flight; on a screen whose value
// changes many times a second it also throws away the whole screen many times
// a second. Valid only while s_viewSig still names the screen that made them.
lv_obj_t* s_ring      = nullptr;   // OTA progress ring
lv_obj_t* s_ringPct   = nullptr;
lv_obj_t* s_signal    = nullptr;   // Wi-Fi strength readout

void onAction(lv_event_t* e) {
    s_action = (screen_settings::Action)(intptr_t)lv_event_get_user_data(e);
}
void onBright(lv_event_t* e) { s_newBright = (int)(intptr_t)lv_event_get_user_data(e); }
void onSleep(lv_event_t* e)  { s_newSleep  = (int)(intptr_t)lv_event_get_user_data(e); }
void onRotate(lv_event_t* e) { s_newRot    = (int)(intptr_t)lv_event_get_user_data(e); }

// A row of exclusive choices. Each option is 44 px tall, which is the floor for
// something you tap without looking twice.
void segmented(lv_obj_t* parent, const char* const* labels, const int* values,
               int n, int current, lv_event_cb_t cb) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 44);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < n; i++) {
        lv_obj_t* b = lv_btn_create(row);
        lv_obj_remove_style_all(b);
        lv_obj_add_style(b, theme::rowStyle(), 0);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_height(b, 44);
        lv_obj_set_style_pad_all(b, 0, 0);
        bool on = values[i] == current;
        if (on) {
            lv_obj_set_style_bg_color(b, lv_color_hex(theme::ACCENT), 0);
        }
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void*)(intptr_t)values[i]);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, labels[i]);
        lv_obj_set_style_text_font(l, &font_ui_12, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(on ? 0x0B0D10 : theme::TEXT), 0);
        lv_obj_center(l);
    }
}

lv_obj_t* kv(lv_obj_t* parent, const char* k, const char* v, uint32_t colour) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* a = lv_label_create(row);
    lv_label_set_text(a, k);
    lv_obj_set_style_text_font(a, &font_ui_12, 0);
    lv_obj_set_style_text_color(a, lv_color_hex(theme::TEXT_DIM), 0);
    lv_obj_t* b = lv_label_create(row);
    lv_label_set_text(b, v);
    lv_label_set_long_mode(b, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(b, 132, 0);
    lv_obj_set_style_text_font(b, &font_ui_12, 0);
    lv_obj_set_style_text_color(b, lv_color_hex(colour), 0);
    return row;
}
}  // namespace

namespace screen_settings {

Action takeAction()   { Action v = s_action; s_action = A_NONE; return v; }
int  takeBrightness() { int v = s_newBright; s_newBright = -1; return v; }
int  takeSleep()      { int v = s_newSleep;  s_newSleep  = -1; return v; }
int  takeRotation()   { int v = s_newRot; s_newRot = ROT_NONE; return v; }

void showWifi(const char* ssid, const char* ip, const char* mac, bool connected,
              int rssi) {
    // The signal moves by a decibel or two every second. Hashed into the
    // signature it rebuilt this screen continuously; it is written into its
    // label instead.
    char sig_[16];
    snprintf(sig_, sizeof(sig_), "%d dBm", rssi);

    uint32_t sig = 0xA0000000u ^ hashOf(ssid) ^ hashOf(ip) ^ (uint32_t)connected;
    if (sameView((const void*)showWifi, sig)) {
        if (s_signal) lv_label_set_text(s_signal, connected ? sig_ : "-");
        return;
    }
    claimView((const void*)showWifi, sig);
    s_signal = nullptr;

    lv_obj_t* body = frame::build("Wi-Fi", onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* name = lv_label_create(body);
    lv_label_set_text(name, connected ? ssid : i18n::T(S_NO_NETWORK));
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, LV_PCT(100));
    lv_obj_set_style_text_font(name, &font_ui_20, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(connected ? theme::OK : theme::TEXT_DIM), 0);
    lv_obj_set_style_pad_bottom(name, 16, 0);

    // The address and the MAC, because a DHCP reservation needs the second one
    // and there is nowhere else on the device to read it. See docs/ONBOARDING.md.
    // The number behind the colour of the home screen's Wi-Fi glyph. Printed
    // because "the icon is orange" is not something anyone can act on, and
    // dBm is - it says move the box or move the router.
    // kv() hands back the row; the value is its second child.
    s_signal = lv_obj_get_child(
        kv(body, i18n::T(S_SIGNAL), connected ? sig_ : "-", theme::TEXT), 1);
    kv(body, "IP", ip, theme::TEXT);
    kv(body, "MAC", mac, theme::TEXT);

    lv_obj_t* spacer = lv_obj_create(body);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 18);

    frame::button(body, i18n::T(S_CHANGE_NETWORK), 0,
                  []() { s_action = A_CHANGE_WIFI; });
}

void showAccount(const char* email, int printers, bool linked) {
    uint32_t sig = hashOf(email) ^ ((uint32_t)printers << 8) ^ (uint32_t)linked;
    if (sameView((const void*)showAccount, sig)) return;
    claimView((const void*)showAccount, sig);

    lv_obj_t* body = frame::build(i18n::T(S_TT_ACCOUNT), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    if (!linked) {
        frame::caption(i18n::T(S_ADD_WEB), theme::TEXT_DIM);
        return;
    }

    lv_obj_t* e = lv_label_create(body);
    lv_label_set_text(e, email);
    lv_label_set_long_mode(e, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(e, theme::SCREEN_W - 2 * theme::PAD - 6);
    lv_obj_set_style_text_align(e, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(e, &font_ui_16, 0);
    lv_obj_set_style_pad_bottom(e, 18, 0);

    char n[32];
    snprintf(n, sizeof(n), "%d", printers);
    kv(body, i18n::T(S_PRINTER), n, theme::TEXT);

    lv_obj_t* spacer = lv_obj_create(body);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 22);

    // Signing out clears the session AND the imported printers: leaving them
    // behind would show a list belonging to an account nobody is logged into.
    frame::button(body, i18n::T(S_SIGN_OUT), 2, []() { s_action = A_SIGN_OUT; });
}

void showScreen(uint8_t brightness, int sleepSeconds, int rotation, bool autoRot) {
    uint32_t sig = 0xB0000000u ^ ((uint32_t)brightness << 16)
                 ^ (uint32_t)sleepSeconds ^ ((uint32_t)rotation << 12)
                 ^ (autoRot ? 0x00000800u : 0u);
    if (sameView((const void*)showScreen, sig)) return;
    claimView((const void*)showScreen, sig);

    lv_obj_t* body = frame::build(i18n::T(S_SCREEN), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    char b[8];
    snprintf(b, sizeof(b), "%u%%", brightness);
    kv(body, i18n::T(S_BRIGHTNESS), b, theme::TEXT);
    static const char* const bl[] = { "30", "60", "80", "100" };
    static const int bv[] = { 30, 60, 80, 100 };
    segmented(body, bl, bv, 4, brightness, onBright);

    lv_obj_t* spacer = lv_obj_create(body);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 16);

    char sl[16];
    if (sleepSeconds) snprintf(sl, sizeof(sl), "%d s", sleepSeconds);
    else              snprintf(sl, sizeof(sl), i18n::T(S_NEVER));
    kv(body, i18n::T(S_SLEEP_AFTER), sl, theme::TEXT);
    // Not static: a static array holding i18n::T() is filled once, with
    // whatever language was current the first time this screen was opened, and
    // then keeps it forever. The same word as the value above, too - the row
    // said "Never" while the chip under it said "Off", for one state.
    const char* const tl[] = { "30s", "1m", "5m", i18n::T(S_NEVER) };
    static const int tv[] = { 30, 60, 300, 0 };
    segmented(body, tl, tv, 4, sleepSeconds, onSleep);

    lv_obj_t* spacer2 = lv_obj_create(body);
    lv_obj_remove_style_all(spacer2);
    lv_obj_set_size(spacer2, 1, 16);

    // Which way up the panel is depends on how the board sits in its shell,
    // and both mountings are in use. The chips are the two angles rather than
    // words: "Normal" only means anything to someone who already knows which
    // way their own device is, and 180 turns it over whichever way that is.
    // Three positions rather than a switch beside a pair: Auto is not a
    // modifier on the choice, it IS one of the choices, and saying so in one
    // control removes the state where "auto" is on and an angle is also
    // selected and neither explains the other.
    kv(body, i18n::T(S_ORIENTATION),
       autoRot ? i18n::T(S_AUTO) : (rotation == 0 ? "0" LV_DEG : "180" LV_DEG),
       theme::TEXT);
    const char* const rl[] = { i18n::T(S_AUTO), "0" LV_DEG, "180" LV_DEG };
    static const int rv[] = { AUTO_ROT, 0, 2 };
    segmented(body, rl, rv, 3, autoRot ? AUTO_ROT : rotation, onRotate);

    // Nothing else to say. A settings screen that ends with an instruction is
    // a settings screen that did not explain itself above.
}

// The update view redraws as the state machine moves through checking,
// downloading and finishing, so its signature carries the state as well as the
// version. Everything else on this screen is static; this one is a progress
// report and has to be allowed to change.
// A glyph in a ring, in one colour. It is what the eye lands on first on this
// screen: the state is legible from arm's length before a word is read.
static void badge(lv_obj_t* parent, const char* glyph, uint32_t colour) {
    lv_obj_t* ring = lv_obj_create(parent);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 62, 62);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(colour), 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_opa(ring, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* g = lv_label_create(ring);
    lv_label_set_text(g, glyph);
    lv_obj_set_style_text_font(g, &font_ui_24, 0);
    lv_obj_set_style_text_color(g, lv_color_hex(colour), 0);
    lv_obj_center(g);

    lv_obj_t* gap = lv_obj_create(parent);
    lv_obj_remove_style_all(gap);
    lv_obj_set_size(gap, 1, 8);
}

void showUpdate(const char* version, const char* channel,
                int otaState, const char* latest, int percent) {
    // `percent` is written into the ring, never hashed into the signature.
    // A download reports a hundred times, and a screen rebuilt on each report
    // is an arc that restarts from nothing a hundred times instead of sweeping
    // once - which is the whole reason it is an arc.
    uint32_t sig = 0xC0000000u ^ hashOf(version) ^ hashOf(channel)
                 ^ ((uint32_t)otaState << 20) ^ hashOf(latest);
    if (sameView((const void*)showUpdate, sig)) {
        if (s_ring) {
            int v = (otaState == ota::DONE) ? 100 : percent;
            lv_arc_set_value(s_ring, v);
            if (s_ringPct) {
                char b[8]; snprintf(b, sizeof(b), "%d%%", v);
                lv_label_set_text(s_ringPct, b);
            }
        }
        return;
    }
    claimView((const void*)showUpdate, sig);
    s_ring = s_ringPct = nullptr;

    // While the image is being written there is nothing to go back to: the
    // download runs on its own task and leaving would hide it. So the whole
    // screen becomes the progress ring, without a header.
    const bool busy = (otaState == ota::DOWNLOADING || otaState == ota::DONE);

    lv_obj_t* body = busy ? frame::build(nullptr, nullptr)
                          : frame::build(i18n::T(S_UPDATE), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    if (busy) {
        // A ring rather than a bar: it is round, it is centred, and at this
        // size a bar reads as a sliver. Same shape as the scale's, in this
        // product's colours.
        lv_obj_t* ring = s_ring = lv_arc_create(body);
        lv_obj_set_size(ring, 152, 152);
        lv_arc_set_rotation(ring, 270);          // start at twelve o'clock
        lv_arc_set_bg_angles(ring, 0, 360);
        lv_arc_set_range(ring, 0, 100);
        lv_arc_set_value(ring, otaState == ota::DONE ? 100 : percent);
        // An arc is a control by default. This one reports, so the drag handle
        // goes and it stops taking touches away from what is underneath.
        lv_obj_remove_style(ring, nullptr, LV_PART_KNOB);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(ring, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_width(ring, 10, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(ring, lv_color_hex(theme::SURFACE), LV_PART_MAIN);
        lv_obj_set_style_arc_color(ring,
            lv_color_hex(otaState == ota::DONE ? theme::OK : theme::ACCENT),
            LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(ring, true, LV_PART_INDICATOR);

        // The number sits inside the ring, not under it: the eye is already
        // there, and the ring is empty in the middle by construction.
        lv_obj_t* pct = s_ringPct = lv_label_create(ring);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", otaState == ota::DONE ? 100 : percent);
        lv_label_set_text(pct, buf);
        lv_obj_set_style_text_font(pct, &font_ui_24, 0);
        lv_obj_set_style_text_color(pct, lv_color_hex(theme::TEXT), 0);
        lv_obj_center(pct);

        lv_obj_t* gap = lv_obj_create(body);
        lv_obj_remove_style_all(gap);
        lv_obj_set_size(gap, 1, 18);

        if (otaState == ota::DONE) {
            frame::caption(i18n::T(S_RESTARTING), theme::OK);
        } else {
            frame::caption(i18n::T(S_DOWNLOADING), theme::TEXT);
            // The one screen where this warning earns its place: pulling the
            // plug mid-write leaves a half-written slot and the device boots
            // the old one - recoverable, and it looks like a brick for a
            // minute.
            frame::caption(i18n::T(S_DONT_UNPLUG), theme::TEXT_DIM);
        }
        return;
    }

    // The version is a fact about the device, so it reads as one of its rows -
    // the same shape settings uses everywhere else - rather than as a headline.
    // The headline belongs to the answer the user came for: is it up to date.
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // "v1.5.0", not "1.5.0". The prefix is what the rest of the ecosystem
    // prints, on the scale's screen and on this repository's tags alike.
    char vbuf[24];
    snprintf(vbuf, sizeof(vbuf), "v%s", version);
    // Scrollable, because what goes below depends on the state AND on the
    // language: the available-update case carries a row, a badge, two captions
    // and a button, and in a language with longer words that is more than 276
    // pixels of body. It was cutting the Install button in half - the one
    // control the screen exists for.
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    theme::scrollbar(body);

    frame::row(body, i18n::T(S_INSTALLED), vbuf, false, nullptr, nullptr);

    lv_obj_t* spacer = lv_obj_create(body);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 12);

    switch (otaState) {
    case ota::CHECKING:
        frame::caption(i18n::T(S_CHECKING), theme::TEXT_DIM);
        break;

    case ota::UP_TO_DATE:
        badge(body, LV_SYMBOL_OK, theme::OK);
        frame::caption(i18n::T(S_UP_TO_DATE), theme::OK);
        break;

    case ota::AVAILABLE:
        badge(body, LV_SYMBOL_DOWNLOAD, theme::WARN);
        frame::caption(i18n::T(S_AVAILABLE), theme::TEXT_DIM);
        frame::bigLabel(latest, theme::WARN);
        frame::button(body, i18n::T(S_INSTALL), 1, onInstall);
        break;

    case ota::FAILED:
        badge(body, LV_SYMBOL_WARNING, theme::DANGER);
        frame::caption(ota::message(), theme::DANGER);
        frame::button(body, i18n::T(S_CHECK_UPDATE), 2, onCheck);
        break;

    default:
        frame::button(body, i18n::T(S_CHECK_UPDATE), 1, onCheck);
        break;
    }

}

void showUpdateNotice(const char* current, const char* latest) {
    uint32_t sig = 0xF0000000u ^ hashOf(current) ^ hashOf(latest);
    if (sameView((const void*)showUpdateNotice, sig)) return;
    claimView((const void*)showUpdateNotice, sig);

    // No back chevron: the two buttons are the whole answer, and one of them
    // is "later". A dismissal that has to be discovered is not a dismissal.
    lv_obj_t* body = frame::build(i18n::T(S_UPDATE), nullptr);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Badge, version, two buttons. Nothing else: this screen interrupts
    // somebody, so it earns its place by being answerable at a glance. What is
    // kept across an update belongs on the update page, where the question is
    // being considered rather than answered.
    badge(body, LV_SYMBOL_DOWNLOAD, theme::WARN);
    frame::caption(i18n::T(S_AVAILABLE), theme::TEXT_DIM);
    frame::bigLabel(latest, theme::WARN);

    lv_obj_t* gap = lv_obj_create(body);
    lv_obj_remove_style_all(gap);
    lv_obj_set_size(gap, 1, 16);

    frame::button(body, i18n::T(S_INSTALL), 1, []() { s_action = A_INSTALL_NOW; });
    frame::button(body, i18n::T(S_LATER),   0, []() { s_action = A_LATER; });
}

void showReader(bool ready, const char* err, const TagInfo* tag) {
    // The tag's own identity is the signature: a new spool rebuilds, the same
    // spool held there does not.
    uint32_t sig = 0xB1000000u ^ (uint32_t)ready
                 ^ (tag && tag->ok ? (tag->idProduct * 2654435761u) : 0u);
    if (sameView((const void*)showReader, sig)) return;
    claimView((const void*)showReader, sig);

    lv_obj_t* body = frame::build(i18n::T(S_READER), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);

    lv_obj_t* st = lv_label_create(body);
    lv_label_set_text(st, ready ? i18n::T(S_READER_OK) : i18n::T(S_READER_NONE));
    lv_obj_set_style_text_font(st, &font_ui_16, 0);
    lv_obj_set_style_text_color(st, lv_color_hex(ready ? theme::OK : theme::DANGER), 0);
    lv_obj_set_style_pad_bottom(st, 12, 0);

    if (!ready && err && *err) {
        frame::caption(err, theme::TEXT_DIM);
        return;
    }

    if (!tag || !tag->ok) {
        frame::caption(i18n::T(S_PRESENT_TAG), theme::TEXT_DIM);
        return;
    }

    // Every decoded field, labelled, in one column. This is a bench instrument,
    // not a spool card: the question it answers is "did each value come off the
    // chip correctly", and that needs the values themselves rather than a
    // headline. The colour disc stays, because a colour is the one field a
    // number cannot be checked against - you compare it with the spool.
    lv_obj_t* sw = lv_obj_create(body);
    lv_obj_remove_style_all(sw);
    lv_obj_set_size(sw, 44, 44);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(sw, lv_color_make(tag->r, tag->g, tag->b), 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_bottom(sw, 10, 0);

    char b[40];
    kv(body, "UID", tag->uid.length() ? tag->uid.c_str() : "-", theme::TEXT);
    snprintf(b, sizeof(b), "%lu", (unsigned long)tag->idProduct);
    kv(body, i18n::T(S_TAG_PRODUCT), b, theme::TEXT);
    kv(body, i18n::T(S_TAG_TYPE),  tag->material.c_str(), theme::TEXT);
    kv(body, i18n::T(S_TAG_BRAND), tag->brand.c_str(), theme::TEXT);
    snprintf(b, sizeof(b), "%u-%u\xC2\xB0""C", tag->nozMin, tag->nozMax);
    kv(body, i18n::T(S_NOZZLE), b, theme::TEXT);
    snprintf(b, sizeof(b), "%u / %u\xC2\xB0""C", tag->bedMin, tag->bedMax);
    kv(body, i18n::T(S_BED), b, theme::TEXT);

    snprintf(b, sizeof(b), "%s / %s",
             tag->aspect1Label.c_str(), tag->aspect2Label.c_str());
    kv(body, i18n::T(S_TAG_ASPECT), b, theme::TEXT);
    snprintf(b, sizeof(b), "%s  %s mm",
             tag->kindLabel.c_str(), tag->diameterLabel.c_str());
    kv(body, i18n::T(S_TAG_KIND), b, theme::TEXT);
    snprintf(b, sizeof(b), "%s", tag->protocolLabel.c_str());
    kv(body, i18n::T(S_TAG_PROTOCOL), b, theme::TEXT);
    // Seconds since 2000-01-01 GMT, shown as the date it means. The raw number
    // stays in the serial log: on the panel a date can be checked against when
    // a spool was made, and 836340782 cannot be checked against anything.
    // Page 0x0C also carries the twin tag id, so a value that lands outside a
    // plausible range is printed raw rather than dressed up as a date.
    {
        const time_t t = (time_t)tag->stamp + 946684800L;   // 2000-01-01 -> epoch
        struct tm g;
        if (tag->stamp && gmtime_r(&t, &g)) {
            snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d",
                     g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min);
        } else {
            snprintf(b, sizeof(b), "%lu", (unsigned long)tag->stamp);
        }
        kv(body, i18n::T(S_TAG_STAMP), b, theme::TEXT);
    }
    snprintf(b, sizeof(b), "%u\xC2\xB0""C / %uh", tag->dryTemp, tag->dryHours);
    kv(body, i18n::T(S_TAG_DRY), b, theme::TEXT);

    // Remaining first, quantity second. On a box that sits next to a printer
    // the useful number is how much is left - the factory figure is context
    // for it, which is why they share a line rather than compete for one.
    snprintf(b, sizeof(b), "%lu %s", (unsigned long)tag->available,
             tag->unitLabel.c_str());
    kv(body, i18n::T(S_TAG_LEFT), b, theme::TEXT);
    snprintf(b, sizeof(b), "%lu %s", (unsigned long)tag->measure,
             tag->unitLabel.c_str());
    kv(body, i18n::T(S_TAG_QTY), b, theme::TEXT_DIM);

    snprintf(b, sizeof(b), "%u.%u", tag->tdRaw / 10, tag->tdRaw % 10);
    kv(body, i18n::T(S_TAG_TD), b, theme::TEXT_DIM);

    // Two swatches on the value side, or a dash. A colour is the one field a
    // number cannot be checked against, so the second and third are shown the
    // same way the first is - as colour.
    {
        lv_obj_t* r2 = kv(body, i18n::T(S_TAG_COLOURS),
                          (tag->hasColor2 || tag->hasColor3) ? "" : "-",
                          theme::TEXT_DIM);
        if (tag->hasColor2 || tag->hasColor3) {
            lv_obj_t* box = lv_obj_create(r2);
            lv_obj_remove_style_all(box);
            lv_obj_set_size(box, 44, 16);
            lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(box, LV_FLEX_ALIGN_END,
                                  LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(box, 4, 0);
            lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
            const uint8_t rgb[2][3] = { { tag->c2r, tag->c2g, tag->c2b },
                                        { tag->c3r, tag->c3g, tag->c3b } };
            const bool has[2] = { tag->hasColor2, tag->hasColor3 };
            for (int i = 0; i < 2; i++) {
                if (!has[i]) continue;
                lv_obj_t* d = lv_obj_create(box);
                lv_obj_remove_style_all(d);
                lv_obj_set_size(d, 14, 14);
                lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
                lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
                lv_obj_set_style_bg_color(
                    d, lv_color_make(rgb[i][0], rgb[i][1], rgb[i][2]), 0);
                lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
            }
        }
    }

    kv(body, i18n::T(S_TAG_MESSAGE),
       tag->message.length() ? tag->message.c_str() : "-", theme::TEXT_DIM);

    // The one row on this screen that is a verdict rather than a value, so it
    // is the one row that gets a colour. Everything else is data.
    {
        StrId id = S_SIG_UNREAD;
        uint32_t col = theme::TEXT_DIM;
        switch (tag->signature) {
            case TagInfo::SIG_VALID:   id = S_SIG_VALID;   col = theme::OK;     break;
            case TagInfo::SIG_INVALID: id = S_SIG_INVALID; col = theme::DANGER; break;
            case TagInfo::SIG_NONE:    id = S_SIG_NONE;    break;
            case TagInfo::SIG_NO_KEY:  id = S_SIG_NOKEY;   break;
            default: break;
        }
        kv(body, i18n::T(S_TAG_SIG), i18n::T(id), col);
    }

    if (tag->pages.length()) {
        lv_obj_t* raw = lv_label_create(body);
        lv_label_set_text(raw, tag->pages.c_str());
        lv_label_set_long_mode(raw, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(raw, theme::SCREEN_W - 2 * theme::PAD - 6);
        lv_obj_set_style_text_font(raw, &font_ui_12, 0);
        lv_obj_set_style_text_color(raw, lv_color_hex(theme::TEXT_DIM), 0);
        lv_obj_set_style_pad_top(raw, 10, 0);
    }
}

// Two answers to one question, laid out so neither is pressed by accident.
//
// Not full width: a button that runs edge to edge on a 240 px panel reads as a
// bar rather than as a thing you press, and there is nothing to rest the eye
// against. Not touching each other either - "Restore" and "Cancel" a couple of
// pixels apart on a capacitive screen is a mis-tap waiting to happen, and one
// of the two is not undoable.
void confirmPair(lv_obj_t* body, const char* actionText, int tone,
                 frame::Callback onAction) {
    lv_obj_set_style_pad_row(body, 0, 0);
    lv_obj_t* a = frame::button(body, actionText, tone, onAction);
    lv_obj_set_width(a, LV_PCT(78));

    lv_obj_t* gap = lv_obj_create(body);
    lv_obj_remove_style_all(gap);
    lv_obj_set_size(gap, 1, 16);

    lv_obj_t* c = frame::button(body, i18n::T(S_CANCEL), 0, onBack);
    lv_obj_set_width(c, LV_PCT(78));
}

void showRestart() {
    if (sameView((const void*)showRestart, 0xD0000000u)) return;
    claimView((const void*)showRestart, 0xD0000000u);

    lv_obj_t* body = frame::build(i18n::T(S_RESTART), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // A question and two answers. Nothing else: what the box does while it
    // restarts, and how long it takes, are not decisions anyone makes here.
    //
    // 14 px, not 16. At 16 "Redemarrer la TigerSpool ?" is one character too
    // wide for 226 px and wraps with the question mark alone on the second
    // line - the same orphan the old wording produced at 20. A product name in
    // the sentence is not shortenable, so the type gives way instead.
    frame::caption(i18n::T(S_RESTART_Q), theme::TEXT, &font_ui_14);

    lv_obj_t* spacer = lv_obj_create(body);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 22);

    confirmPair(body, i18n::T(S_CONFIRM), 3, []() { s_action = A_RESTART; });
}

void showFactory() {
    if (sameView((const void*)showFactory, 0xE0000000u)) return;
    claimView((const void*)showFactory, 0xE0000000u);

    lv_obj_t* body = frame::build(i18n::T(S_FACTORY), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // A question and two answers, the destructive one in the destructive
    // colour. This was a two-second hold against a filling bar, which was
    // safer and much less obvious: the bar had to be learned, and a stray
    // press followed by a stray hold is not that much rarer than a stray
    // press. Two buttons say what they do without being taught.
    frame::caption(i18n::T(S_FACTORY_WARN), theme::TEXT, &font_ui_16);

    lv_obj_t* spacer = lv_obj_create(body);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 22);

    confirmPair(body, i18n::T(S_RESTORE), 2, []() { s_action = A_FACTORY; });
}

}  // namespace screen_settings
