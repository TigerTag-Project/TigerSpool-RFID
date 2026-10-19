# Look at the screen yourself

**Rule.** For any visual change, look at the device rather than asking someone: `http://<device>.local/screen.bmp` is the framebuffer, `?preview=<name>` renders a screen without touching state, `/api/tap?x=&y=` injects a touch (panel pixels, 240x320, origin top-left), `/api/lang?l=<code>` switches language.

**Why.** "Does it look right?" is a question to answer yourself, without anyone at the bench; it is the largest saving available on a visual change.

**How.**
- Preview names are listed in `firmware/src/webcfg.cpp`.
- The capture serialises what LVGL drew: trust it for layout, text and translation, not for colour or backlight.
- Restore the device's language and screen after testing.
- Reach the device by mDNS (`LOCAL.md` has its name), not by IP.
