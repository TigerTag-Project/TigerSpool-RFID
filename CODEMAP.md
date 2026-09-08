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
| `webcfg.cpp` | Serves **two** web interfaces — the captive portal for setup, and a separate legacy configuration page over the LAN once provisioned — and has **two** independent Wi-Fi scanners, one async and JSON for the portal, one blocking and HTML for the legacy page. "Change the web page" or "fix the scan" usually means changing the wrong one. |
| `tt_db.cpp` | The TigerTag reference tables exist **twice**, and every lookup goes through here rather than through `tigertag_db.h`. The compiled tables are the floor - what a brand-new box knows offline - and the downloaded ones in LittleFS are preferred when they parse. The fallback is per table, not all-or-nothing, so a corrupt brand file does not throw away a good material file. Calling `tt_material()` directly bypasses the whole mechanism and looks identical until a spool from last month reads as `brand#48804`. |
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
printer session is `setInsecure()` and is far cheaper. Measured on this board,
free internal RAM from a 199 936 byte baseline:

| | cost |
|---|---|
| plain TCP session (Elegoo MQTT 1883) | ~3 KB, and ~0.6 KB for a second |
| TLS session, insecure (Anycubic MQTT 9883) | ~38 KB |
| two plain + one TLS, all open | 158 KB free, largest block 115 KB |
| **three TLS, all open** | **all three connect** - 79 KB free, largest block 64 KB |

So several printers connected at once is a question of how many are TLS, not a
wall. Three plain sessions are essentially free; three TLS ones fit and leave
enough for the UI, though not comfortably alongside an account sync.

## Landmines

Each row is something the code does not say about itself, and that a session
already paid for. Read the row before editing what it names.

### Generated data

| Where | What you need to know |
|---|---|
| `tools/gen_db.py` | **It cannot regenerate its own output.** It writes to `tools/include/tigertag_db.h`, the committed header lives in `firmware/include/`, and `tools/include/` does not exist — so it raises `FileNotFoundError` before writing a byte. The header looks maintained and is not. That the committed file cannot have come from the committed generator is directly visible: the script writes a Portuguese banner, the committed line 1 is English. |
| `tools/gen_db.py`, `esc()` | It escapes backslash and double quote, and nothing else. Everything else in the JSON goes straight into a C string literal. A newline breaks the build, which is safe. A non-breaking space, a NUL byte or a bidirectional override all **compile**: they produce a blank glyph on the panel, a silently truncated string, or source that displays to a reviewer in the wrong order. The dangerous inputs are exactly the ones that compile. |

### Text on the screen

| Where | What you need to know |
|---|---|
| `i18n.cpp`, the `STR` table | **It is positional.** The `static_assert` checks the number of *rows* against the enum and nothing else. It does not check that a row has the right number of entries, that the languages sit in the enum's column order, or that no entry is empty. A language block one column out of place compiles cleanly and mistranslates that entire language. A row one entry short zero-fills the rest, and a NULL reaches `lv_label_set_text`. |
| `enum Lang` (`i18n.h`) and `LANG_SCHEMA` (`i18n.cpp`) | The chosen language is stored in NVS **as an index**. Reordering or removing an entry silently changes what a stored index means, and the device comes back up speaking something else. `LANG_SCHEMA` exists for exactly this: bump it in the same edit and the stale index is discarded instead of misread. |
| Any string or label that reaches the panel | The faces cover Latin-1 and Latin Extended-A now, so accents draw — but **`0xA0` is excluded on purpose**, and everything above Extended-A still is. LVGL draws a missing glyph as a blank box and logs nothing, so nothing fails until a user sees it; `check-ui-fonts.py` is what catches it first. This includes **data**: reference labels are validated against the same range at generation time, which is how two brand names carrying a no-break space were found. |

### LVGL and the screens

| Where | What you need to know |
|---|---|
| `ui/frame.cpp` vs `screen_setup.cpp::addBack()` | **Two different back affordances exist.** The header one is a 56 px button on `LV_EVENT_CLICKED`: a slight drag between press and release cancels it, which is what made going back feel unreliable on a touch panel. `addBack()` is a full-width strip on `LV_EVENT_PRESSED` with no pressed-state highlight, because the screen is gone before a highlight could render. Copy the wrong one and back becomes hard to hit again. |
| `ui/frame.cpp`, screen swap | `lv_scr_load(new)` comes **before** `lv_obj_del(old)`. Deleting first deletes the screen that is still active, and LVGL faults inside the next draw. |
| `ui/screen_home.cpp` | `show()` runs from the main loop on every iteration, so it must be idempotent. It guards on a cheap signature of everything it renders, and **anything that changes what is displayed must be inside that signature** — visibility was once outside it, so hiding a printer changed nothing on screen. Rebuilding unconditionally destroys rows under the user's finger and reads as a frozen device. |
| Anything that loads a screen outside the state machine | Every screen keeps an "already showing, do not rebuild" flag. Code that swaps screens out of band — the screenshot preview route does — must invalidate all of them, or each guarded screen believes it is still displayed and stops redrawing. The device then looks frozen while the web server answers normally. |
| `webcfg.cpp`, and the web server generally | The screenshot handler pumps `lv_timer_handler()` for 400 ms from inside an HTTP handler, and reads the `canvas` sprite as a framebuffer. Both are safe **only because `server.handleClient()` is called from the same loop as the drawing**. Moving the web server onto its own task — which looks like an obvious improvement — has LVGL drawing from two threads, which faults inside its own draw, and serialises the sprite while it is being written, which returns a BMP with a torn band across it. |

### State, storage and the account

| Where | What you need to know |
|---|---|
| NVS printer keys, `main.cpp` | Keys are `p{i}t`, `p{i}n`, `p{i}h`, `p{i}s`, `p{i}c`, `p{i}v` — **a printer's identity is its array index**. Changing the order in which the account import fills the array reassigns every stored field to a different printer, including the user's per-printer visibility flag. There is no stable id anywhere. **Open bug**, not an accepted design: [2026-09-03](docs/reviews/2026-09-03-concurrency-and-identity.md). |
| Account sync, `tigertag_cloud.cpp` | The import is **deliberately not authoritative**, but only conditionally so. Locally-held host and access code win — and that preservation is gated on the imported printer having the same *type* at the same index. When the type at an index changes, name, host, serial and access code are all taken from the import wholesale, and the hand-corrected address is gone. `visible` is never written by sync at all. A stale IP is never auto-corrected either: clearing the field is how you force a refresh. |
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
