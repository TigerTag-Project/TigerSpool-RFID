# Third-party licenses

TigerSpool RFID is MIT-licensed ([LICENSE](LICENSE)). It builds on third-party
components that keep their own licenses.

> This page is kept in step with `lib_deps` in `firmware/platformio.ini` by
> hand. Nothing checks yet that the two agree - CI should, and does not.

## Dependencies

| Component | License | Used for |
|---|---|---|
| [arduino-esp32](https://github.com/espressif/arduino-esp32) | LGPL-2.1 | Framework: Wi-Fi, NVS, web server, DNS, mDNS, TLS, OTA |
| [ESP-IDF](https://github.com/espressif/esp-idf) | Apache-2.0 | Underlying SDK |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | FreeBSD (2-clause BSD) | Display and touch driver |
| [LVGL](https://github.com/lvgl/lvgl) | MIT | UI toolkit: every screen |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | MIT | Parsing printer reports, building commands |
| [WebSockets](https://github.com/Links2004/arduinoWebSockets) | LGPL-2.1 | Creality and Snapmaker transports |
| [PubSubClient](https://github.com/knolleary/pubsubclient) | MIT | MQTT transport: Bambu Lab, Elegoo, Anycubic |
| [PN532 / PN532_HSU](https://github.com/Seeed-Studio/PN532) (Seeed / elechouse) | Apache-2.0 | NFC reader driver — **vendored and patched**, see below |
| [ESP Web Tools](https://github.com/esphome/esp-web-tools) | Apache-2.0 | Browser installer |
| [mbedTLS](https://github.com/Mbed-TLS/mbedtls) | Apache-2.0 | TLS, bundled with ESP-IDF |
| [Font Awesome Free](https://fontawesome.com) | Fonts under SIL OFL 1.1, icon artwork under CC BY 4.0 | Every `LV_SYMBOL_*` glyph, plus the sun on the Display row, extracted into the `firmware/src/ui/font_ui_*.c` faces by `scripts/make-ui-font.sh`. The outlines come from the copy shipped inside the pinned LVGL package, so the library version pins them; no font file is committed. `lv_font_conv` extracts outlines from the font, so it is the OFL that governs what is compiled in. |
| [Montserrat](https://github.com/JulietaUla/Montserrat) | SIL OFL 1.1 | The UI typeface. Latin-1 and Latin Extended-A subsets extracted into `firmware/src/ui/font_ui_*.c` by `scripts/make-ui-font.sh`: the Medium weight from the copy shipped inside the pinned LVGL package, SemiBold from the typeface's own repository, cached outside the tree. No font file is committed. |
| [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) | SIL OFL 1.1 | The NFC tester's page dump, which has to line up in columns a proportional face cannot give. Eighteen glyphs - space, 0-9, A-F and x - extracted into `firmware/src/ui/font_ui_mono_16.c` by `scripts/make-ui-font.sh`, fetched from the project's own repository and cached outside the tree. No font file is committed. |

## The vendored PN532 driver

`firmware/lib/PN532/` will contain a **modified copy** of the Seeed/elechouse
PN532 library, kept in-tree rather than pulled from the registry.

**Why it is vendored:** the driver needs patches that are specific to this
hardware and cannot be applied to an upstream dependency — most importantly, a
wake-up preamble before every command, without which these modules answer once
after boot and then never again. See
[docs/MIGRATION.md](docs/MIGRATION.md#the-vendored-pn532-library).

**Obligations:** it is Apache-2.0. The upstream copyright notice, license text
and `NOTICE` are preserved in the vendored directory, and the modifications are
marked as such. Its own README will state clearly that it is modified, what was
changed, and why.

## LGPL components

Two dependencies are LGPL-2.1 (arduino-esp32 and the WebSockets
library). They are statically linked into the firmware image, which the LGPL
permits provided the user can relink against a modified version of the library.

That is satisfied here in the strongest way available: **the entire build is open
source.** The firmware source, the exact dependency versions and the build
configuration are in this repository, and CI reproduces the published binaries
from them. Anyone can substitute a modified library and rebuild.

## Trademarks

The **TigerTag** and **TigerSpool** names and logos are trademarks of the
TigerTag Project and are not covered by the MIT license. See
[TRADEMARK.md](TRADEMARK.md).

Bambu Lab, Creality, FlashForge, Snapmaker, Elegoo and Anycubic are trademarks of
their respective owners. TigerSpool is not affiliated with, endorsed by, or
sponsored by any of them. Their printer protocols are supported through
independent reverse-engineering, and their names are used only to state which
printers this project works with.

## Corrections

If a license here is wrong, or a component is missing, open an issue — we will
fix it.
