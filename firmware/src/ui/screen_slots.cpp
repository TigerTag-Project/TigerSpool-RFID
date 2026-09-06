#include "screen_slots.h"
#include "frame.h"
#include "theme.h"
#include "i18n.h"
#include <lvgl.h>

namespace {
lv_obj_t* s_grid = nullptr;
bool s_built = false;
int  s_tapped = -1;
bool s_back = false;
int  s_lastCount = -1;
uint32_t s_lastSig = 0;

void onCell(lv_event_t* e) { s_tapped = (int)(intptr_t)lv_event_get_user_data(e); }
void onBack()              { s_back = true; }

// A cheap signature of what is on screen, so the grid is only rebuilt when the
// printer actually reports something different. Rebuilding every loop would
// cancel the scroll under the user's finger.
uint32_t signature(PrinterBackend* b, int n, int selected) {
    uint32_t h = 2166136261u ^ (uint32_t)selected;
    for (int i = 0; i < n; i++) {
        const SlotState& s = b->slot(i);
        uint32_t v = (s.r << 16) | (s.g << 8) | s.b;
        v ^= (uint32_t)s.known << 24;
        v ^= (uint32_t)s.selected << 25;
        for (const char* p = s.type.c_str(); *p; p++) v = v * 16777619u ^ (uint8_t)*p;
        h = h * 16777619u ^ v;
    }
    return h;
}
}  // namespace

namespace screen_slots {

void invalidate() { s_built = false; s_lastCount = -1; s_lastSig = 0; }

void show(const char* printerName, PrinterBackend* backend,
          int selected, bool readerReady) {
    if (!backend) return;
    const int n = backend->slotCount();
    const uint32_t sig = signature(backend, n, selected);
    if (s_built && n == s_lastCount && sig == s_lastSig) {
        // The reader dot is gone: it was green on every screen, always, because
    // the reader is always ready - a pixel that says nothing. What it used to
    // claim is now provable under Settings, on a screen that actually reads a
    // tag. Only the printer connection is reported here, where it varies.
    frame::setDots(-1, backend->connected(), -1);
        return;
    }
    s_built = true; s_lastCount = n; s_lastSig = sig;

    lv_obj_t* body = frame::build(printerName, onBack);
    // The reader dot is gone: it was green on every screen, always, because
    // the reader is always ready - a pixel that says nothing. What it used to
    // claim is now provable under Settings, on a screen that actually reads a
    // tag. Only the printer connection is reported here, where it varies.
    frame::setDots(-1, backend->connected(), -1);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

    s_grid = lv_obj_create(body);
    lv_obj_remove_style_all(s_grid);
    lv_obj_set_width(s_grid, LV_PCT(100));
    lv_obj_set_height(s_grid, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(s_grid, theme::GAP, 0);
    lv_obj_set_style_pad_column(s_grid, theme::GAP, 0);
    lv_obj_clear_flag(s_grid, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < n; i++) {
        const SlotState& st = backend->slot(i);

        lv_obj_t* cell = lv_btn_create(s_grid);
        lv_obj_remove_style_all(cell);
        lv_obj_add_style(cell, theme::rowStyle(), 0);
        lv_obj_add_style(cell, theme::rowPressedStyle(), LV_STATE_PRESSED);
        lv_obj_set_size(cell, 107, theme::CELL_H);
        lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(cell, 3, 0);
        lv_obj_add_event_cb(cell, onCell, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        if (i == selected) {
            lv_obj_set_style_outline_width(cell, 2, 0);
            lv_obj_set_style_outline_color(cell, lv_color_hex(theme::ACCENT), 0);
        }

        // Slot name above, the colour block with its material written inside,
        // the brand underneath - the same three-part cell the mobile app uses,
        // so somebody who has both in front of them is reading one design.
        //
        // A block rather than a disc: the material has to sit INSIDE the
        // colour, and a word inside a circle either overflows the circle or
        // shrinks the circle until the colour stops carrying.
        lv_obj_t* label = lv_label_create(cell);
        lv_label_set_text(label, backend->slotLabel(i));
        lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(theme::TEXT_DIM), 0);

        lv_obj_t* block = lv_obj_create(cell);
        lv_obj_remove_style_all(block);
        lv_obj_set_size(block, 74, 46);
        lv_obj_set_style_radius(block, 8, 0);
        lv_obj_set_style_bg_opa(block, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(block,
            st.known ? lv_color_make(st.r, st.g, st.b) : lv_color_hex(0x3A424E), 0);
        lv_obj_clear_flag(block, LV_OBJ_FLAG_SCROLLABLE);

        // Black on a pale spool, white on a dark one. Perceived brightness,
        // not the arithmetic mean: the eye reads green as far brighter than
        // blue, and a mean puts black text on navy.
        const int lum = st.known ? (st.r * 299 + st.g * 587 + st.b * 114) / 1000 : 0;
        lv_obj_t* mat = lv_label_create(block);
        lv_label_set_text(mat, st.known && st.type.length() ? st.type.c_str() : "?");
        lv_label_set_long_mode(mat, LV_LABEL_LONG_DOT);
        lv_obj_set_width(mat, 66);
        lv_obj_set_style_text_align(mat, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(mat, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(mat, lv_color_hex(lum > 150 ? 0x101010 : 0xFFFFFF), 0);
        lv_obj_center(mat);

        lv_obj_t* brand = lv_label_create(cell);
        lv_label_set_text(brand, st.brand.length() ? st.brand.c_str() : "-");
        lv_label_set_long_mode(brand, LV_LABEL_LONG_DOT);
        lv_obj_set_width(brand, 90);
        lv_obj_set_style_text_align(brand, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(brand, lv_color_hex(theme::TEXT_DIM), 0);
        lv_obj_set_style_text_font(brand, &lv_font_montserrat_12, 0);
    }

    if (n == 0) frame::caption(i18n::T(S_FIND_PRINTERS), theme::TEXT_DIM);
}

int  takeTappedSlot() { int v = s_tapped; s_tapped = -1; return v; }
bool takeBack()       { bool v = s_back; s_back = false; return v; }

}  // namespace screen_slots
