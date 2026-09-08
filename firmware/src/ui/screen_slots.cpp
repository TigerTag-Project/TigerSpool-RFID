#include "screen_slots.h"
#include "fonts.h"

// Where a failure sends someone. A wiki rather than text on the panel: four
// causes do not fit on 240 px, and a page can be corrected the day a new
// printer joins the list without shipping firmware to do it.
static const char* LINK_HELP_URL = "https://wiki.tigersystem.io";

#include "frame.h"
#include "theme.h"
#include "i18n.h"
#include <lvgl.h>

namespace {
// The white margin a scanner needs around a QR code, in pixels, on every side.
const lv_coord_t QUIET = 7;

lv_obj_t* s_grid = nullptr;
bool s_built = false;
int  s_tapped = -1;
bool s_back = false;
lv_obj_t* s_progress = nullptr;   // the line under the spinner, or null
int  s_lastCount = -1;
uint32_t s_lastSig = 0;

void onCell(lv_event_t* e) { s_tapped = (int)(intptr_t)lv_event_get_user_data(e); }
void onBack()              { s_back = true; }

// A cheap signature of what is on screen, so the grid is only rebuilt when the
// printer actually reports something different. Rebuilding every loop would
// cancel the scroll under the user's finger.
uint32_t signature(PrinterBackend* b, int n, int selected, int link) {
    uint32_t h = 2166136261u ^ (uint32_t)selected ^ ((uint32_t)link << 8);
    // `s.known` below carries more weight than it looks: a backend reports its
    // slot count from the printer's model before it has connected, so an
    // unreachable printer still yields five cells. Known-ness is what separates
    // the spinner from the grid, and folding it in here is what lets the first
    // real answer replace the spinner.
    if (!b) return h;
    for (int i = 0; i < n; i++) {
        const SlotState& s = b->slot(i);
        uint32_t v = (s.r << 16) | (s.g << 8) | s.b;
        v ^= (uint32_t)s.known << 24;
        v ^= (uint32_t)s.selected << 25;
        for (const char* p = s.type.c_str();  *p; p++) v = v * 16777619u ^ (uint8_t)*p;
        for (const char* p = s.brand.c_str(); *p; p++) v = v * 16777619u ^ (uint8_t)*p;
        h = h * 16777619u ^ v;
    }
    return h;
}
}  // namespace

namespace screen_slots {

void invalidate() { s_built = false; s_lastCount = -1; s_lastSig = 0; }

bool s_retry = false;
void onRetry(lv_event_t*) { s_retry = true; }

// What to say under the spinner. Reading the account comes first because it is
// what actually happens first on a retry - the address may be what was wrong,
// so it is re-read before anything is dialled - and "Attempt 2 of 3" is worth
// more than a bare spinner: it says the device has a plan and how much of it
// is left.
void progressText(char* out, size_t n, int tries, int budget, bool fetching) {
    if (fetching)   { snprintf(out, n, "%s", i18n::T(S_LINK_FETCH)); return; }
    if (tries <= 0) { snprintf(out, n, "%s", i18n::T(S_LINK_TRY));   return; }
    snprintf(out, n, i18n::T(S_LINK_ATTEMPT), tries, budget);
}

void show(const char* printerName, PrinterBackend* backend,
          int selected, bool readerReady, int link,
          int tries, int budget, bool fetching) {
    // No backend is a state to DRAW, not a reason to draw nothing. When the
    // link has given up there is no backend at all, and that is exactly the
    // moment the user needs a screen with a retry button on it.
    const int n = backend ? backend->slotCount() : 0;
    bool anyKnown = false;
    for (int i = 0; i < n && !anyKnown; i++) anyKnown = backend->slot(i).known;
    const uint32_t sig = signature(backend, n, selected, link);
    // Nothing has changed: leave the screen alone. The header was built with
    // this same `link`, so its dot and its retry button are already right -
    // rebuilding them here once per frame stacked a fresh button on the old
    // one every loop, and the spinner below would have restarted mid-turn.
    if (s_built && n == s_lastCount && sig == s_lastSig) {
        // The one thing that may change without a rebuild. Written into the
        // label that is already there, so the spinner beside it keeps turning.
        if (s_progress) {
            char t[48];
            progressText(t, sizeof(t), tries, budget, fetching);
            lv_label_set_text(s_progress, t);
        }
        return;
    }
    s_built = true; s_lastCount = n; s_lastSig = sig;
    s_progress = nullptr;              // whatever it pointed at is about to go

    lv_obj_t* body = frame::build(printerName, onBack);
    // The reader dot is gone: it was green on every screen, always, because
    // the reader is always ready - a pixel that says nothing. What it used to
    // claim is now provable under Settings, on a screen that actually reads a
    // tag. Only the printer connection is reported here, where it varies.
    // Connecting is not a colour either - it is the spinner in the body, and
    // the dot stays hidden rather than flashing red at someone who is already
    // being told the box is working on it. Given up is a button.
    if (link == 3) {
        lv_obj_t* r = lv_btn_create(frame::header());
        lv_obj_remove_style_all(r);
        lv_obj_set_size(r, 52, theme::HEADER_H);
        lv_obj_align(r, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(r, onRetry, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* g = lv_label_create(r);
        lv_label_set_text(g, LV_SYMBOL_REFRESH);
        lv_obj_set_style_text_font(g, &font_ui_20, 0);
        lv_obj_set_style_text_color(g, lv_color_hex(theme::WARN), 0);
        lv_obj_center(g);
        frame::setDots(-1, -1, -1);
    } else {
        // While the spinner is up it IS the indicator; a red dot beside a
        // ring that says "connecting" is the screen arguing with itself. The
        // dot comes back the moment there is something to be red about - a
        // link that was working and dropped, with the last slots still drawn.
        frame::setDots(-1, link == 2 ? 1 : (anyKnown ? 0 : -1), -1);
    }
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

    // The external spool is not one of the four. It is a different thing that
    // happens to be next to them - the CFS holds four, and the spool hanging
    // off the side is the fifth place a filament can be. Giving it its own row
    // says that without a word, and puts the four that belong together on one
    // line where they can be compared at a glance.
    //
    // The rule generalises past this printer: FIRST slot alone, the rest four
    // to a line. A Bambu with four AMS units gets its external spool on top and
    // four rows of four beneath, which is also how those units are grouped.
    // Giving up is a screen of its own, not an empty grid. It says one thing
    // and offers one action. The causes - printer off, wrong network, wrong
    // settings, a Bambu that has run out of connection slots - all live behind
    // the QR, where a wiki page can be corrected the day a new printer joins
    // that list. A panel 240 px wide cannot argue a case; it can point at one.
    // Trying is not nothing, and it must not look like nothing. An empty grid
    // for forty seconds is what made the box feel broken when the printer was
    // merely off; a turning ring says the device is working on it, and it is
    // the difference between waiting and wondering. It replaces the grid only
    // while there is no grid to show - a reconnection after a drop keeps the
    // slots on screen, because stale filament is better than a blank screen.
    if (link < 2 && !anyKnown) {
        // A transparent box to hold the gap. Margin styles are compiled out of
        // this build, and padding on the arc itself insets the arc inside its
        // own bounds - 48 top and 14 bottom on a 56 px spinner left negative
        // room and it drew nothing at all, a caption sitting alone under a gap.
        lv_obj_t* pad = lv_obj_create(body);
        lv_obj_remove_style_all(pad);
        lv_obj_set_size(pad, 56, 116);
        lv_obj_clear_flag(pad, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* sp = lv_spinner_create(pad, 1000, 60);
        lv_obj_set_size(sp, 56, 56);
        lv_obj_set_style_arc_color(sp, lv_color_hex(theme::LINE), LV_PART_MAIN);
        lv_obj_set_style_arc_color(sp, lv_color_hex(theme::ACCENT), LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(sp, 6, LV_PART_MAIN);
        lv_obj_set_style_arc_width(sp, 6, LV_PART_INDICATOR);
        lv_obj_align(sp, LV_ALIGN_TOP_MID, 0, 44);
        char t[48];
        progressText(t, sizeof(t), tries, budget, fetching);
        s_progress = frame::caption(t, theme::TEXT_DIM);
        return;
    }

    if (link == 3) {
        lv_obj_t* t = lv_label_create(body);
        lv_label_set_text(t, i18n::T(S_LINK_FAIL));
        lv_obj_set_style_text_font(t, &font_ui_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(theme::DANGER), 0);
        lv_obj_set_style_pad_bottom(t, 8, 0);

        // The quiet zone is a white BOX around the code, not a border on it.
        //
        // A border grows an lv_qrcode outward from its canvas, and the bottom
        // edge came out as a white strip with a dark line through it - visible
        // on the panel as a bar cutting across the last row of modules, which
        // is also the row a scanner needs. A plain white container with padding
        // gives the same margin and draws nothing of its own.
        // The card takes ITS size from the code, not the other way round.
        //
        // lv_qrcode picks the largest whole number of pixels per module that
        // fits the size asked for, so it comes out at 99 rather than 104 - and
        // a fixed 114 box then left 7 px on one side and 8 on the other, and
        // 6 against 7 vertically. Sized to content with equal padding, the
        // quiet zone is exactly the padding on all four sides whatever size
        // the code lands on, and it stays right if the URL ever gets longer.
        lv_obj_t* frameBox = lv_obj_create(body);
        lv_obj_remove_style_all(frameBox);
        lv_obj_set_style_pad_all(frameBox, 0, 0);
        lv_obj_set_style_bg_color(frameBox, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(frameBox, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(frameBox, 4, 0);
        lv_obj_clear_flag(frameBox, LV_OBJ_FLAG_SCROLLABLE);
        // No padding on this box. lv_obj_center places a child inside the
        // CONTENT area, so a bottom pad of 6 pushed the code three pixels up
        // and left an uneven margin - a quiet zone that is wider on one side
        // than the other is exactly what a quiet zone must not be. The gap to
        // the caption below is the caption's business, set on it instead.

        lv_obj_t* q = lv_qrcode_create(frameBox, 104, lv_color_black(),
                                       lv_color_white());
        lv_qrcode_update(q, LINK_HELP_URL, strlen(LINK_HELP_URL));

        // Size the card from the code's ACTUAL width, measured after the fact.
        //
        // Two attempts got this wrong in ways worth writing down. A fixed 114
        // box left 7 px against 8, because lv_qrcode rounds the size down to a
        // whole number of pixels per module and comes out at 99, not 104.
        // LV_SIZE_CONTENT with padding was worse: 112 wide but 118 tall, so the
        // card was not even square. Asking the widget how big it ended up and
        // adding the quiet zone twice is the only version that is exact, and it
        // stays exact if the URL changes length.
        // Layout FIRST. lv_qrcode_update resizes the canvas to the real code,
        // but the new width is not readable until the layout pass has run -
        // asking before it does returns a placeholder, and the card came out
        // the size of a postage stamp with the code shrunk inside it.
        lv_obj_update_layout(q);
        const lv_coord_t qw = lv_obj_get_width(q);
        // Square it explicitly. lv_qrcode_update narrows the canvas to the real
        // code but leaves the object's HEIGHT at the size it was created with,
        // so width came back 98 and height 104 - and the card built from them
        // was 112 x 118, six pixels taller than it was wide. That, and not the
        // alignment, is what made the code look off-centre.
        lv_obj_set_size(q, qw, qw);
        lv_obj_set_size(frameBox, qw + 2 * QUIET, qw + 2 * QUIET);
        // Placed, not centred. lv_obj_center halves an odd leftover and hands
        // one pixel to one side - which is what left 6 against 7. Pinning the
        // code to the top-left at exactly QUIET makes all four margins equal by
        // construction, and the card is sized from the code so there is no
        // leftover to distribute in the first place.
        lv_obj_align(q, LV_ALIGN_TOP_LEFT, QUIET, QUIET);

        lv_obj_set_style_pad_top(
            frame::caption(i18n::T(S_LF_SCAN), theme::TEXT_DIM), 8, 0);

        return;
    }

    s_grid = lv_obj_create(body);
    lv_obj_remove_style_all(s_grid);
    lv_obj_set_width(s_grid, LV_PCT(100));
    lv_obj_set_height(s_grid, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(s_grid, theme::GAP, 0);
    lv_obj_set_style_pad_column(s_grid, theme::GAP, 0);
    lv_obj_clear_flag(s_grid, LV_OBJ_FLAG_SCROLLABLE);

    // 240 wide, 8 of body padding each side, three 6 px gaps between four
    // cells: (240 - 16 - 18) / 4.
    constexpr lv_coord_t CELL_W = 51;

    for (int i = 0; i < n; i++) {
        const SlotState& st = backend->slot(i);

        // A row break after the first, so the four land together underneath -
        // but only where the first slot really is the one external spool. An
        // Anycubic box is four slots and none of them is external, so breaking
        // after A1 would leave it alone above A2, A3 and A4.
        if (i == 1 && n > 1 && backend->firstIsExternal()) {
            lv_obj_t* brk = lv_obj_create(s_grid);
            lv_obj_remove_style_all(brk);
            lv_obj_set_size(brk, LV_PCT(100), 1);
            lv_obj_clear_flag(brk, LV_OBJ_FLAG_SCROLLABLE);
        }

        // No card around the cell. The coloured block is already an object;
        // wrapping it in a second rounded panel drew a box around a box and
        // spent contrast saying nothing.
        lv_obj_t* cell = lv_btn_create(s_grid);
        lv_obj_remove_style_all(cell);
        lv_obj_set_size(cell, CELL_W, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(cell, 3, 0);
        lv_obj_add_event_cb(cell, onCell, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        if (i == selected) {
            lv_obj_set_style_outline_width(cell, 2, 0);
            lv_obj_set_style_outline_pad(cell, 2, 0);
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
        lv_obj_set_style_text_font(label, &font_ui_12, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(theme::TEXT_DIM), 0);

        lv_obj_t* block = lv_obj_create(cell);
        lv_obj_remove_style_all(block);
        // Taller than wide: a spool's material name is two or three words and
        // wants the vertical room, and five portrait blocks fill a 240 px
        // panel better than five squares with dead space above and below.
        lv_obj_set_size(block, CELL_W, 62);
        lv_obj_set_style_radius(block, 8, 0);
        lv_obj_set_style_bg_opa(block, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(block,
            st.known ? lv_color_make(st.r, st.g, st.b) : lv_color_hex(0x3A424E), 0);
        lv_obj_clear_flag(block, LV_OBJ_FLAG_SCROLLABLE);
        // A hairline edge, because the panel's ground is black and so are
        // plenty of filaments. Without it a black spool is an invisible cell -
        // the colour is right and the slot looks empty.
        lv_obj_set_style_border_width(block, 1, 0);
        lv_obj_set_style_border_color(block, lv_color_hex(theme::LINE), 0);
        // And not clickable. A bare lv_obj is clickable by default in LVGL 8,
        // and this one covers almost the whole cell - so every press aimed at
        // the slot landed on the colour block and stopped there. The cell was
        // a button that could only be pressed on the three millimetres of text
        // above and below it, which reads as a grid that simply does not
        // respond. Same trap as the drawn icons in icons.cpp.
        lv_obj_clear_flag(block, LV_OBJ_FLAG_CLICKABLE);

        // Black on a pale spool, white on a dark one. Perceived brightness,
        // not the arithmetic mean: the eye reads green as far brighter than
        // blue, and a mean puts black text on navy.
        const int lum = st.known ? (st.r * 299 + st.g * 587 + st.b * 114) / 1000 : 0;
        lv_obj_t* mat = lv_label_create(block);
        lv_label_set_text(mat, st.known && st.type.length() ? st.type.c_str() : "?");
        lv_label_set_long_mode(mat, LV_LABEL_LONG_DOT);
        lv_obj_set_width(mat, CELL_W - 4);
        lv_obj_set_style_text_align(mat, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(mat, &font_ui_12, 0);
        lv_obj_set_style_text_color(mat, lv_color_hex(lum > 150 ? 0x101010 : 0xFFFFFF), 0);
        lv_obj_center(mat);

        lv_obj_t* brand = lv_label_create(cell);
        lv_label_set_text(brand, st.brand.length() ? st.brand.c_str() : "-");
        lv_label_set_long_mode(brand, LV_LABEL_LONG_DOT);
        lv_obj_set_width(brand, CELL_W);
        // One line, or a long vendor wraps and pushes its cell taller than the
        // three beside it - "Snapmaker" came out as "Snapma / ker" and took the
        // row with it. With the height pinned, LONG_DOT ellipsises instead.
        lv_obj_set_height(brand, 14);
        lv_obj_set_style_text_align(brand, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(brand, lv_color_hex(theme::TEXT_DIM), 0);
        lv_obj_set_style_text_font(brand, &font_ui_12, 0);
    }

    if (n == 0) frame::caption(i18n::T(S_FIND_PRINTERS), theme::TEXT_DIM);
}

int  takeTappedSlot() { int v = s_tapped; s_tapped = -1; return v; }
bool takeBack()       { bool v = s_back; s_back = false; return v; }
bool takeRetry()      { bool v = s_retry; s_retry = false; return v; }

}  // namespace screen_slots
