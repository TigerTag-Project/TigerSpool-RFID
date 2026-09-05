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

