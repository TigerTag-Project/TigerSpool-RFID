# firmware/

The TigerSpool firmware: one PlatformIO project, one environment, `tigerspool`,
for the Waveshare ESP32-S3-Touch-LCD-2 with a PN532 reader.

## Layout

```
firmware/
├── platformio.ini          env: tigerspool
├── partitions.csv          two OTA slots, frozen - docs/OTA.md
├── include/                board and LVGL configuration, generated headers
│   ├── version.h           TIGERSPOOL_FW_VERSION, the one version macro
│   └── tigertag_db.h       generated from tools/tigertag_db - never hand-edited
├── lib/
│   └── PN532/              vendored reader driver - ONE copy, patches documented
├── tools/tigertag_db/      mirror of the public TigerTag reference tables
└── src/
    ├── main.cpp            the state machine and the only writable device state
    ├── printer.h           the backend interface and the printer record
    ├── printer_budget.h    how many printers at once - docs/CONNECTION-BUDGET.md
    ├── backend_*.cpp       one backend per brand, one object per printer
    ├── bambu_cloud.cpp     the TLS session every cloud Bambu on an account shares
    ├── reader.cpp          PN532 use, TigerTag decoding
    ├── tigertag_cloud.cpp  account sign-in, pairing, printer import
    ├── tt_db.cpp           reference-table lookups, compiled and downloaded
    ├── webcfg.cpp          captive portal, LAN page, /screen.bmp, /api/*
    ├── i18n.cpp            every on-screen string, in eight languages
    ├── net/                captive DNS, OTA, TLS helpers
    └── ui/                 LVGL screens, theme, icons, generated fonts
```

What each file owns, what it must not be asked to do, and the landmines to read
before editing it are in [../CODEMAP.md](../CODEMAP.md). This tree is the
overview; the map is the reference.

## Rules for this tree

**No credentials, ever.** Not Wi-Fi, not printer access codes, not API keys with
authority. Everything comes from NVS at runtime. The published binary is the same
for every user. See [../SECURITY.md](../SECURITY.md).

**One copy of the vendored library.** The prototype carries three, which have
already drifted into different behaviour —
[../docs/MIGRATION.md](../docs/MIGRATION.md#the-vendored-pn532-library). Patches
to a vendored library carry a comment saying what breaks without them.

**English only.** Comments, identifiers, log messages, commit messages.
User-facing strings go through the i18n layer (`i18n.cpp`), in every language
at once.

**Hardware facts get a comment explaining why.** Every constant that looks
arbitrary — a delay, a page range, a retry count — is arbitrary-looking because
it was expensive to find. Say what breaks if it changes.

## Build

```bash
pio run -e tigerspool                 # compile
bash ../scripts/flash.sh --monitor    # build, flash over USB, open the console
bash ../scripts/verify.sh             # the guards CI runs, plus a build
```

`flash.sh` checks the board's MAC before writing, so it cannot flash the wrong
device on a bench with several plugged in.

Most users will never do this — they install from the browser, and after that
the device updates itself over the air ([../docs/OTA.md](../docs/OTA.md)).
