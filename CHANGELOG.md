# Changelog

All notable changes to this project are documented here.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.21.0] - 2026-09-05

### Changed

- **No screen is rebuilt to reflect a value any more.** Rebuilding throws away
  the scroll position, the focus and any animation in flight, and three screens
  were doing it many times a second:

  - the OTA progress ring restarted from zero on every percent reported,
    instead of sweeping once — which is the entire reason it is a ring;
  - the Wi-Fi screen rebuilt itself whenever the signal moved by a decibel,
    which on a real link is constantly;
  - the factory-reset bar rebuilt on every frame of the press-and-hold, so it
    could not animate at all.

  Each now builds once and writes the changing value into the widget that holds
  it. It is written into `AGENTS.md` as a settled rule rather than left as
  something to rediscover.

- **The live screen page reacts in a tenth of a second.** It polled the
  framebuffer on a timer — 150 KB an image, so the timer had to be slow, so
  moving between screens felt laggy. It now polls a four-byte paint counter
  every 120 ms and fetches the image only when the panel actually changed.


## [1.20.0] - 2026-09-05

### Fixed

- **Toggling a printer sent the list back to the top.** With more than a
  handful of printers, turning one on meant scrolling back down to reach the
  next — and the further down you were, the worse it got.

  The screen was rebuilt to show the new switch position, and a rebuilt list
  has lost where it was scrolled to. The switch is the only thing that changed,
  so it is now the only thing that changes: it flips in place, and the redraw
  signature no longer watches the visibility flags. What still rebuilds the
  screen is what only an account sync can change — which printers exist and
  what they are called.


## [1.19.0] - 2026-09-05

### Fixed

- **The Install button was cut in half by the bottom of the screen** when an
  update was available — the one control that page exists for. The line about
  Wi-Fi, account and printers being kept is gone, the spacing is tighter, and
  the page scrolls, so a longer translation cannot push a button off the panel
  again.

### Added

- `?preview=setupdate` renders the update page with a version waiting, and
  `?preview=updone` with none. Being behind is not a state a device can be put
  into on demand, and it is the state whose layout is tightest — which is how
  the cut button reached a release in the first place.


## [1.18.0] - 2026-09-05

### Fixed

- **The captive portal served the prototype's old configuration form.** A
  device that had joined Wi-Fi and later dropped to the setup access point
  showed the wrong page entirely. The route table was built twice — once when
  the device came up on the network, once when the access point started — and
  the ESP32 web server keeps handlers in a list where the first match wins, so
  the second registration of `/` was dead. It is registered once now and
  decides inside the handler, which cannot go stale.
- **The screen no longer sleeps during setup.** Every setup screen puts
  something on the panel that has to be read off it — a QR code to scan, a
  pairing code to type — and a screen that goes dark while someone is holding a
  phone up to it has failed at its one job. The sleep timeout applies to the
  home screen, where the device sits idle between spools.

### Removed

- **The prototype's configuration form is gone.** The portal replaced it for
  Wi-Fi, and printers come from the TigerTag account rather than being typed
  into a web form. On the local network `/` now goes to the account page.
- With it, `/save`, `/retry`, and `/reset` — the last of which wiped every
  stored namespace on a plain GET, with no confirmation and no authentication,
  from anywhere on the network. Factory reset is on the device, behind a hold.


## [1.17.0] - 2026-09-05

### Added

- **The Wi-Fi screen shows the signal in dBm.** The home screen colours its
  Wi-Fi glyph by strength, and "the icon is orange" is not something anyone can
  act on. A number is: it says whether to move the box or move the router.
  Green above −60, yellow to −75, orange below, red with no connection.


## [1.15.0] - 2026-09-05

### Fixed

- **The word "Text" in the header while the firmware downloads.** The download
  screen deliberately has no title, and an LVGL label created and then handed a
  null string keeps the placeholder it ships with. It appeared on the one
  screen that tells the user not to unplug the box — where anything unexplained
  is the most alarming. No title now means no label.
- **Google sign-in took minutes to complete after it had already been
  approved.** The device only learned of the approval when the phone's browser
  refreshed the waiting page — and the approval opens in a second tab by
  construction, which iOS then suspends. On a real phone the pairing sat
  unnoticed for two minutes, until the user thought to switch back to the first
  tab.

  Nothing about a pairing needs a browser. The device holds the token, so it
  now asks Google itself, every few seconds, for as long as the pairing screen
  is up. Approval completes whether or not anyone is looking at a phone.


## [1.14.0] - 2026-09-05

### Changed

- **The device checks for updates every six hours, not just once at boot.**
  This box sits on a shelf next to a printer and plenty of them will never be
  switched off — a device that checks once in its life learns about one update
  and then stops.

  It says so once per version, not once per check. A version already declined
  stays declined: repeating the notice every six hours is nagging, and nagging
  is what teaches people to dismiss without reading. A *newer* version speaks
  up again, because that is news. The Update row in Settings stays orange
  either way.


## [1.13.0] - 2026-09-05

### Changed

- The update notice is down to the badge, the version and two buttons. What an
  update keeps belongs on the update page, where someone is considering the
  question — not on a screen that interrupts them and has to be answerable at a
  glance.


## [1.12.0] - 2026-09-05

### Added

- **The device tells you when there is an update, instead of waiting to be
  asked.** It already checked once, twenty seconds after boot, and coloured the
  Update row orange — but that row is only seen by someone who went looking. A
  waiting update now says so on the home screen: the version, the line
  confirming Wi-Fi, account and printers are kept, and two buttons. *Install*
  starts it there and then; *Later* goes back and does not ask again this boot.
  It is held to the home screen, so it can never land on top of a spool being
  assigned.

### Fixed

- **Safari's "this form is not secure" warning on the Google button.** The
  button was a form, and Safari warns on any form posted over plain HTTP — even
  one carrying no data, which this one was. It is a link now. The same warning
  on the e-mail form is *correct* and stays: that password really does cross an
  unencrypted connection, which is the strongest argument for the Google route,
  where no password is typed at all.


## [1.11.0] - 2026-09-05

### Fixed

- **The network list is ready before you get there again.** It had been empty
  on first open since 1.7.0, filling in only after pressing "rescan".

  An ESP32 shares one radio between its access point and its station
  interface. Once a phone is associated to the access point, that radio is
  committed to serving it, and a scan started afterwards comes back with
  nothing — it does not fail, it succeeds and finds zero networks, which is why
  the earlier attempt at this looked for an error that was never reported.

  So the scan happens at the one moment it can: when the access point comes up,
  before anyone can join. It runs asynchronously, so the setup QR still appears
  instantly — that was the point of taking the *blocking* scan off this path in
  1.7.0, and removing the scan altogether was the mistake. The result is
  collected the moment it lands and served from there.

  The station interface also stays up now. Three places tore it down after
  every scan, and a scan cannot start on an interface that has not finished
  starting, so each one left the next scan worse off. Idle, it costs nothing.

  Verified on hardware: `scan cached: 18 network(s)` in the boot log before any
  client joined, and the endpoint serving eight named networks with signal
  strengths.


## [1.10.0] - 2026-09-05

### Fixed

- **The network picker came up empty on first open**, and only filled in after
  pressing "rescan". A regression from 1.7.0, where the blocking scan was taken
  off the access point's startup path to stop the setup QR appearing four
  seconds late. The replacement started the station interface and asked it to
  scan in the same breath — and a scan issued before that interface is up fails
  outright. The failure was silent, so the first attempt returned nothing.

  It now lets the interface settle, and retries once. The scan endpoint also
  stops reporting a failed scan as an empty list: those are different answers,
  and saying "no networks found" in a flat full of them left the user to guess
  that a button might help.


## [1.9.0] - 2026-09-05

### Fixed

- **The captive portal's DNS answered the wrong question, and that is why
  Android never opened the sign-in page.** The core's `DNSServer` writes an
  answer of type A whatever type was asked for, so a query for an AAAA record
  came back with a question section saying AAAA and an answer section holding
  four bytes of IPv4. That is not a valid response and a resolver may discard
  it. Android asks for A and AAAA in parallel and waits for both: the A answer
  arrived, the AAAA answer was thrown away, and the lookup sat there until it
  timed out — past the deadline of Android's captive-portal probe. The portal
  was up, encrypted and serving the page the whole time.

  iOS was never affected: its resolver is more forgiving and its probe retries.
  That asymmetry is what hid this behind two earlier theories.

  `net/captive_dns` replaces the core server. A queries get the device's
  address; every other type gets NOERROR with no answers, which is the correct
  way to say the name exists but has no record of that type, and lets the
  client use the address it already has. Verified against the running firmware
  with `dig`: `A` returns one answer, `AAAA` returns zero, both NOERROR.

### Added

- **A fallback QR, shown the moment a phone joins the setup network.** The
  screen switches from *join this Wi-Fi* to a QR pointing at the portal itself.
  If the sign-in sheet does not open on its own — on any phone, for any reason
  — scanning again lands on the page, with the address printed underneath for
  anyone who would rather type it. The page was always reachable; this is how
  someone reaches it without being read an IP address out loud.


## [1.8.0] - 2026-09-05

### Fixed

- **The setup access point is WPA2 instead of open, which is what stopped
  Android from opening the sign-in page.** A Galaxy S24 joined the setup
  network and never showed the captive-portal sheet. Open networks are why:
  Android treats one with no internet as a mistake to correct rather than a
  destination, and Samsung's adaptive Wi-Fi falls back to mobile data instead
  of probing for a portal. An encrypted network is handled as somewhere the
  user chose to be. iPhones were unaffected, which is what made this look like
  a timing problem for so long.

  **Nothing is typed.** The Wi-Fi QR format carries a key, so the phone joins
  in one tap exactly as before. The old reasoning for leaving it open — that a
  password would have to be read off a 2" screen — had a hole in it: the trade
  it described was never real.

  It also closes something that should not have been open. This portal accepts
  the user's home Wi-Fi password and their TigerTag password over plain HTTP,
  and on an open access point both crossed the air in clear to anyone in range.

  The key is derived from the MAC — `tiger` plus the last three bytes — so the
  QR, the screen and the radio always agree, and a factory reset comes back
  with the same key rather than stranding whoever wrote it down. It is printed
  under the network name for a camera that will not scan.


## [1.7.0] - 2026-09-05

### Changed

- **The sign-in page has been rebuilt.** It is the one screen of this product a
  stranger reaches by scanning a QR code, on a `192.168.x.x` address their
  browser decorates with a warning triangle, and it is where they are asked for
  a password — and it carried no mark of who was asking. It now opens with the
  TigerTag icon, keeps only what is needed to sign in, and ends with a way out:
  a Tiger Studio Manager button, GitHub, Discord and tigersystem.io.
  The password is masked with an eye to reveal it, the Google button carries
  the real Google mark and reads *Continue with Google*, the accents are back —
  that page is drawn by the phone's browser, so the panel font's ASCII limit
  never applied to it — and the firmware version is printed at the foot, read
  from the version macro rather than typed.
- **The Google pairing page was rebuilt with it, and the device joins in.** For
  as long as that page is waiting, the TigerSpool shows the same pairing QR and
  code on its own screen — so someone who started on their phone can finish on
  a computer by scanning the box instead of retyping a code, and a box that is
  waiting stops looking like a box that is idle. The page leads with *scan the
  QR code on the TigerSpool screen*, then the code, then *or* and a Continue
  with Google button for a browser already signed in. It no longer guesses
  where you are reading it: "open this link on a phone or PC" was advice to
  someone who had already done it, and equally wrong for someone who typed the
  address off the device's screen at a desk.
- Nothing on those pages is fetched from the internet. The phone reading it is
  joined to the device's own access point, where a remote font or logo fails
  silently — an empty box on the one screen where trust is being decided. The
  icon is served from flash by the device itself.

### Fixed

- **Choosing a language on a new device no longer freezes the screen.** After
  the language, the setup QR took several seconds to appear — long enough to
  read as a crash rather than as work. Two causes, both removed: the access
  point ran a full Wi-Fi scan on its way up, synchronously, blocking everything
  behind a list nobody had asked for yet; and the QR was drawn only after the
  radio had finished. The QR now goes up first — its SSID comes from the MAC
  and is known before the radio does anything — and the access point comes up
  behind it, in well under a second. The network list is fetched by the portal
  when it is opened, which is where it is actually needed.


## [1.6.0] - 2026-09-05

### Changed

- **The account icon is a person again, drawn the way the TigerScale draws it.**
  Two solid discs and no outline: the shoulders disc overflows the box and the
  clipped pixels are the whole mechanism — a circle cut off at the bottom reads
  as a bust. The previous attempt used outlines and came out as a different
  shape entirely.
- The printer and the sun follow the same silhouette rules the scale's own six
  icons follow: three strokes at most, one dominant form owning two thirds of
  the box, nothing thinner than 2 px, and recognisable when filled in solid.

- **The home screen shows Wi-Fi strength**, next to the gear, the way the
  TigerScale does. LVGL has one Wi-Fi glyph rather than a set of bar counts, so
  the strength is carried by colour — green above −60 dBm, yellow to −75,
  orange below, red when there is no connection. It is bucketed into those four
  levels before it reaches the redraw signature: raw dBm drifts by a few points
  every second on a still desk, and a screen that rebuilds on that loses the
  scroll position while someone is reading it.
- **Screen orientation is a setting.** Both mountings of the board exist, so
  which way up the panel is belongs to the user rather than to a build
  constant. Under Display, alongside brightness and sleep; applied immediately
  and remembered across reboots. LovyanGFX turns the touch coordinates with the
  display, so there is nothing to recalibrate.
- **The Settings rows are outlined rather than filled**, with chevrons at half
  the row height instead of a twelfth. On a black screen a filled row floats;
  an outlined one sits on the page, and eight of them read as a list rather
  than as eight separate objects. Matches the TigerScale.
- **The Account row shows the account's display name**, not its address — a
  name is what you call an account, an address is thirty characters that
  truncate to `benoit@atom…`. Its icon is green once signed in. Devices already
  in the field pick the name up on their next sync rather than waiting to be
  signed out and back in.
- **The live screen page is clickable.** `http://<device>.local/screen` already
  refreshed the panel image; a click on it is now forwarded as a touch, and a
  drag as a swipe. Navigating the device from a laptop no longer means
  hand-writing query strings.
- The printer icon keeps its original proportions. A heavier body and output
  tray followed the "one dominant form" rule and read worse on the panel.
- **The Display row now carries the real sun**, the same Font Awesome glyph the
  TigerScale draws, rather than an approximation of it. Drawing it was never
  going to work: LVGL primitives are axis-aligned rectangles, and that sun has
  eight pointed rays. `scripts/make-icon-font.sh` fetches Font Awesome 6.5.2 at
  a pinned tag and extracts the one codepoint — 4 KB of source, no change to
  the flash figure. The font file is cached locally and never committed.


### Fixed

- **The captive portal did not open on recent Android phones.** Scan the Wi-Fi
  QR code on a Galaxy S24, join the setup network, and the sign-in sheet never
  appeared — the portal was reachable the whole time, the phone had simply
  stopped looking. A background Wi-Fi scan was launched from the last line of
  the access point's startup, and a scan needs the station interface, which
  makes the radio hop channels. Android probes for a captive portal within
  about a second of associating, so that probe landed while the access point
  was away. It times out, the network is filed under "connected, no internet",
  and Android does not ask again. The scan now starts when the portal page has
  been served — which is itself proof the probe already succeeded — and the
  network list is still ready before anyone reaches the picker. iOS was
  unaffected because it retries its probe; Android's is effectively one-shot.


## [1.5.0] - 2026-09-05

### Changed

- **The update screen answers the question you came with.** Opening it now runs
  the check itself, instead of showing a "Check for update" button and waiting
  for you to state an intent you stated by arriving. The button remains for the
  two cases that still need it: no Wi-Fi, and retrying after a failure.
- **The progress ring replaces the progress bar.** While an image is being
  written the whole screen becomes a 152 px ring with the percentage inside it,
  no header and no way to leave — the download runs on its own task, and
  leaving would only hide it. Below it, a warning against pulling the plug: a
  half-written slot boots the old image, which is recoverable and looks like a
  brick for a minute.
- **The header is no longer a bar.** It is the same ground as the rest of the
  screen with a single rule under it, on every screen. The title reads as part
  of the page rather than a strip bolted above it.
- **The installed version is a row, and the state is a badge.** "Installed
  1.5.0" now uses the same row shape as the rest of settings, and the answer —
  up to date, an update waiting, or a failure — is a glyph in a coloured ring,
  legible before a word is read.
- The update channel row is gone. It offered one channel.
- **Orange is now a category of its own, not a weaker red.** Red means an action
  destroys something that cannot be rebuilt from the device; orange means it
  interrupts what is on screen and destroys nothing. Restart is orange, factory
  reset stays red. Without the distinction everything consequential turns red
  and red stops meaning anything.
- **A waiting update announces itself on the Settings menu**, in orange, with
  the new version on the row. The device checks once, twenty seconds after
  boot. That is the only announcement it gets: a spool reader is not a phone,
  and it is never checked on a timer afterwards.
- The update page now says "Version installed — v1.5.0", with the `v` the rest
  of the ecosystem prints, and answers the question people ask before pressing
  Install: Wi-Fi, account and printers are kept.
- **Every Settings row carries an icon, and the icon carries the state.** Green
  when something is reachable — Wi-Fi joined, account signed in, printers
  present — red when it is not, orange for what interrupts, red for what
  destroys. The label stays white, so eight rows read at a glance instead of
  turning into a colour chart. Taken from the TigerScale, which does the same.
- Icons LVGL has no glyph for are drawn rather than imported: a person for the
  account, a globe for the language, a printer, a sun. A kilobyte of code and
  no data — no font to generate and no licence to carry.

### Fixed


- The Settings menu showed an empty Wi-Fi network. `WiFi.SSID()` returns a
  String by value, and the temporary was dying before the menu read it. It
  never crashed; it just looked like a network problem.

### Changed

- Wording aligned with the TigerScale where the two devices name the same
  thing: "Your TigerSpool is up to date" as a sentence rather than "Up to
  date", and "Installed version" rather than "Installed".

### Fixed

- `scripts/flash.sh` failed with `PORT_ARG[@]: unbound variable` unless
  `--port` was passed. Expanding an empty array under `set -u` is an error in
  bash 3.2, which is what `/bin/bash` on macOS still is.


## [1.4.0] - 2026-09-04

### Fixed

- **The boot screen was gone before it could be read.** The device comes up
  faster than the eye, so LVGL began repainting over the logo within a fraction
  of a second — which looks exactly like an image that does not fit the screen.
  It is now held for one second, and the second is not wasted: the deadline is
  set where the image is drawn and only waited out at the end of startup, so
  LVGL, the language table, the storage read and the reader handshake all happen
  inside it. A board slower than a second to boot waits for nothing.

### Added

- `?preview=splash` draws the boot screen on demand, so it can be checked from a
  desk. It cannot otherwise be captured: it is shown before the web server
  exists, which is the whole point of drawing it that early.


## [1.3.0] - 2026-09-04

### Added

- **A boot screen.** Pushed to the panel from flash immediately after the display
  is initialised and before LVGL exists, so everything that follows — LVGL
  starting, the language table, the NVS read, the reader handshake — happens with
  the logo already up instead of behind a black screen. No timer holds it: it
  stays until the first real screen replaces it, so the device is never slower
  than it needs to be in order to look considered. 150 KB of flash, taking the
  app partition from 36.5% to 40.3%.

### Changed

- The installer's shopping list carries only what is actually bought. The jumper
  wires come in the PN532's box and the case comes off your own printer; both are
  in the bill of materials instead.


## [1.2.0] - 2026-09-04

### Fixed

- **The sign-in QR led to the Wi-Fi picker.** It carried the device's root URL,
  and that page opens with the network selector and puts the account form some
  forty lines below it — so a phone scanning it to sign in landed on a Wi-Fi list.
  The first control under the finger was also a save-and-restart for the network
  settings, on a device that had just joined a network. The QR now points at
  `/login`, a page with one job. Reported by a user.

### Added

- **A web installer**, at
  [tigertag-project.github.io/TigerSpool-RFID](https://tigertag-project.github.io/TigerSpool-RFID/) —
  plug a board in, press Install. Built on the same design system as the
  TigerScale installer, with the parts list, their links and the two things that
  otherwise cost an evening: Erase wipes the saved Wi-Fi and account, and a board
  offering no serial port is usually a charge-only cable.
- The installer speaks **nine languages**, one more than the device: Chinese
  renders in a browser and would reach the panel as empty boxes.

### Changed

- **The README starts where people actually get stuck.** A TigerSpool reads its
  printers from a TigerTag account, so an empty printer list is an account with
  no printers in it — not a fault. Installing Tiger Studio Manager, creating the
  account and adding the printers is now the section above the quick start.
- **The bill of materials is three things**, with links: the board, the PN532 and
  a USB cable that carries data.
- **Four wires, not six.** `config.h` declares the UART and nothing else, so the
  reset line the wiring guide described as "optional, recommended" was recommended
  by nothing — no code reads or writes it. Both the guide and the pinout say it is
  not connected, and the four wires come with the PN532.


## [1.1.0] - 2026-09-04

**First official release.** A TigerTag NFC chip on a spool, read by the box, and
the filament it names written into a printer's slot — material, brand, colour
and temperatures — without typing anything on the device.

### The device

- **First boot end to end**: language in eight locales, Wi-Fi over a QR code and
  a captive portal that joins without rebooting, then linking a TigerTag account
  by email or by Google.
- **Every screen is LVGL**, over a display port that keeps its DMA draw buffers
  in internal RAM and the LVGL heap in PSRAM.
- **Settings**: printers, Wi-Fi, account, screen, language, update, restart and
  factory reset. The factory reset is a two-second hold and clears all four NVS
  namespaces, so it cannot quietly undo itself.
- **A printer picker**, because an account can hold ten printers while the
  machine next to the box is one of them. Hiding is not deleting.
- **The screen sleeps** — dim, dark, wake on touch. The waking touch is
  consumed, so reaching for a sleeping device cannot send filament to a slot.
- **Per-device names**, `tigerspool-xxxx.local` and `TigerSpool-Setup-XXXX`, so
  two of these can share a network.
- **Slot names match the printers'**: `Ext.` plus `1A`–`1D` on Creality and
  FlashForge, `A1`–`A4` then `B1`–`B4` on Bambu, `E1`–`E4` on Snapmaker.

### Over-the-air update

- Fetch the published manifest, compare versions, stream the image into the
  spare OTA slot while hashing it, refuse it unless the checksum matches, and
  restart into it. Nothing touches the running slot, so a failure costs a
  download and nothing else.
- The manifest is generated at release time from the release's own artefacts,
  never committed and never rebuilt, and published by exactly one workflow —
  which then verifies what is actually being served.
- **The image is not signed.** The connection is verified; who produced the
  firmware is not proven. See [docs/OTA.md](docs/OTA.md).

### Security

- **Certificates are verified on every call that leaves the network** — the
  account sign-in, the token refresh, the pairing, the Firestore import, the
  manifest and the firmware. All of them previously ran without checking who
  answered.
- The Bambu backend is the one deliberate exception: a printer on the local
  network presenting a self-signed certificate, trusted through the access code.

### The working contract

- `AGENTS.md`, `CLAUDE.md`, `CODEMAP.md` and `WORKLOG.md`, plus eleven guards
  behind one command, `scripts/verify.sh`, which CI runs rather than its own
  copy. They cover file format, generated files against their generators,
  translation tables against their enums, every drawn string against the
  compiled font, that committed text is English and comes from the translation
  table, that documented device names and reader wiring match the code, and that
  a release has notes.
- `/screen.bmp` and `/api/tap` make the panel readable and drivable from a desk.

### Documentation

- Product definition and positioning ([README.md](README.md)).
- Target firmware architecture: layers, state machine, printer backend
  abstraction, transport/protocol split ([docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)).
- User journey, screen by screen ([docs/ONBOARDING.md](docs/ONBOARDING.md)).
- Wi-Fi provisioning design — built-in captive portal, Wi-Fi join QR code
  ([docs/WIFI-PROVISIONING.md](docs/WIFI-PROVISIONING.md)).
- Account pairing design — QR pairing for Google accounts, email/password from
  the phone, with the endpoints that need confirming
  ([docs/ACCOUNT-PAIRING.md](docs/ACCOUNT-PAIRING.md)).
- OTA design — two-slot partition layout, rollback, signing, channels
  ([docs/OTA.md](docs/OTA.md)).
- Complete verified wiring and the failure modes behind it
  ([docs/WIRING.md](docs/WIRING.md), [hardware/pinout.md](hardware/pinout.md)).
- Honest three-level printer compatibility matrix
  ([docs/PRINTER-COMPATIBILITY.md](docs/PRINTER-COMPATIBILITY.md)).
- Migration plan from the bench prototype, including the hardware facts that must
  not be lost ([docs/MIGRATION.md](docs/MIGRATION.md)).
- Bill of materials ([hardware/BOM.md](hardware/BOM.md)).
- 3D model directory structure and the rule that only the shell changes
  ([models/README.md](models/README.md)).
- The account data model — what the device reads from a TigerTag account and
  what it writes back after a scan ([docs/ACCOUNT-DATA.md](docs/ACCOUNT-DATA.md)).
- Web installer design ([installer/README.md](installer/README.md)).
- CI workflow placeholders, issue and PR templates.
- MIT license, trademark policy, security policy, code of conduct, contributor
  credits.

### Decided

- **Account pairing keeps both email/password and Google**, as TigerScale does.
  RFC 8628 was examined and rejected — the QR cannot carry the code, polling
  needs a `client_secret` a public binary must not ship, and it is Google-only.
  ([docs/ACCOUNT-PAIRING.md](docs/ACCOUNT-PAIRING.md#why-not-rfc-8628))
- **The reader is on GPIO43/44**, never GPIO6/7 — bench-verified, and it
  contradicts the prototype's own draft README.
  ([docs/WIRING.md](docs/WIRING.md))
- **The captive portal is a state in the main firmware**, never a separate binary
  to flash. ([docs/WIFI-PROVISIONING.md](docs/WIFI-PROVISIONING.md))
- **Nine languages**, aligned with TigerScale V3.
- **Elegoo and Anycubic are targeted for v1 as a port, not a
  reverse-engineering exercise.** Both protocols are documented and working in
  Tiger Studio from live slicer captures: Elegoo is MQTT over plain TCP on 1883,
  Anycubic is MQTT/TLS on 9883. No firmware backend exists for either yet.
  ([docs/PRINTER-COMPATIBILITY.md](docs/PRINTER-COMPATIBILITY.md))
- **An Anycubic printer must have been paired in AnycubicSlicerNext once.** Its
  broker credentials exist nowhere else and cannot be derived from the printer.
  Tiger Studio reads them into the account; TigerSpool imports them. This is the
  product working as designed, and it is documented rather than left to surface
  as a failure.
- **The partition table is set before the first public release**, because
  changing it afterwards costs every user a USB reflash.
  ([docs/OTA.md](docs/OTA.md))
- **Pairing targets the deployed Cloud Functions** (`pairStart` / `pairPoll`),
  which is what TigerScale V3 calls in shipped firmware. The `/api/device/pair/*`
  path in V3's documentation is not implemented anywhere and is treated as a
  future surface. ([docs/ACCOUNT-PAIRING.md](docs/ACCOUNT-PAIRING.md#endpoints))
- **OTA images are signed, and TLS certificates are validated.** No TigerTag
  signing key exists to reuse: V3 verifies a SHA-256 fetched over the same
  unauthenticated TLS connection as the image itself, and skips verification
  entirely when no hash is supplied. TigerSpool refuses an unsigned image rather
  than installing it. ([docs/OTA.md](docs/OTA.md#integrity-and-authenticity))
- **The device writes slot changes back to the account.** A confirmed assignment
  updates the slot's material, colour, vendor and the scanned tag's UID, so a
  spool scanned in the workshop is visible in Tiger Studio and on a phone. Never
  for cloud-mode printers, whose state the vendor's cloud already owns, and never
  before the printer confirms.
  ([docs/ACCOUNT-DATA.md](docs/ACCOUNT-DATA.md#writing-back))
- **Credentials are a named bag, not a fixed struct.** Six brands use six
  different credential vocabularies in the account, and a printer may be
  cloud-only with no local address at all.
  ([docs/ACCOUNT-DATA.md](docs/ACCOUNT-DATA.md#credentials-are-not-one-shape))
- **Rollback is implemented deliberately, not assumed.** Two OTA partitions make
  an update possible, not reversible; real rollback needs the bootloader option
  *and* an explicit validity call. V3 has neither while its comments claim
  otherwise. ([docs/OTA.md](docs/OTA.md#rollback))
- Identifiers fixed: `tigerspool-xxxx.local`, `TigerSpool-Setup-XXXX`, PlatformIO env
  `tigerspool`, NVS namespace `tigerspool`.

### Verified on hardware

Recorded because they were measured rather than assumed.

- **End-to-end assignment on a FlashForge AD5X** — a model the prototype's
  FlashForge backend was not written for. The tag was read on the first attempt,
  the assignment was sent, and the change was **confirmed by re-reading the
  printer's own slot state**, not by trusting its acknowledgement.
- **Fidelity loss is real and user-visible.** `PLA High Speed / #DC123F` arrived
  as `PLA / #F82D29`. The result screen has to say the colour was adapted.
- **The reader works on the first try at GPIO43/44**, with no retry and no
  rejected reads, on a second board.
- **Anycubic's broker does not require TLS 1.2 or a client certificate**, at
  least on a Kobra X — contradicting the ecosystem's own protocol notes and
  removing the largest stated risk for that backend.

### Added since the bootstrap

The firmware landed on `phase-2/firmware-import` and the UI was rebuilt on
LVGL. Highlights, in the order they matter to someone holding the device:

- **First boot works end to end** — language in eight locales with their
  accents, Wi-Fi over a QR and a captive portal that joins without
  rebooting, then linking a TigerTag account by email or by Google.
- **Settings**, eight entries: printers, Wi-Fi, account, screen, language,
  update, restart, factory reset. Each shows its current value on the row.
- **A printer picker.** An account can hold ten printers while the machine
  next to the box is one of them. Hiding is not deleting, and visibility
  belongs to the user rather than the account.
- **The screen sleeps** — dim, dark, wake on touch. Only the light stops.
- **Per-device names**: `tigerspool-xxxx.local` and `TigerSpool-Setup-XXXX`,
  because two devices could not previously coexist on one network.
- **Two OTA partitions** on a 16 MB layout. The prototype declared no table
  and built against an 8 MB default, leaving half the flash unreachable.

### Not yet done

- No OTA. The partitions are ready; the update code is not.
- No web installer page.
- No 3D models.
- No web installer page.
- No Elegoo or Anycubic firmware backend, though both protocols are documented.

[Unreleased]: https://github.com/TigerTag-Project/TigerSpool-RFID/commits/main
