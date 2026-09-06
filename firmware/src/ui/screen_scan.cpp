#include "screen_scan.h"
#include "frame.h"
#include "theme.h"
#include "i18n.h"
#include <lvgl.h>
#include <stdio.h>

namespace {
bool s_cancel = false, s_send = false, s_dismiss = false;
enum Which { NONE, SCAN, REVIEW, RESULT } s_which = NONE;
uint32_t s_sig = 0;

void onCancel()  { s_cancel = true; }
void onSend()    { s_send = true; }
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
void showScan(const char* slotLabel, const char* errorOrNull) {
    uint32_t sig = hashStr(slotLabel) ^ hashStr(errorOrNull ? errorOrNull : "");
    if (s_which == SCAN && sig == s_sig) return;
    s_which = SCAN; s_sig = sig;

    char title[24];
    snprintf(title, sizeof(title), "%s %s", i18n::T(S_SLOT), slotLabel);
    lv_obj_t* body = frame::build(title, onCancel);

    lv_obj_t* sp = lv_spinner_create(body, 1400, 55);
    lv_obj_set_size(sp, 104, 104);
    lv_obj_set_style_arc_color(sp, lv_color_hex(0x1E2530), LV_PART_MAIN);
    lv_obj_set_style_arc_color(sp, lv_color_hex(theme::ACCENT), LV_PART_INDICATOR);

    frame::caption(i18n::T(S_BRING_TAG), theme::TEXT, &lv_font_montserrat_14);
    frame::caption(i18n::T(S_TO_READER), theme::TEXT, &lv_font_montserrat_14);

    // A failed read names the fix rather than the fault: "move it closer" is
    // actionable, "read error" sends someone to a forum.
    if (errorOrNull && *errorOrNull)
        frame::caption(errorOrNull, theme::DANGER);

    frame::button(body, i18n::T(S_CANCEL), 2, onCancel);
}

void showReview(const char* slotLabel, const TagInfo& tag) {
    uint32_t sig = hashStr(slotLabel) ^ (tag.r << 16 | tag.g << 8 | tag.b)
                 ^ hashStr(tag.material.c_str()) ^ hashStr(tag.brand.c_str());
    if (s_which == REVIEW && sig == s_sig) return;
    s_which = REVIEW; s_sig = sig;

    char title[28];
    snprintf(title, sizeof(title), "%s %s", LV_SYMBOL_RIGHT, slotLabel);
    lv_obj_t* body = frame::build(title, onCancel);
    lv_obj_set_style_pad_row(body, 4, 0);

    swatch(body, tag.r, tag.g, tag.b, 68);
    frame::bigLabel(tag.material.c_str(), theme::TEXT);
    frame::caption(tag.brand.c_str(), theme::ACCENT, &lv_font_montserrat_14);

    char line[40];
    snprintf(line, sizeof(line), "%s  %u-%u C", i18n::T(S_NOZZLE), tag.nozMin, tag.nozMax);
    frame::caption(line, theme::TEXT_DIM);
    snprintf(line, sizeof(line), "%s  %u-%u C", i18n::T(S_BED), tag.bedMin, tag.bedMax);
    frame::caption(line, theme::TEXT_DIM);

    lv_obj_t* actions = lv_obj_create(body);
    lv_obj_remove_style_all(actions);
    lv_obj_set_width(actions, LV_PCT(100));
    lv_obj_set_height(actions, theme::BUTTON_H);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(actions, theme::GAP, 0);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
    // frame::button() is full-width by design, because almost every button in
    // this UI is stacked. Two of them in a flex ROW means the first takes the
    // whole row and the second is laid out past the edge - and this container
    // does not scroll, so it simply is not there. flex_grow makes them share.
    lv_obj_set_flex_grow(frame::button(actions, i18n::T(S_NO), 2, onCancel), 1);
    lv_obj_set_flex_grow(frame::button(actions, i18n::T(S_SEND), 1, onSend), 1);
}

void showResult(const char* slotLabel, bool ok, const char* message,
                const TagInfo& tag, uint32_t sentColour) {
    uint32_t sig = hashStr(message) ^ (uint32_t)ok ^ sentColour;
    if (s_which == RESULT && sig == s_sig) return;
    s_which = RESULT; s_sig = sig;

    lv_obj_t* body = frame::build(slotLabel, onDismiss);

    lv_obj_t* icon = lv_label_create(body);
    lv_label_set_text(icon, ok ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(ok ? theme::OK : theme::DANGER), 0);

    frame::bigLabel(message, theme::TEXT);

    const bool adapted = ok && sentColour != 0xFFFFFFFFu &&
        sentColour != ((uint32_t)tag.r << 16 | (uint32_t)tag.g << 8 | tag.b);
    if (adapted) {
        // Two swatches and one sentence. The printer did what it could; the
        // screen owes the user the difference rather than a silent "OK".
        lv_obj_t* pair = lv_obj_create(body);
        lv_obj_remove_style_all(pair);
        lv_obj_set_width(pair, LV_PCT(100));
        lv_obj_set_height(pair, 32);
        lv_obj_set_flex_flow(pair, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(pair, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(pair, 8, 0);
        lv_obj_clear_flag(pair, LV_OBJ_FLAG_SCROLLABLE);

        swatch(pair, tag.r, tag.g, tag.b, 26);
        lv_obj_t* arrow = lv_label_create(pair);
        lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(arrow, lv_color_hex(theme::TEXT_DIM), 0);
        swatch(pair, (sentColour >> 16) & 0xFF, (sentColour >> 8) & 0xFF,
               sentColour & 0xFF, 26);

        frame::caption(i18n::T(S_COLOUR_ADAPTED), theme::ACCENT);
    } else if (ok) {
        char t[48];
        snprintf(t, sizeof(t), "%s  %s", tag.material.c_str(), tag.brand.c_str());
        frame::caption(t, theme::TEXT_DIM);
    }

    frame::caption(i18n::T(S_TAP_BACK), theme::TEXT_DIM);
}

bool takeCancel()  { bool v = s_cancel;  s_cancel = false;  return v; }
bool takeSend()    { bool v = s_send;    s_send = false;    return v; }
bool takeDismiss() { bool v = s_dismiss; s_dismiss = false; return v; }

}  // namespace screen_scan
