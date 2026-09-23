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

## 2026-09-23 - the account page, and a sign-in that no longer resets the box (released in 1.67.0)

### Added

- `/login`, signed in, is the account page: picture (`photoUrl` from
  `accounts:lookup`, once per boot on the sync task, stored in NVS only when
  there is one), name, address, a sign-out button that asks first, and a
  drawer with every printer, an on/off switch each and "Sync machines now" at
  its top. Nothing navigates: `/api/account` is read back after a sync
  (`syncCount()` moves and `changePending()` clears) and after a switch.
  Switches move at once and are put back only on a budget refusal - the web
  server is on the loop, measured 5-6 s behind a printer handshake.

### Fixed

- Email sign-in reset the device (PR #15's backtrace: `handleTtLogin` ->
  `syncNow` -> mbedTLS on the 8 KB loop stack). All four web-side syncs now
  call `ttcloud::requestSync()`; the loop stack stays at 8 KB. Measured: the
  page answers in 0.3 s, the sync runs 11.7 s on its task, taps during it
  answer in 0.45-0.57 s.

### Documentation

- CODEMAP: the `gen_db.py` rows described a generator that no longer exists;
  the battery "threshold" row contradicted the declared battery. AGENTS: the
  ASCII-only and no-Chinese rows contradicted the fonts.

## 2026-09-21 - the battery is declared, and the screen always answers (released in 1.66.0)

### Changed

- Battery detection removed entirely; `battery::declare()` / `declared()`
  replace it, stored in NVS as `bdecl`, and `present()` is the declaration.
  Benoit: "le plus simple est que l'utilisateur declare sa batterie ... pas
  besoin de gerer ca par algorythme impreccis". The measurements that killed
  the heuristics, both on the same board: a flat pack at 3.31 V rippled 6.0 mV
  (read as "no cell"), and the same board with the pack REMOVED sat at 4.05 V
  rippling 1.7 mV (read as "a cell"). The other board, empty, sits at 4.27 V
  rippling 9 mV. No threshold separates those.
  The Settings row is now always present - it is the only way in to declare -
  and carries the percentage once declared. The switch sits at the TOP of the
  battery screen: at the foot it was below the fold, and every change of state
  rebuilds that view and resets the scroll, so the control moved out from under
  the finger reaching for it. The switch also moves in its own event callback,
  before main.cpp has decided anything: the loop can be a second deep in a TLS
  handshake when the finger lands, and a control that agrees a second later is
  one somebody presses twice. The printer list has done this since it was
  written, and the rest of the view follows in the same frame through
  `lv_async_call` - deferred, because rebuilding deletes the screen the
  callback itself belongs to and LVGL walks back into that object when the
  callback returns. Six toggles in a row, uptime 53 s -> 67 s: no reset.
  Verified on the board: declare on -> 71% and the
  measured view, declare off -> present=false, percent=-1.

### Fixed

- `screen_settings`: the printers view repainted at frame rate at rest. Found
  by measuring `/screen.ver` while Benoit reported the screen "moving on its
  own": 0 frames a second on the home screen, 29-55 on this one. `updateGauge`
  rewrote both gauge labels every pass, and `setReloadBusy` re-applied the
  HIDDEN flag every pass - LVGL invalidates on both regardless of whether
  anything changed. Both are now guarded by a cache, cleared where the objects
  are rebuilt (both the settings view and the first-boot chooser). 0 frames a
  second at rest afterwards, verified on the board with the battery.
- Battery presence: the ripple test is no longer asked below 4.15 V, where the
  level already answers. Found with a real pack at 3.31 V on the second bench
  board - ripple 6.0 mV, exactly RIPPLE_NONE_MV, so the device declared no cell
  while running on one. Benoit confirmed the pack was nearly empty. After the
  fix: "3.43 V is below the charger's own level - a cell is on the connector",
  8%.
- `printer_ids` published empty, always. Two faults, one on top of the other.
  The ids were written to NVS and read back only when a sync reported a change,
  so a sync that found the same printers left them in flash with nobody reading
  them - and then the write itself turned out to fail: `putString` returned 0
  for a 361-byte value with 130 free entries, because NVS wants a blob's
  entries contiguous inside one page and this frozen 20 KB partition (500/630
  used) has no run that long. They are kept in RAM now, refreshed by every
  sync; `PrinterCfg::docId` and the `pids` key are gone. A booted device
  publishes no ids until its first sync, which is a minute or two.
  Verified on the bench: `[presence] 2 printer(s) active, 2 id(s):
  4thj8F4g0SALP27GnrOm,t1nFx693oORWqjbd3ceD`.

## 2026-09-21 - the collection goes plural (released in 1.65.0)

### Changed

- Presence writes to `tigerspools/` (plural). The Firebase side pointed out
  that every account collection is plural - scales, printers, racks - and the
  singular would have been the one exception. Free to change: 1.64.0's writes
  were all refused, so no document exists under the old path.

## 2026-09-21 - the device declares itself (released in 1.64.0)

### Added

- Presence: `ttcloud::heartbeat()` and `ttcloud::deviceId()`, plus the
  scheduler in main.cpp (`presenceTick` / `presenceWatch`). One Firestore
  `documents:commit` with an updateMask, `last_heartbeat_at` as REQUEST_TIME,
  full-then-deltas, explicit nulls, display_name read before written.
  `docs/PRESENCE.md` holds the contract.
- `PrinterCfg::docId`: the printer's document id in the account, captured at
  import and kept in NVS as ONE newline-separated key (`pids`). Twenty-four
  separate keys would have cost about fifty of the hundred and thirty NVS
  entries this device has left, in a partition that cannot grow over the air.
- pairStart now sends kind `tigerspool`, model `TigerSpool`, the real version
  and the mDNS name.
- A change of IP address forces a beat. It is what somebody uses to reach the
  box, and the moment they need it is right after the router handed out a
  different one.

**Blocked on Firebase.** Every beat comes back `403 PERMISSION_DENIED` on the
bench: the account's security rules do not name this path yet. The rule needed
is in `docs/PRESENCE.md`. Until it lands, the code is verified only as far as
"builds, runs, sends a well-formed commit and backs off when refused" - which
it does: three refusals hold the next beat for five minutes, confirmed on the
bench board (4 attempts in 80 s, then the hold).

## 2026-09-20 - the sign-in reboot, and the scan flow settled (released in 1.63.0)

### Fixed

- Email/password sign-in rebooted the ESP32; Google pairing did not. Reported
  by a user. `handleTtLogin`, `handleTtSync` and the browser-driven Google poll
  all set `restartAt`, and the reason they needed to was in main.cpp: the
  ST_MAIN and ST_PRINTER sites tested `if (asyncTake(s)) { if
  (consumeChanged()) ... }`, so a synchronous `syncNow()` from the web handler
  - which hands nothing to asyncTake - never triggered `loadCfg()`. The two
  questions are now asked separately, and the three restarts are gone with
  `W_RESTART_SUFFIX`. `forget()` still restarts; dropping a session is the one
  case where starting clean is the point.
  Verified on the bench: a POST to /tt-sync runs the full account sync (12.7 s,
  16 LAN printers) with no `rst:` in the serial log and a frame counter that
  does not restart. The email path itself is UNVERIFIED here - signing in needs
  someone's real credentials, which I do not handle.

### Changed

- `S_SIG_VALID` is "Certified" and its translations - Certifié, Zertifiziert,
  Certificado, Certificato, Certyfikowany, 已认证 - replacing "genuine" /
  "authentique". The English word was used in all nine first; Benoit: "il faut
  le mettre dans la bonne langue". It is drawn in one place, the reader
  screen's header; the NFC tester shows "OK" and is untouched.
- `S_READ_MODE` is "Scan" (Escanear / Scansione / Skanuj / Digitalizar / 扫描)
  and `S_READ_HINT` is "What is this?" - the row now names the action and asks
  the question instead of describing the hardware. The home row's hint is
  clamped to ONE line: the first French wording wrapped and pushed the row's
  own label off centre.
- `screen_read::showTag` puts the back handler on the screen, the body and the
  header, and clears LV_OBJ_FLAG_CLICKABLE on the value rows and the colour
  disc - an LVGL container is clickable from birth and swallows the click.
- `showScan` lost its `errorOrNull` argument: a failed read is logged, not
  drawn. Benoit: "ne met pas le texte rouge quand tu attends la puce".
- The scan screen keeps its Cancel button for its whole life, including the
  write. Hiding it the instant the chip was caught was tried and thrown out:
  the write is short, so the button vanished a fraction of a second before the
  screen changed anyway, and a flicker on the way out makes the device feel
  unsteady.

## 2026-09-20 - the send, straight through (released in 1.62.0)

### Changed

- ST_REVIEW is now ST_SENDING, and it has NO screen of its own. The write
  starts by itself when the tag is read and the scan screen simply stays up,
  with one change: `showScan(..., caught=true)` drops the Cancel button, since
  from that instant there is nothing left to call off. Two earlier attempts
  were thrown away first - a full screen for the send (spool, temperatures,
  spinner), then the same screen with its words changed to "Sending" - because
  the send is short enough that anything shown for it flashes past. Benoit:
  "je veux pas voir l'ecran intermediaire car c'est trop rapide". `S_SENDING`
  went with them.
  The chevron still cancels while the product endpoint is being waited for;
  once `assign()` starts it is one blocking call and runs to its end.
  `S_SEND`, `S_NO` and `S_SLOT` are gone with the buttons and the old title;
  `S_SENDING` replaces them.
- The scan screen is titled with the slot's name alone, like the receipt.
- `screen_scan::showResult` rebuilt. Benoit: un seul ecran de succes, confirmer
  l'envoi et donner l'etape suivante -> in English: one success screen, confirm
  what was sent and name the next step. Layout: tick in a green ring, "Sent to
  the printer", the spool in a bordered card, then a fixed 68 px green block
  carrying S_INSERT_IN with the slot label ALWAYS on its second line - it must
  not change height between "1" and "AMS2-4", or the screen jumps between two
  spools. The colour-adaptation pair of swatches is gone with it.
- The success screen now runs a five-second countdown drawn as a draining bar,
  and the whole screen is clickable so a tap anywhere dismisses it. `msLeft` is
  deliberately OUT of the redraw signature: the bar's width is set on the early
  return, so the screen is not rebuilt sixty times a second.
- New strings S_SENT_TO_PRINTER and S_INSERT_IN, nine languages.
- Previews `scan`, `review`, `result`, `resultlong` (AMS2-4, the layout's worst
  case) and `resultfail`: these three screens had none, so nobody could look at
  them without a printer switched on and a spool in hand.

### Added

- The drying row on the reader screen (`S_DRYING`, nine languages): "55 °C -
  4 h" from `TagInfo::dryTemp` / `dryHours`. Benoit: "le séchage c'est
  important".

### Changed

- Reader screen geometry tightened to hold a fourth value row on 320 px: disc
  68 -> 60, row gap 6 -> 4, and 8 px off the two paddings above and below the
  brand line. Verified with a real spool: PLA High Speed, drying 55 °C - 4 h,
  matching the serial decode, with "Restant 476 g" fully on screen.

## 2026-09-19 - a home screen, and reader mode (released in 1.61.0)

### Added

- A home screen that asks what you came for: two 72 px rows, **Printers** with
  how many are online under it and **Reader** with what it does. The printer
  list is one tap in and comes back with a chevron.
- **Reader mode** (`ui/screen_read.cpp`): the spool's colour as a disc, the
  material, "brand - finish - diameter", the nozzle and bed windows, what is
  left on the spool, and a green "genuine" mark in the header. The NFC tester
  keeps every raw field and stays where it was, in Settings.
- A third home row, **Write**, greyed and not clickable: no chevron, no
  pressed state, "Coming soon" under it.
- Previews `main`, `read` and `readtag` for the three new screens.

### Changed

- The account and Wi-Fi icons are on the home screen only. On the printer list
  the header carries a back chevron, the title and the gear, and 240 px does
  not hold all six.
- Settings returns to the screen the gear was pressed on, not always the home.
- The home rows start at the top rather than centred: the list grows, and a
  centred stack moves every row down each time one is added.

### Fixed

- `ui/icons.cpp`: the canvas icons (the turned Wi-Fi wave used for NFC) shared
  one static pixel buffer. Two of them on the home screen - read and write -
  meant the second drawn overwrote the first, and the read row came out grey
  instead of amber. Each one now allocates its own buffer from the LVGL heap
  (PSRAM) and frees it on LV_EVENT_DELETE; internal RAM use drops 5.8 KB.

- `scripts/flash.sh` no longer reports "no board is plugged in" when the
  PlatformIO virtualenv has lost its `esptool` module: it falls back to the
  copy that ships with the platform, and says so plainly if neither is there.

Verified on the bench board (dc:b4:d9:24:99:18) over `/api/tap` and
`/screen.bmp`: home -> printers -> back, home -> reader with a real R3D PLA
High Speed spool (215-230 / 50-60 C, 476 g, signature valid, matching the
serial decode), gear from both faces and back to each. `bash scripts/verify.sh`
passes, build included.

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

## 2026-09-09 - the heap had room, just not in one piece (released in 1.43.1)

### Fixed

- HTTPS failed outright with several printers connected: 66 KB free but a
  largest contiguous block of 29 KB, against the ~40 KB a TLS session needs in
  one piece. The update screen said "manifest HTTP -1" and the account refresh
  failed the same way. Background links now stand down for the duration of a
  TLS operation and come straight back; the selected printer keeps its link,
  and nothing new is opened while the handshake needs the room.

## 2026-09-10 - the NFC tester, and a Wi-Fi that came back (released in 1.44.0)

### Added

- `scripts/make-ui-font.sh` builds two more faces: Montserrat SemiBold 16 for
  titles and row labels, and JetBrains Mono 16 - twenty glyphs, 8 KB - for the
  page dump. Both fetched from their own projects, cached outside the tree,
  never committed; both cited in THIRD_PARTY_LICENSES.md.
- `?preview=icons` draws every icon beside its name, and `?preview=greys` six
  card fill/border pairs plus a primary and grey ramp.
- The NFC Tester's page dump is a screen of its own behind a HEX Code button:
  pages 0x04 to 0x17, four bytes a line, addressed by their real page numbers.
- `tt_aspect_colors()` in the generated database, from the `color_count` the
  API already carried. Three aspects are multicolour, not two.

### Changed

- The NFC Tester is a technical view in English in every language, one field
  per row, in a fixed order: a colour bar across the width, then UID, brand,
  tag type, material, message, aspects, kind, weights, diameter, temperatures,
  the stamp raw AND as a date, the three colours in hex, HueForge TD and the
  signature verdict.
- The colour bar carries one band per colour, and how many is the ASPECT's
  answer - never the colour bytes'. 00 00 00 is a real black and also what an
  unused slot holds; only the aspect tells them apart.
- Colours are shown in hex, and the primary with its alpha: page 0x08 is four
  bytes and the tester shows what is on the chip.
- Waiting for a tag is the reader's wave in a ring over "Waiting NFC...", with
  the reader's own state above it.
- Titles and row labels are SemiBold 16; buttons are 20; the printer list's
  names match. Weight is on titles and labels only - weight everywhere is
  weight nowhere.
- The reader row has its own icon: the Wi-Fi wave turned a quarter turn, drawn
  by rotating the glyph itself rather than redrawing it by hand.

### Fixed

- Wi-Fi is asked for again when it goes away. See its own commit.
- A tag's readings STAY on screen when the spool is taken away, and are
  replaced only by a different chip. Clearing on removal made the result vanish
  at the moment somebody wanted to read it - and made the screen flicker
  whenever detection dropped a poll, which it does.
- Padding on a button is not space above it: a label is centred in the content
  area, which padding shrinks from the top, so the word sat below the middle.
  Same fault put a ten pixel strip of bare ground inside the colour bar. Space
  is an object now, `gap()`.
- The Display row's sun matches the Language row's globe; a long title no
  longer pushes the header's rule off the screen.

Verified on hardware throughout, on a real R3D spool.

## 2026-09-10 - the pushall nobody needed (released in 1.45.0)

### Changed

- The Bambu backend no longer asks for a `pushall` every eight seconds. The
  printer publishes an AMS report by itself when a spool changes, and that
  report is 1.2 KB against the 5.9 KB of a full dump. Measured: two printers
  answering 40 pushalls in 150 seconds - 81 KB a minute - for trays that had
  not moved. A slot changed by hand arrived on its own 164 seconds after the
  last request we made. What is left is one pushall at connect, one every five
  minutes as a backstop, and one when a printer's screen is opened.
- The update page puts Install at the foot of the screen, and scrollbars are
  back to appearing only when the content overflows - the mode was only ever
  forced because `lv_obj_remove_style_all` had taken the theme's scrollbar
  style with it, which the explicit style now replaces.

### Fixed

- MQTT sessions stopped dropping. `setSocketTimeout(4)` - added this morning to
  stop the interface freezing - bounds every READ, not only the connect, and a
  5.9 KB report over a -80 dBm link sometimes takes longer than four seconds.
  The client was hanging up on printers that were answering perfectly, once per
  poll. With the poll gone the only large read is at connect: four minutes of
  observation, zero sessions lost, against four reconnections in 150 seconds
  before. A `session lost, state N` line stays in each MQTT backend so
  PubSubClient says why rather than leaving it to be guessed.

## 2026-09-10 - the update page, as one page (released in 1.45.1)

### Changed

- The update page keeps the version card on screen in every state, and shows
  what it is doing under it: a turning ring while it checks, a tick when there
  is nothing newer, and the download icon with `from > to` and Install when
  there is. The card used to be hidden the moment an update appeared - the card
  is a statement about the device and the step line is a projection, and a page
  that removes the first when the second arrives has nothing steady on it.
- The card's label is "Version", not "Installed version". The number beside it
  is the device's own; nothing else on the page could be meant.

## 2026-09-10 - what a printer actually costs (released in 1.45.2)

### Added

- `/api/memtest` (and `?all=1` for every printer on the account): closes every
  link, holds the account sync off, opens printers one at a time and reports
  each one's settled cost in internal RAM and PSRAM to the serial console.
  Written because every number taken in normal running was polluted - the sum
  of per-link deltas came to 193 KB against 135 KB measured on the whole.

### Changed

- `linkCost()` uses the measured prices. The MQTT buffers were being counted
  as internal RAM and they are not: the framework is built with
  CONFIG_SPIRAM_USE_MALLOC and a 4 KB threshold, so every buffer over that -
  the 50 KB Bambu one included - is in PSRAM already. What is internal is
  mbedTLS, pinned there by CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC. A printer's cost
  is whether it speaks TLS: 40-48 KB if it does, 2-5 KB if it does not.
- The roadmap entry that proposed moving the MQTT buffers to PSRAM is rewritten:
  they are there. It now names the two real ways past three TLS printers -
  mbedTLS in PSRAM, or one TLS session shared by every Bambu on a cloud account.

Measured: three TLS printers fit and a fourth does not (47.9 KB free after the
third, and the largest free block had already fallen to 18 KB in one run - a
new session needs a piece that size); non-TLS printers are capped by the code at
ten, not by memory.

## 2026-09-10 - one session for the cloud, and a loop that cannot stay stuck (released in 1.46.0)

### Changed

- Every Bambu printer on a cloud account shares one TLS session
  (`bambu_cloud.cpp`): the second one costs 3.2 KB instead of 47.8 KB, six
  links leave 99 KB free instead of 60, and the largest free block holds at
  50 KB instead of falling to 18. Reports are routed strictly by the serial in
  their topic; proved on a real slot change on the X1C. LAN printers untouched.
- The TLS stand-down no longer closes cloud printers when the selected printer
  keeps the session open - that freed nothing and cost each a resubscribe and a
  full pushall at every account sync.
- Wi-Fi modem sleep is off on station connections, as it already was in the
  setup portal: pings ranged from 18 ms to a full second with it on.

### Fixed

- The main loop is under the task watchdog, 30 s. It froze once with no line
  on the console and would have stayed frozen until the power was cut; a stuck
  pass now panics with a backtrace and the device reboots.

### Not a firmware fault

- The device went unreachable from the network - pings lost at 97% - while its
  printer connections carried on. Not the shared session (disabled entirely:
  same), not modem sleep (off: same), and not this cycle's work at all: the
  published 1.45.2, flashed back for the test, did the same. The radio link is
  at -75 to -88 dBm.

## 2026-09-10 - a load budget, shown as a percentage (released in 1.47.0)

### Added

- `printer_budget.h`: a printer is switched on only if its **load slots** fit
  in a budget of 160 - one load slot is one kilobyte of internal RAM. Prices are
  the worst of five `/api/memtest` passes rounded up: first cloud Bambu 50,
  each further one 4, LAN Bambu 50 (assumed - never measured, both were off),
  Anycubic 41, Creality 6, Snapmaker 5, Elegoo 3, FlashForge 2. The selected
  printer is counted whether switched on or not, since it is linked either way.
- A Load gauge above the printer list in Settings > Printers and in the
  first-boot Choose printers step: a percentage, accent colour, orange from 85%,
  red with "Not enough room" for 2.5 s when a switch is refused. The refused
  switch does not move; switching off is never refused. Raw counts go to the
  console as `[budget] '<printer>' refused: <n> of 160 load slots`.
  `S_LOAD` and `S_NO_ROOM` in all eight languages.
- `docs/CONNECTION-BUDGET.md`: the reference for all of it - the measurements,
  why 160 and not 200 (32 KB survival floor, ~166 usable) and not 166 (a ~6 KB
  reserve for what the model does not count), per printer and per connection,
  and what the model does not see (fragmentation, the ten-link cap).

### Changed

- The budget was 150 with a 16 KB reserve; raised to 160 on Benoit's call,
  trading ten kilobytes of reserve for one more printer. Safe because the budget
  is not what prevents a crash - the 32 KB floor and the sustained low-memory
  closer in `main.cpp` are.
- Load slots are the measured worst case per printer and the margin lives once,
  in the reserve. An earlier draft claimed each price was "rounded up by a
  fifth"; three brands were in fact at or above their price, hidden by the
  first Bambu over-counting.
- Documentation brought in line with the firmware: README (brand table, a
  capacity section, known limitations - the "no Elegoo or Anycubic backend" and
  "no accents" items were false), PRINTER-COMPATIBILITY (Elegoo and Anycubic
  reading proven, Bambu cloud read only, the Anycubic TLS question answered),
  firmware/README (it said nothing builds), installer/README (the manifest
  example had a filesystem image that does not exist), THIRD_PARTY_LICENSES
  (LVGL was missing), ROADMAP (the shared cloud session is built), CODEMAP and
  CLAUDE.md.

### Verified on hardware

- Bench account, six printers on: gauge reads 68% (109 of 160). Switching on
  the P2S (LAN Bambu, 50) is accepted at 159 - it would have been refused at
  150 - and the bar goes orange at 99%. Then the P1P (a further cloud Bambu, 4)
  is refused: the switch stays off, the bar turns red with "Plus assez de
  place", and the console logs `'P1P Office' refused: 163 of 160 load slots`.
  P2S switched back off, 68% again. The P2S itself was unreachable from this
  network, so a LAN Bambu is still unmeasured.

### Not verified

- The vendored PN532 driver's files say BSD, THIRD_PARTY_LICENSES says
  Apache-2.0 and promises licence and NOTICE files that are not there. Flagged,
  not fixed here.

## 2026-09-10 - a printer is what it is, not where it sits (released in 1.47.1)

### Fixed

- The AD5X HOME never connected: the device dialled 192.168.20.131, an address
  the printer had left; the account said 192.168.40.105. Two causes. The import
  merged by POSITION with the stored host and access code winning whenever the
  brand matched, so a new address from the account never landed; and two
  account documents for the same AD5X (old address, new address) were both
  imported because duplicates were only recognised at the same address. The
  printer itself was fine - measured from the Mac: /checkCode answers Success at
  the new address, and it authenticates on the check code alone (any serial
  passes, a wrong code is refused with a misleading "SN is different").
- The same positional merge made X1C Home (Lan) dial the A1's address with the
  A1's code after the second AD5X document shifted the list - rc=5 for ever.
  That is finding 2 of the 2026-09-03 review, deferred then, happening now.
- Fix, without a storage migration: `samePrinter()` (type; serial, with
  FlashForge's optional "SN" prefix dropped; then same address or same mode)
  decides both duplicates and which stored entry an imported printer is. The
  switch and a discovered host move with the printer; `printerIdx` follows it.
  The account owns name, serial, access code and the Anycubic fields; the host
  is taken from the account whenever it differs from what the account gave last
  time (new key `p{i}a`), so a K2 address found by LAN discovery still stands
  until the account changes. A printer new to the device arrives switched off.
- `updatedAt` stored as a Firestore timestamp read as no date, so the rewritten
  AD5X document would have counted as the oldest of the two. `fsMillis()` reads
  integer, double and timestamp; date math checked against the account's own
  value (2026-09-10T21:20:06.421Z = 1789075206421).
- A link now carries `cfgSig()` of the settings it was opened with and
  reconnects when a sync changes them; before, an open link kept talking to the
  old address under the new name.
- The "probe says unreachable" log line names the address it probed.

### Verified on hardware

- Flashed onto the bench: first sync wrote the account's addresses, the AD5X
  link logged "settings changed - reconnecting", then `/checkCode` Success, four
  slots read (1A PLA #F82D29, 1B PLA #871787, 1C PLA 2981E6, 1D PLA #F82D29),
  link up. A1, X1C, Centauri Carbon 2 and Kobra X up beside it. The stale AD5X
  document had already been deleted from the account by then, so the duplicate
  merge and the timestamp reading are compiled and unit-checked, not yet seen
  against a live duplicate.
- U1-showroom was found switched off. No position moved in this sync, so the
  new code wrote no switch; it was shuffled by the old positional merge before
  the flash.

## 2026-09-11 - the home header's three icons, one height (released in 1.48.0)

### Changed

- Measured on /screen.bmp first: bust 21 px of ink, gear 21, Wi-Fi wave 14 -
  and 9 px then 20 px between them. Now all three occupy rows 12-32 (the gear
  10-33, see below), with 11 px of black between one icon's ink and the next.
- The wave moved to font_ui_24 (30 x 23 box, 22 rows of ink). Its signal-level
  clips were re-cut from the glyph bitmap in font_ui_24.c, not scaled from the
  16 px numbers: one level is the bottom 7 rows (row 16 is the empty gap above
  the dot), two levels are the bottom 14 rows but only 22 columns wide,
  because the outer arc's tips reach row 9 beside the inner arc's top. The
  label is dropped 5 px so the glyph fills its box exactly (font_ui_24 leaves 5
  empty rows under it).
- All three open Settings: the gear's button now spans the icon group, x 126
  to 240, the whole header height. The bust and the wave are not clickable, so
  a tap on either lands on it. The longest title ("Imprimantes",
  "Impressoras") ends at x 117, outside it.
- The gear moved to font_ui_24 too. At 20 it matched the others' ink height
  exactly and still read as the small one - Benoit saw it straight away. A
  gear is mostly gaps; it needs a few more pixels to weigh the same.

### Verified on hardware

- Bench, after four flashes: ink at x 134-150, 162-191, 203-224; gaps 11 and
  11; bust and wave rows 12-32, gear 10-33. Seen at signal level 1 only (the
  bench sits at -79 dBm); levels 2 and 3 are checked against the bitmap, not
  yet seen on the panel.
- Taps at x 128, 142, 176, 198, 213 and 232 (y 22) all open Settings; x 120,
  on the title, does not. One capture at x 176 was taken before the screen
  had switched and read as a miss; three repeats with a second's settle all
  opened Settings.

## 2026-09-11 - the nearest access point, not the first (released in 1.48.0)

### Fixed

- Benoit: the Wi-Fi icon is low on every TigerSpool, while the TigerScale next
  to it shows full reception. Not a measurement error - the formula is the
  scale's own - and not the antenna: the device's own scan heard the network
  at -47 dBm while it was associated at -79. It was on another access point.
  The driver's default WIFI_FAST_SCAN takes the first radio it hears with the
  right name, and the cached BSSID brings it back there on every reconnect and
  reboot. The TigerScale had found exactly this (-79 on CC:BA:BD:83:47:90
  against -35 on 10:5A:95:74:DE:50, the same two radios) and fixed it with an
  erased config plus WIFI_ALL_CHANNEL_SCAN and WIFI_CONNECT_AP_BY_SIGNAL. Ported
  as `staBegin()`, used at all three station connects; the portal's join sets
  the scan and sort without erasing anything.
- The "unreachable device, 97% pings lost" of 2026-09-10 was put down to "the
  radio link at -75 to -88 dBm, not this firmware". The link was that weak
  because of this; the comment says so now.

### Added

- `[wifi] OK` logs the signal, BSSID and channel. `/api/scan?all=1` lists every
  radio the device hears - SSID, BSSID, RSSI, channel, security - from a fresh
  scan.

### Verified on hardware

- First boot with the fix: still CC:BA:BD:83:47:90 at -72 - the one miss. Every
  boot after it, four of them: 10:5A:95:74:DE:50, ch 11, -45 to -54 dBm.
  The full scan shows why the choice matters: that network is on three radios
  here, at -46 (ch 11), -72 (ch 6) and -84 (ch 1).
- Pings once settled: 0/60 lost, 16 ms average, 99 max - against 20% lost and
  545 ms average on the far radio.
- The header's Wi-Fi icon at -45 to -50 dBm shows two arcs of three: full needs
  -40 or better, the same thresholds as the TigerScale. The level-2 clip is
  therefore now seen on the panel too, outer arc's tips unlit.

## 2026-09-11 - full signal from -60 dBm (released in 1.48.0)

### Changed

- `wifiLevelFromRssi()`: 3 from -60 dBm, 2 from -70, 1 from -80, else 0 -
  Benoit's call, after the bench at -45 dBm showed two arcs of three. It was
  the TigerScale's arithmetic, kept identical on purpose; the two products now
  differ, knowingly. The portal's `bars()` moved with it.

## 2026-09-11 - full brightness out of the box (released in 1.48.1)

### Changed

- Brightness with nothing stored is 100%, not 80% (`BRIGHTNESS_DEFAULT` in
  main.cpp) - Benoit's call: the setup screens are the first impression, and
  lowering it is one row under Display. The key `bright` is written by any
  Display change and by the rotate button on the first-boot language screen,
  so a device that has done either keeps its value; one that never did gets
  100% after updating.

## 2026-09-14 - what a Creality slot is sent, and from where (released in 1.49.0)

### Fixed

- `CrealityBackend::assign()` sent `rfid "0"` (never a Creality material id),
  the chip's temperatures or 190/230, the raw material as the name, and no
  pressure/selected/percent/editStatus/state. It now sends the TigerTag Connect
  app's frame with every value resolved by `filament::resolve()`:
  temperatures (as a pair) endpoint -> chip -> table -> 190/240; `rfid` and
  pressure endpoint -> table -> "0"/0.04; name endpoint `crealityLabel` ->
  "Generic <material>". The endpoint counts only for a TigerTag+
  (protocol 3155151767) and only for its own product id. Invalid values fall
  through: a pair with a 0 or min > max, a string null/""/"-", a pressure that
  is not a positive number or numeric string (the endpoint sends it as "").
- The endpoint (`product/get?uid=<uid as decimal>&product_id=`) is asked by
  `product_api` on a core-0 task the moment a TigerTag+ is read for a Creality,
  so the answer is normally in before Send; Send waits only while that fetch is
  inside 3 s, and a late answer is cached for the next time, never sent.
- The material table keeps `metadata.crealityID`,
  `metadata.crealityPressureAdvance`, `recommended.nozzleTempMin/Max`: in the
  downloaded file (PSRAM, beside the labels) and in the compiled header
  (`TT_MATERIAL_INFO`, emitted by gen_db.py). `tt_db::materialInfo()` answers
  from the same layer as `material()`. db_update.py needed no change - it
  already stores the API's JSON whole; gen_db.py is what compiles it.
- The immediate re-read after a write is deferred 1.5 s on Creality: the
  printer answers boxsInfo with the old slot for about a second. main.cpp's
  generic refresh() after Send lands in that window, so refresh() itself
  reschedules rather than assign() just not calling it.

### Changed

- The TLS stand-down is split: every TLS request still holds new links back,
  but the product fetch stands links DOWN only if the largest free block was
  under 48 KB when it started. Measured with a temporary loop timer: during a
  fetch the worst loop pass stayed at 115-117 ms (baseline ~100); in the three
  seconds AFTER it, 776-952 ms - the five stood-down links dialling back, the
  Anycubic's TLS connect blocking the loop - landing exactly on Send. The
  stand-down itself takes 7-10 ms and now says so in its log line.

### Added

- `/api/resolve?protocol=&product=&uid=<hex>&material=&nozmin=&nozmax=`: the
  whole resolution from query parameters, without a spool and without sending.

### Verified

- Host test (clang, ArduinoJson from .pio/libdeps, the generated header with a
  stub Arduino.h), with the real endpoint answer and injected variants: all
  eight scenarios of the brief plus nine validity cases pass. One difference
  from the brief's table: `id_material[38219]` carries
  `crealityPressureAdvance: 0.04`, so pressure resolves `0.04(db)`, not
  `0.04(default)` - same value, the source the rules give.
- Device, `/api/resolve`: plain TigerTag -> `requested:false`, no network;
  TigerTag+ -> handler answers in 40-109 ms while the fetch runs (861-1000 ms),
  then `215/230(api) 00001(api) Generic PLA(api)` from the cache; material not
  in the table -> `0(default)`, `190/240(default)`.
- Ender-3 V4 + CFS at 192.168.40.103, a real TigerTag+ spool (product
  1127944810, R3D PLA High Speed, chip 215-230): fetch 861 ms at read, Send
  resolved `rfid=00001(api) temp=215/230(api) pa=0.04(db) name="Generic
  PLA"(api)` and sent the frame field for field. Read back over the printer's
  own WebSocket from the Mac: slot 1D took type, vendor, name and colour.
  It reported `rfid "0"`, `editStatus 0`, `state 0` - 1D holds no spool, and
  every empty slot on that CFS reads back the same way - and minTemp/maxTemp
  0, which boxsInfo reports for every slot including one with rfid "00004".
  Whether a slot holding a spool keeps the rfid is not verified yet.
- Then every slot (Ext, 1A-1D, three of them holding spools), through the
  TigerSpool with the same spool: type, vendor, name, colour and pressure
  landed everywhere; `rfid` read back "0" everywhere - 1A's previous "00004"
  included, so the printer does write that field and is refusing ours.
- Why, found by sending variants straight from the Mac to slot 1C and reading
  back: the printer keeps an rfid ONLY when vendor, type and name match its own
  entry for that id.
  `00001` + PLA + R3D + "Generic PLA"            -> rfid "0"
  `00001` + PLA + Generic + "Generic PLA"        -> rfid "00001" kept
  `00001` + PLA + Generic + "R3D PLA High Speed" -> rfid "0"
  `01001` + PLA + R3D + "Hyper PLA"              -> rfid "0"
  `01001` + PLA + Creality + "Hyper PLA"         -> rfid "01001" kept
  Pressure advance is stored either way (0.055 and 0.066 read back as sent).
  minTemp/maxTemp read back 0/0 in every case, kept rfid or not - boxsInfo does
  not report them, so whether the printer stores them cannot be seen from here.
- So the brief's frame - vendor = the tag's brand, type = the material label -
  can never keep a Creality id for a spool that is not Generic. Open, for
  Benoit: send vendor "Generic" / type = material family / name =
  crealityLabel when a Creality id is resolved (the printer then shows
  "Generic PLA", not R3D), or keep the brand and label and lose the id.
- Every slot was put back to its values from before the tests and read back
  identical, 1A's "00004" included.
- Where a Creality printer keeps temperatures, looked for rather than assumed:
  - boxsInfo reports minTemp/maxTemp 0 for every slot an application wrote,
    rfid kept or not. Tiger Studio's RETRO.md has the one non-zero case: a
    slot the CFS read from a Creality spool's own tag, 01001, 190/240.
  - `reqMaterials` on this Ender-3 V4 returns its material library: 18
    profiles, each an id with a brand, a name, a type and a temperature window
    (00001 Generic / Generic PLA / PLA 190-240; 01001 Creality / Hyper PLA /
    PLA 190-240; 00004 Generic ABS 240-280; ...). That library is what the
    vendor+type+name check above is checked against.
  - Klipper's own state over Moonraker (`box`, `filament_rack`) holds, per
    slot, `material_type` (the id, "000004"), `color_value` and `vender` - and
    no temperature field at all.
  So on this printer a slot's temperatures exist only as the library profile
  its id points to. The minTemp/maxTemp in modifyMaterial are sent (the log
  shows 215/230) and nothing on the printer that can be read back holds them.
  Benoit's call for now: keep the tag's brand and label, so the id is dropped
  and no profile applies. Whether the printer's own screen shows the sent
  window is the one check left, and it needs someone at the printer.

## 2026-09-14 - slot screen text in white (released in 1.49.0)

### Changed

- screen_slots.cpp: the slot name above each colour block and the brand under
  it are theme::TEXT, not TEXT_DIM - Benoit's request; at 12 px on black the
  grey read poorly.

## 2026-09-14 - a Creality type is the material family (released in 1.49.0)

### Changed

- Benoit's call after reading the printer by hand: the frame's `type` is the
  material table's `material_type` for the chip's idMaterial ("PLA" for 24629
  "PLA High Speed"), falling back to the label when the table has none (four
  materials carry "" today). Carried like the other fields: `MaterialInfo`,
  the downloaded table, `TT_MATERIAL_INFO` (gen_db.py validates it against the
  UI font, since the printer echoes it back and the slot screen draws it), the
  resolver (`typeSrc`), the `[creality]` log line and `/api/resolve`. Vendor
  and name unchanged: the tag's brand and the endpoint's label, for now.

### Verified

- Host test: five type cases added (24629 -> PLA db, 38219 -> PLA db, 425
  ABS-CF -> ABS db, 51007 empty type -> label, unknown id -> label); all pass
  with the earlier ones.
- Ender-3 V4, the same R3D spool to 1D through the TigerSpool:
  `type=PLA(db) rfid=00001(api) temp=215/230(api) pa=0.04(db) name="Generic
  PLA"(api)`; read back type "PLA", vendor R3D, name, colour, pressure - and
  rfid "0", as expected with vendor R3D (the library match needs Generic).
  1D put back to its original values.

## 2026-09-14 - a filled material's type carries its filler (released in 1.49.1)

### Changed

- Benoit: Creality's type is `material_type`, plus "-" and `filled_type` when
  that is given - 425 ABS-CF is "ABS" + "CF" = "ABS-CF"; a null (or "", "-")
  filled_type adds nothing. `filledType` is carried beside `materialType`
  through `MaterialInfo`, the downloaded table, `TT_MATERIAL_INFO` (font-
  validated) and the resolver, which composes it.
- The rule reads `filled_type` only, not the `filled` flag, and the two
  disagree for eight materials today: PES, PETG-PTFE, PEI-9085, PAHT, PA11-GF,
  PETG-ESD and PLA-ESD are `filled: true` with no filled_type (sent as the bare
  family - PA11-GF as "PA"), and PC-PTFE is `filled: false` with filled_type
  "PTFE" (sent as "PC-PTFE"). Upstream data, for the TigerTag database.

### Verified

- Host test: 425 -> ABS-CF, 10738 -> PC-PTFE, 6605 -> PA, 38219 and 24629 ->
  PLA, a "-" filled_type -> no suffix; all earlier cases still pass.
- Device, downloaded table, `/api/resolve`: 425 ABS-CF (db) rfid 00004
  240/280; 38219 PLA; 24629 PLA; 10738 PC-PTFE; 6605 PA.
- Benoit corrected the source in Xano; `db_update.py` pulled it
  (filament_materials 1781878743554 -> 1789347116788). Changed: PA11-GF
  filled_type null -> "GF" (now "PA-GF"), PC-PTFE filled false -> true (type
  unchanged, "PC-PTFE"); density null -> 0 on PEI-9085, PAHT and PES, which the
  firmware does not read. Still `filled: true` without a filled_type: PES,
  PETG-PTFE, PEI-9085, PAHT, PETG-ESD, PLA-ESD. Header regenerated.

## 2026-09-14 - temperatures need a decimal point (released in 1.49.2)

### Fixed

- Benoit reported the TigerTag RFID Connect app's temperatures are kept by a
  Creality and the TigerSpool's are not. Slot 1C, written from his iPhone,
  read back minTemp 190 / maxTemp 240 with rfid "0" - so the printer DOES store
  per-slot temperatures, and the earlier conclusion in this file ("nothing on
  the printer holds them", "only the library profile") was wrong.
- The app's code (tigertag_connect1, creality_websocket_page.dart) builds the
  temperatures as Dart doubles, which jsonEncode writes as "190.0". The
  TigerSpool wrote integers. Sent straight to slot 1D from the Mac, the same
  frame otherwise: 215.0/230.0 -> read back 215/230; 216/231 -> 0/0;
  217.0/232.0 -> 217/232. The printer keeps a temperature only when the JSON
  number has a decimal point.
- `withPoint()` in backend_creality.cpp writes minTemp, maxTemp and pressure
  with one ("215.0", "0.04") and hands them to ArduinoJson as serialized text,
  since ArduinoJson writes the double 215.0 as "215".
- Tiger Studio has the same trap: PROTOCOL.md's table says float but its
  example writes 190, and renderer/printers/creality/index.js sends a JS number
  that JSON.stringify writes as 190. Not tested from Studio.

### Verified

- Ender-3 V4, the Duramic 3D PLA chip the iPhone wrote to 1C, through the
  TigerSpool to 1D: frame `"minTemp":190.0,"maxTemp":240.0`; read back 190/240,
  same as the app's write to 1C.

## 2026-09-14 - slot names in bold (released in 1.49.3)

### Changed

- screen_slots.cpp: the slot name above each colour block is font_ui_bold_16
  (Montserrat SemiBold), up from font_ui_12 regular - Benoit's request. 16 is
  the only bold size compiled in.

### Verified

- Bench: X1C (Ext., B1-B4) and Anycubic Kobra X (A1-A4) grids drawn in bold,
  nothing clipped. Not new, but a little closer now: the slot body does not
  scroll, and a printer with a third row of slots (a Bambu with two AMS, an
  Anycubic with more ACE units) already ran past the bottom of the panel.
- The brand under each slot, when it does not fit the 51 px cell, is cut
  with a single "." (`setFittedText()` in screen_slots.cpp, whole UTF-8
  characters, trailing space dropped before the dot) instead of LVGL's "...".
  Applies to every printer's slot screen. LV_LABEL_DOT_NUM left alone: it would
  change every truncated label in the product. Bench: "Duramic 3D" under the
  Ender-3's 1C and 1D reads "Durami.".

## 2026-09-14 - the Wi-Fi screen, redesigned (released in 1.50.0)

### Changed

- `screen_settings::showWifi()`: a network card (the header's wave via
  icons::wifiWave/setSignal, SSID in font_ui_bold_16, the level as a word in
  its colour - Excellent/Good OK green, Fair orange, Weak red, from the same
  -60/-70/-80 thresholds - with the dBm at 12 px beside it) and a details card
  (IP, MAC, channel at 14 px, dim name / white value). The wave, the word and
  the dBm update in place; the screen rebuilds only when the network, address,
  connection or channel changes. New parameter `channel`
  (WiFi.channel()); strings S_SIG_EXCELLENT/GOOD/FAIR/WEAK and S_CHANNEL in all
  eight languages. Previews `setwifi`, `setwifi-fair`, `setwifi-none`.

### Verified

- Bench: the three previews and the live screen (Stargate, Excellent,
  -47 dBm, channel 11) captured over /screen.bmp; nothing clipped.
- Details card rows (IP, MAC, channel) in font_ui_bold_16, name and value -
  Benoit's request. MAC fits with ~20 px to spare.
- The portal screens opened from Settings > Wi-Fi > Change network had no
  header and no exit. `screen_setup::showWifi()` / `showPortalReady()` take
  `withBack`; main.cpp passes it only when the portal was opened from Settings
  (`s_apFromSettings`), since a first boot has nowhere to go back to. Back
  calls the new `webcfg::endAP()` (captive DNS off, softAP down, STA mode),
  restores persistence and auto-reconnect that beginAP() turned off,
  `staBegin()` to rejoin the saved network, and returns to ST_SET_WIFI. The
  join screen's QR is 112 px instead of 132 and its spacing tighter under the
  header, or the password ran off the bottom. The setup header's chevron and
  title now match frame::build (white 24 px, bold title) on every titled setup
  screen. Previews `apwifi`, `apportal`.
- Verified: the three previews fit (captured). The back arrow itself is NOT
  verified from here: opening the portal takes the bench off the network the
  capture and tap API are reached over.
- A drawn padlock (icons::LOCK, added at the end of the enum so no existing
  value moves - FontAwesome's lock is not in the compiled symbol set) before
  the setup access point's password, on first boot and from Settings alike.
  Benoit's point: two accent lines under each other read as one name.
  Captured on the bench.

## 2026-09-15 - Chinese (released in 1.51.0)

### Added

- LANG_ZH, last in enum Lang so every stored index keeps its meaning (no
  LANG_SCHEMA bump - appending needs none). A ninth column on all 148 rows of
  the STR table; vocabulary aligned with the TigerScale's Chinese column
  (设置, 账户, 打印机, 耗材/料盘, 更新...). The NFC tester rows that are the
  same in every language stay English. NAMES: "中文". The Wi-Fi portal page
  gets a zh block and a "zh" language code; the four-column legacy page table
  still falls back to English for it, as it does for four other languages.
- The font, as the TigerScale does it: a subset of Noto Sans SC Medium
  (Sans2.004, OFL 1.1) holding only the characters the source uses, listed by
  the new scripts/cjk-chars.py (216 glyphs today), merged with --symbols into
  every regular and bold UI face by make-ui-font.sh - so no fallback chain is
  needed. Unlike the bold face the download is required: faces rebuilt without
  it would overwrite the committed ones, so the script exits 3 instead.
- gen-font-range.py records the subset in font_range.json ("symbols") and
  checks every face carries the same one; font_range.py adds it to the allowed
  set, so check-ui-fonts.py fails on a Chinese character the faces lack.
- Flash: 2 189 201 bytes, 52% of an OTA slot (was ~1.99 MB).
- README, firmware/README, installer page ("nine"), THIRD_PARTY_LICENSES,
  CLAUDE.md, CODEMAP updated.

### Verified

- Bench, language set to Chinese: settings menu, Wi-Fi screen, setup QR screen
  with the padlock, printer list with the load gauge, the slot grid (外置), and
  the language picker scrolled to 中文 - all drawn, no boxes. Put back to French.
- Benoit, in Chinese: the home screen's header still said "Imprimantes". The
  home screen builds its header once, with the screen, and rebuilds only the
  list - so the title kept the language the device booted in, in any language,
  not only Chinese. s_title is now updated whenever the language differs from
  the one it was drawn in, and the language is part of the list's signature so
  its own words follow too. Bench, from the home screen without a restart:
  打印机 -> Imprimantes -> 打印机.

## 2026-09-15 - language names in bold (released in 1.51.1)

### Changed

- screen_setup::showLanguage(): each language's own name in font_ui_bold_16,
  up from font_ui_14 - Benoit's request. The bold face carries the Chinese
  subset like every UI face (gen-font-range.py fails otherwise), so 中文 draws
  bold too. Captured on the bench: English to Italiano in bold.
- Seen in the same capture, not changed: on the first-boot picker the French
  title "Choisissez votre langue" runs into the rotate button at the right of
  the header.

## 2026-09-15 - a one-word language title (released in 1.51.2)

### Fixed

- The first-boot language screen's title was S_CHOOSE_LANG, which in French
  ("Choisissez votre langue") ended 2 px from the rotate button. Benoit chose
  the one-word title: S_LANGUAGE on both paths, S_CHOOSE_LANG removed (its
  Chinese characters stay in use elsewhere, so the faces did not change).
  Bench, first-boot preview: titles end by x 77 in fr, it, pt-PT, pl, de, zh;
  the button starts at x 201.

### Noted

- Issue #5 opened at Benoit's request: with several access points on one SSID
  the device does not reliably pick or keep the strongest - staBegin()'s sort
  applies only at association and nothing roams afterwards. To study later.

### Removed

- The rotate button on the first-boot language header, at Benoit's call: it
  read as refresh, and loadScreenPrefs() already takes the orientation from
  the accelerometer on a first boot. screen_setup::takeRotate(), its press
  handler and the ST_LANG code that flipped the rotation went with it. Display
  still sets orientation and auto-rotation. Captured on the bench: header with
  the title alone.

## 2026-09-15 - bambuID, and the external spool Bambu actually accepts (released in 1.52.0)

### Changed

- Bambu uses the resolver: `tray_info_idx` from the TigerTag+ endpoint's
  metadata.bambuID, then the table's metadata.bambuID (80 of 113 materials
  carry one), then the old keyword guess (`bambuMat`, now the fallback only);
  `tray_type` from the table's material family (+ filled_type), else the
  guess; nozzle temperatures by the Creality order. bambuId added to
  ProductData, MaterialInfo, the downloaded table, TT_MATERIAL_INFO and
  `/api/resolve`; the resolver leaves it empty rather than guessing, since the
  generic fallback is a Bambu fact. The product lookup now also starts for a
  TigerTag+ read for a Bambu in LAN mode. Log line
  `[bambu] Ext type=PLA(db) id=GFL99(api) temp=215/230(api)`.
- Note on this spool: the endpoint gives GFL99 (Generic PLA) while the table
  gives 24629 PLA High Speed GFL95 - the endpoint wins, by the rule.

### Fixed

- The external spool was sent as ams_id 255 / tray_id 254 - the ids the report
  uses. On the X1C (firmware 01.12.00.00, LAN mode) that command is accepted
  and IGNORED: after the TigerSpool's send the tray stayed grey for minutes on
  the device's own session. ams_id 255 / tray_id 0 / slot_id 0 - what current
  Bambu Studio sends - lands within seconds. slot_id also goes on AMS trays,
  equal to tray_id. The slot map keeps 255/254 for reading the report.

### Verified

- Host test: seven bambuID cases (endpoint wins; endpoint without id -> table;
  no answer -> table; plain TigerTag ignores the endpoint; table without id ->
  none; "-" and null absent) pass.
- X1C at 192.168.20.181, LAN mode, the R3D PLA High Speed TigerTag+ spool,
  through the TigerSpool to Ext with the new addressing: read back from the
  Mac over the printer's own MQTT, 28 consecutive reports GFL99 / PLA /
  DC123FFF / 215-230; the device's session saw grey -> red. Ext put back to
  its original GFL99 / PLA / 808080FF / 190-240 and confirmed.
- How the old addressing was found to be ignored took a wrong turn worth
  keeping: the FIRST report after a new MQTT connection carries an empty
  vt_tray (color 00000000, temps 0) before the real one, so reading only the
  first report looked like "the command cleared the tray". The device's own
  log shows the same empty report periodically ("Ext: - #000000" followed
  immediately by the real tray) - the slot grid can flicker empty for a
  moment. Not fixed here.

### Not verified

- An AMS tray write with slot_id, and the new external-spool addressing on
  other models (A1, P1P, P2S) or older firmware: none was in LAN mode on the
  bench.

## 2026-09-18 - the Wi-Fi wave: a scale for this radio, and no more twitching (released in 1.60.0)

- Benoit: the ESP32 hears poorly, users read "Weak" and worry, shift everything
  by 10 dB. Fair, and the bench supports it: two of our boards five centimetres
  apart reported -47 and -60 dBm on the same access point, so the absolute
  figure cannot be compared with a phone's and a scale built for the phone's
  numbers describes a working device as a failing one. Thresholds are now
  -70 / -80 / -90, in icons.cpp and in the portal's own copy. The Wi-Fi screen
  still prints the dBm beside the word, so nothing is hidden - said plainly to
  Benoit: at -85 the screen will read "Fair" while the link is genuinely weak,
  and the number beside it is what will explain a support case.
- Then: the wave moved every second when the signal wandered across a boundary
  (-70, -71, -70). Two mechanisms, in ui/signal_level.h - header-only and free
  of LVGL on purpose, so the arithmetic can be run on a computer:
  - the average is TIME-based, one sample a second, not one per call. The
    screens ask for the level on every pass of the main loop, tens of times a
    second, and a per-call average converges before the wobble it exists to
    absorb has happened.
  - an arc has to be earned: 3 dB past a boundary to gain one, 3 dB back to
    lose one. A jump of more than 15 dB bypasses both - that is the box being
    moved, not noise.
- Verified on a host build rather than by waiting for the right signal: 20 s of
  -69/-72 gives ZERO changes, 12 s of -79/-82 gives zero, a slow slide from
  -50 to -89 gives two in the right direction, and an abrupt -45 to -88 shows
  immediately. On the device, the -76 dBm fixture now reads "Bon" with two arcs
  where it read "Moyen" in orange.

## 2026-09-18 - the interface on its own task (released in 1.59.0)

- Benoit, on the plan to move the printers onto a task: why not move the
  INTERFACE instead, and leave what is already on core 0 - the Wi-Fi stack, the
  account sync, the product lookup - with the memory it has? He is right, and
  for a reason I had missed: the freeze is not about which core the network
  runs on, it is that ONE task does the network and the drawing. Move either
  one and the screen keeps moving; moving the interface is far less dangerous,
  because the interface READS state and does not write it. Issue #6 rewritten
  around that.
- Done: a task whose only job is lv_timer_handler(), core 1 beside the loop, at
  priority 2. A blocked task yields, so it draws while the loop waits in a
  socket. Nothing on core 0 moved.
- The three hazards, each answered rather than hoped over:
  - LVGL is not reentrant. lvgl_port::Lock (a recursive mutex) is taken by all
    43 public functions in ui/ and by webcfg's preview builders, its repaint
    and its read of the canvas sprite. The old CODEMAP rule - "safe because the
    web server and the drawing are the same loop" - is no longer true and now
    says so.
  - Flags set from an LVGL event callback are now set on the drawing task and
    read on the loop: s_tapped, s_back, s_pick and eighteen others are
    volatile.
  - Internal RAM. The stack is 8 KB and uses 3.1 of it (measured with
    uxTaskGetStackHighWaterMark), leaving the largest free block at 29 KB -
    comfortably over the 16 KB the account sync needs contiguous.
- Measured, same bench and same six printers as 1.55.0 and 1.58.0:

  | | 1.58.0 | now |
  |---|---|---|
  | screen blind | 7.9 s / 80 s | 1.2 s / 80 s |
  | worst gap | 1 324 ms | 105 ms |

  The target in issue #6 was under 1 s and under 100 ms; this is within a
  rounding error of both, and the remaining gaps are the loop holding the lock
  while it builds a screen.
- Checked by hand as well: navigation, back, /api/tap, /screen.bmp and the
  ?preview= renderers all still work, which is what a missed lock would have
  broken first.
- Benoit asked the right question before publishing: have you checked you added
  no bug, and will the OTA still work? Both tests found one.
  - **The OTA failed.** With the drawing task's 8 KB gone from internal RAM,
    the download opened its handshake with 24 KB free and 11.5 KB contiguous
    and got HTTP -1. The control - v1.58.0 rebuilt as 1.57.0 and offered the
    same update - connected with 34 KB and 20 KB and installed. So the task
    pushed a budget over the edge, and the budget was already the problem:
    applyTask() sets the DOWNLOADING state and reaches the handshake in
    milliseconds, while the stand-down that frees the memory runs from the
    loop. It was a race, and published firmware wins it by luck. Now the task
    announces the state and WAITS for a 20 KB block, up to three seconds: the
    links step aside in 16 ms and the handshake opens with 42 KB. Retested end
    to end - downloaded, written, verified, rebooted into 1.58.0. The drawing
    stack also came down from 8 KB to 6.
  - **Sleep broke the touch panel.** After the screen had slept once, no tap
    was acted on again. sleepTick() polled the touch controller from the loop
    to notice the wake, while the drawing task polls the same controller
    through LVGL every 5 ms - two tasks on one I2C bus, which is hazard number
    two from the issue, live. The wake now reads the timestamp touchCb()
    already writes, so there is one reader again. Retested: first tap wakes and
    is consumed, second tap acts.
  - Also verified after both fixes: all 30 previews render, navigation and
    /api/tap answer, and a real spool write - PLA High Speed from R3D onto the
    Ender-3's slot 1C, 215/230 - lands.
  - The first-boot flow end to end: Benoit walked a freshly erased board
    through language, portal, Wi-Fi, account pairing, printer import and a
    spool - "tout a bien fonctionné, aucun freeze". That is the one part this
    bench could not drive, and it is now covered.
  - Worth knowing, from the log of that walkthrough: after a full chip erase
    LittleFS fails to mount and prints three red lines, formats itself, and the
    reference tables arrive on the next boot - 293 entries from 7 files. Not a
    fault, but it looks like one in a log. And the tables' last_update call
    answered HTTP 503 once during setup, which is the TigerTag API rather than
    the device; it recovered on its own.
- And the loading spinner on the printer import: 40 px with 24 px of padding
  left 16 px of arc, high and off-centre - the same trap as the battery bar,
  padding on an arc coming out of the arc. 64 px, centred, with the words under
  it.
- One guard corrected on the way: check-ui-translated.py reported the FreeRTOS
  task name "ui" as untranslated on-screen text. A task name is an identifier
  for a debugger; the guard now skips xTaskCreate* the way it already skipped
  Serial.

## 2026-09-18 - the roaming, tested rather than trusted (released in 1.58.0)

- PR #9 (roaming, disconnect reasons, RF tuning) and #3 (the hardware table)
  merged, after review. What the bench said about #9, in order:
- The fault is real and was reproduced: the device sat on an access point at
  -73 dBm while another with the SAME SSID was at -48, and roamCheck() moved
  it. Benoit's network has three APs per SSID, at about -45, -73 and -85.
- A roam costs a reconnection. Timestamped: staBegin() at t=103852 ms, the six
  printer links back between t=107713 and t=118743 - four to fifteen seconds,
  every link dropped. The PR claimed no disruption; it is not true, and the
  CHANGELOG says what actually happens.
- No ping-pong: 25 scans across two boards, 13 of them with the hysteresis cut
  from 8 dB to 1, and not one unjustified move. Not a proof of the 8 dB - at
  this spot the other two APs are 25 dB behind, so nothing can compete - and
  that limit is stated rather than glossed over.
- Four reboots, four times the right AP: the scan-by-signal choice works, so
  the wrong association is occasional rather than systematic.
- A correction of my own: I read a 24 dB gap between two boards 5 cm apart as
  evidence for HT20, and it was not. Swapping the firmware between the boards
  left the gap on the same BOARD - 13 dB on the same BSSID - so it is that
  unit's radio, not the bandwidth setting. And the two boards were on
  different SSIDs at the time, which Benoit pointed out; the A/B was worth
  nothing until he put them on one network.
- Added, because a roam mid-write is the one thing this feature could cost a
  user: roamCheck() holds while sendWaiting is set and on ST_SCAN, ST_REVIEW
  and ST_RESULT. Verified with a temporary log: four scans in the minute
  before, ZERO in 75 s on the review screen, four again in the 40 s after
  leaving it.
- And the write itself, with roaming live: a PLA High Speed from R3D onto the
  Ender-3's slot 1D, temperatures 215/230 with their decimal point, the grid
  showing R3D where it showed Durami before.

## 2026-09-17 - telling a battery from a charger (released in 1.57.0)

- Benoit, on a board he says has no cell: it showed 73%. It reads 4.01 V, and
  the presence test was a threshold at 4.24 - taken from the only other
  battery-less board there was, whose rail sits at 4.27. One board's charger
  floats lower than another's, so the threshold was never going to hold.
- What separates them is not the level but the MOVEMENT. A cell drifts: 2.4 mV
  a minute at the pin while charging, about 5 while running the device. A rail
  held by a charger with nothing to charge does not move at all. So the level
  still answers at boot, and a drift test settles it: the reading, smoothed,
  compared with itself ten minutes later, 6 mV of movement deciding it.
- First attempt compared raw readings over a 2 mV window and declared "there is
  a cell" within a minute on a board that has none: the ADC wanders 4-5 mV
  between samples, which is more than a charging cell drifts in a minute. The
  smoothing is what makes the test mean anything.
- The verdict is kept in NVS (bnocell), so a board settles the question once.
- Then Benoit asked the right question: can the board not simply say? The
  schematic answers it. The charger is an ETA6098; its STAT output (pin 9)
  drives a red LED through R13 and goes nowhere near the processor, and the
  battery connector J4 is two wires. The information exists, it lights an LED,
  and it is not wired to the MCU. (Rendering the schematic to a PNG with
  qlmanage and reading it is how this was settled - the PDF's text layer is one
  character per draw call and greps to nothing.)
- But the schematic gave the answer anyway: the charger is a SWITCHER, with a
  2.2 uH inductor, and its output ripples. A cell on the connector is an
  enormous capacitor across it and swallows the ripple. Measured, eight reads
  back to back, both boards on USB: 1.8-3.2 mV with a cell, 6.6-21.8 mV
  without, peaks over 50. The noise that had defeated the first attempt was the
  signal.
- So presence is decided on ripple, smoothed, with a band between 4 and 6 mV
  where nothing changes. It answers in a second instead of ten minutes, and it
  also settles the case the drift test got wrong - a FULL cell on a charger,
  which is perfectly still and still damped. Verified on both boards at once:
  "ripple 3.2 mV - a cell is damping it", 77%, against "(no battery)" on the
  other.

## 2026-09-17 - the AMS HT, and a report nobody was reading (released in 1.56.0)

- A user (mediastorm2000) reported that an AMS HT is not enumerated on an X1C
  that shows its two AMS Gen 1 and its external spool, and sent the full MQTT
  dump. The dump settles it: the units arrive as id "0", "1" and "128", the
  HT holding a single tray of PLA-CF, and `tray_exist_bits` is "100ff" - bit 16
  for the HT rather than a bit inside the AMS range. rebuildMap() dropped
  anything over id 3.
- Now: ids 0-3 are AMS units of four trays, 128-131 are HT units of one, BMAX
  is 21 and the labels are A1..D4 and HT1..HT4. Letters for the HT would have
  been a guess at what Bambu Studio shows; HT1 is not.
- Verified by REPLAYING the user's dump onto our own X1C's report topic:
  mosquitto_pub to device/<sn>/report on the printer's broker, with the
  printer's own certificate pulled by openssl s_client, and the device saw it
  as a report from that printer. The log then read "AMS: 3 unit(s) -> 10
  slots" and "HT1: PLA-CF #F72323" - the external spool, A1-A4, B1-B4 and HT1.
  Worth keeping as a technique: a bug reported with a dump can be reproduced on
  hardware nobody here owns.
- That replay found a second fault immediately: the first publish changed
  nothing, because a background Bambu had an 8 KB MQTT buffer and the report is
  8.9 KB. PubSubClient drops an oversized message silently. So a printer with
  two AMS was only ever updating its slots while it was the printer on screen.
  The background buffer is 20 KB now; it lives in PSRAM at that size.
- Not verified, and it needs the reporter's hardware: WRITING to an HT slot.
  The command goes out as ams_id 128, tray_id 0, which is what the mapping
  implies, but nothing here can confirm the printer accepts it.

## 2026-09-17 - what the interface spends its time on (released in 1.55.0)

### Measured

- Benoit: find the freezes, everywhere. Instrumented the loop on the bench
  board (six printers imported, several switched off) and measured the thing
  that matters, which is not the length of a pass but the gap between two
  LVGL frames - the screen going blind is what a finger feels.
- Baseline, 75 s of running: 76 gaps over 40 ms, median 74 ms, worst 1318 ms,
  8.9 s blind in total. That is 12% of the time with the screen dead.
- Where it goes: the printer backends, 90 slow passes in 90 s. Broken down,
  backend 0 (a Bambu, MQTT/TLS) 61 of them, median 58 ms, and the spikes of
  1.0-1.4 s are TLS handshakes while dialling. The JSON parsing is NOT the
  cost - under 10 ms with the filter - it is the socket read and the wait for
  the rest of a 30 KB report.

### Changed

- net/buffered_client.h: a 1 KB read buffer in front of any Client, because
  PubSubClient reads one byte at a time and over TLS each of those goes into
  mbedtls. In front of the Bambu socket it took that backend from 58 ms median
  to 44.
- A frame is drawn BETWEEN printers now, not after all of them, so the gap is
  capped at one printer's turn rather than the sum of six.
- The loop slept a flat 15 ms per pass, including passes where LVGL had an
  animation in hand. It now sleeps for what lv_timer_handler() asks for,
  capped at 15.
- Together: 7.9 s blind per 80 s, median gap 56 ms, worst unchanged at 1.3 s.

### Fixed, from Benoit's screen

- Benoit: the account says it is connected but the icon is orange. It was
  right: health() returns 2 when the last exchange failed, and the log had
  "[account] xTaskCreate failed - sync skipped" every minute. 50 KB free, and
  the largest block 16 372 against the 16 384 the sync task's stack needs.
- The deadlock behind it: standDownForTls() frees internal RAM when
  needsTheRoom() is true, and that reads ttcloud::asyncBusy() - which is only
  set once the task exists. A sync that could not start therefore never asked
  for the room that would have let it start. ttcloud::needsRoom() now exists,
  is set before the attempt when the largest block is under the stack plus a
  margin, and is read by needsTheRoom() like product_api's already was. The
  stack size is one constant now, SYNC_STACK, rather than a literal in the
  task and a figure in a comment.
- Verified on the bench board: links stood down, the sync ran to completion in
  11.7 s (13 LAN printers, 4 cloud), and the account icon went green.

### Still open

- The worst gaps are TLS handshakes to printers that are switched off, and no
  amount of buffering touches them: they are blocking connects on the UI loop.
  The fix is to move printer network IO onto its own task, which is a chantier
  of its own - the backends hold state that screens read, so it needs a
  boundary drawn first. Proposed to Benoit, not started.
- Leftovers in Portuguese, found while reading the backends and translated at
  Benoit's request: twelve status strings across five backends ("Bambu: slots
  atualizados", "K2: ligado", "FF: autenticado", "Snap: parado" and their
  kind), four comments, and one more in tigertag_cloud.cpp. Worth knowing:
  status() is not called anywhere - no screen, no log, no page reads it - so
  none of this was ever visible. It is still committed non-English text, which
  is what the rule is about.
- And the guard that should have caught them learned the words it walked past:
  atualizado, ligado, parado, subscrito, autenticado, relendo, confirma,
  assentar, topologia, vazio, deixa, campo, validar. The last of them found a
  thirteenth leftover on the first run.

## 2026-09-16 - the battery (released in 1.54.0)

### Added

- Benoit asked for step 1 of the battery work - measure the pin - and for a
  Settings entry that appears when a battery is detected.
- First measurement said the hypothesis was wrong: GPIO5 read 1324 mV on the
  board WITH a cell and 1398-1424 mV on the bench board WITHOUT one, and a
  coefficient of 2.0 made that 2.65 V, which is not a working cell. A sweep of
  every free ADC pin (2-10) on both boards found nothing that looked like a
  battery either.
- The board's schematic settled it: a 200K/100K divider, so the pin sees a
  THIRD of the rail. 1324 mV x 3 = 3.97 V, a half-charged cell; 1424 x 3 =
  4.27 V, which is the charger's own output on a board with nothing to charge.
  Both boards were on USB, which is why neither read like a battery at rest.
- battery.cpp: eight averaged reads every two seconds, a piecewise curve from
  voltage to percent, and presence as a threshold - between 2.8 V and 4.24 V,
  twice in a row. It cannot be more certain than that: the pin measures the
  rail, the charger holds the rail up, and this board has no charge-status
  line. Nothing here claims to know whether it is charging.
- Settings: a battery row, with the charge on it, skipped entirely when there
  is no cell (E_BATTERY, MenuState::batteryPct = -1). The row set is now part
  of the menu's redraw signature, which it was not - the signature was a
  constant. The view shows the percentage, a bar, the voltage, and says the
  percentage is an estimate from the voltage.
- A drawn battery icon: LVGL's symbols carry a battery per charge level, and
  the row needs one that means "battery" whatever the charge.
- Bug found and fixed while looking at it: padding on an lv_bar is taken off
  its INDICATOR, so 14 px under a 10 px bar left nothing to draw and a full
  battery read as empty. The air is a spacer object now.

### Verified

- Battery board: row shows 73%, the view shows 73% and 3.97 V, captured.
- Bench board, no cell: no battery row in Settings, captured, and the log says
  4.27 V (no battery).
- On the cell alone, with the USB unplugged (Benoit, board reachable over
  Wi-Fi the whole time): the device kept running - which is the presence test
  no threshold can fake - and the voltage fell 3.97 -> 3.96 -> 3.94 V over two
  minutes, 73% -> 69%. The reading follows the cell, and the divider of 3 puts
  it where a part-charged LiPo belongs. The cell is a 3.7 V 1000 mAh LiPo with
  a protection board (Benoit).
- Benoit: the percentage jumps when the cable goes back in. It does, and it is
  not a fault: the same cell read 69% / 3.94 V on its own and 82% / 4.04 V a
  second after plugging in, because the pin measures the rail and the charger
  owns the rail. Measured on both sides, so the screen now says what it knows.
  battery::charging() reads the SHAPE of the curve, not its level: on the cell
  the voltage falls about 15 mV a minute with the screen lit, on the charger it
  is flat to a millivolt or two (both measured over several minutes). A fast
  window of 30 s catches the 100 mV step of a cable going in or out; a slow one
  of 5 minutes catches the drift of a cell in the flat part of its curve. While
  charging, the row and the view say "Charging" and show the voltage, and the
  bar is not drawn - no number is better than a number that moves for the wrong
  reason. Verified on the board: row reads "En charge" on USB (captured).
  Not verified: the switch back to a percentage within 30 s of unplugging.
- The whole discharge, end to end, on Benoit's board over a day on its cell:
  4.04 -> 3.97 -> 3.94 -> 3.26 -> 3.22 -> 3.21 V, 83% down to 0%. A normal LiPo
  curve, which validates the divider of 3 across the range rather than at one
  point, and the switch back from "Charging" to a percentage happened within
  the 30 s window. I first read the 3.2 V as a weak cell, having compared two
  readings hours apart as though they were minutes apart; Benoit said he had
  left it discharging all day.
- Benoit pointed at the TigerScale, which handles this well. It cannot be
  copied: that board carries an AXP2101 PMIC on I2C 0x34 and READS presence
  (STATUS1 bit 3), the cable (bit 5), the charger state machine (STATUS2) and
  a coulometer percentage (0xA4). This board has none of it - a charger and a
  200K/100K divider, nothing else - which is why charging here is inferred
  from the shape of the curve. Worth borrowing from it later: its asymmetric
  hysteresis on the charging flag (shown at once, withdrawn after 10 s of
  quiet), which exists because a nearly full cell makes a charger oscillate.
- Benoit asked for a mockup of the Battery view and took it as drawn, plus a
  time to full. Built: a drawn battery filled to the level (LVGL objects, not
  an icon - the fill and the colour change), the percentage under it, then
  State / Runtime left or Full in / Voltage as value rows, and the note. Three
  previews: setbatt, setcharge, setbattlow.
- The charger's offset is taken off the level rather than hiding the level:
  the device read 3.48 V with the cable in and 3.35 V with it out, moments
  apart, so percent() subtracts 0.13 V while charging. That is what stops the
  jump Benoit reported. Note what this is and is not: both numbers come from
  the device's own ADC, so the DIFFERENCE is measured - it is the same chain
  either way - but the absolute scale still is not. Nothing here has been
  compared against a meter. I first wrote this up as a multimeter reading;
  Benoit corrected it, he had read both values off the screen.
- Time left is the observed slope of the LEVEL, smoothed, over five minutes,
  and it is dropped entirely when the direction changes: the rate at which a
  cell fills says nothing about how long it will then last. No estimate is
  shown until there is one to make.
- Two bench corrections along the way. The bolt was drawn in the background
  colour and vanished into the empty half of the battery at 42%, which is
  exactly when somebody looks; it now takes the background colour only once
  the fill has passed the middle. And the charge/discharge window was 30 s,
  then 60 s, and both were too short: this cell charges at 2.4 mV a minute at
  the pin, so the row still said "On battery" with the cable in. Three minutes
  catches it, and the step rule still catches a cable going in or out within
  six seconds.
- Verified on the board: the three previews, the live row reading "36% (bolt)"
  and the live view reading "En charge / 3.86 V" on USB, and the percentage
  moving from 53% to 36% the moment charging was detected, which is the offset
  being applied.
- Benoit recorded a plug-and-unplug session against a new /api/batt endpoint -
  the serial console is no use for this, its cable being the one under test -
  and it found a real fault. The step detector compared a reading against the
  one from six seconds earlier, so a cable going in and out inside that window
  compared two readings at the same level, saw nothing, and left "charging"
  set: the level kept the charger's 0.13 V taken off it after the cable was
  out, and the screen showed 23% for a cell reading 44%. It now compares
  against the previous reading, one second: a cable is 40 mV, the drift
  between two readings is one. Replayed over his recording the five bad
  readings disappear; then verified live over three of his cycles, the state
  flipping within two seconds each way and the level steady at 51-52%.
  Recorded numbers, on this board: plugged 1298-1303 mV at the pin, unplugged
  1256-1261, so the charger's contribution is 0.126 V at the cell.
- The time estimate went through three faults of mine, all found on the bench.
  It was measured on the LEVEL, which moves one point every seventy seconds -
  far too coarse - so it is measured on the voltage and converted through the
  slope of the curve at that point. A window that came back as exactly zero
  millivolts, which is common late in a charge, was being read as "nothing
  measured yet", so the estimate never started - a flag now says which. And
  the threshold below which nothing was claimed erased the line for the last
  third of a charge, where the charger tapers.
- Benoit: a value is needed immediately, adjusted as it goes - "no answer for
  two minutes" reads as broken. The first answer now comes from a nominal
  model (1000 mAh, ~300 mA into the cell, ~200 mA out of it) and the measured
  slope takes over once it means something. Verified: 70 minutes on the first
  reading after a boot, "Charge pleine dans 1 h 10" on the screen.
- The bolt is white. It was drawn in the background colour, then in whichever
  colour the fill was not, and both vanish at the level where the fill edge
  runs through the glyph - which is the level somebody is looking at. And at
  Benoit's request the rows are bold with white labels: this screen is the
  value, not a settings list with a value on the right.
- Benoit, again on the level: 64% with the cable in against 75% with it out,
  4.04 V and 3.98 V. The offset is not a constant - it is the charge current
  through the cell's resistance, so it shrinks as the charger tapers, and a
  fixed 0.13 V over-corrected by more than it corrected. The device now
  MEASURES it, at the only moment it can be measured: the step in the reading
  when a cable moves is the offset. It keeps the value in NVS (boffmv), and
  until it has one it uses a prior scaled between 0.13 V on an empty cell and
  0.01 V on a full one - a flat default put a nearly full cell at 60% against
  the 74% it read a second after the cable moved, which is what Benoit saw
  after every reboot.
- And the step threshold came down from 8 mV at the pin to 5: the step shrinks
  with the charge current, and he caught one at 20 mV (0.06 V at the cell).
- The three-minute trend is asymmetric now, +3 mV to say charging and -6 to say
  otherwise. A nearly full cell on a charger is FLAT, which at 3 mV read the
  same as a discharge, so the row said "On battery" with the cable in.
- The back arrow on the battery screen barely worked, and the cause is the one
  the CODEMAP already warns about: the voltage was in the redraw signature, so
  the screen - the arrow with it - was rebuilt once a second under the finger.
  Signature is now the shape of the screen only (charging, whether there is a
  time to show); the level, the fill width, the time and the voltage are
  written into the widgets. Verified: three presses, three returns.
- One trap, noted for the next session: /screen.bmp?preview=setbatt DRAWS the
  fixture on the panel, so a capture taken right after it shows 3.97 V and 73%
  whatever the battery is doing. Read the live view, not a preview, when the
  value is the thing being measured.
- Still not verified: the reading against a multimeter - the divider of 3
  rests on the schematic (200K/100K) and on the readings being where a LiPo
  belongs across a full discharge, which is evidence but not calibration - and
  the 4.24 V presence threshold against a cell that has just come off charge.

## 2026-09-15 - a way back from a wrong language, and the language scrollbar (released in 1.53.0)

### Fixed

- Benoit, installing a new board: a mis-tap picked Español and nothing led back
  to the language list. main.cpp `s_apFromLang`: set when goAfterLang() opens
  the portal on a device with no saved network; the portal screens then get
  the Wi-Fi header with a back arrow, and back calls webcfg::endAP() and
  returns to ST_LANG. Picking a language opens the portal again. A device
  whose saved network merely failed at boot keeps the header-less QR.
- Benoit, same board: after picking a language, going back was "very hard".
  The back arrow worked (every press that was read reached ST_LANG in the
  log), but webcfg::beginAP() ran on the loop right after the QR was drawn and
  held it for 861-891 ms (measured: station stop and mode change 117-145 ms,
  softAP() about 360 ms, then fixed delays of 100 and 300 ms). A press in that
  window is never read - the CST816S reports only the current state. endAP()
  measured 2 ms, so the way back itself was never slow. The radio half of
  beginAP() (disconnect, mode, softAPConfig, softAP, the async scan) now runs
  on a one-shot task, core 0; webcfg::loop() starts the captive DNS and the web
  server once it is done. endAP() and beginAP() wait for a running bring-up
  before touching the radio, and apClients() reads nothing until the server is
  up.
- Benoit, on the pairing screen: it did not fit. showPairing() was headerless
  with the 56 px back strip, a 124 px QR, S_SCAN_TO_LINK wrapping as
  "tigersystem." / "io/pair" and the countdown clipped (captured). It now uses
  frame(S_TT_ACCOUNT, true) like the Wi-Fi screen, a 112 px QR, S_OR_GO_TO
  over the address in bold - taken from verify_url without scheme and query,
  so no literal URL is drawn - and the code and countdown share a row.
  S_SCAN_TO_LINK is gone.
- Benoit, on the live screen: the address still did not fit. The preview uses
  a made-up short URL; the real verify_url is
  https://tigertag-cdn.web.app/pair.html?c=..., thirty characters once the
  scheme and the code are off it, and it was clipped at both ends. The "Or go
  to" caption is gone with it - the line it took is what makes the address
  fit - and the address is drawn on one line at the largest of three faces
  that fits it, measured with lv_txt_get_size rather than guessed from the
  length. Captured on the device: the live one,
  tigertag-cdn.web.app/pair.html, at 12 px; the preview's
  tigersystem.io/pair in bold 16.
- Benoit: at the end of the countdown, go back to the sign-in choice on its
  own. ST_ACCOUNT POLLING with left <= 0 set FAILED with a generic message and
  waited for a tap; it now hides the screen and returns to CHOICE. Verified on
  the battery board with the window cut to 20 s for the test: the log shows
  "pairing code expired - back to the sign-in choice" and the capture taken
  after it shows the two buttons. The 600 s window is back in the source.
- Worth knowing: the address the screen used to print, tigersystem.io/pair,
  answers 307 to /en/pair and then 404 (checked). Nobody following it reached
  a pairing page. docs/ACCOUNT-PAIRING.md carried it as the example
  verify_url; corrected to what the backend actually returns. Benoit had it
  fixed the same day: https://tigersystem.io/pair and
  https://tigersystem.io/pair?c=... both answer 200 with no redirect, and
  https://tigertag-cdn.web.app/pair.html?c=... still answers 200 (checked).
  The short link then became https://tigersystem.io/pair/K7QF-3M2P - the code
  in the path, not a parameter - so the printed line drops a last path segment
  equal to the code as well as a query, compared without its dash. Captured
  with the preview on the new format: the QR carries the code, the line reads
  tigersystem.io/pair in bold 16. The backend still hands out the long link;
  the day pair/start returns the short one nothing needs rebuilding.
- Three leftovers from the prototype, in Portuguese, found while reading
  tigertag_cloud.cpp: the error "pairStart vazio" (empty), which is not only
  logged but shown on the pairing failure screen, and two comments. Now in
  English. The Portuguese column of webcfg's portal table is a translation and
  stays.
- Benoit: with nothing selected, put a button on the home screen that opens
  the printer picker - and no sentence above it. screen_home draws one
  full-width button on the empty list, S_SELECT_PRINTERS, wrapped over two
  lines in a button 24 px taller than the standard one (French "Choisir les
  imprimantes" is 23 characters and would not fit on one), and takePickTap()
  sends ST_PRINTER to ST_PICK. S_ALL_HIDDEN is gone - the distinction it drew
  between "hidden" and "none" was answered on the same screen either way.
  Verified on the device in French and English, and the button opens the
  picker (captured).
- Benoit, on the live screen: the printer choice was cut. The body sets
  pad_row = theme::GAP and spaces its children itself, and both lists were
  sized as though it did not: showChoosePrinters added three explicit spacers
  on top of five automatic gaps and overflowed the body by 30 px - centred, so
  15 came off the gauge and 15 off the Confirm button - and showPrinters
  overflowed by 12. The spacers are gone and the heights now subtract the gaps
  the body adds. Measured on the device after the fix: the button spans rows
  261-310 of 320, and nothing on either screen passes row 311.
- Benoit: the countdown under the code, not beside it - next to it, it read as
  part of the code. Captured.
- S_AP_JOIN shortened to "Scan the QR code" at Benoit's request (FR as he
  wrote it, "Scanner le QR Code"). Three new Chinese characters, fonts
  regenerated.
- The language list called no scrollbar style: frame() strips the theme's, so
  LV_SCROLLBAR_MODE_AUTO drew nothing. It now uses theme::scrollbar() like
  every other list.

### Verified

- Language scrollbar: captured on the bench (preview=lang), bar on the right,
  rows narrowed to leave its gap.
- First-boot back arrow, with the radio bring-up on its task: pressed by
  Benoit nine times in a row on the battery board, each read and each back on
  ST_LANG in the log. The longest loop pass in ST_AP was 159 ms (a temporary
  gap log, removed), the screen rebuild; before, 861-891 ms.
- Pairing screen: captured in French (preview=pair) - everything inside the
  panel. Chinese and German captured on the Wi-Fi QR screen with the new
  instruction; the pairing preview did not redraw after a language change
  (showPairing skips a rebuild with the same code and seconds), so those two
  languages were not captured on it.
- Benoit, on the battery board with no PN532 wired: the language list did not
  scroll smoothly and taps were slow. The loop retried reader::begin() every
  2 s; with nothing connected begin() runs five attempts with 200 ms between
  them and took 1 430 ms (measured) - the loop was blocked for 1.4 s of every
  3.4. reader::probe() - one wake-up and one firmware query, 32 ms with no
  module (measured) - now gates the retry; begin() runs only when something
  answers. The boot-time begin() is unchanged: it runs behind the splash.
  Not verified: a reader plugged in while running being picked up through the
  probe - the board had none to plug.

