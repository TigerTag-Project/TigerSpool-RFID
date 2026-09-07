#include "theme.h"
#include "fonts.h"

namespace {
    lv_style_t s_screen, s_header, s_row, s_rowPressed;
    bool s_ready = false;
}

namespace theme {

void init() {
    if (s_ready) return;
    s_ready = true;

    // Re-colour LVGL's own theme before anything else.
    //
    // The default dark theme paints screens in lv_palette BLUE_GREY, and a
    // near-black background added as a style is not guaranteed to beat it on
    // every object - a device showing a blue-grey screen where black was asked
    // for is that palette, not a broken panel. Pointing the theme at our own
    // colours means nothing inherits blue by accident, including widgets we
    // have not styled yet.
    lv_disp_t* disp = lv_disp_get_default();
    if (disp) {
        lv_theme_t* th = lv_theme_default_init(
            disp,
            lv_color_hex(ACCENT),     // primary
            lv_color_hex(OK),         // secondary
            true,                     // dark
            &font_ui_14);
        lv_disp_set_theme(disp, th);
    }

    lv_style_init(&s_screen);
    lv_style_set_bg_color(&s_screen, lv_color_hex(BG));
    lv_style_set_bg_opa(&s_screen, LV_OPA_COVER);
    lv_style_set_border_width(&s_screen, 0);
    lv_style_set_pad_all(&s_screen, 0);
    lv_style_set_text_color(&s_screen, lv_color_hex(TEXT));

    // The header is not a filled bar. It is the same ground as the rest of the
    // screen, separated by a single rule - so the title reads as part of the
    // page instead of a strip bolted above it, and a 240 px wide screen keeps
    // every pixel of its vertical space visually.
    lv_style_init(&s_header);
    lv_style_set_bg_color(&s_header, lv_color_hex(HEADER));
    lv_style_set_bg_opa(&s_header, LV_OPA_COVER);
    lv_style_set_border_color(&s_header, lv_color_hex(LINE));
    lv_style_set_border_width(&s_header, 1);
    lv_style_set_border_side(&s_header, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_opa(&s_header, LV_OPA_COVER);
    lv_style_set_radius(&s_header, 0);
    lv_style_set_pad_all(&s_header, 0);

    // A dark interior inside a lighter outline, rather than a lighter block on
    // a dark ground. On an OLED-black screen a filled row floats; an outlined
    // one sits on the page, and eight of them read as a list instead of eight
    // separate objects.
    lv_style_init(&s_row);
    lv_style_set_bg_color(&s_row, lv_color_hex(SURFACE));
    lv_style_set_bg_opa(&s_row, LV_OPA_COVER);
    lv_style_set_radius(&s_row, RADIUS);
    lv_style_set_border_color(&s_row, lv_color_hex(LINE));
    lv_style_set_border_width(&s_row, 1);
    lv_style_set_border_opa(&s_row, LV_OPA_COVER);
    lv_style_set_pad_left(&s_row, 10);
    lv_style_set_pad_right(&s_row, 10);
    lv_style_set_text_color(&s_row, lv_color_hex(TEXT));

    // A press has to be visible: on a 240 px screen with no cursor, the only
    // feedback that a tap landed is the row itself changing.
    lv_style_init(&s_rowPressed);
    lv_style_set_bg_color(&s_rowPressed, lv_color_hex(0x333D4A));
}

lv_style_t* screenStyle()     { return &s_screen; }
lv_style_t* headerStyle()     { return &s_header; }
lv_style_t* rowStyle()        { return &s_row; }
lv_style_t* rowPressedStyle() { return &s_rowPressed; }

}  // namespace theme
