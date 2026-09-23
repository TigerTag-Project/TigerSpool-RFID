# Code map

**The [Landmines](#landmines) table is this file.** Everything above it is the
short list of module facts that `ls` and `grep` do not give you; everything a
grep would have told you has been deliberately left out.

No line numbers are recorded here: nothing verifies them, and a number nothing
verifies is a number that lies. Grep for the symbol. If a file has outgrown what
this says, say so rather than working around it.

Build-level and hardware-level facts live in [CLAUDE.md](CLAUDE.md) and are not
repeated here.

## Modules worth a note

| File | What is not obvious |
|---|---|
| `main.cpp` | Owns the state machine and the only writable copy of device state — and also owns the display device itself (`lcd`, and the PSRAM `canvas` sprite that exists solely so `/screen.bmp` has something to serialise). It draws no widgets, and must not start: screens render from state passed in. It also carries an inline Creality LAN scanner, which is not where anyone looks for it. |
| `webcfg.cpp`, account page | Signed in, `/login` is the account page, and it **never syncs or switches anything itself**. Its handlers run on the loop task: a sync there nested a TLS handshake in a web handler, overflowed the loop's 8 KB stack on every email sign-in, and held the screen still for the whole sync. So a handler calls `ttcloud::requestSync()` and the loop starts the task; a printer switch is a mask read by `takePrinterSwitch()` and applied by `main.cpp` through `tryToggle()`, the device's own switch and budget check. `/api/account` answers anyone on the LAN, so it carries names, brands and addresses and never a serial or an access code. |
| `webcfg.cpp` | Serves **two** web interfaces — the captive portal for setup, and a separate legacy configuration page over the LAN once provisioned — and has **two** independent Wi-Fi scanners, one async and JSON for the portal, one blocking and HTML for the legacy page. "Change the web page" or "fix the scan" usually means changing the wrong one. |
| `tt_db.cpp` | The TigerTag reference tables exist **twice**, and every lookup goes through here rather than through `tigertag_db.h`. The compiled tables are the floor - what a brand-new box knows offline - and the downloaded ones in LittleFS are preferred when they parse. The fallback is per table, not all-or-nothing, so a corrupt brand file does not throw away a good material file. Calling `tt_material()` directly bypasses the whole mechanism and looks identical until a spool from last month reads as `brand#48804`. The material table keeps more than labels - `crealityID`, `crealityPressureAdvance` and the recommended nozzle window, in the download and in `TT_MATERIAL_INFO` - and `materialInfo()` answers from the same layer as `material()`. |
| `printer_budget.h` | Decides whether a printer may be **switched on**, in *load slots* - one is a kilobyte of internal RAM. It never sees memory: the prices are constants taken from `/api/memtest`, and `docs/CONNECTION-BUDGET.md` holds the measurements they must stay equal to. A new brand with no entry in `loadSlotsFor()` costs 0 and is never refused. "Load slot", never plain "slot" - a slot is a spool tray everywhere else in this code. |
| `backend_bambu.cpp`, external spool | **Read as ams 255 / tray 254, written as ams 255 / tray 0 / slot 0.** A write to tray 254 is accepted and ignored by current firmware (X1C 01.12). The first report after a new MQTT connection carries an empty external tray before the real one. |
| `bambu_cloud.cpp` | One MQTT/TLS session shared by **every** cloud Bambu on the account; `backend_bambu.cpp` attaches and detaches, ref-counted. Reports are routed by the serial in the topic and nothing else - a report is never handed to a printer it did not name. So one cloud printer's "connected" is the session's, and closing one printer does not close the session while another is attached. LAN Bambus do not use it. |
| `filament_resolve.cpp` | Decides what a printer is sent for a spool - temperatures, material family, Creality material id, Bambu filament id, pressure advance, name - field by field from the TigerTag+ product endpoint, the chip, the material table, a default. **Pure**: no network, no Arduino, the endpoint's answer is handed in, so it is tested on a computer with injected answers. The chip never carries a printer's material id; only the endpoint and the table do. A plain TigerTag never gets the endpoint's answer considered, even if one is passed. |
| `product_api.cpp` | Fetches the product endpoint on a task, **started when a TigerTag+ is read** (for a Creality today), not when Send is pressed; Send waits only while that fetch is inside its 3 s budget. One cached answer, by product id. It asks main.cpp to stand background links down only if the heap was short of a handshake when it started: standing them down cost nothing during the request and 800-900 ms of loop AFTER it, while they dialled back. `/api/resolve` runs the whole resolution from query parameters without a spool and without sending. |
| `tigertag_cloud.cpp` | Its network calls are on a **mixed** regime, not a uniform one. The sync and the pairing start run on their own FreeRTOS tasks; `pairPoll()` and `signInWithCustomToken()` are called straight from the main loop and stall it for about a second each. Neither pattern is the rule, so check which one a call is on before adding another. |

### One board, three TLS talkers - and what it actually costs

The account sync, the firmware check and the reference-table update each open
their own `WiFiClientSecure`. Two at once is fine; **three is not** - the third
gets a refused connection. That is why the table update runs on its own clock
ninety seconds after boot rather than beside the update check, and skips while
either of the other two is talking. It was measured, as `HTTP -1` on every
attempt.

**Do not generalise that to printer sessions.** Those three verify against the
Arduino core's root CA bundle, and parsing it is most of what they cost. A
printer session is `setInsecure()` and is cheaper - but a TLS printer still
takes 40-48 KB of internal RAM against 2-6 KB for any other, and the device
holds about three TLS sessions before fragmentation stops the fourth. Every
measurement, and the load budget that enforces it (`printer_budget.h`), is in
[docs/CONNECTION-BUDGET.md](docs/CONNECTION-BUDGET.md). A one-off TLS request
beside three printer sessions gets room from the TLS stand-down in `main.cpp`,
not from the budget.

## Landmines

Each row is something the code does not say about itself, and that a session
already paid for. Read the row before editing what it names.

### Generated data

| Where | What you need to know |
|---|---|
| `tools/tigertag_db/gen_db.py` | **It refuses a label the panel cannot draw.** Every label is checked against the compiled font range before a byte is written, and the build stops on the first one outside it. Exactly two things are repaired instead of refused - a no-break space becomes a space, a superscript becomes its digit - and both are printed as warnings so the upstream defect stays visible. Adding a repair is a decision about what a brand name IS; see the comment above `NBSP` before making one. |

### Text on the screen

| Where | What you need to know |
|---|---|
| `i18n.cpp`, the `STR` table | **It is positional.** The `static_assert` checks the number of *rows* against the enum and nothing else. It does not check that a row has the right number of entries, that the languages sit in the enum's column order, or that no entry is empty. A language block one column out of place compiles cleanly and mistranslates that entire language. A row one entry short zero-fills the rest, and a NULL reaches `lv_label_set_text`. |
| `enum Lang` (`i18n.h`) and `LANG_SCHEMA` (`i18n.cpp`) | The chosen language is stored in NVS **as an index**. Reordering or removing an entry silently changes what a stored index means, and the device comes back up speaking something else. `LANG_SCHEMA` exists for exactly this: bump it in the same edit and the stale index is discarded instead of misread. |
| Any string or label that reaches the panel | The faces cover Latin-1 and Latin Extended-A, so accents draw — but **`0xA0` is excluded on purpose**, and everything above Extended-A is not, **except the Chinese characters the source already uses**: the faces carry a Noto Sans SC subset of exactly those (`scripts/cjk-chars.py`), so a Chinese string with one new character needs the faces regenerated before it can draw. LVGL draws a missing glyph as a blank box and logs nothing, so nothing fails until a user sees it; `check-ui-fonts.py` is what catches it first. This includes **data**: reference labels are validated against the same range at generation time, which is how two brand names carrying a no-break space were found. |

### LVGL and the screens

| Where | What you need to know |
|---|---|
| `ui/frame.cpp` vs `screen_setup.cpp::addBack()` | **Two different back affordances exist.** The header one is a 56 px button on `LV_EVENT_CLICKED`: a slight drag between press and release cancels it, which is what made going back feel unreliable on a touch panel. `addBack()` is a full-width strip on `LV_EVENT_PRESSED` with no pressed-state highlight, because the screen is gone before a highlight could render. Copy the wrong one and back becomes hard to hit again. |
| `ui/frame.cpp`, screen swap | `lv_scr_load(new)` comes **before** `lv_obj_del(old)`. Deleting first deletes the screen that is still active, and LVGL faults inside the next draw. |
| `battery.cpp` (is there a cell) | **The user says so.** There is no detection left, because none of it worked: the board has no sense line, the rail with an empty connector sits anywhere from 4.05 to 4.27 V depending on the board, and the charger's ripple - which a healthy cell damps - is not damped by a tired one. Measured on our two boards, the ripple test was wrong in both directions: one reported a battery it did not have, the other denied the pack it was running on. `battery::declare()` writes the answer to NVS (`bdecl`) and `present()` is that answer, full stop. Everything downstream - the percentage, the runtime, `battery_present` in the heartbeat - follows it. |
| `battery.cpp` (charging) | Charging is not a fact this board reports - there is no status line - so it is inferred from the SHAPE of the curve: a step of 5 mV at the pin between two readings is a cable moving, and over three minutes a rise of 3 mV means charging while it takes a fall of 6 to say otherwise. That asymmetry is deliberate: a nearly full cell on a charger is flat, and judged symmetrically it reads as a discharge. The charger's contribution to the voltage is measured from that same step and kept in NVS, because it is not a constant - it shrinks as the charger tapers, and a fixed value put a nearly full cell fifteen points low. |
| `battery.cpp` (the pin) | The ADC on GPIO5 measures the battery **rail**, not the battery, through a 200K/100K divider (so times three). On USB that rail is held up by the charger whether a cell is plugged in or not - which is why presence is declared rather than read, and charging is inferred from the curve rather than reported. |
| `ui/signal_level.h` | The dBm-to-arcs scale, header-only and free of LVGL so it can be compiled and run on a computer - which is how the hysteresis was checked, rather than by waiting for a signal to wobble at the right moment. It is written **twice**: here for the panel and as JavaScript in `net/portal_page.h` for the portal's network list. `scripts/check-signal-scale.py` fails when the two disagree. |
| `ui/lvgl_port.cpp` | LVGL runs in a task of its own, core 1, priority 2 - it draws and reads the touch panel while the main loop is blocked inside a socket, which is what a TLS handshake to a printer that is switched off does for 1.3 s. Two consequences. **LVGL is not reentrant**: every public function in `ui/` takes `lvgl_port::Lock`, and so does anything in `webcfg.cpp` that touches it. **Flags set from an LVGL event callback are set on that task** and read on the loop, so they are `volatile` - `s_tapped`, `s_back` and their kind. `lvgl_port::loop()` no longer pumps LVGL when the task is running; it yields, so a screen just built is on the glass before the loop moves on. |
| `ui/screen_home.cpp` | `show()` runs from the main loop on every iteration, so it must be idempotent. It guards on a cheap signature of everything it renders, and **anything that changes what is displayed must be inside that signature** — visibility was once outside it, so hiding a printer changed nothing on screen. Rebuilding unconditionally destroys rows under the user's finger and reads as a frozen device. |
| `ui/screen_home.cpp`, the two faces | One screen, two faces: `showMain()` draws the choice (printers or reader) and `show()` draws the printer list, into the SAME container and header. Each keeps its own redraw signature, so the face itself is in the test - without it a face whose signature had not changed since it was last shown declines to rebuild and the other face's rows stay on screen. `headerFace()` owns what the header shows: the account and Wi-Fi icons are on the choice only, because a chevron, a title and three icons do not fit across 240 px. |
| `ui/screen_read.cpp` | Reader mode, and NOT the NFC tester (`screen_settings.cpp::showReader`). This one answers "what is this spool"; the tester answers "what is on this chip", field by field, and keeps every id and the raw pages. Adding a field here is a decision about the first question, not a free addition. |
| `tigertag_cloud.cpp`, presence | The device's own document in the account, `users/{uid}/tigerspools/{mac}`. The identity, liveness and power field NAMES are shared with the TigerScale on purpose - Studio renders both from one template - so they are not renamed for local taste. `docs/PRESENCE.md` is the contract, including the Firestore rule the account needs before any of it is allowed. The document id is the Wi-Fi MAC and its format is frozen. |
| Anything that loads a screen outside the state machine | Every screen keeps an "already showing, do not rebuild" flag. Code that swaps screens out of band — the screenshot preview route does — must invalidate all of them, or each guarded screen believes it is still displayed and stops redrawing. The device then looks frozen while the web server answers normally. |
| `webcfg.cpp`, and the web server generally | The screenshot handler renders a preview screen, forces a repaint, and reads the `canvas` sprite. It is **not** on the same thread as the drawing any more - the interface has its own task (`lvgl_port`) - so every one of those touches `lvgl_port::Lock` first. The rule that replaced "same loop, therefore safe": anything outside the drawing task that calls into LVGL, or reads what LVGL paints, takes that lock. Miss one and the fault is a torn screenshot on a good day and a corrupted object tree on a bad one. |

### State, storage and the account

| Where | What you need to know |
|---|---|
| NVS printer keys, `main.cpp` | Keys are positional - `p{i}t/n/h/s/c/d/u/m/k/v/a`, eleven per printer - but **a printer is recognised by what it is, not where it sits**: `samePrinter()` in `tigertag_cloud.cpp` (type, serial with FlashForge's optional `SN` dropped, then address or mode). The import matches every printer to its stored entry that way and moves what the device owns - the switch `v`, a discovered host - with it. Anything else that reads or writes `p{i}*` must not assume position `i` is still the same machine after a sync; `printerIdx` is remapped by the import for that reason. `main.cpp` links notice a change of settings under them by `cfgSig()` and reconnect. |
| Wi-Fi association, `main.cpp` | **Never call `WiFi.begin()` directly** for the home network - call `staBegin()`. The driver's default scan stops at the first access point with the right name and caches that radio for good, so on a multi-AP network the device stays on whichever it heard first: measured at -79 dBm with the same network at -46 one room away. `staBegin()` erases the cached association and connects by signal; the setup portal's join sets the same scan and sort without erasing (the phone is on the device's own AP). `/api/scan?all=1` lists every radio the device hears, with BSSID and channel. |
| Account sync, `tigertag_cloud.cpp` | **The account owns** a printer's name, serial, access code and Anycubic fields; an empty account field keeps what the device had. **The host is shared:** LAN discovery (`reconcile()` in `main.cpp`) may correct a K2's address, and that correction stands only while the account keeps giving the address it gave last time - `p{i}a` remembers it. When the account says something new, it wins. Two documents for one printer are merged, newer `updatedAt` first (integer or Firestore timestamp; unreadable counts as oldest). A printer new to the device arrives switched off. |
| The two FreeRTOS tasks | `ttSync` (16 KB) and `ttPair` (12 KB) are the only threads in an otherwise single-loop firmware. They signal completion through bare `volatile bool`, and **there is no mutex anywhere in `firmware/src/`**. The sync task writes the `tigerspool` NVS namespace while the UI loop reads it through `loadCfg()`. Nothing serialises that. `loadCfg()` reads six string keys per printer in sequence, so a sync landing between two of them yields a printer whose host came from the new import and whose name came from the old — a row on the home screen that matches no printer that exists. **Open bug**, not an accepted design: [2026-09-03](docs/reviews/2026-09-03-concurrency-and-identity.md). |
| Factory reset, `main.cpp` | It must clear **all four** NVS namespaces, current and legacy: clearing only the current one leaves the legacy data, and the migration path restores the user's whole configuration on the next boot — a factory reset that quietly undoes itself. The list of four is written out **twice**, in `main.cpp` and in `webcfg.cpp`, with nothing tying them together. Add a namespace to one and the other reset silently stops being a reset. |
| The Firestore fetch, `tigertag_cloud.cpp` | The server-side `mask.fieldPaths` and the client-side `Filter` + `NestingLimit(40)` look redundant and are **independent**: the mask cuts bytes, the filter cuts nesting depth. Each is explained in a comment beside itself, forty lines from the other, so removing one because "we already filter" fails the other way — as `TooDeep`, or as a 47 KB body for one brand. |
| Every HTTPS call | All of them use `setInsecure()`: sign-in, refresh, pairing, Firestore. No certificate is verified anywhere, so anyone on the local network can read the TigerTag refresh token and the account credentials off the wire. Acceptable on a bench, and a release blocker. |

### Build and release

| Where | What you need to know |
|---|---|
| `platformio.ini`, flash settings | `memory_type` and `flash_mode` are a **per-board** fact, not a house rule. A sibling project mandating different values for a different board is not a precedent to copy: the wrong pair here produces a boot loop, or a board that never finds its PSRAM and dies in the first LVGL allocation. |
| Release assets, `boot_app0.bin` | Shipped by the Arduino core, not built here, and its path inside the package tree is not ours to depend on. Locate it; never hard-code it. A release that omits it produces a device that boots the wrong slot after its first OTA. |
| The documentation guards | They live inline in `.github/workflows/build.yml`, not in `scripts/`, so they cannot be run locally today. The GPIO6/7 one fires when a pin number and a signal name land on the same line: it cannot tell a wiring instruction from a description of itself, and has already failed on a document explaining what it checks. It also passes silently on an empty input set, and greps untracked build directories. A guard that cannot fail, or that fails on something correct, is one people learn to bypass. |
