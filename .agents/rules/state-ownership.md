# State and screens

**Rule.** `firmware/src/main.cpp` is the only owner of state; screens are drawn from state and never mutate it. A screen is built once and its changing values are written into the widgets that hold them; only a change in what the screen contains justifies a rebuild.

**Why.** 2026-09-05: a printer toggle threw the list back to the top, and the OTA ring restarted from zero a hundred times per download, because screens were rebuilt to show a value. A screen that mutates state disagrees with the device after the next redraw.

**How.**
- Read `CODEMAP.md` for which file owns which state, and its Landmines table before editing a file.
- The interface runs on its own task (`firmware/src/ui/lvgl_port.cpp`): anything outside it that calls into LVGL or reads what LVGL painted takes `lvgl_port::Lock` first.
