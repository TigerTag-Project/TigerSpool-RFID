# LVGL only

**Rule.** Every screen is LVGL, inside the shared frame in `firmware/src/ui/frame.cpp`. No raw pixel drawing.

**Why.** Raw pixel drawing was removed when the interface was rebuilt on LVGL (PR #1, 2026-09-03); one drawing model keeps every screen inside the theme and the frame.

**How.**
- Use the theme in `firmware/src/ui/theme.h` and the helpers in `frame.cpp` rather than styling a widget by hand.
- Text comes from the translation table (`i18n-keys.md`) and the UI fonts (`ui-fonts.md`).
