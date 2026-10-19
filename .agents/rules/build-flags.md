# Build flags that must stay

**Rule.** Keep `board_build.arduino.memory_type = qio_opi`, `-DARDUINO_USB_MODE=1` and `-DARDUINO_USB_CDC_ON_BOOT=1` in `firmware/platformio.ini`.

**Why.** Without `qio_opi` the octal PSRAM is never initialised and every LVGL allocation returns NULL: a crash inside LVGL whose backtrace names neither PSRAM nor the flag. Without the USB flags UART0 reclaims GPIO43/44 and the console talks over the reader.

**How.**
- Board: Waveshare ESP32-S3-Touch-LCD-2, ESP32-S3R8, 16 MB flash, 8 MB octal PSRAM.
- Two environments: `tigerspool` (built and released by CI) and `tigerspool-bench` (adds `TIGERSPOOL_BENCH`).
- The CI cache is keyed on `platformio.ini`; a build that fails only in CI usually means a stale local `.pio`.
