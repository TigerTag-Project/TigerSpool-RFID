#include "screen_settings.h"
#include "fonts.h"

// U+00B0. One of the three characters outside ASCII the font carries,
// spelled as bytes so the source itself stays ASCII.
#define LV_DEG "\xC2\xB0"
#include "frame.h"
#include "icons.h"
#include "theme.h"
#include "../printer_budget.h"
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
uint32_t s_chooseSig = 0;
bool s_hex = false;
bool s_chosen = false;

void onEntry(lv_event_t* e) {
    s_entry = (screen_settings::Entry)(intptr_t)lv_event_get_user_data(e);
}
// Vertical space between two things, as an object rather than as padding.
//
// Padding on a BUTTON is not space above it: a button's label is centred in its
// content area, and padding shrinks that area from the top - so the word slides
// down and sits below the middle of the control. It is visible at a glance once
// you know, and invisible until then. An empty object of the right height puts
// the space where it was meant to go and leaves the button alone.
void gap(lv_obj_t* parent, lv_coord_t h) {
    lv_obj_t* g = lv_obj_create(parent);
    lv_obj_remove_style_all(g);
    lv_obj_set_size(g, 1, h);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_CLICKABLE);
}

void onBack()  { s_back = true; }
void onHex()   { s_hex  = true; }
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
// The tap only ASKS. It used to flip the switch on the spot, which was fine
// while every request was granted - and wrong the moment one can be refused:
// a printer that would overflow the load budget stayed off while its switch
// showed on. The switch now follows the printer's real state, synced on every
// call to the screen (see syncSwitches), so a refusal is a switch that does
// not move.
void onToggle(lv_event_t* e) {
    s_toggled = (int)(intptr_t)lv_event_get_user_data(e);
}

// One switch per printer index, filled when the rows are built.
lv_obj_t* s_sw[MAX_PRINTERS] = { nullptr };

// The load gauge, kept so it can be updated without rebuilding the list - a
// rebuild throws away the scroll position, which is how toggling a printer
// used to send the view back to the top.
lv_obj_t* s_gaugeBar = nullptr;
lv_obj_t* s_gaugeVal = nullptr;
lv_obj_t* s_gaugeKey = nullptr;

uint32_t hashOf(const char* s, uint32_t h = 2166136261u) {
    for (; s && *s; s++) h = h * 16777619u ^ (uint8_t)*s;
    return h;
}
}  // namespace

namespace screen_settings {

void invalidate() {
    s_menuSig = 0; s_pickSig = 0; s_chooseSig = 0;
    s_gaugeBar = s_gaugeVal = s_gaugeKey = nullptr;
    for (int i = 0; i < MAX_PRINTERS; i++) s_sw[i] = nullptr;
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
        { E_READER,   i18n::T(S_READER),   "",      icons::NFC,     0 },
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
bool  takeHex()   { bool v = s_hex;    s_hex = false;    return v; }

// Height of the load gauge block: a label line and the bar under it.
static const lv_coord_t GAUGE_H = 30;

// The load gauge: how much of the connection budget the switched-on printers
// take, as a bar that fills. See printer_budget.h for what a load slot is.
static void gauge(lv_obj_t* parent) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, LV_PCT(100), GAUGE_H);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 4, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* line = lv_obj_create(box);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(line, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    s_gaugeKey = lv_label_create(line);
    lv_obj_set_style_text_font(s_gaugeKey, &font_ui_12, 0);
    s_gaugeVal = lv_label_create(line);
    lv_obj_set_style_text_font(s_gaugeVal, &font_ui_12, 0);

    s_gaugeBar = lv_bar_create(box);
    lv_obj_set_size(s_gaugeBar, LV_PCT(100), 8);
    lv_bar_set_range(s_gaugeBar, 0, budget::LOAD_SLOTS);
    lv_obj_set_style_radius(s_gaugeBar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(s_gaugeBar, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_gaugeBar, lv_color_hex(theme::LINE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_gaugeBar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_gaugeBar, LV_OPA_COVER, LV_PART_INDICATOR);
}

// Written in place on every call. The colour says how close it is: the accent
// while there is room, orange from 85%, red for the moment after a switch was
// refused - with the reason in words, because a bar turning red on its own
// explains nothing to somebody who only pressed a switch.
static void updateGauge(uint16_t used, bool refused) {
    if (!s_gaugeBar) return;
    const uint16_t shown = used > budget::LOAD_SLOTS ? budget::LOAD_SLOTS : used;
    lv_bar_set_value(s_gaugeBar, shown, LV_ANIM_OFF);
    uint32_t col = theme::ACCENT;
    if (used * 100 >= budget::LOAD_SLOTS * 85) col = theme::WARN;
    if (refused) col = theme::DANGER;
    lv_obj_set_style_bg_color(s_gaugeBar, lv_color_hex(col), LV_PART_INDICATOR);

    lv_label_set_text(s_gaugeKey, i18n::T(refused ? S_NO_ROOM : S_LOAD));
    lv_obj_set_style_text_color(s_gaugeKey, lv_color_hex(refused ? theme::DANGER : theme::TEXT_DIM), 0);
    // A percentage to the user, not "109 / 150". Load slots are how the
    // device counts; what a person needs is how full it is, and a percentage
    // says that without asking them to know what the 150 is. The slot counts
    // stay in the log and in docs/CONNECTION-BUDGET.md.
    char b[16];
    snprintf(b, sizeof(b), "%u%%",
             (unsigned)((used * 100 + budget::LOAD_SLOTS / 2) / budget::LOAD_SLOTS));
    lv_label_set_text(s_gaugeVal, b);
    lv_obj_set_style_text_color(s_gaugeVal, lv_color_hex(refused ? theme::DANGER : theme::TEXT), 0);
}

// The switches follow the printers, every call. Cheap - a dozen state checks -
// and it is what makes a refused toggle a switch that simply does not move.
static void syncSwitches(const PrinterCfg* printers, int count) {
    for (int i = 0; i < count && i < MAX_PRINTERS; i++) {
        lv_obj_t* sw = s_sw[i];
        if (!sw) continue;
        const bool on = printers[i].visible;
        if (on != lv_obj_has_state(sw, LV_STATE_CHECKED)) {
            if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
            else    lv_obj_clear_state(sw, LV_STATE_CHECKED);
        }
    }
}

// The list itself, shared by the settings view and the setup chooser: same
// rows, same switches, same target area. Only the frame around it differs.
static void printerRows(lv_obj_t* body, const PrinterCfg* printers, int count) {
    for (int i = 0; i < MAX_PRINTERS; i++) s_sw[i] = nullptr;
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
        s_sw[i] = sw;

        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, onToggle, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    if (!shown) {
        lv_obj_t* none = lv_label_create(body);
        lv_label_set_text(none, i18n::T(S_NO_PRINTERS));
        lv_obj_set_style_text_font(none, &font_ui_14, 0);
        lv_obj_set_style_text_color(none, lv_color_hex(theme::TEXT_DIM), 0);
        lv_obj_set_style_pad_top(none, 12, 0);
    }
}

void showPrinters(const PrinterCfg* printers, int count, bool syncing,
                  uint16_t used, bool refused) {
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
        // The things that change without a rebuild. The button is its own
        // progress indicator: a control that does something invisible for
        // fifteen seconds gets pressed again, and again.
        setReloadBusy(syncing);
        syncSwitches(printers, count);
        updateGauge(used, refused);
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
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    // The gauge stays put and the list scrolls under it: a budget you can only
    // see at the top of a long list is one you cannot see at the moment you
    // flip the switch at the bottom of it.
    gauge(body);
    gap(body, 6);

    lv_obj_t* list = lv_obj_create(body);
    lv_obj_remove_style_all(list);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_height(list, theme::SCREEN_H - theme::HEADER_H - 2 * theme::PAD
                            - GAUGE_H - 6);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, theme::GAP, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    theme::scrollbar(list);

    printerRows(list, printers, count);
    syncSwitches(printers, count);
    updateGauge(used, refused);
}

// The setup step: which printers does this box talk to?
//
// It exists because the alternative is a first boot that silently opens a
// connection to every printer on the account - which is not what someone with
// thirteen of them wants, and is more than the device can hold anyway. Nothing
// is selected when this screen appears; the user turns on the ones they want.
//
// The list scrolls and the button does not. A "confirm" that has to be scrolled
// to is one a person does not know is there, and this screen is the only thing
// between them and a device that works.
void showChoosePrinters(const PrinterCfg* printers, int count, bool syncing,
                        uint16_t used, bool refused) {
    uint32_t sig = 2166136261u ^ (syncing ? 0x5AA5u : 0u);
    for (int i = 0; i < count; i++) {
        if (printers[i].type == PT_NONE) continue;
        sig = hashOf(printers[i].name.c_str(), sig);
    }
    if (sig == s_chooseSig) {
        syncSwitches(printers, count);
        updateGauge(used, refused);
        return;
    }
    s_chooseSig = sig;

    // No back chevron: this is a step, not a place, and the button below is
    // the way out of it.
    lv_obj_t* body = frame::build(i18n::T(S_CHOOSE_PRINTERS), nullptr);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    // The budget is part of the choice being made here, so it sits above it.
    gauge(body);
    gap(body, 6);

    lv_obj_t* list = lv_obj_create(body);
    lv_obj_remove_style_all(list);
    lv_obj_set_width(list, LV_PCT(100));
    // An explicit height, NOT flex-grow.
    //
    // Grow only hands out space that is left over, and thirteen rows leave
    // none: the list kept its content height, overflowed a body that clips,
    // and so had nothing to scroll - no scrollbar, and no scrolling either.
    // The height the list may have is what the screen has minus the header,
    // the button, and the padding around them; that is a number, so it is
    // written as one.
    lv_obj_set_height(list, theme::SCREEN_H - theme::HEADER_H
                            - 2 * theme::PAD - theme::BUTTON_H - theme::GAP - 8
                            - GAUGE_H - 6);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, theme::GAP, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    theme::scrollbar(list);

    if (syncing) {
        lv_obj_t* sp = lv_spinner_create(list, 900, 60);
        lv_obj_set_size(sp, 40, 40);
        lv_obj_set_style_arc_width(sp, 4, LV_PART_MAIN);
        lv_obj_set_style_arc_width(sp, 4, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(sp, lv_color_hex(theme::LINE), LV_PART_MAIN);
        lv_obj_set_style_arc_color(sp, lv_color_hex(theme::ACCENT), LV_PART_INDICATOR);
        lv_obj_set_style_pad_top(sp, 24, 0);
    } else {
        printerRows(list, printers, count);
    }

    gap(body, 8);
    frame::button(body, i18n::T(S_CONFIRM), 1, []() { s_chosen = true; });
    syncSwitches(printers, count);
    updateGauge(used, refused);
    // The button is the last thing on the screen; without this it sits on the
    // bezel.
    gap(body, 6);
}

int takeToggled() { int v = s_toggled; s_toggled = -1; return v; }
bool takeChosen()  { bool v = s_chosen; s_chosen = false; return v; }

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

// A label and its value side by side, reading left to right, for the block
// beside the colour swatch.
//
// kv() cannot serve here: it spreads the pair to the two ends of a full-width
// row, which is right for a list and wrong in a 150 px column - the label and
// the value ended up touching, "TypePLA High Speed", with no space in between
// to say which was which.
lv_obj_t* pair(lv_obj_t* parent, const char* k, const char* v) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 5, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* a = lv_label_create(row);
    lv_label_set_text(a, k);
    lv_obj_set_style_text_font(a, &font_ui_12, 0);
    lv_obj_set_style_text_color(a, lv_color_hex(theme::TEXT_DIM), 0);

    lv_obj_t* b = lv_label_create(row);
    lv_label_set_text(b, v);
    lv_label_set_long_mode(b, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_style_text_font(b, &font_ui_12, 0);
    lv_obj_set_style_text_color(b, lv_color_hex(theme::TEXT), 0);
    return row;
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
// "1.41.0 > 1.42.0" - where the device is, and where it would be.
//
// A version on its own answers the wrong question. Someone reading an update
// prompt already knows the number under it is a version, so the caption saying
// so ("Version available") spent a line to repeat the obvious - while the one
// thing they actually needed, what they are moving FROM, was not on the screen
// at all. Both versions on one line, and the caption is not missed.
//
// One label rather than three, using LVGL's recolour markup, so the two
// versions cannot drift apart vertically or wrap independently: the version
// being left behind is dim, the one being offered is the accent, and the
// chevron between them belongs to the first.
static void versionStep(lv_obj_t* parent, const char* current, const char* latest) {
    char buf[96];
    // The installed version in WHITE, not in the dim grey secondary text uses.
    // It is not an aside: half of what this line says is where the device is
    // now, and a reader compares the two numbers. Dimming one of a pair being
    // compared makes it look like a caption for the other.
    snprintf(buf, sizeof(buf), "#%06X %s " LV_SYMBOL_RIGHT " ##%06X %s#",
             (unsigned)theme::TEXT, current, (unsigned)theme::WARN, latest);

    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_recolor(l, true);
    lv_label_set_text(l, buf);
    lv_obj_set_width(l, theme::SCREEN_W - 2 * theme::PAD);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(l, &font_ui_20, 0);
}

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
        // The unfilled part of the ring, in the outline grey rather than in
        // SURFACE: SURFACE is the screen's black now, and a track painted in
        // it is a track nobody can see - a progress ring with no circle to
        // fill, only an arc floating in the dark.
        lv_obj_set_style_arc_color(ring, lv_color_hex(theme::LINE), LV_PART_MAIN);
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

    // ALWAYS. The installed version is the fact this page is about, and it is
    // what somebody opening it came to see - whether or not there is anything
    // newer. It was hidden when an update was on offer, on the grounds that the
    // step line below repeats the number; but the card is a statement about the
    // device and the step line is a statement about what would happen, and a
    // page that removes the first the moment the second appears has nothing
    // steady on it.
    frame::row(body, i18n::T(S_INSTALLED), vbuf, false, nullptr, nullptr);

    lv_obj_t* spacer = lv_obj_create(body);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 12);

    switch (otaState) {
    case ota::CHECKING: {
        // Something that MOVES while the network is being asked. A line of text
        // saying "checking" is indistinguishable from a line of text that is
        // stuck, and this wait is a TLS handshake plus a fetch - seconds, on a
        // weak link. The ring turns, so the page is visibly doing the thing it
        // said it would do on arrival.
        lv_obj_t* sp = lv_spinner_create(body, 900, 60);
        lv_obj_set_size(sp, 44, 44);
        lv_obj_set_style_arc_width(sp, 4, LV_PART_MAIN);
        lv_obj_set_style_arc_width(sp, 4, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(sp, lv_color_hex(theme::LINE), LV_PART_MAIN);
        lv_obj_set_style_arc_color(sp, lv_color_hex(theme::ACCENT), LV_PART_INDICATOR);
        gap(body, 10);
        frame::caption(i18n::T(S_CHECKING), theme::TEXT_DIM);
        break;
    }

    case ota::UP_TO_DATE:
        badge(body, LV_SYMBOL_OK, theme::OK);
        frame::caption(i18n::T(S_UP_TO_DATE), theme::OK);
        break;

    case ota::AVAILABLE: {
        badge(body, LV_SYMBOL_DOWNLOAD, theme::WARN);
        versionStep(body, version, latest);
        // The button goes to the BOTTOM of the screen, not under the text.
        //
        // A growing spacer rather than an alignment: this body is a scrolling
        // flex column, so pinning the button to the bottom edge would have it
        // sit over the list on a screen whose content does overflow. Given the
        // leftover height the spacer takes it and the button lands at the foot;
        // given none, it takes none and the button follows the content, which
        // is where it belongs then.
        lv_obj_t* push = lv_obj_create(body);
        lv_obj_remove_style_all(push);
        lv_obj_set_width(push, 1);
        lv_obj_set_flex_grow(push, 1);
        lv_obj_clear_flag(push, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(push, LV_OBJ_FLAG_CLICKABLE);

        frame::button(body, i18n::T(S_INSTALL), 1, onInstall);
        break;
    }

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
    versionStep(body, current, latest);

    lv_obj_t* gap = lv_obj_create(body);
    lv_obj_remove_style_all(gap);
    lv_obj_set_size(gap, 1, 16);

    frame::button(body, i18n::T(S_INSTALL), 1, []() { s_action = A_INSTALL_NOW; });
    frame::button(body, i18n::T(S_LATER),   0, []() { s_action = A_LATER; });
}

// Every page the tag proper occupies, 0x04 to 0x17, four bytes to a line.
//
// Numbered by their REAL addresses, not 1 to 20: that is what the memory map
// calls them and what a reader log prints, so a line here sits beside a line
// there without anybody counting. The sixteen pages after 0x17 are the ECDSA
// signature - read separately, verified, and of no use as hex.
void showReaderHex(const TagInfo* tag) {
    uint32_t sig = 0xB2000000u ^ (tag ? tag->idProduct * 2654435761u : 0u);
    if (sameView((const void*)showReaderHex, sig)) return;
    claimView((const void*)showReaderHex, sig);

    lv_obj_t* body = frame::build(i18n::T(S_NT_HEX), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    theme::scrollbar(body);

    if (!tag || !tag->ok) return;

    char b[16];
    const String& hex = tag->pages;
    for (int i = 0, page = 0x04; i < (int)hex.length(); page++) {
        int sp = hex.indexOf(' ', i);
        if (sp < 0) sp = hex.length();

        // A fixed column for the address, the bytes left-aligned after it, and
        // both set in a MONOSPACE face.
        //
        // A dump is read down a column - you scan for the byte that changed -
        // and that needs every character to sit under the one above it. Left-
        // aligning the value got the lines starting together and no further:
        // Montserrat's digits share a width but A to F do not, so the columns
        // drifted apart across eight characters. No amount of alignment fixes
        // a proportional font; the columns have to come from the typeface.
        lv_obj_t* row = lv_obj_create(body);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        // No "Page" in front of the number. It is the same word on every line
        // of the screen, and the screen is titled HEX Code already.
        snprintf(b, sizeof(b), "0x%02X", page);
        lv_obj_t* ad = lv_label_create(row);
        lv_label_set_text(ad, b);
        // Wider at 16 px than it was at 12: four monospace characters plus the
        // gap to the bytes.
        lv_obj_set_width(ad, 62);
        lv_obj_set_style_text_font(ad, &font_ui_mono_16, 0);
        lv_obj_set_style_text_color(ad, lv_color_hex(theme::TEXT_DIM), 0);

        lv_obj_t* v = lv_label_create(row);
        lv_label_set_text(v, hex.substring(i, sp).c_str());
        lv_obj_set_style_text_font(v, &font_ui_mono_16, 0);
        lv_obj_set_style_text_color(v, lv_color_hex(theme::TEXT), 0);

        i = sp + 1;
    }
    gap(body, 12);
    frame::button(body, i18n::T(S_BACK), 1, onBack);
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

    // WAITING is a state this screen spends most of its life in, and it was two
    // lines of text on an otherwise black panel - nothing said where to put the
    // spool, or that the device was doing anything at all. It now says the one
    // thing it is for, the way the scan screen already does: the reader's own
    // wave, large, over the instruction.
    if (ready && (!tag || !tag->ok)) {
        lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // The reader's own state first, above everything else. It is the
        // premise of the screen - whether the hardware is even there - and it
        // belongs before the thing it makes possible, not tucked underneath it
        // like a footnote.
        lv_obj_t* st = frame::caption(i18n::T(S_READER_OK), theme::OK);
        lv_obj_set_style_text_font(st, &font_ui_bold_16, 0);
        lv_obj_set_style_pad_bottom(st, 16, 0);

        // In a ring, like the update screen's download and the up-to-date
        // check. This device has one shape for "here is the state of the thing
        // you came to look at", and a waiting reader is exactly that; a bare
        // glyph floating in the middle would have been a fourth idea.
        lv_obj_t* ring = lv_obj_create(body);
        lv_obj_remove_style_all(ring);
        lv_obj_set_size(ring, 86, 86);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(ring, lv_color_hex(theme::ACCENT), 0);
        lv_obj_set_style_border_width(ring, 2, 0);
        lv_obj_set_style_border_opa(ring, LV_OPA_COVER, 0);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_center(icons::build(ring, icons::NFC, theme::ACCENT, 200));

        lv_obj_t* c = frame::caption(i18n::T(S_WAITING_NFC), theme::TEXT);
        lv_obj_set_style_pad_top(c, 16, 0);
        lv_obj_set_style_text_font(c, &font_ui_16, 0);
        return;
    }

    // No "reader ready" line above the results.
    //
    // It answers a question the screen has already answered by being full of a
    // chip's contents, and it cost the top of a view that has fifteen fields to
    // fit. The waiting state still says it, which is where it means something.
    if (!ready) {
        lv_obj_t* st = lv_label_create(body);
        lv_label_set_text(st, i18n::T(S_READER_NONE));
        lv_obj_set_style_text_font(st, &font_ui_16, 0);
        lv_obj_set_style_text_color(st, lv_color_hex(theme::DANGER), 0);
        lv_obj_set_style_pad_bottom(st, 12, 0);
    }

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
    // Everything the chip carried, one field per row, in the order somebody
    // checking a tag reads them: identity, then what the spool is, then its
    // numbers, then the raw pages the numbers came out of.
    //
    // The scrollbar is set HERE and not at the top of the function, because the
    // waiting state has nothing to scroll and a bar spanning a full, still
    // track is furniture pretending to be information.
    theme::scrollbar(body);

    // The colour as a bar across the width, in as many bands as the spool has
    // colours. It is the one field that cannot be checked against a number -
    // you hold the spool next to it - so it gets the width rather than a
    // corner.
    //
    // How many bands is the ASPECT's answer, never the colour bytes'. A second
    // colour of 00 00 00 is a black one on a bicolour spool and an unused slot
    // on every other, and the chip stores both the same way; only "Bicolor" or
    // "Tricolor" on the aspect tells them apart. The count is resolved from the
    // reference database when the tag is decoded - see reader.cpp.
    {
        lv_obj_t* bar = lv_obj_create(body);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, LV_PCT(100), 34);
        lv_obj_set_style_radius(bar, 6, 0);
        lv_obj_set_style_clip_corner(bar, true, 0);
        lv_obj_set_style_border_color(bar, lv_color_hex(theme::LINE), 0);
        lv_obj_set_style_border_width(bar, 1, 0);
        lv_obj_set_style_border_opa(bar, LV_OPA_COVER, 0);
        // NO padding on the bar. Its bands fill the CONTENT area, and padding
        // shrinks that from the inside - which left a ten pixel strip of bare
        // ground along the bottom, inside the border, looking like a fourth
        // colour that happened to be black. The space below belongs between the
        // bar and the list, not inside the bar.
        lv_obj_set_style_pad_all(bar, 0, 0);
        lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

        const uint8_t n = tag->colorCount < 1 ? 1 : (tag->colorCount > 3 ? 3 : tag->colorCount);
        const lv_color_t cols[3] = {
            lv_color_make(tag->r,   tag->g,   tag->b),
            lv_color_make(tag->c2r, tag->c2g, tag->c2b),
            lv_color_make(tag->c3r, tag->c3g, tag->c3b),
        };
        for (uint8_t i = 0; i < n; i++) {
            lv_obj_t* band = lv_obj_create(bar);
            lv_obj_remove_style_all(band);
            lv_obj_set_height(band, LV_PCT(100));
            lv_obj_set_flex_grow(band, 1);
            lv_obj_set_style_bg_color(band, cols[i], 0);
            lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
            lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);
        }
    }
    gap(body, 10);

    char b[64];
    const char* DASH = "-";
    auto orDash = [&](const String& v) { return v.length() ? v.c_str() : DASH; };

    kv(body, "UID", orDash(tag->uid), theme::TEXT);
    kv(body, i18n::T(S_NT_BRAND),    orDash(tag->brand),         theme::TEXT);
    kv(body, i18n::T(S_NT_TYPE),     orDash(tag->protocolLabel), theme::TEXT);
    kv(body, i18n::T(S_NT_MATERIAL), orDash(tag->material),      theme::TEXT);
    kv(body, i18n::T(S_NT_MESSAGE),  orDash(tag->message),       theme::TEXT);

    {
        const bool a1 = tag->aspect1Label.length() > 0;
        const bool a2 = tag->aspect2Label.length() > 0;
        snprintf(b, sizeof(b), "%s / %s",
                 a1 ? tag->aspect1Label.c_str() : DASH,
                 a2 ? tag->aspect2Label.c_str() : DASH);
        kv(body, i18n::T(S_NT_ASPECT), b, theme::TEXT);
    }

    kv(body, i18n::T(S_NT_KIND), orDash(tag->kindLabel), theme::TEXT);

    snprintf(b, sizeof(b), "%lu %s", (unsigned long)tag->measure, tag->unitLabel.c_str());
    kv(body, i18n::T(S_NT_WTOTAL), b, theme::TEXT);
    snprintf(b, sizeof(b), "%lu %s", (unsigned long)tag->available, tag->unitLabel.c_str());
    kv(body, i18n::T(S_NT_WAVAIL), b, theme::TEXT);

    snprintf(b, sizeof(b), "%s mm", orDash(tag->diameterLabel));
    kv(body, i18n::T(S_NT_DIAM), b, theme::TEXT);

    snprintf(b, sizeof(b), "%u - %u\xC2\xB0""C", tag->nozMin, tag->nozMax);
    kv(body, i18n::T(S_NT_NOZZLE), b, theme::TEXT);
    snprintf(b, sizeof(b), "%u - %u\xC2\xB0""C", tag->bedMin, tag->bedMax);
    kv(body, i18n::T(S_NT_BED), b, theme::TEXT);
    snprintf(b, sizeof(b), "%u\xC2\xB0""C / %uh", tag->dryTemp, tag->dryHours);
    kv(body, i18n::T(S_NT_DRY), b, theme::TEXT);

    // The stamp raw AND as a date. The raw number is what a reader log and the
    // SDK print, so it is what a value is compared against; the date is what
    // tells a person whether the number is plausible at all. Page 0x0C also
    // carries the twin tag id, so a stamp outside a sane range shows as a dash
    // rather than as a date from 1970.
    snprintf(b, sizeof(b), "%lu", (unsigned long)tag->stamp);
    kv(body, i18n::T(S_NT_STAMP), b, theme::TEXT);
    {
        const time_t t = (time_t)tag->stamp + 946684800L;   // 2000-01-01 -> epoch
        struct tm g;
        if (tag->stamp && gmtime_r(&t, &g))
            snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d",
                     g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min);
        else
            snprintf(b, sizeof(b), "-");
        kv(body, i18n::T(S_NT_DATE), b, theme::TEXT);
    }

    // The colours in hex, the way the chip stores them and the way every
    // slicer, every palette and the spool's own listing writes them. Decimal
    // triplets had to be converted in the head before they could be compared
    // with anything.
    //
    // The primary carries its alpha too - #RRGGBBAA - because page 0x08 is four
    // bytes and this screen exists to show what is on the chip.
    snprintf(b, sizeof(b), "#%02X%02X%02X%02X", tag->r, tag->g, tag->b, tag->a);
    kv(body, i18n::T(S_NT_COLOR1), b, theme::TEXT);

    // Shown whenever the aspect says the colour exists, INCLUDING #000000.
    // That is a real black on a bicolour spool, and printing a dash for it
    // would hide the very byte somebody opened this screen to check.
    if (tag->colorCount >= 2) snprintf(b, sizeof(b), "#%02X%02X%02X", tag->c2r, tag->c2g, tag->c2b);
    else                      snprintf(b, sizeof(b), "-");
    kv(body, i18n::T(S_NT_COLOR2), b, theme::TEXT);
    if (tag->colorCount >= 3) snprintf(b, sizeof(b), "#%02X%02X%02X", tag->c3r, tag->c3g, tag->c3b);
    else                      snprintf(b, sizeof(b), "-");
    kv(body, i18n::T(S_NT_COLOR3), b, theme::TEXT);

    if (tag->tdRaw) snprintf(b, sizeof(b), "%u.%u", tag->tdRaw / 10, tag->tdRaw % 10);
    else            snprintf(b, sizeof(b), "-");
    kv(body, i18n::T(S_NT_TD), b, theme::TEXT);

    // The signature verdict, and its colour carries it: this is the one row
    // where the value is a judgement rather than a reading.
    {
        const char* v = "-";
        uint32_t col = theme::TEXT_DIM;
        switch (tag->signature) {
            case TagInfo::SIG_VALID:   v = "OK";      col = theme::OK;     break;
            case TagInfo::SIG_INVALID: v = i18n::T(S_NT_INVALID); col = theme::DANGER; break;
            case TagInfo::SIG_NONE:    v = i18n::T(S_NT_NONE);    break;
            case TagInfo::SIG_NO_KEY:  v = i18n::T(S_NT_NOKEY);  col = theme::WARN;   break;
            default: break;
        }
        kv(body, i18n::T(S_NT_CERT), v, col);
    }

    // The hex dump is a screen of its own, behind a button.
    //
    // Twenty rows of it doubled the length of this view, and they are not what
    // anyone reads first: the decoded fields answer "is this spool right", and
    // the raw pages answer "why is that field wrong" - a question you only ask
    // after the first one. One button keeps the second question one tap away
    // without putting it in front of the first.
    gap(body, 12);
    frame::button(body, i18n::T(S_NT_HEX), 0, onHex);
    // And a way out that is not the chevron. This view scrolls well past a
    // screenful, and the header is at the top of it - somebody at the bottom of
    // the page had to scroll back up to leave.
    // The way out is the primary action of a screen you came to read: HEX Code
    // is where you go next only if something looked wrong.
    gap(body, 6);
    frame::button(body, i18n::T(S_BACK), 1, onBack);

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
