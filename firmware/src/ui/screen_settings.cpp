#include "screen_settings.h"

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
uint32_t s_menuSig = 0;
uint32_t s_pickSig = 0;

void onEntry(lv_event_t* e) {
    s_entry = (screen_settings::Entry)(intptr_t)lv_event_get_user_data(e);
}
void onBack()  { s_back = true; }
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

void invalidate() { s_menuSig = 0; s_pickSig = 0; }

void showMenu(const MenuState& st) {
    uint32_t sig = hashOf(st.network) ^ hashOf(st.account)
                 ^ ((uint32_t)st.visiblePrinters << 8) ^ (uint32_t)st.totalPrinters
                 ^ (st.wifiUp   ? 0x00010000u : 0u)
                 ^ (st.signedIn ? 0x00020000u : 0u)
                 ^ (st.updateWaiting ? 0x5A5A5A5Au : 0u) ^ hashOf(st.latest);
    if (sig == s_menuSig) return;
    s_menuSig = sig;

    lv_obj_t* body = frame::build(i18n::T(S_SETTINGS), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

    char printersVal[16];
    snprintf(printersVal, sizeof(printersVal), "%d/%d",
             st.visiblePrinters, st.totalPrinters);

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
        { E_PRINTERS, i18n::T(S_PRINTER),    printersVal,
          icons::PRINTER, st.totalPrinters ? 0u : theme::DANGER },
        { E_WIFI,     "Wi-Fi",               st.network,
          icons::WIFI,    st.wifiUp   ? theme::OK : theme::DANGER },
        { E_ACCOUNT,  i18n::T(S_TT_ACCOUNT), st.account,
          icons::USER,    st.signedIn ? theme::OK : theme::DANGER },
        { E_SCREEN,   i18n::T(S_SCREEN),     "",
          icons::SCREEN,  0 },
        { E_LANGUAGE, i18n::T(S_LANGUAGE),   i18n::name(i18n::current()),
          icons::GLOBE,   0 },
        { E_UPDATE,   i18n::T(S_UPDATE),
          st.updateWaiting && st.latest && *st.latest
              ? st.latest : TIGERSPOOL_FW_VERSION,
          icons::UPDATE,  st.updateWaiting ? theme::WARN : 0 },
        { E_RESTART,  i18n::T(S_RESTART),    "",
          icons::RESTART, theme::WARN },
        { E_FACTORY,  i18n::T(S_FACTORY),    "",
          icons::ERASE,   theme::DANGER },
    };
    for (auto& r : rows) {
        lv_obj_t* row = frame::row(body, r.label, r.value, true, onEntry,
                                   (void*)(intptr_t)r.id, r.icon, r.tint);
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

void showPrinters(const PrinterCfg* printers, int count) {
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
    if (sig == s_pickSig) return;
    s_pickSig = sig;

    lv_obj_t* body = frame::build(i18n::T(S_PRINTER), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

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
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);

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
int  s_newRot    = -1;
bool s_holding   = false;
uint32_t s_viewSig = 0;

// Widgets kept from the last build, so a value that changes can be written
// into the screen instead of rebuilding it. A rebuild throws away the scroll
// position, the focus and any animation in flight; on a screen whose value
// changes many times a second it also throws away the whole screen many times
// a second. Valid only while s_viewSig still names the screen that made them.
lv_obj_t* s_holdFill  = nullptr;   // factory reset progress
lv_obj_t* s_holdLabel = nullptr;
lv_obj_t* s_ring      = nullptr;   // OTA progress ring
lv_obj_t* s_ringPct   = nullptr;
lv_obj_t* s_signal    = nullptr;   // Wi-Fi strength readout

void onAction(lv_event_t* e) {
    s_action = (screen_settings::Action)(intptr_t)lv_event_get_user_data(e);
}
void onBright(lv_event_t* e) { s_newBright = (int)(intptr_t)lv_event_get_user_data(e); }
void onSleep(lv_event_t* e)  { s_newSleep  = (int)(intptr_t)lv_event_get_user_data(e); }
void onRotate(lv_event_t* e) { s_newRot    = (int)(intptr_t)lv_event_get_user_data(e); }
void onHoldDown(lv_event_t*) { s_holding = true; }
void onHoldUp(lv_event_t*)   { s_holding = false; }

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
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
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
    lv_obj_set_style_text_font(a, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(a, lv_color_hex(theme::TEXT_DIM), 0);
    lv_obj_t* b = lv_label_create(row);
    lv_label_set_text(b, v);
    lv_label_set_long_mode(b, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(b, 150, 0);
    lv_obj_set_style_text_font(b, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(b, lv_color_hex(colour), 0);
    return row;
}
}  // namespace

namespace screen_settings {

Action takeAction()   { Action v = s_action; s_action = A_NONE; return v; }
int  takeBrightness() { int v = s_newBright; s_newBright = -1; return v; }
int  takeSleep()      { int v = s_newSleep;  s_newSleep  = -1; return v; }
int  takeRotation()   { int v = s_newRot;    s_newRot    = -1; return v; }
bool factoryHolding() { return s_holding; }

void showWifi(const char* ssid, const char* ip, const char* mac, bool connected,
              int rssi) {
    // The signal moves by a decibel or two every second. Hashed into the
    // signature it rebuilt this screen continuously; it is written into its
    // label instead.
    char sig_[16];
    snprintf(sig_, sizeof(sig_), "%d dBm", rssi);

    uint32_t sig = 0xA0000000u ^ hashOf(ssid) ^ hashOf(ip) ^ (uint32_t)connected;
    if (sig == s_viewSig) {
        if (s_signal) lv_label_set_text(s_signal, connected ? sig_ : "-");
        return;
    }
    s_viewSig = sig;
    s_signal = nullptr;

    lv_obj_t* body = frame::build("Wi-Fi", onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* name = lv_label_create(body);
    lv_label_set_text(name, connected ? ssid : i18n::T(S_NO_NETWORK));
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, LV_PCT(100));
    lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
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
    if (sig == s_viewSig) return;
    s_viewSig = sig;

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
    lv_obj_set_style_text_font(e, &lv_font_montserrat_16, 0);
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

void showScreen(uint8_t brightness, int sleepSeconds, int rotation) {
    uint32_t sig = 0xB0000000u ^ ((uint32_t)brightness << 16)
                 ^ (uint32_t)sleepSeconds ^ ((uint32_t)rotation << 12);
    if (sig == s_viewSig) return;
    s_viewSig = sig;

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
    kv(body, i18n::T(S_ORIENTATION), rotation == 0 ? "0" LV_DEG : "180" LV_DEG,
       theme::TEXT);
    static const char* const rl[] = { "0" LV_DEG, "180" LV_DEG };
    static const int rv[] = { 0, 2 };
    segmented(body, rl, rv, 2, rotation, onRotate);

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
    lv_obj_set_style_text_font(g, &lv_font_montserrat_24, 0);
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
    if (sig == s_viewSig) {
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
    s_viewSig = sig;
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
        lv_obj_set_style_text_font(pct, &lv_font_montserrat_24, 0);
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
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

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
        badge(body, LV_SYMBOL_DOWNLOAD, theme::ACCENT);
        frame::caption(i18n::T(S_AVAILABLE), theme::TEXT_DIM);
        frame::bigLabel(latest, theme::ACCENT);
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
    if (sig == s_viewSig) return;
    s_viewSig = sig;

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

void showRestart() {
    if (s_viewSig == 0xD0000000u) return;
    s_viewSig = 0xD0000000u;

    lv_obj_t* body = frame::build(i18n::T(S_RESTART), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    frame::bigLabel(i18n::T(S_RESTART_Q), theme::TEXT);
    frame::caption(i18n::T(S_RESTART_NOTE), theme::TEXT_DIM);

    lv_obj_t* spacer = lv_obj_create(body);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 22);

    frame::button(body, i18n::T(S_RESTART), 1, []() { s_action = A_RESTART; });
}

void showFactory(int holdPercent) {
    // The hold progress is written into the bar, never rebuilt into it: this
    // value changes on every frame someone keeps their finger down, and a
    // screen rebuilt on every frame is a screen that cannot animate at all.
    const int pct = holdPercent < 0 ? 0 : (holdPercent > 100 ? 100 : holdPercent);
    if (s_viewSig == 0xE0000000u) {
        if (s_holdFill)  lv_obj_set_width(s_holdFill, LV_PCT(pct));
        if (s_holdLabel) lv_label_set_text(s_holdLabel,
                             pct > 0 ? i18n::T(S_KEEP_HOLDING) : i18n::T(S_HOLD_ERASE));
        return;
    }
    s_viewSig = 0xE0000000u;
    s_holdFill = s_holdLabel = nullptr;

    lv_obj_t* body = frame::build(i18n::T(S_FACTORY), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    frame::caption(i18n::T(S_FACTORY_WARN),
                   theme::TEXT);

    lv_obj_t* spacer = lv_obj_create(body);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 10);

    // What comes back matters as much as what goes: the printers live in the
    // account, so linking it again restores them. That line is the difference
    // between a frightening button and a usable one.
    frame::caption(i18n::T(S_FACTORY_NOTE), theme::TEXT_DIM);

    lv_obj_t* spacer2 = lv_obj_create(body);
    lv_obj_remove_style_all(spacer2);
    lv_obj_set_size(spacer2, 1, 20);

    // Hold, not tap. A destructive action on a touchscreen has to cost more
    // than a stray finger, and the bar is the only feedback that says so.
    lv_obj_t* hold = lv_btn_create(body);
    lv_obj_remove_style_all(hold);
    lv_obj_set_size(hold, LV_PCT(100), theme::BUTTON_H);
    lv_obj_set_style_radius(hold, theme::RADIUS, 0);
    lv_obj_set_style_bg_color(hold, lv_color_hex(0x4A1D1A), 0);
    lv_obj_set_style_bg_opa(hold, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(hold, onHoldDown, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(hold, onHoldUp, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(hold, onHoldUp, LV_EVENT_PRESS_LOST, nullptr);

    // Built at zero width and kept: it exists so it can be widened.
    s_holdFill = lv_obj_create(hold);
    lv_obj_remove_style_all(s_holdFill);
    lv_obj_set_size(s_holdFill, LV_PCT(pct), LV_PCT(100));
    lv_obj_align(s_holdFill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(s_holdFill, lv_color_hex(theme::DANGER), 0);
    lv_obj_set_style_bg_opa(s_holdFill, LV_OPA_80, 0);

    s_holdLabel = lv_label_create(hold);
    lv_label_set_text(s_holdLabel, pct > 0 ? i18n::T(S_KEEP_HOLDING) : i18n::T(S_HOLD_ERASE));
    lv_obj_set_style_text_font(s_holdLabel, &lv_font_montserrat_14, 0);
    lv_obj_center(s_holdLabel);
}

}  // namespace screen_settings
