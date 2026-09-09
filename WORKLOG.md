# Work log

Everything done since the last commit, in Keep a Changelog's headings, so a
release entry is synthesised from this file rather than re-derived from a diff.

**Append the moment a change is done**, not in a batch at the end. Written at the
end, this file is reconstructed from the diff — which is the exact thing it
exists to prevent.

**Describe the end state, not the journey.** An "Added X" and a later "Fixed X"
from the same cycle collapse into one entry. Anything reverted disappears
entirely: it never shipped.

At each checkpoint, synthesise this file into one line, use that as the commit
message, and reset it to this header.

---

## Unreleased

## 2026-09-05 - the update screen, and the header everywhere

### Changed

- The update page checks on entry. The button stays for offline and for retry.
- A 152 px progress ring, percentage inside, no header and no exit while the
  image is being written.
- The header is the ground plus a rule, not a filled bar. One style, so every
  screen changed together.
- Installed version as a settings row; the state as a glyph in a coloured ring.
- Removed the channel row and the now-dead `S_CHANNEL` key.

### Fixed

- `scripts/flash.sh` was broken without `--port`: empty array under `set -u`.

Verified on hardware: home, settings, update (checking / up to date), Wi-Fi
setup and Wi-Fi settings, driven over `/api/tap` and read back from
`/screen.bmp`.

### Changed (same cycle, folded into 1.5.0)

- `theme::WARN` added. Restart orange, factory reset red, and the Settings
  Update row orange when a version is waiting.
- One OTA check twenty seconds after boot, so the menu can show that.
- Update page: `v` prefix, smaller badge (62 px, proportionate to 240 px),
  "Wi-Fi, account and printers are kept" under an available update.
- Wording taken from the scale after reading its live view directly:
  "Installed version", "Your TigerSpool is up to date".

Answers from the TigerScale agent are in `_internal/TIGERSCALE-UI-ANSWERS.md`.
Still open from them: row icons (the scale carries the colour on a 26 px icon
and keeps the label white - we carry it on the label, having no icons), and the
font fallback chain that would restore accents.

- Settings rows now have icons. `frame::row` takes an optional LV_SYMBOL_* and
  a tint; the value's width cap drops from 108 to 84 when there is one, because
  the 28 px icon column was pushing the chevron off the row. `showMenu` takes a
  `MenuState` struct - four adjacent bools as positional arguments is a swap
  waiting to happen.

- Row icons are now drawn from LVGL primitives in `ui/icons.{h,cpp}` rather than
  taken from `LV_SYMBOL_*` where LVGL has no glyph for the thing: a person for
  the account (an envelope says "messages"), a globe for the language (a
  keyboard is not a language), a printer, a sun. The globe uses the
  TigerScale's own 22 px coordinates verbatim. About a kilobyte of code and no
  data - no font to generate, no licence to carry, nothing for a guard to
  police. The remaining four rows keep LV_SYMBOL_*, which is the right answer
  where LVGL already has the shape.
- Colour rule tightened to the scale's: an icon is plain white unless it says
  something worth seeing without reading the row. Three tinted in the healthy
  case, not six.

### Fixed

- The Settings menu showed an empty Wi-Fi network. `WiFi.SSID()` and
  `ttcloud::email()` both return String BY VALUE, and moving the call into a
  MenuState struct meant the temporaries died before the struct was read. It
  never crashed - it just looked like a network problem. Held in named locals.

## 2026-09-05 - the icons, from the scale's actual code

- `CI_USER` rebuilt verbatim: two solid discs, no outline, the shoulders clipped
  by the box. My reconstruction had used outlines and could not have matched.
- Printer and sun redrawn against the scale's silhouette rules.
- Recorded for the font work: the scale's sun is FontAwesome 6.5.2 Free Solid
  U+F185, fetched at generation time from the pinned tag, never committed. It
  rides in the generated Latin face rather than a face of its own -
  `lv_font_conv` takes several `--font` in one call. The FA font files are
  SIL OFL 1.1 and the icons CC BY 4.0; both get cited. Note that the resulting
  `Opts` line carries two `-r`, which is a trap for `check-generated.py`.
- Also from that exchange: a drawn icon costs one lv_obj per stroke in RAM for
  as long as the screen is loaded. Negligible on a settings row that exists
  once; not negligible on a list that can hold twenty. Answers in
  `_internal/TIGERSCALE-UI-ICONS-2.md`.

- The sun is now the real glyph rather than a drawing. `make-icon-font.sh`
  registered with `check-generated.py`, which had to learn that a generator is
  not always Python - it ran everything through `sys.executable` and would have
  reported "failed to run" for the wrong reason. The .ttf is cached under
  `.cache/fonts/`, keyed on the tag, so the guard re-running on every verify
  does not mean a download on every verify.
- Attribution added: Font Awesome fonts are SIL OFL 1.1, the icon artwork
  CC BY 4.0. lv_font_conv extracts outlines from the .ttf, so the OFL is what
  governs the compiled result.
- **When the Latin face is generated, fold this glyph into it and delete the
  script.** One call, several `--font`, no second face and no extra link in the
  fallback chain - and watch that the resulting `Opts` line carries two `-r`,
  which a naive generated-file check reads as one.

- Printer icon reverted to its first geometry. Widening the body and fattening
  the output tray followed the scale's "one dominant form" rule, and on the
  glass it read worse: the heavy block at the bottom took over and the printer
  stopped looking like a printer. A rule about silhouettes is not a substitute
  for looking at the thing.

## 2026-09-05 - orientation, row style, account name, a clickable live view

- `screenRotation` stored in NVS as `rot`, applied after `lvgl_port::begin()`
  so the boot logo and everything after it agree. `lvgl_port::setRotation()`
  keeps the panel in the port; main owns the value.
- `theme::LINE` is now the scale's `0x2E3646` and rows carry a 1 px border of
  it; radius 7 -> 9; row chevron 12 -> 24 px, back chevron 20 -> 24. The value
  cap drops to 74 with an icon, because the icon column and a full-size chevron
  together cost about 55 px of a 240 px row.
- `ttcloud::displayName()` - the name when there is one, the address otherwise.
  `accounts:signInWithPassword` returns `displayName`; the QR pairing path does
  not, so `fetchProfileName()` fills it from `accounts:lookup` on the next sync
  when it is missing. That is what makes it appear on a device that was already
  signed in.
- `/screen` forwards clicks to `/api/tap`, drags over 12 px as swipes.
  Coordinates come from the image's bounding rect, so it works on a phone where
  the image is scaled down.

- Wi-Fi strength on the home header, left of the gear. Four levels by colour;
  the level, not the dBm, goes into the redraw signature.

### Fixed

- Captive portal not opening on Android (reported on a Galaxy S24, iPhone on
  the same firmware was fine). `startBackgroundScan()` moved out of the last
  line of `beginAP()` and into `handlePortal()`. The AP was channel-hopping
  during the exact second Android probes for a portal. Two comments in this
  file already said scanning destabilises the AP; the call site contradicted
  them and nothing connected the two.
- **Not reproduced here** - no S24 on the bench, and the mechanism is a timing
  window that needs a real Android probe. Diagnosed from the code path and the
  iPhone/Android asymmetry. Needs the reporter to confirm.
- First boot: `startConfigAP()` draws the QR before calling `beginAP()`, and
  `beginAP()` no longer calls `doScan()`. Those two together were the several
  seconds of frozen language screen. `doScan()` stays for `/?rescan=1`.
- `beginAP()` now raises WIFI_AP rather than WIFI_AP_STA. The station interface
  was only there for the scan that no longer runs, and the file already said
  AP-only is the stable one.

## 2026-09-05 - the sign-in page

- `handleLogin()` rebuilt. Mark at the top, two fields, an eye on the password,
  the Google mark, a Tiger Studio Manager button, three community marks, and a
  legal line whose version comes from `TIGERSPOOL_FW_VERSION`.
- `scripts/make-web-asset-header.py` turns a file into a PROGMEM C string;
  `firmware/include/web_assets.h` holds the tiger icon and `/tiger-icon.svg`
  serves it. Registered with `check-generated.py`. +0.5 points of flash.
- Five new words in four languages, and W_TT_LOGIN reworded from "Connect and
  import" to "Sign in" - a button says what it does, not what follows.
- Inputs are 16 px on purpose: below that, iOS Safari zooms the page on focus.
- The last character of a typed password showing briefly is the phone's own
  behaviour, on every site. It is not controllable from a page, and the usual
  workaround (a text field with -webkit-text-security) is worse: it breaks
  password managers and puts the plaintext in the DOM.
- `pageOpen()`/`pageClose()` extracted so the sign-in and pairing pages cannot
  drift apart; the Google mark, the Studio link and the social row are PROGMEM
  blobs shared by both.
- `webcfg::webPairing()` reports a live web-initiated pairing; main draws
  `screen_setup::showPairing()` from it in a new `ST_WEB_PAIR`. Read by main,
  never pushed by webcfg - a screen driven from two places disagrees with
  itself after the next redraw.
- Two bugs found by looking at the rendered page: the pairing page emitted two
  `<!doctype html>` and two `<head>`, and the Google SVG was sized only on the
  white button, so on the orange one it filled the screen.

## 2026-09-05 - the setup AP is encrypted

- `AP_PASS` from the MAC, `WiFi.softAP(AP_SSID, AP_PASS, ...)`, QR payload
  `WIFI:T:WPA;S:...;P:...;;`, key shown under the SSID, `webcfg::apPass()`.
- `docs/WIFI-PROVISIONING.md` argued FOR the open AP. Rewritten rather than
  left to contradict the code - the argument it made was wrong on its own
  terms, because the QR format has always been able to carry a key.
- The captive-portal timing fix in 1.6.0 stands on its own, but this is the
  cause the reporter identified. Still needs the S24 to confirm.

## 2026-09-05 - the captive portal, third time

- Root cause found and TESTED, not guessed: the core `DNSServer` hardcodes
  `answerType = DNS_TYPE_A` in `replyWithIP()`, so AAAA queries get a
  type-mismatched response. `net/captive_dns.{h,cpp}` written to replace it.
- Verified with `dig` against the running device, by temporarily binding the
  resolver to port 5354 in station mode: A -> NOERROR/1 answer/60s TTL, AAAA ->
  NOERROR/0 answers. Hook reverted before commit.
- `screen_setup::showPortalReady()` + `webcfg::apClients()` drive the fallback
  QR in ST_AP. Drawn on the transition only - encoding a QR is the expensive
  part of that screen.
- The two earlier theories were both real defects and both stand: the AP no
  longer scans during the probe window (1.6.0), and it is WPA2 rather than open
  (1.8.0). Neither was the cause. Said plainly here so nobody re-litigates them
  as failed fixes.

### Fixed

- Empty network picker on first open. `startBackgroundScan()` and
  `handleApiScan()` both issued a scan immediately after raising WIFI_AP_STA;
  the scan fails if the station interface is not up yet. 80 ms settle plus one
  retry, and `if (n < 0) n = 0` no longer turns a failure into an empty result
  - the JSON carries `error` instead.

### Fixed

- Empty network picker, properly this time. The scan is not failing, it is
  succeeding with zero results because the radio is busy serving the associated
  client - so no error flag would ever have caught it. `startBackgroundScan()`
  back at the end of `beginAP()` (async), `harvestScan()` called from
  `webcfg::loop()` so the result is kept without waiting for a request, and
  `netsJson` served from there. WIFI_AP_STA is the resting mode in AP; the
  three `WiFi.mode(WIFI_AP)` drop-backs are gone.
- Proven from the boot log rather than inferred: `scan cached: 18 network(s)`
  appears before any client can associate.

## 2026-09-05 - the update notice, and a warning that was ours to remove

- `ST_UPDATE_NOTICE` + `screen_settings::showUpdateNotice()`. Entered once per
  boot, only from ST_PRINTER, only when the boot check found AVAILABLE.
  A_INSTALL_NOW calls `ota::applyAsync()` directly - they already pressed
  Install once.
- Google button: `<form method=POST>` -> `<a href>`, route no longer POST-only.
  Safari warns on any form over HTTP; that one submitted nothing.
- The warning on the e-mail form is truthful and left alone. Removing it would
  mean HTTPS, and a self-signed certificate on a device would trade a true
  warning for a scarier one.
- Update notice: `S_UPDATE_KEEPS` removed from it. Still on the update page.
- OTA check on a 6 h timer, first at 20 s. Notice gated on `notifiedVersion`
  rather than a once-per-boot flag. Verified by temporarily shortening the
  interval to 40 s and watching three checks land in the serial log at 17 s,
  55 s and 95 s; the interval was restored before commit.
- `webcfg::pairTick()` polls `ttcloud::pairPoll` from main's ST_WEB_PAIR loop.
  The meta refresh on the wait page was the only thing driving the poll, and
  iOS suspends background tabs. Measured at two minutes on a real iPhone for an
  approval that had already happened.- `frame::build(nullptr, ...)` created the title label anyway, so LVGL's
  placeholder "Text" showed in the header during the OTA download.
- `Signal` row on the Wi-Fi settings screen, from `WiFi.RSSI()`. Added to
  answer "why is the icon orange" and worth keeping: the bench unit reads
  -78 dBm while a scan sees the same SSID's beacon at -55, which is the
  difference between the beacon of the nearest access point and the live
  association - a multi-AP network, or the printed case.
- `check-ui-translated.py` now judges what is left after format specifiers word
  by word instead of whole, so `"%d dBm"` passes on dBm being a unit while
  `"Signal %d dBm"` still fails on Signal. Verified both ways.
- v1.16.0 is a dead tag: the pre-commit hook rejected the commit, the tag was
  pushed anyway, and its release workflow refused it in 11 s on the tag/macro
  mismatch. Nothing was published under it and it is left alone - the rule is
  not to re-point a pushed tag. Released as 1.17.0.

## 2026-09-05 - the legacy page, and two things it was hiding

- `routes()` registers once; `/` and `handleCaptive` decide on `apMode` at
  request time. Serving the old form to the captive portal was the symptom.
- No screen sleep in ST_LANG / ST_WIFI / ST_AP / ST_ACCOUNT / ST_WEB_PAIR.
- Deleted `page()`, `handleRoot()`, `handleSave()`, `handleReset()`,
  `handleRetry()`, `doScan()`, `load()`, `apScan`. `/reset` in particular was
  an unauthenticated GET that cleared every NVS namespace.
- The W_ table keeps its now-unused rows: the order is checked against the
  enum, and renumbering it to save a few hundred bytes is not worth the risk.
- Update page: `S_UPDATE_KEEPS` removed from it, spacer 26 -> 12, badge gap
  14 -> 8, body scrollable. The AVAILABLE layout was 20 px over 276 and the
  Install button was clipped.
- `?preview=setupdate` now renders AVAILABLE rather than IDLE. The old preview
  showed the one state that was already fine.
- Printer picker: `visible` dropped from the redraw signature, `onToggle`
  flips the switch widget in place. Rebuilding to reflect a value is the
  recurring bug in any long-lived UI - the TigerScale warned about exactly
  this - and here it cost the scroll position on every tap.

## 2026-09-05 - never rebuild a screen to show a value

- Audited every redraw signature. `showUpdate` hashed `percent`, `showWifi`
  hashed `rssi`, `showFactory` hashed `holdPercent` - all three change many
  times a second. Each keeps its widgets now and writes into them.
- Rule added to AGENTS.md's settled table, with the two costs already paid.
- `/screen`: `lvgl_port::frameCounter()` increments in the flush callback,
  `/screen.ver` serves it, the page polls that at 120 ms and fetches the 150 KB
  bitmap only on a change.
- Still on the list: `showMenu` and `showAccount` rebuild when a background
  sync changes what they show. That is a real content change rather than a
  value, so it is correct today - but the settings menu scrolls, and a sync
  landing while someone is scrolled down will move them. Worth the same
  treatment when it next matters.
- `showMenu` builds once and writes into four value labels and four icons;
  `icons::tint()` recolours a glyph or a set of drawn strokes in place. The
  update path checks `frame::screen()` before writing - the pointers do not
  survive another screen, and writing into freed LVGL objects is a crash.

## 2026-09-05 - the board knows which way up it is

- `imu.{h,cpp}`: QMI8658 at 0x6B, accelerometer only, gyroscope left off.
- Both facts measured on hardware rather than assumed. An I2C scan: GPIO6/7
  empty, touch bus carries 0x15, 0x6B, 0x7E. And the axis - Y was the obvious
  guess and reads ~50; gravity is on X, -9360 with the device upright in its
  printed holder. Threshold 4000 counts, about a quarter of a gravity, so a box
  lying flat answers "cannot tell" instead of guessing.
- First boot uses it once, when `rot` has never been stored. `autorot` is the
  new key. Rotate button on the language header sets a fixed angle and clears
  autorot; Display offers Auto / 0 / 180.
- NOT verified here: I cannot turn the device over. The reading is correct in
  its current orientation and the thresholds are measured, but somebody has to
  hold it upside down.

## 2026-09-06 - "connected permanently", and what that actually means

Answers in `_internal/TIGERSCALE-CLOUD-ANSWERS.md`. The scale separates three
things people call being connected, and the distinction settles the request:

- **A, a session that is always ready to read and write.** Verified present in
  this firmware: `refresh`/`uid` in NVS and loaded by `ttcloud::begin()`;
  `ensureToken()` is lazy - it returns immediately under 50 minutes and is
  called at the head of each authenticated operation, no timer; the periodic
  sync runs on a 16 KB task, not the UI loop. So A is done.
- **B, being told about changes.** The TigerScale does not have this AT ALL. No
  push, no Listen, no SSE, no RTDB. What looks live in Tiger Studio is a LOCAL
  WebSocket on the LAN with the scale as server - nothing to do with the
  account.
- **C, a heartbeat.** The scale PATCHes telemetry every 30 s / 5 min. We have no
  write path at all.

Done here: refresh on opening the printer picker - his recommendation 1, and
90% of the benefit for a few lines.

Not done, and it needs decisions rather than code:
- **Who owns each field.** The scale has no write conflicts because it never
  writes a field Studio owns; `displayName` is read and never written. His
  strongest advice, and it comes before the first line of any write path.
- **Who writes the signal, and when.** Any push mechanism - command queue or
  RTDB - is inert until something in the backend writes to it.
- A blocking `syncNow()` still runs from four webcfg handlers, and the web
  server is polled from the main loop, so those stall the UI for a second or
  two. Not new, and only on a page the user is already waiting on.

## 2026-09-06 - INCIDENT: firmware written to the wrong board

TigerSpool firmware was flashed onto the bench TigerScale four times.

**What happened.** Both boards were plugged into the same Mac. The TigerSpool's
port (`/dev/cu.usbmodem141401`) disappeared and a different port
(`/dev/cu.usbmodem12101`) was present, so it was assumed to be the same device
re-enumerated. It was the TigerScale. `esptool read_mac` afterwards:
`dc:b4:d9:24:99:18` is the TigerSpool, `20:6e:f1:9a:17:b4` is the TigerScale.

**What it cost beyond the TigerScale.** Every build in that window went to the
wrong board, so the TigerSpool kept running older firmware - and the "account
icon renders as a dot" bug chased for an hour did not exist. What was on screen
was the OLD sync dot, on a device that had never received the new code. A
question was sent to the sibling project about a bug that was never real.

**The fix.** `flash.sh` now reads the MAC with `esptool --no-stub read_mac`
before uploading, matches it against `.bench-mac` (gitignored), and picks that
port among all of them. No match, no write. `--any` overrides deliberately.
With no `.bench-mac` and one board it proceeds and prints the line to record;
with several it refuses and lists them.

**The lesson, and it generalises.** A serial port name is not an identity - it
is whatever the OS handed out this time. Anything destructive addressed by a
name the OS chooses needs to verify what is on the other end first.

### Added

- Account health on the home screen. `ttcloud::health()` returns 0..3 and green
  asserts that the LAST EXCHANGE succeeded: `g_lastOkMs` is stamped by every
  successful token refresh and sync, with a 12 minute TTL - two missed syncs.
- `theme::BUSY` (#2F7FFF), the fourth state colour.

## 2026-09-06 - the printer workflow, measured

Benoit's proposed workflow: sessions held open continuously to every active
printer, for live slot contents. Measured before building any of it, and the
measurement said not to.

- `backend->begin()` blocks **0 ms**. It starts a connection, it does not wait.
- On a reachable printer (Ender-3, Creality), `connected()` goes true after
  **264 ms**. There is no latency problem to solve.
- The "long loading" was a printer that is switched off. The grid appears
  instantly with the right slot labels, then waits for contents that never
  arrive, and says nothing about it. Persistent connections would not have
  changed that by one millisecond.
- And they do not fit: `MAX_PRINTERS` is 8, the Bambu backend holds a
  `WiFiClientSecure` plus a **51 200 byte** MQTT buffer, so one Bambu session is
  80-100 KB of internal RAM against 320 KB total with 37 already in LVGL's draw
  buffer. Two would be tight. Eight is impossible.

Still to do on that screen: say "unreachable" instead of waiting for ever, and
draw the last known slot contents while reconnecting.

### Added

- `showReader()` under Settings, and `ST_SET_READER` which reads continuously
  while it is open. The reader dot is gone from the slot screen.- Slot cell rebuilt to the app's three-part shape; `SlotState` gained `brand`,
  parsed from Creality's `vendor`. The other three backends leave it empty and
  the cell shows "-", which is honest rather than blank.
- `check-ui-translated.py`: "ID" added to the allow list, beside IP and MAC.

## 2026-09-06 - what was blocking the interface

Instrumented the loop to name whatever held it for more than a frame, rather
than reading the code and guessing. It printed `state 12` - ST_PRINTER, the
home screen - at 600 to 1550 ms, over and over.

Two causes, both in that state:
- `probeOne()`: `WiFiClient::connect(host, port, 900)`, one printer per pass,
  every 1200 ms. Blocking, and a printer that is off costs the full 900 ms.
- `disc::tick()`: four `connect(ip, 9999, 150)` per pass = 600 ms, repeated
  until 254 addresses are done.

Both moved to a 4 KB task on core 0. `disc` split into `sweep()` on the task
and `finish()` in the loop, because `reconcile()` rewrites `printers[].host`
and commits NVS while the screens read that array - the slow half moved, the
half that touches shared state did not.

After: 62-71 ms. The 420 ms spikes that remain are `/api/tap` pumping LVGL for
400 ms so a remote capture is stable; a real finger never goes through it.

## 2026-09-06 - the printer link is a state, not a call

Asked to confirm the connection is permanent and visible everywhere. It was
neither: `backToPrinters()` did `backend->stop(); backend = nullptr;`, so the
session lived in one view and nothing opened it when the network came up.

`linkTick()` now owns it - LINK_IDLE / TRYING / UP / GAVE_UP, five attempts,
8 s per attempt, called every loop and doing nothing once connected. A drop
after being up gets a fresh budget rather than inheriting a spent one.
`linkRetry()` re-syncs the account before trying again.

Verified from a cold boot: `[wifi] OK` then `[link] attempt 1/5` then
`[link] up`, with no churn afterwards.

**Not done:** the state shows on the slot screen (dot, or a retry button when
it has given up). The home screen still shows per-printer reachability from the
probe rather than the link state, and the settings screens show nothing. "Every
view" is therefore partly true - worth finishing once we agree where it goes,
since the header is already carrying the account and Wi-Fi at 240 px wide.
- Connection failure screen: title, QR to wiki.tigersystem.io, four causes.
  Drawn by `screen_slots` when `link == 3`, which is why that screen had to
  learn to render with no backend at all.
- **Bug I introduced in 1.28.0 and caught on hardware:** holding the session
  open meant `linkTick` saw the PREVIOUS printer's backend connected and
  reported LINK_UP. Header said K2Pro, grid showed the Ender-3's filament. The
  backends are singletons, so switching printers has to stop one before
  re-pointing it.
- Benoit, on the failure screen: leave only the title and "Scan the QR code",
  delete everything under it. He was right and I had already flagged the
  symptom myself - the fourth cause needed scrolling to reach.
- Spinner while connecting. First attempt showed a caption and no ring: the
  slot count comes from the printer MODEL, so an unreachable printer still
  reports five cells and `n == 0` was never true. The test is whether any slot
  is `known`, not how many there are.
- Second attempt still drew no ring: padding on an lv_arc insets the arc inside
  its own bounds, so 48 top + 14 bottom on a 56 px spinner left negative room.
  Margin styles are compiled out of this build; a transparent spacer object
  holds the gap instead.
- The header dot went red while the spinner said "connecting" - the screen
  arguing with itself. Hidden while the spinner is up; it returns only when
  there are slots on screen to be red about.
- Wi-Fi bars: `icons::signalBars` / `icons::setSignal`, three bars in the same
  22 px box as every other icon, written into rather than rebuilt.
- Benoit: the connect animation freezes, and the Wi-Fi one never turns at all.
  Instrumented the main loop with a 2-second worst-case report instead of
  guessing. It said `backend=5006` and `passes=1` in a five-second window -
  WebSockets' blocking connect, default WEBSOCKETS_TCP_TIMEOUT. Set to 1200 and
  re-measured: 1203 ms, passes back to ~100 per 2 s. The remaining 1.2 s is one
  stall per 8-second attempt. Removing it entirely means printer I/O on its own
  task, which is a real change and not one to make in passing - the backends
  are singletons the UI reads directly, so it needs a handoff that does not
  race on Strings.
- The Wi-Fi join screen had BOTH failure modes: rebuilt per pass (new spinner
  every frame, always at zero) and a 250 ms delay in the wait loop. Added a
  generation counter to screen_setup so a screen can tell "nothing changed"
  from "someone cleaned the screen under me" - guessing wrong either way is a
  blank panel or a spinner that never turns.
- Wi-Fi icon: arcs, not bars. Benoit's call, and the portal was already right -
  bars are the GSM symbol. Same four thresholds as `bars()` in portal_page.h.
  Bench reads -81 dBm and the panel lights the dot alone, which is what the
  phone shows for that network.
- Progress line under the spinner. It must NOT be part of the rebuild
  signature: a counter that rebuilt the screen would destroy the spinner and
  restart it at zero on every tick, which is precisely the Wi-Fi bug above.
  Written into a label held in `s_progress`, cleared on every rebuild.
- Wi-Fi arcs: mine drew correctly and the thresholds matched the portal, but
  Benoit said the image was still wrong next to the TigerScale. Asked that repo
  for its LVGL code rather than guessing a fourth time, and the answer was that
  there is no drawing at all: two stacked LV_SYMBOL_WIFI labels, one dimmed at
  50% and one lit inside a container whose HEIGHT is the level. Reproducing a
  shape could never match displaying it. Three attempts at arcs were three
  attempts at the wrong problem.
  Details worth keeping: four levels and not five, because the glyph has three
  pieces and a fifth cut lands mid-arc; clip heights measured off the panel,
  not calculated; full strength paints ONE copy because a mask cannot reach the
  apex; 50% and not iOS's 30%, which vanishes on grey cards; and LVGL does not
  re-run alignment after a size change, so the clip and the lit label must be
  re-aligned on every level change or the two copies drift apart.
  Their dBm arithmetic came across too, and I moved the portal's picker onto it
  as well - otherwise the panel and the phone would have disagreed about the
  same network, which is the problem I had just finished solving.
- Benoit asked for a scan page when a slot is tapped. It already existed -
  ST_SCAN, "present the spool", cancel - and had never been reachable: the
  colour block swallowed every press. lv_obj is CLICKABLE by default in LVGL 8.
  This is the third time that default has cost a session; icons.cpp documents
  it for drawn strokes, and it is worth remembering that it applies to any bare
  lv_obj_create placed on top of a button.
- Status dots removed from the scan/review/result screens, and the
  printerUp/readerUp parameters removed with them rather than left unused.
- **OPEN, and it matters: pressing Send takes the whole device down.** Benoit
  reported it and the board was confirmed dead - no network, no USB CDC, esptool
  could not connect. Not a sleep, not a reboot: a hang. A serial capture was
  armed to catch the backtrace and the session moved on before it was
  reproduced, so the cause is still unknown. Do NOT theorise it away - the one
  thing needed is that trace. The null slot name in the Creality status line was
  found by reading and fixed, but it invalidates a String rather than faulting,
  so it is almost certainly not this.
- The NFC-tester "freeze" was a reboot. Decoding the backtrace was what turned
  a week of guessing into ten minutes: lv_label_set_text <- showWifi, on a heap
  assert. One shared s_viewSig across screens, each XORing a constant into a
  content hash - so the tester's product-id hash could collide with the Wi-Fi
  screen's and make it write into freed widgets. A crash whose reproducibility
  depended on the contents of a chip.
- Everything else this round came from the same 2-second worst-case reporter:
  reader::read at 575 ms per frame, a 20-attempt failing read at 6.8 s,
  backend->loop at 1.2 s on every screen, webcfg::loop at 12 s while a browser
  watched, and the account sync sharing core 1 with the UI.
- Three attempts at the QR quiet zone, all wrong for different reasons, worth
  remembering: lv_qrcode rounds the requested size down to whole pixels per
  module (104 -> 98); LV_SIZE_CONTENT plus padding gave a card 6 px taller than
  wide; and lv_qrcode_update narrows the canvas WIDTH but leaves the object's
  height at the creation size. Measure the object after a layout pass, square
  it yourself, and size the card from that.
- Runtime reference tables: LittleFS was never mounted before this, and the
  partition is labelled `littlefs` while Arduino's LittleFS.begin() looks for
  one called `spiffs` - it returns false, formats nothing, and says nothing.
- Accents. The roadmap entry said "generate a Latin subset and attach it as a
  fallback face"; a fallback chain turned out to be unnecessary. One face per
  size, built from Montserrat AND FontAwesome in the same lv_font_conv call,
  replaces the built-in outright - same metrics, same symbols, wider alphabet.
  The built-ins are switched off in lv_conf.h so lv_font_montserrat_16 does not
  quietly link a second copy of every glyph.
- Two traps worth remembering. The generated sources include <lvgl/lvgl.h>
  unless LV_LVGL_H_INCLUDE_SIMPLE is defined. And a custom LV_FONT_DEFAULT must
  be declared through LV_FONT_CUSTOM_DECLARE, not with LV_FONT_DECLARE at file
  scope in lv_conf.h: that file is read before lv_font_t exists, and the error
  surfaces tens of lines deep inside LVGL's own headers.
- Cost: flash went from 30% to 45% of a 4 MB slot. Five sizes x ~350 glyphs.
- Benoit on both confirm screens: as bare as possible. The wording is his -
  "Etes-vous sur de vouloir restaurer les parametres d'usine ?" and "Redemarrer
  la TigerSpool ?".
- The restart question orphaned its question mark AGAIN at 16 px, exactly as
  the old wording did at 20. A product name in a sentence cannot be shortened,
  so the type gives way: 14 px. Worth remembering that this panel is 226 px of
  usable width and a centred question is one of the easiest things to overrun.
- The boot screen's white edges were in the PNG, not the code: columns 0 and
  239 held RGB(14,14,14) for 212 rows. Fixed in the asset and verified by
  decoding the old and new files and diffing every pixel - 424 differ, all of
  them in those two columns, dimensions unchanged. Worth doing that check
  whenever an image is rewritten programmatically; "I only meant to touch the
  edges" is not evidence.
- Elegoo and Anycubic LAN backends, ported from Tiger Studio's PROTOCOL.md for
  each brand rather than reverse-engineered. Both verified only as far as the
  bench allows: they compile, and the account import now maps the two brands to
  them (seen in the log as `-> type 5` and `-> type 6` where it used to say
  "has no backend").
- The Anycubic on this LAN has 9883 open, so the backend CAN be tested here -
  but the account holds twelve printers and MAX_PRINTERS is eight, and the
  import walks brands in a fixed order, so the two new brands are the ones that
  get dropped. Worth fixing before anyone tries to test them.
- Anycubic's three extra credentials had been anticipated in printer.h with a
  note that the sn/cc pair would not stretch. It did not. They are named fields
  now, with three more NVS keys and the same "local value wins" rule the others
  follow.
- Elegoo, live against a Centauri Carbon 2: TCP connects, the broker answers,
  and it returns rc=5 - unauthorized. Transport, port, client id and topics are
  therefore all correct and the only missing piece is the access code, which is
  empty in the account while Tiger Studio's form shows Q2CQoJ. Far more than
  "it compiles": the backend is proven up to authentication.
- Anycubic, live against a Kobra X on the same LAN: the account holds deviceId
  and password but NOT username and NOT acuModelId. Both are written by Tiger
  Studio, so this is an account-data gap rather than a firmware one - and it is
  why the TLS question the protocol notes call the biggest unknown is STILL
  unanswered. Nothing has handshaked yet.
- The missing credentials were never missing. Benoit pasted the Firestore doc
  and mqttPassword was right there; the client-side ArduinoJson filter simply
  did not name it. Two lists that had to agree, forty lines apart, with a
  CODEMAP landmine explaining that they do DIFFERENT jobs - which is true and
  is exactly what stopped anyone noticing they must still agree on names.
- Anycubic over mbedTLS: works. That was the biggest open question in the
  protocol notes and the answer is plain - connected + subscribed, TLS 1.2,
  self-signed, no client certificate.
- "Three printer connections at once" - Benoit pushed back on my claim that it
  would not fit, and he was right. Measured rather than argued: a plain MQTT
  session costs ~3 KB (~0.6 KB for the next), a setInsecure TLS session ~38 KB,
  and THREE TLS sessions all connect with 79 KB still free. My earlier "three
  TLS talkers fail" is true of the account sync, the OTA check and the table
  update, which verify against the root CA bundle - a different and much more
  expensive thing. Generalising from it was wrong.
- Multi-connection, the cheap way: one link per BRAND. The backends are
  singletons per brand, and a fleet is usually one machine per brand, so an
  array of links buys simultaneous sessions with no change to any backend. Two
  printers of one brand still cannot both connect - that needs instancing, and
  that is the day this design ends.
- The Snapmaker 403 took the library's own DEBUG_ESP_PORT trace to find. Worth
  remembering that WebSocketsClient sends `Origin: file://` and
  `Sec-WebSocket-Protocol: arduino` by default, and that Moonraker rejects the
  first. Nothing in our own logging could have shown it: the client never
  raised an event, so every layer above saw silence.
- Verified end to end on a U1: tag read, review, send, and the printer itself
  then reports E4 as R3D / PLA / High_Speed / DC123FFF.
- Bambu cloud, read-only: everything needed for the actual session is now
  known and written down here so the next step is short.
    Firestore: users/{uid}/printers/bambulab/secrets/cloud_session
    fields:    bambuUid, mqttUsername ("u_<uid>"), region ("eu"|"us"),
               accessToken, and an expiry (Bambu's tokens last ~3 months)
    broker:    mqtts://<region>.mqtt.bambulab.com:8883, insecure TLS
    auth:      username = mqttUsername, password = accessToken
    client id: must be unique per device - a shared one makes the broker kick
               the phone or the desktop off
    topics:    subscribe device/<dev>/report, publish device/<dev>/request
               with the same pushall the LAN path already sends
  The report format is IDENTICAL to LAN, so the existing Bambu parser is
  reused unchanged. What is missing is the account fetch and a cloud flag on
  the backend.
- Bambu cloud, working: `[bambu] connecting to us.mqtt.bambulab.com:8883
  (cloud)` and the X1C's slots on the panel. Credentials come from
  users/{uid}/printers/bambulab/secrets/cloud_session, read during the account
  sync. The report is identical to the LAN one, so nothing in the parser moved.
- The eviction rule that made it possible is worth remembering: one backend per
  brand means the SELECTED printer has to be able to take it from a sibling,
  or a second printer of the same brand can never connect however long you
  wait. It cost an hour of watching the wrong IP in a log.
- check-text-english.py learned that a bare hostname is not prose. It read
  "us.mqtt.bambulab.com" as Portuguese, because ".com" is a word in it.


## 2026-09-08 - one connection per printer, and two things that hid behind that (released in 1.42.0)

### Changed

- The cloud slot notice is one message, not two: "Working only with LAN Mode +
  Dev Mode" in orange, the QR, and "Scan for tutorial" under it.
- Every backend is instantiable. All six kept their host, socket, slots and
  connected flag in file statics, so exactly one printer per brand could exist;
  state now lives in the object, and `main.cpp` builds one backend per link and
  destroys it with the link. The brand-eviction rule is gone with the reason
  for it.
- The Bambu MQTT receive buffer is 50 KB for the printer on screen and 8 KB for
  the others. A background link is asked one question - connected or not - and
  that comes from the session, not the report.
- MQTT client ids carry the printer's serial (Bambu) or device id (Anycubic).

### Fixed

- A link no longer claims a connection it did not make. `tickLink` asked the
  backend `connected()` before ever dialling; with a shared backend the answer
  belonged to the sibling that had it, so a Creator 5 Pro that was not on the
  network showed a green dot and the AD5X's four spools.
- The FlashForge backend had no `stop()` at all - it inherited the empty one -
  so it stayed authenticated after losing its link.
- A sync that reached one brand out of six no longer writes itself as the whole
  account. Five requests failed with six links open and a list of thirteen
  printers became three. A brand that did not answer now contributes what was
  stored for it, in its own place.
- Two documents for one printer: the newer one wins. An A1 switched to cloud
  mode kept its stale LAN document - and the access code the printer had since
  rotated - because dedup kept whichever came first.
- Links are opened while the heap allows and closed below 55 KB free, with a
  back-off that grows to eight minutes. Measured: nine printers switched on,
  six links live, 65 KB free, four minutes without a restart.
- The QR quiet zone on the cloud notice: the card carried a `pad_top`, and
  `lv_obj_align` measures from the content area, so the code sat off-centre and
  ran into the white edge a scanner needs.

Verified on hardware throughout: A1 Home and X1C Home connected at the same
time showing their own slots, AD5X green beside a red Creator 5 Pro, and a
forced two-brand sync failure keeping all thirteen printers.

## 2026-09-08 - the update screen says what it is moving from (released in 1.42.1)

### Changed

- The update notice and the update page show "1.41.0 > 1.42.0" - the version
  being left in dim, the one being offered in the accent - instead of the
  caption "Version available" over a single number. One label with LVGL's
  recolour markup, so the two cannot wrap or drift apart.
- The update page drops its "Installed" row while an update is offered: the
  step line already carries that version.
- `S_AVAILABLE` removed; nothing draws it any more.
- `?preview=notice` shows an upgrade rather than a downgrade.

## 2026-09-09 - links that come back, and a tool that was writing nothing (released in 1.43.0)

### Added

- **Choose printers**, a setup step after the account is linked: the account's
  printers with nothing selected, a scrollbar, and Confirm pinned below the
  list. A flag in NVS means it belongs to setting the box up, and to a factory
  reset, rather than returning after every sign-in.
- A third dot state: blue while the device is dialling a printer, green
  connected, red not.
- A heap heartbeat in the log every thirty seconds - free, largest block, links
  up, Wi-Fi and RSSI - and `?preview=greys`, six card fill/border pairs plus a
  primary and grey ramp for questions a screenshot cannot answer.

### Changed

- Cards are a black interior and a grey outline. Chosen on the glass, not from
  a capture: every near-black grey candidate sits where the IPS gamma curve
  stops being linear, and one of them rendered as pale blue slabs on the panel
  while the buffer held near black.
- Scrollbars: one definition for every list, 6 px, in the secondary text grey
  instead of the outline grey it was invisible in, with a gutter beside it and
  a gap at each end.
- Titles are one line. LV_LABEL_LONG_DOT wraps before it truncates, so a long
  translation grew the label and pushed the header's rule off the screen.

### Fixed

- **A link that gave up never dialled again** until somebody tapped the
  printer. Left alone through one Wi-Fi blink, a device ended with every dot
  red and no way back. Giving up is a minute's pause now.
- **The interface froze whenever the network went bad.** PubSubClient waits 15
  seconds on a socket and an Arduino TLS handshake waits two minutes; six
  backends retry together the moment Wi-Fi drops. One link may dial at a time,
  holding the slot for the whole attempt, and the waits are bounded to 4 and 5
  seconds. Gating only the pump was worse than nothing: attempts were counted
  while the backend never got the call that dials.
- Links are priced before they are opened, one per pass, so the cost lands
  before the next decision. The survival floor is 32 KB and acts only on a
  shortage lasting three seconds - it was 55 KB and instantaneous, which closed
  working links on the dip an account sync makes.
- `scripts/flash.sh` wrote nothing, silently, whenever a second USB serial
  device was present: reading its MAC failed, and under `set -e` with
  `pipefail` that ended the script before it flashed. Three flashes in a row
  were believed to have landed and had not.

Measured on hardware: six printers connect unattended and hold, heap flat at
66 KB for six minutes, no restart, interface answering in 2 s. Separately, the
RFID reader was cleared of suspicion for the weak Wi-Fi - 38 samples with the
field active read 2.3 dB BETTER than 30 with it idle.
