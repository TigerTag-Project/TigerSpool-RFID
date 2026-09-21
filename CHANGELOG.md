# Changelog

All notable changes to this project are documented here.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- **A FlashForge that changed address was never found again.** LAN discovery
  existed for Creality alone, so a Creator 5 Pro whose address moved was dialled
  at the old one for good while Tiger Studio, which finds it by itself, worked
  fine. It is now swept for on port 8898 and matched by asking each unreachable
  entry `/checkCode` with its own serial and access code: `{"code":0}` means the
  address IS that printer, not merely that something listens there.
- **A corrected printer address did not survive.** NVS was full - eleven keys a
  printer over 24 positions, and an empty string costs two entries exactly like a
  full one, so the fields only an Anycubic uses filled four pages to the last
  entry (504 of 630). A full partition cannot update anything, so `putString`
  returned 0 in silence and the old address came back on the next load. An empty
  field is now stored as no key, and the ones already stored are removed at the
  start of each import: 504 of 630 entries before, 384 after.

## [1.66.0] - 2026-09-21

### Changed

- **The battery is declared, not detected.** A box does not know whether a cell
  is on its connector: there is no sense line, the rail with an empty connector
  reads anywhere from 4.05 to 4.27 V depending on the board, and the charger's
  ripple - which a healthy cell damps - is not damped by a tired one. Both of
  our boards proved it, in opposite directions: one reported a battery it did
  not have, the other denied the pack it was running on, at 3.3 V, while
  running on it. So Settings now has a Battery row on every device, with one
  switch: yes or no. The level, the runtime and what the device tells the
  account all follow that answer.
- **Nothing that blocks starts in the half second after a touch.** Dialling a
  printer and beating to the account both open a TLS session on the main loop,
  and that loop is also what reads the back chevron: a handshake started the
  moment a finger lands is a press that appears to do nothing. Navigation comes
  first.

### Fixed

- **The screen could stop answering after a control redrew its own view.** A
  finger stays on the glass for ten reads or so; a screen rebuilt while it is
  down destroys the object LVGL believes is being pressed, and LVGL only clears
  that pointer for the deleted object itself, never for its children. Every
  later touch was then treated as a continuation of a press that no longer
  existed. Any screen rebuild now cancels the press in flight.
- **The printer list repainted the whole screen thirty to fifty times a
  second, doing nothing.** Two updaters ran on every pass of the loop and wrote
  what was already there: `lv_label_set_text` invalidates a label whatever it
  is handed, and `lv_obj_add_flag` invalidates an object when HIDDEN is in the
  mask even if it was already hidden. Both now write only on a change. Measured
  with `/screen.ver`: 29-55 frames a second at rest before, 0 after.
- `printer_ids` was always published empty, so Studio could not tell which
  printers a TigerSpool stands in front of. They were written to a single NVS
  key that the flash refused - 361 bytes, and this frozen 20 KB partition has
  no contiguous run that long left - and read back only when a sync reported a
  change. They are kept in RAM now, refreshed by every sync.

## [1.65.0] - 2026-09-21

### Changed

- The device document moved to `users/{uid}/tigerspools/{mac}` - plural, like
  every other collection of things under an account. 1.64.0 wrote the singular
  for one release and never created a document anywhere, because the account's
  rules refused every write, so there is nothing under the old name.

## [1.64.0] - 2026-09-21

### Added

- **The device declares itself to the TigerTag account.** It writes
  `users/{uid}/tigerspool/{mac}` every 30 seconds - every 5 minutes with the
  screen off, and at once when a spool is written or a cable moves - so Tiger
  Studio can list it, show whether it is online, and see which printers it
  stands in front of. The identity, liveness and battery fields carry the same
  names a TigerScale writes. See `docs/PRESENCE.md`.
- The pairing page now says what it is adopting: `TigerSpool`, with the real
  firmware version. It announced itself as a "TigerTag Bridge" running `cfs_ui`.

  **The account's security rules must name this path before any of it is
  allowed** - every beat is refused until they do, and the device holds off for
  five minutes after three refusals rather than retrying for ever. The rule is
  in `docs/PRESENCE.md`. Several TigerSpools on one account are supported by
  construction: one document each, keyed by MAC.

## [1.63.0] - 2026-09-20

### Fixed

- **Signing in with an email and password no longer restarts the device.** One
  route rebooted and the other did not, which is what a user reported. The
  restart was hiding a real gap: the loop only reloaded the printer list when
  it had asked for the sync itself, so a sync done from the web page landed in
  NVS with nobody reading it back. It reads it back now, and nothing reboots -
  not after an email sign-in, not after a Google one, not after a manual sync.

### Changed

- The Cancel button stays for the whole of the scan screen, including the write
  that follows the read. 1.62.0 took it away the instant the chip was caught,
  which turned out to be a flicker a fraction of a second before the screen
  changed anyway.
- The screen that waits for a chip no longer prints the reader's error in red
  under its own instruction. "Hold it closer" in red, under "hold the spool
  against the box", while somebody is doing exactly that, reads as a fault they
  have caused. The error still goes to the serial log.
- The reader is called **Scan** on the home screen and in its own header, and
  its second line asks the question it answers: "What is this?".
- In reader mode, a tap anywhere on the spool's data goes back, not only the
  chevron: the hand that is not holding the spool should not have to find a
  corner.
- Reader mode marks a valid chip **Certified** rather than "genuine",
  translated in each language.

## [1.62.0] - 2026-09-20

### Changed

- **The spool goes to the printer as soon as it is read.** The confirm step
  with its Send / No buttons is gone: choosing the slot and presenting the
  spool were the decision, and being asked a third time only stood between the
  user and the machine. Nothing new appears for the write itself either - the
  screen that asked for the spool stays exactly as it is, minus the Cancel
  button, which has nothing left to call off once the chip is caught. The
  write is over before a screen of its own could be read, and one that flashes
  past reads as a fault rather than as progress.
- The three screens of one send now share a title: the slot's name, alone.
  "Slot B2" on one and "B2" on the next read as two different places.
- **The screen after a send was rewritten.** One success screen instead of two:
  it confirms what was sent to the printer - the spool's colour, material,
  brand and diameter - and then names the next step in a green block, with the
  slot on its own line. It no longer shows what the slot held before, nor the
  colour the printer settled on. It stays five seconds, counted down by a bar
  that drains, and a tap anywhere ends it sooner.

### Added

- Reader mode shows the **drying** instruction the chip carries - temperature
  and hours on one line. The disc above it lost 8 px to make room, which is
  what a 320 px screen costs.

## [1.61.0] - 2026-09-19

### Added

- A home screen that asks what you came for: **Printers** or **Reader**. The
  printer list is one tap in, with a chevron back; the account and Wi-Fi state
  live on the home screen, which is where the device starts.
- **Reader mode**: put a spool on the pad and read it - colour, material,
  brand, finish, diameter, the two temperature windows, what is left on the
  spool, and whether the chip proved itself genuine. It is the NFC tester's
  question without the NFC tester's thirty fields, which stay in Settings.
- A **Write** row on the home screen, greyed and marked as coming: writing a
  spool from the device itself is not built yet, and the row says so rather
  than appearing one day and moving everything under it.

### Changed

- The account and Wi-Fi icons are on the home screen only. The printer list
  carries a back chevron, its title and the gear instead: 240 px does not hold
  all six, and the title was running under the icons.
- Settings returns to the screen the gear was pressed on.
- The reader row wears the same amber as the reader screen: one subject, one
  colour.
- Internal: a guard now fails the build when the device and the setup portal
  disagree about how many bars a signal is worth. The scale is written in two
  languages, in two files, and nothing but a person holding a phone beside the
  device would have noticed them drifting apart.

### Fixed

- Two NFC icons on one screen shared a single pixel buffer, so the second one
  drawn overwrote the first and both wore the second's colour. Each canvas icon
  now owns its buffer and frees it with the object.
- `scripts/flash.sh` no longer claims no board is plugged in when the
  PlatformIO virtualenv has lost its `esptool` module. It falls back to the
  copy that ships with the platform, and says so plainly if neither is there.

## [1.60.0] - 2026-09-18

### Changed

- **The Wi-Fi icon is calibrated for this radio, and it stops twitching.** The
  scale moved 10 dB: full strength is now -70 dBm rather than -60, so a network
  that works is no longer drawn as one that is failing - the ESP32 reads low,
  and by an amount that differs between boards, so the old scale worried people
  for nothing. The exact dBm is still printed beside the word. And the wave no
  longer moves every second when the signal wanders across a boundary: the
  reading is averaged once a second, and an arc is gained or lost only after
  3 dB past the line. A change of more than 15 dB is shown at once, because
  that is somebody moving the device rather than noise.

## [1.59.0] - 2026-09-18

### Fixed

- **An update could fail to install on a device with several printers.** The
  download opened its connection before the device had freed the memory a
  secure connection needs, and whether it succeeded came down to how much
  happened to be free: on a bench with seven printers linked it failed outright
  with 11 KB of contiguous memory where it needed 20. It now waits for the room
  - the printers step aside in a few milliseconds - and connects with 42 KB.
- **The screen woke but stopped answering.** After the display had gone to
  sleep once, taps did nothing at all: the sleep check and the drawing were
  both reading the touch panel, on the same bus, from two places.
- **The loading spinner on the first printer import is the right size and
  centred**, and says what it is waiting for. Padding on an LVGL arc is taken
  off the arc itself, so a 40 px spinner with 24 px of padding had 16 px left
  to draw in.

### Changed

- **The screen no longer freezes while the device talks to a printer.** Drawing
  and the touch panel now run on their own task, so a connection to a printer
  that is switched off - which blocks for over a second - no longer stops the
  interface. Measured on a bench with six printers: the panel went blind for
  7.9 seconds out of every 80, in gaps of up to 1.3 seconds; it is now 1.2
  seconds out of 80, and the worst gap is a tenth of a second. Scrolling stays
  smooth while a printer is being dialled, and a tap during it is no longer
  lost - it is acted on when the device comes back, instead of never.

## [1.58.0] - 2026-09-18

### Fixed

- **A device stuck on a distant access point now moves to the near one.** On a
  network with several access points sharing a name, the device chose once - at
  the moment it connected - and stayed there however bad the signal became. On
  the bench it sat on an access point at -73 dBm while another on the same
  network was at -48. It now checks once a minute and moves when another is
  clearly better. Moving costs a reconnection: the printers drop and come back
  within four to fifteen seconds, which is why it only moves for a large
  difference.
- **A spool write is never interrupted by the device changing access point.**
  Moving costs a reconnection, and one landing between Send and the printer's
  answer would turn a write into a failure the user watches happen. The device
  now stays where it is while a write is in flight and while a spool is being
  scanned, reviewed or sent.
- **A wrong Wi-Fi password is reported at once** instead of after a 30-second
  wait, and the log says why a connection was lost rather than only that it
  was.

## [1.57.0] - 2026-09-18

### Fixed

- **A board with no battery no longer shows a battery.** The charge is measured
  on a pin that reads the battery rail, and on USB that rail is held up by the
  charger whether a cell is plugged in or not - on one board at 4.27 V, on
  another at 4.01, which is where a real cell sits. A level alone cannot tell
  them apart, and the board has no pin to ask. The device now listens to the
  charger's own ripple instead: a cell swallows it, an empty connector does
  not. Measured on two boards, 2-3 mV against 7-22, and the answer takes a
  second.

## [1.56.0] - 2026-09-17

### Fixed

- **A Bambu AMS HT is now seen.** A printer reports its HT units as 128 and
  up, and everything above id 3 was being dropped: an X1C with two AMS and an
  AMS HT showed the two AMS and the external spool, and the dryer not at all.
  Reported with a full MQTT dump, which is what made it findable.
- **A Bambu with more than one AMS updates its slots in the background too.**
  The report from such a printer is about 9 KB and the background buffer held
  8, so it was dropped without a word - the slots only ever refreshed while
  that printer was the one on screen.

## [1.55.0] - 2026-09-17

### Fixed

- **The account stopped syncing on a device with many printers, and only the
  orange account icon said so.** The sync needs 16 KB of contiguous memory for
  its task; with seven printers linked the largest free block was 16 372 bytes
  - twelve short - and the mechanism that frees memory only ran once a sync had
  started, which this one never could. The room is now asked for before the
  attempt, and the log says how much memory there was when one fails.

### Changed

- Internal: the last Portuguese left in the firmware - status strings and
  comments in the printer backends - is now English, and the guard that checks
  for it knows the words it had been walking past.
- **The screen keeps moving while the printers are talked to.** Measured on a
  bench with six printers: the panel was blind for 8.9 seconds out of every 75
  - a Bambu report costs about 70 ms to read and they were all read back to
  back before the screen was allowed to draw. Three changes bring that to 7.9
  seconds and halve the typical gap: a read buffer in front of the MQTT socket,
  a frame drawn between printers rather than after all of them, and a loop that
  sleeps only as long as the interface says it can.

## [1.54.0] - 2026-09-17

### Added

- **A battery entry in Settings, on the boards that have a battery.** A
  battery that fills to the level, the charge as a percentage, the state in
  words, the voltage it was all worked out from, and how long is left - to
  empty on the cell, to full on the charger, from the first second and
  corrected as the device watches its own rate. On a board with no cell the
  entry is not there at all.
- **The level reads the same with the cable in and out.** A charger holds the
  voltage above the cell's own, and the device measures how much - at the
  moment a cable moves, which is the only moment it can be measured - rather
  than assuming it. It remembers the figure across restarts.
- `/api/batt` returns the same numbers as JSON, for anyone watching a device
  from a desk while the cable moves.

## [1.53.0] - 2026-09-16

### Fixed

- **A wrong language picked at first start can be changed back.** The Wi-Fi
  setup screen that follows the language choice now has a back arrow to the
  language list; there was no way back before. The arrow answers from the
  moment the screen appears: for almost a second after it was drawn, the
  device was starting its setup network and every touch was ignored.
- **The account pairing screen fits on the screen.** The instruction ran edge
  to edge, the address broke in two and the countdown was cut off at the
  bottom. The screen now has the same header as the Wi-Fi one, the address on
  a line of its own, the code under it and the countdown under that.
- **The address under the QR is the one the QR points at.** It read
  `tigersystem.io/pair`, which was written into the translations and answers
  404; it is now taken from the pairing link itself, so the two can never
  disagree again, and it is drawn at the largest size that fits it on one
  line. The pairing code is taken off it, whether the link carries it as a
  parameter or as the last part of the address, so the line is the address
  and nothing else.
- **An empty printer list offers a button, not an instruction.** It used to
  say "All printers hidden. Settings > Printers" and leave the rest to the
  reader; the home screen now carries a Select printers button that opens the
  picker.
- **The printer lists no longer run off the screen.** On the first-boot
  printer choice the Confirm button was sliced along the bottom bezel and the
  gauge clipped at the top; on the Settings printer list the last row was cut.
  Both lists were measured without the space the screen puts between its own
  rows.
- **An expired pairing code returns to the sign-in choice by itself.** It used
  to stop on an error screen that had to be tapped; a code nobody got to in
  ten minutes is not a failure, and choosing Google again fetches a fresh one.
- **"Scan the QR code"**: a shorter instruction above the QR codes, in every
  language.
- **The language list shows its scrollbar**, like every other list - nine
  languages do not fit on one screen and nothing said there were more.
- **The screen stays smooth when no NFC reader is connected.** The device kept
  retrying the reader in a way that froze the interface for more than a second
  in every three - scrolling stuttered and a tap took a moment to land, which
  showed on first setup before the reader is wired.

## [1.52.0] - 2026-09-15

### Fixed

- **Writing the external spool on a Bambu Lab printer works on current
  firmware.** The device addressed the external spool the old way, which an
  X1C on firmware 01.12 accepts and silently ignores - the slot kept its old
  filament. It now uses the addressing current Bambu Studio sends.

### Changed

- **A Bambu Lab slot is sent the filament's own Bambu filament id and
  temperatures.** The filament id comes from TigerTag's product page for a
  TigerTag+ spool, otherwise from the material table built into the device -
  instead of a guess from the material's name, which is now only the fallback.
  The type is the material family (a filled material keeps its filler), and the
  nozzle temperatures follow the same order as on a Creality: product page,
  chip, material table, default.

## [1.51.2] - 2026-09-15

### Fixed

- **The language screen's title no longer runs into the rotate button.** At
  first start it read "Choose your language", which in French touched the
  button beside it; it is now the one word "Language", as when the screen is
  opened from Settings.

### Removed

- **The rotate button on the first-start language screen.** It looked like a
  refresh button, and the accelerometer already sets the orientation on first
  start; it can still be changed under Settings > Display.

## [1.51.1] - 2026-09-15

### Changed

- **The languages in the language picker are written in bold.**

## [1.51.0] - 2026-09-15

### Added

- **Chinese.** The TigerSpool now speaks simplified Chinese - 中文 is the last
  entry in the language list, on the device and on the Wi-Fi setup page. The
  panel draws it from a subset of Noto Sans SC holding only the characters the
  interface uses, as the TigerScale does. The NFC tester's field names stay in
  English, as in every language.

### Fixed

- **The home screen's title follows a language change.** It kept the language
  the device had started in until the next restart.

## [1.50.0] - 2026-09-14

### Changed

- **The Wi-Fi settings screen is clearer.** The network gets a card of its
  own with the Wi-Fi wave, its name in bold and the signal as a word in colour
  - Excellent, Good, Fair, Weak - beside the dBm. The address, MAC and the
  Wi-Fi channel sit in a second card, in bold.
- **Changing network can be backed out of.** The setup screen opened from
  Settings > Wi-Fi > Change network now has a header with a back arrow: it
  closes the setup access point, rejoins the saved network and returns to the
  Wi-Fi screen. A first start, with no network to go back to, is unchanged.
- **The setup access point's password has a padlock in front of it**, so it
  reads as the Wi-Fi password rather than a second line of the network name.

## [1.49.3] - 2026-09-14

### Changed

- **Slot names are bold and larger** on a printer's slot screen - Ext., 1A,
  A1 - so the slot is easy to find at a glance.
- **A brand too long for its slot is cut with one dot, not three**, on every
  printer: "Durami." instead of "Dura...", two more letters to recognise it by.

## [1.49.2] - 2026-09-14

### Fixed

- **A Creality slot keeps the nozzle temperatures it is sent.** The printer
  stores them only when they are written as decimal numbers - 215.0, not 215 -
  and the TigerSpool sent whole numbers, so every slot it wrote read back 0/0.
  Measured on an Ender-3 V4: the same frame with 215.0 is kept, with 215 it is
  dropped. The TigerTag RFID Connect app always wrote them with a decimal
  point, which is why its writes kept their temperatures.

## [1.49.1] - 2026-09-14

### Fixed

- **A filled material keeps its filler in a Creality slot's type.** An ABS-CF
  spool is sent as "ABS-CF", not "ABS": the type is the material family plus
  its filler when the material has one, from the TigerTag material table.
- **The material table built into the device is current**: PA11-GF is now
  known as a glass-filled material and PC-PTFE as a filled one.
- **The 1.49.0 notes said a Creality printer recognises the filament.** It
  does not always: the notes under 1.49.0 now say what the printer keeps.

## [1.49.0] - 2026-09-14

### Fixed

- **A Creality slot is sent the filament's real values.** It used to receive a
  material id of "0" and fixed default temperatures. It now receives
  Creality's material id, the nozzle temperatures and the pressure advance, in
  the same frame the TigerTag Connect app sends. For a TigerTag+ spool they
  come from TigerTag's product page when the device can reach it; otherwise,
  and for every TigerTag, from the chip and the material table built into the
  device, so this works offline too.

  What the printer keeps, measured on an Ender-3 V4 with a CFS: the type,
  brand, name, colour and pressure advance land in the slot. The material id
  is kept only when brand, type and name match the printer's own material
  library - "Generic PLA" does, a spool sent under its own brand does not.
  The temperatures in this release did NOT land: see 1.49.2.
- **A Creality slot's material type is the material family.** A "PLA High
  Speed" spool is sent as type "PLA", taken from the TigerTag material table,
  because that is what the printer's type field means.
- **A slot no longer flickers back to its old spool right after a write** on
  a Creality: the device waits a moment before reading the slot again.

### Changed

- **Slot names and brands on a printer's slot screen are white**, not grey, so
  they read at a glance.

## [1.48.1] - 2026-09-11

### Changed

- **A new device starts at full brightness**, instead of 80%. Lower it any
  time under Settings > Display. A device that already has a brightness saved
  keeps it.

## [1.48.0] - 2026-09-11

### Fixed

- **The device connects to the nearest Wi-Fi access point, not the first one
  it hears.** On a home network with several access points under one name, it
  could stay attached to a distant one for good - through reboots - and show a
  weak signal while a TigerScale beside it showed full bars. On the bench it
  went from -79 dBm to -46 dBm, and from one ping in five lost to none.

### Changed

- **The three icons at the top of the home screen line up.** Account, Wi-Fi
  and settings are now the same height, on the same line, evenly spaced. The
  Wi-Fi wave was a third smaller than the other two, and the gaps between them
  were uneven.
- **Tapping the account or the Wi-Fi icon opens Settings**, like the gear.
  Only the gear used to answer.
- **The Wi-Fi icon shows full signal from -60 dBm**, then two arcs from -70
  and one from -80, the thresholds phones use. Full used to need -40 or
  better, so an excellent connection showed two arcs of three. The setup
  page's network list uses the same thresholds.

## [1.47.1] - 2026-09-11

### Fixed

- **A printer that moves to a new address is followed.** When your TigerTag
  account gives a printer a new IP, the device now uses it. Before, the address
  the device already held always won, so a printer that had moved stayed
  unreachable on TigerSpool for good while Tiger Studio talked to it fine - an
  AD5X did exactly that.
- **Printers no longer swap addresses and access codes when the account
  changes.** Adding or removing a printer in Tiger Studio shifted the device's
  list by one, and every printer after that point took its neighbour's address
  and code - a Bambu Lab would then refuse the connection with an access-code
  error, and a switched-on printer could turn into a different one. Each
  printer is now recognised by its serial number, wherever it sits in the list,
  and keeps its own settings and its own switch.
- **Two entries in the account for the same printer become one.** An old entry
  left at a previous address beside the current one used to show up as two
  printers, the stale one first. The newer entry wins.
- **A printer whose settings change reconnects straight away**, instead of
  staying on the old connection until something else closed it.

### Changed

- A printer newly added to your account arrives on the device switched off, as
  on first start, so it does not take room you did not choose to give it.

## [1.47.0] - 2026-09-10

### Added

- **The device tells you how full it is before a printer stops connecting.**
  Settings > Printers, and the Choose printers step at first start, show a
  **Load** bar with a percentage. Some printers take far more of the device's
  memory than others - a Bambu Lab or an Anycubic about ten times an Elegoo,
  a FlashForge or a Creality - so the limit is not a number of printers but how
  much they load it. A printer that would not fit is refused when you switch it
  on: the switch does not move, the bar turns red and says **Not enough room**.
  Switching a printer off always works. How it is counted, and every
  measurement behind it, is in docs/CONNECTION-BUDGET.md.

### Changed

- **The documentation says what the firmware does today.** Elegoo and Anycubic
  were still listed as not implemented; both read slots on real printers. A
  Bambu Lab in cloud mode is now documented as read only, which it is. The
  firmware README no longer says nothing builds, and the web installer's page
  shows the manifest that is actually published.

## [1.46.0] - 2026-09-10

### Changed

- **More printers can be connected at once if you use Bambu Lab in cloud
  mode.** Every Bambu Lab printer on your account now shares a single secure
  connection instead of opening one each. That connection is the most
  memory-hungry thing the device does, so a second cloud printer now costs
  about a fifteenth of what it did, and there is room left for others. Each
  printer still shows its own spools and its own status; only the connection
  behind them is shared. Printers in LAN mode are unaffected.
- The device answers faster on the network: Wi-Fi power saving is off, which is
  the right choice for something that runs from a wall socket.

### Fixed

- **The device can no longer freeze indefinitely.** If its main program ever
  stops responding, it now restarts itself within thirty seconds and records
  why, instead of staying frozen until it is unplugged.


## [1.45.2] - 2026-09-10

### Fixed

- **The printer you are looking at connects more readily.** The device was
  reserving far more memory than a Bambu Lab printer on screen actually uses -
  its large receive buffer lives in a separate memory the device has plenty of,
  and was being counted against the scarce one. Connection limits now use costs
  measured on real hardware, printer by printer.


## [1.45.1] - 2026-09-10

### Changed

- **The update page is one page now.** Opening it shows your version and a
  turning ring while it checks - the check has always started on arrival, but
  nothing on screen said so. If something newer exists, the offer appears under
  your version instead of replacing it: the download icon, the version you are
  on and the one on offer, and Install at the bottom of the screen.
- The card is labelled "Version" rather than "Installed version".


## [1.45.0] - 2026-09-10

### Changed

- **Bambu Lab printers are no longer polled.** The device asked each one for
  its entire state every eight seconds - about 81 KB a minute for two printers,
  for spools that had not moved. The printer already announces a spool change
  by itself, in a message five times smaller, so the device now listens instead
  of asking. Your screen updates exactly as before; the Wi-Fi does a fraction
  of the work.
- The update page puts **Install at the bottom of the screen**, and scrollbars
  appear only when a page actually has more to show.

### Fixed

- **Printer connections stopped dropping and reconnecting.** A four-second
  read limit added to keep the interface responsive was also cutting off large
  replies, so the device hung up on printers that were answering perfectly -
  once every poll. Four minutes of observation now show none at all, where
  there were four reconnections in two and a half minutes before.


## [1.44.0] - 2026-09-10

### Fixed

- **The device reconnects to Wi-Fi on its own.** It asked for the network once
  at startup and never again - so if the association failed at boot, or the
  access point dropped the device later, it stayed there with a lit screen,
  every printer red and no way back short of unplugging it. It now asks again
  after fifteen seconds and keeps asking. This is the most important fix in the
  release.

### Changed

- **The NFC Tester is a proper bench instrument.** It reads in English whatever
  language the device is set to - it is used beside a reader log and a
  datasheet, and two people comparing the same spool have to see the same
  words. One field per row, in a fixed order, with a colour bar across the top
  and the raw chip pages behind a HEX Code button.
- **A spool's readings stay on screen when you take the spool away.** They used
  to clear the moment the chip left the reader, which is exactly when you want
  to read them.
- **The colour bar shows two or three colours for bicolour and tricolour
  spools**, decided by the spool's aspect rather than by the colour bytes -
  black is a real colour, and an unused slot is stored the same way.
- Colours are shown in hex, the main one with its alpha byte, and the page dump
  is set in a monospace face so it lines up in columns.
- **Titles, menu rows and printer names are heavier and larger**, and buttons
  are set in a size meant to be read at arm's length.
- The NFC Tester has an icon of its own instead of borrowing the Display row's
  sun, and the sun now matches the other icons' size.


## [1.43.1] - 2026-09-09

### Fixed

- **The device can check for updates and refresh its account again while
  several printers are connected.** With six connections open there was still
  memory free, but not in one continuous piece large enough for a secure
  connection - so the update screen showed "manifest HTTP -1" and the printer
  list stopped refreshing. The background connections now step aside for the
  second or two a secure request needs, and come straight back. The printer you
  are looking at keeps its connection throughout.


## [1.43.0] - 2026-09-09

### Added

- **A "Choose printers" step when you set the device up.** Right after your
  account is linked, the device lists every printer on it with none selected,
  and you turn on the ones this box should talk to. An account with a dozen
  printers should not have a new device dialling all of them on its first boot.
  The step belongs to setup and to a factory reset; signing in again does not
  wipe what you chose.
- **A blue dot while a printer is being connected to**, green once it is, red
  when it is not. Red for a machine the device is actively dialling is wrong,
  and it is wrong for exactly the seconds you are standing there watching.

### Fixed

- **A printer that lost its connection now comes back on its own.** Once a link
  ran out of attempts it never tried again until you tapped the printer - so a
  device left alone through a Wi-Fi blink or a printer reboot ended up with
  every dot red and no way back. It now starts over after a minute.
- **The interface no longer freezes when the network goes bad.** Opening a
  connection blocks, and the underlying defaults are very long - fifteen seconds
  for an MQTT socket, two minutes for a TLS handshake. With one connection per
  printer, all of them retried at once when Wi-Fi dropped and the screen stayed
  lit but stopped responding. One printer is dialled at a time now, and the
  waits are capped at a few seconds.
- **Connections are no longer closed to keep memory in reserve.** Each one is
  priced before it is opened, and the safety limit only acts on a shortage that
  actually lasts, instead of on the brief dip an account refresh causes.
- The cards behind rows are a black interior with a grey outline, which is what
  the panel actually shows well; scrollbars are visible, the same everywhere,
  and no longer touch the rows or the header.
- A long title no longer pushes the header's rule off the screen.


## [1.42.1] - 2026-09-08

### Changed

- **The update screen shows both versions**, the one installed and the one on
  offer: `1.42.0 > 1.42.1`, the version being left in grey and the new one in
  orange. The caption "Version available" is gone - it spent a line saying what
  the number under it plainly was, while what you are updating *from* was not
  on the screen at all.
- The update page no longer repeats the installed version in a row of its own
  while an update is waiting.


## [1.42.0] - 2026-09-08

### Changed

- **Every printer gets its own connection.** Until now there was one backend
  object per brand, shared by every printer of that brand: two Bambus, or two
  FlashForges, could not both be connected, and the second inherited the
  first's answers. Each printer now has its own object, its own socket and its
  own slots. Two Bambu Lab printers show their own spools side by side.
- **Connections are opened while there is memory for them.** A TLS session
  costs about 40 KB of internal RAM, so a device with nine printers switched on
  holds roughly six links at once. The printer on screen is always served
  first; the others are opened in list order, closed if free memory runs low,
  and retried after a wait that grows. A printer without a link reads as
  disconnected, because it is.
- **The cloud slot notice is one message.** "Working only with LAN Mode + Dev
  Mode" in orange, the QR code, and "Scan for tutorial" beneath it.

### Fixed

- **A printer that is switched off no longer shows as connected.** A link
  asked its backend whether it was connected before ever dialling, and with a
  shared backend the answer belonged to the other printer. A Creator 5 Pro that
  was not on the network at all showed a green dot and another machine's four
  spools. A link must now dial for itself before it can report a connection.
- **Reloading the printer list could delete most of it.** With several
  connections open there was not always enough contiguous memory for a TLS
  request, and a sync that reached one brand out of six wrote its result as if
  it were the whole account: thirteen printers became three. A brand that does
  not answer now keeps what the device already knew about it.
- **A printer switched to cloud mode is recognised.** When an account holds
  both an old LAN document and the newer cloud one for the same machine, the
  newer wins. An A1 in cloud mode was still being dialled on the LAN, with an
  access code the printer had since changed.
- **Two Bambu printers no longer disconnect each other.** They shared one MQTT
  client identifier, so the broker dropped whichever had connected first. The
  identifier now carries the printer's serial number.
- **The QR code on the cloud notice is centred**, with an even white margin on
  all four sides instead of running into the edge a scanner needs.


## [1.41.0] - 2026-09-08

### Added

- **A Bambu Lab printer in cloud mode shows its spools.** The device connects
  to Bambu's own regional broker with the session Tiger Studio put in your
  account - it never signs in to Bambu itself - and reads the same report the
  LAN path reads, so the existing parser handles it unchanged. An X1C reachable
  only through the cloud now lists `Ext.` and `B1`-`B4` on the panel.
- **Tapping one of those slots says why it cannot be written**, on a screen of
  its own, with a QR code to the page about switching the printer to LAN mode.
  The slot is tappable on purpose: a cell that ignores a finger teaches nothing.
- **A Cancel button on the connecting screen.** It stops the attempt as well as
  leaving it - connections are held open across screens now, so walking away
  would have left a printer that is not answering retrying unseen.
- **The printer lists have a visible scrollbar.** Both call
  `lv_obj_remove_style_all`, which takes the theme's scrollbar with it, so the
  mode was set on lists that drew nothing - and eleven printers gave no sign
  there were six more below the fifth.

### Fixed

- **The selected printer now gets its brand's connection.** One backend exists
  per brand, and the first visible printer of a brand held it - so selecting a
  second Bambu showed a screen that never connected while the log reported the
  other one's address. Whoever the user is looking at takes it.


## [1.40.0] - 2026-09-08

### Changed

- **Refreshing the printer list shows a screen, not a tinted icon.** The
  account read takes about fifteen seconds, and a coloured glyph in a corner is
  a still picture - which is what makes somebody press the button a second
  time. A turning ring and "Importing printers" instead, until the answer is
  in; then straight back to the list, rebuilt. It also stops the list being
  touched while it is about to change underneath.
- **The refresh button is gone from the main printer list.** It belongs where a
  missing printer is noticed and dealt with, which is Settings > Printers - and
  its absence gives the main screen its full-size title back.


## [1.39.0] - 2026-09-08

### Added

- **Bambu Lab printers in cloud mode appear on the device.** They used to be
  dropped at import, so a printer you own simply was not there and nothing said
  why. They are listed now, and opening one explains itself: **cloud printer,
  read only**, with a QR code to the page that explains switching it to LAN
  mode. Their slots are not tappable, because a cell that looks pressable and
  can never do anything is worse than one that plainly is not - Bambu's cloud
  broker accepts a report subscription and refuses the command that sets a
  tray.

  **What is not here yet:** the slots themselves. Reading them needs a session
  against Bambu's own broker, and everything that needs is now known - see
  WORKLOG.md. Nothing is dialled meanwhile: a cloud printer is never probed and
  never gets a link, so it costs nothing to list.

### Fixed

- **A FlashForge's first slot is no longer split onto its own row.** The
  station has four slots, 1A to 1D, and none of them is an external spool - so
  an AD5X showed 1A alone above the other three.


## [1.38.2] - 2026-09-08

### Added

- **The refresh button is on the main printer list too**, not only in Settings.
  Both ask the account now rather than waiting out the five-minute cycle, and
  both turn amber while the sync runs.

  Fitting it on the main screen costs its title a size: four controls and a
  word share 240 px, and the offsets there are measured off a capture rather
  than chosen - at the old size "Imprimantes" ended at x=114 and the refresh
  glyph began at 117.


## [1.38.1] - 2026-09-08

### Changed

- **The refresh button moved to Settings > Printers**, where it belongs: that
  is the screen where a missing printer is noticed. It has a header of its own
  with room for it, so the home screen goes back exactly as it was - full-size
  title, three controls.


## [1.38.0] - 2026-09-08

### Added

- **A refresh button in the printer list's header.** The list already reloads
  itself every five minutes, which is right for a box on a shelf and useless to
  somebody who has just added a printer in Tiger Studio and is standing in
  front of the device. The button asks the account now. It is the progress
  indicator too - it turns amber while the sync runs, because a control that
  does something invisible for fifteen seconds gets pressed again, and again.

### Changed

- **The printer list's title is a size smaller.** Four controls and a title
  share 240 px: at the old size "Imprimantes" ended at x=114 and the refresh
  glyph began at 117, which reads as one run of ink. Only this screen changes -
  every other has at most a chevron beside its title.


## [1.37.0] - 2026-09-08

### Added

- **Several printers stay connected at once.** Every printer you leave switched
  on in Settings gets its own live session, kept open whatever screen you are
  looking at - so opening one is instant instead of a connection you wait for.
  Four brands were held open together on the bench: Creality, Snapmaker, Elegoo
  and Anycubic.

  It costs almost nothing, which was measured before it was written: a plain
  MQTT session is about 3 KB of internal RAM and a TLS one about 38 KB, and
  three TLS sessions open together still leave 79 KB free.

  **The limit this leaves:** two printers of the SAME brand cannot both be
  connected, because a backend holds its state in file statics. Whichever is
  first in the list wins.

### Fixed

- **Snapmaker never connected at all, and said nothing about it.** Moonraker
  answered **403 Forbidden** to the WebSocket upgrade and the client fired no
  event, so a printer answering perfectly over HTTP looked exactly like one
  switched off. The cause is a default nobody would look for: the WebSocket
  library sends `Origin: file://` on every handshake, and Moonraker refuses an
  origin that is not in its `cors_domains`. The same upgrade without an Origin
  is accepted.
- **A Snapmaker showed no vendor.** `filament_vendor` was in every report and
  never read, so four spools Moonraker names R3D, Generic, Generic and
  Snapmaker all showed a dash. The variant joins the family too, the way the
  printer's own screen writes it: "PLA Silk", not "PLA".
- **Writing to a Snapmaker put the whole name in the wrong field.** It sent
  `FILAMENT_TYPE=PLA_High_Speed` where the printer keeps a family and a variant
  separately. Split at the first space now, so a written slot reads like the
  ones the printer wrote itself.
- **A Snapmaker's four extruders are on one row.** No external spool exists on
  a U1, so nothing should be split off onto a row of its own.
- **A black spool was an invisible cell.** The panel's ground is black and so
  are plenty of filaments; the colour was right and the slot looked empty. A
  hairline edge fixes it.
- **A long vendor name no longer wraps** and takes its row's height with it.
- **The dots on the home screen tell the truth.** They came from the
  reachability probe alone, which walks one printer at a time and expires - so
  a printer the box was actively talking to could show red between two sweeps.
  An open link lights the dot.

### Changed

- The WebSocket timeout goes from 1200 ms to 2500. 1200 was set to stop a
  five-second freeze per attempt on an unreachable printer; the probe gate does
  that job now, and the same macro is also the deadline for an upgrade
  response.


## [1.36.1] - 2026-09-08

### Fixed

- **An Elegoo with its Canvas hub connected no longer shows an empty `Ext.`.**
  On a Centauri Carbon 2 the hub and the single spool are the same feed path:
  plug the Canvas in and the external holder stops existing, unplug it and the
  four trays do. It is one or the other, never both, and drawing an empty
  external cell beside four full trays invented a fifth place a spool can be.
  Four trays with the hub, one external spool without it.


## [1.36.0] - 2026-09-08

### Fixed

- **Elegoo and Anycubic now connect. The credentials had been there all along.**
  The import kept two lists of field names forty lines apart - a server-side
  mask deciding what Firestore sent, and a client-side filter deciding what the
  parser kept - and nothing made them agree. Three fields were added to the
  first and not the second, so Firestore sent `mqttPassword`, `username` and
  `acuModelId` and the parser threw all three away before anything read them.
  On the bench that looked exactly like an account with no Elegoo access code
  and an Anycubic with no username. It is one list now, asked twice.
- **An Anycubic's slots are all on one row.** The grid drew the first slot
  alone above the rest, which is right for every brand that has a single
  external spool and wrong for Anycubic: its box -1 is a four-slot unit and
  none of them is external. A backend says which it is rather than the screen
  assuming.

### Verified on hardware

- **A Centauri Carbon 2 reads through the device**: `Ext.` empty, `S1` PLA
  Sunlu, `S2` PLA Silk Generic, `S3` PLA Sunlu, `S4` ASA Landu - the same four
  the printer reports to Tiger Studio.
- **A Kobra X reads through the device**, and its TLS handshake was the single
  biggest unknown in the protocol notes: mbedTLS completes it against the
  self-signed broker. `A1` PLA, `A2` PET, `A3` PLA, `A4` PC.


## [1.35.0] - 2026-09-08

### Changed

- **Every printer in the account reaches the device.** The limit was eight, and
  eight was worse than it sounds: the import walks the six brands in a fixed
  order and stops when full, so an account with more printers did not lose a
  random eight - it lost the LAST BRANDS, every time, which is Elegoo and
  Anycubic. The two newest backends supported printers that could never appear.
  Twenty-four now; a bench account of twelve imports whole, and NVS reports
  278 of 630 entries used.

### Fixed

- **The reachability probe knocked on the wrong port for the two new brands.**
  It has a port per brand and falls back to Creality's 9999, so Elegoo and
  Anycubic were probed on a port they do not open, marked unreachable, and then
  never dialled - a Centauri Carbon 2 reported "probe says unreachable" on the
  bench while Tiger Studio was talking to it. Every backend must be added to
  that table, and the failure when it is not is silent and total.
- **Anycubic kept dialling a broker it had no address for.** Its readiness
  guard tested the device id alone, so a record carrying that but missing the
  username connected to a server that had never been configured, printing
  "connecting..." for ever beside the line explaining why it could not.
- **A missing Anycubic credential now names itself.** All four come from the
  account and none can be read off the printer, so "one of them is missing" was
  not an answer anyone could act on.


## [1.34.0] - 2026-09-08

### Added

- **Elegoo, over the LAN.** MQTT on port 1883 with no TLS at all - the simplest
  transport of the six brands. Two protocols in one printer, and a cable
  decides which: with the Canvas hub plugged in, four trays are read with
  method 2005 and written with 2003; without it, the printer reports a single
  spool through 1061 and is written through 1055, and 2003 answers an error.
  The backend follows whichever is live. Slots are `Ext.` and `S1`-`S4`.
- **Anycubic, over the LAN.** MQTT on port 9883 with TLS against a self-signed
  certificate. The layout is read from the printer rather than assumed: box -1
  is the external unit and is **not** one spool - an ACE Pro 2 reports it with
  four slots - so it is drawn as a unit like any other, and a Kobra X with four
  ACE units reports twenty slots. Labels are `A1`-`A4`, `B1`-`B4`, one letter
  per unit.
- **The printer record carries named credentials.** Anycubic's broker needs a
  device id and a username beside the password, and its topics carry the
  printer's numeric model id. Three named fields rather than another overloaded
  pair: the sixth brand proved the pair does not generalise.

### Known limits

- **Neither backend has ever talked to a printer.** Both are written from
  protocol captures that Tiger Studio proved on real hardware, and both compile
  and are recognised by the account import - but a protocol read off a document
  and a protocol that answers are two different claims.
- **An account with more than eight printers drops the last brands first.** The
  import walks brands in a fixed order and stops at eight, and Elegoo and
  Anycubic are last in that order - so on a large account they are exactly the
  ones that do not arrive.
- **Anycubic in cloud mode is not supported and is not a variant of this.** A
  cloud-mode printer opens no local port at all; reaching it means Anycubic's
  own service, with signing secrets that live inside closed binaries. See
  docs/ROADMAP.md - it is blocked on a decision about what the account stores,
  not on firmware work.


## [1.33.2] - 2026-09-07

### Fixed

- **The boot screen had a line down each side.** The source artwork carried
  RGB(14,14,14) in its first and last pixel column, over 212 of the 320 rows -
  invisible in an image editor on a white desktop, and a bright line on a panel
  that inverts. 424 pixels, blacked out in the asset rather than papered over
  in the generator: the columns beside them are pure black, so nothing of the
  artwork was in them.


## [1.33.1] - 2026-09-07

### Changed

- **The two buttons on a confirm screen have room around them.** They ran edge
  to edge, which reads as a bar rather than as something you press, and they
  touched each other - "Restore" and "Cancel" a couple of pixels apart on a
  capacitive screen, one of which is not undoable. 78% of the width, and a gap
  between them.


## [1.33.0] - 2026-09-07

### Changed

- **Restart and Factory reset are a question and two buttons.** Restart said
  "Restart" in its header, asked "Restart the box?" underneath, and offered a
  button marked "Restart" - the screen saying one word three times, with a note
  about how long it takes and a promise that nothing is lost. Factory reset
  listed what it erased and then reassured you about what came back: two
  paragraphs arguing over how frightening the button should be. Each is now one
  question and two answers.
- **Factory reset is a red button, not a two-second hold.** The hold against a
  filling bar was safer on paper and much less obvious in the hand: the bar had
  to be learned, and a stray press followed by a stray hold is not that much
  rarer than a stray press. A button that says "Restore" in the destructive
  colour, beside one that says "Cancel", needs no teaching.


## [1.32.0] - 2026-09-07

### Added

- **French is written in French.** Every accent is back: Réglages, Écran, Mise
  à jour, Redémarrer le boîtier, Récupération des données, Lecteur prêt. They
  were left out because the font could not draw them - LVGL's built-in
  Montserrat carries ASCII, a degree sign and a bullet, and renders anything
  else as a blank box while logging nothing - so the translation was written
  around a limitation and looked like nobody had proof-read it.

  The faces are generated now, by `scripts/make-ui-font.sh`, from the same two
  source fonts LVGL uses and with LVGL's own symbol list, over
  `0x20-0x7F,0xA1-0x17F,0x2022`. That is Latin-1 and Latin Extended-A: enough
  for German, Spanish, Italian, Portuguese and Polish too, whose translations
  are still to be revisited by someone who reads them. `0xA0` is left out
  deliberately - a no-break space with no glyph is a visible one, which is how
  two brand names carrying one were found in the first place.

  The Display row's sun came along in the same pass, so the face that existed
  solely to carry it is gone and `scripts/make-icon-font.sh` with it.

### Changed

- **The font range is read from the repository rather than from the build
  tree.** It used to be extracted from LVGL's own sources under
  `firmware/.pio/libdeps`, which a fresh clone does not have - so the guard
  that validates every drawn string could not run until someone ran
  `pio pkg install`. The faces are ours and committed, so it needs nothing
  installed.


## [1.31.0] - 2026-09-07

### Fixed

- **The device rebooted while you used it, and which spool you scanned decided
  whether it did.** Every settings screen shared one signature variable, each
  folding its own constant into a hash of its contents - which is not a
  namespace: one screen's signature can land on another's. The NFC tester
  hashes the tag's product id, so scanning a particular spool and then walking
  back through Settings could make the Wi-Fi screen take its "nothing changed"
  path and write into a label LVGL had already destroyed. The assert named it
  exactly: `free() target pointer is outside heap areas`. Screens now claim
  their view by identity rather than by a number that can collide.
- **The NFC tester could not be left with a spool on the reader.** The back
  press was read twice - once to clear the tag, once to leave - and the second
  read found the flag already spent. The chevron cleared the tag and stayed
  put, and the spool put it straight back.
- **Reading a tag froze the interface.** `reader::read()` costs 575 ms - nine
  page transactions and a signature check - and the tester called it on every
  pass for as long as a spool lay there: one frame every 610 ms. It reads once
  per spool now, and the poll that watches for one is rate-limited. A read that
  never succeeds is also bounded in time; one that ran its full twenty attempts
  used to delay a back press by 6.8 seconds.
- **A switched-off printer made every screen stutter.** The WebSocket client
  opens its connection with a blocking connect, from the loop that draws the
  panel, so an unreachable printer cost 1.2 seconds a go - in Settings, in the
  tester, everywhere. The reachability probe already knew the answer; the link
  asks it before dialling now, and hangs up so the library stops retrying on
  its own.
- **The account sync ran on the same core as the interface.** Fifteen seconds
  of TLS and JSON at the same priority as the Arduino loop, every five minutes.
  It moved to the core that already carries Wi-Fi, along with the reference
  table update.
- **Watching the device made it stutter.** The whole 230 KB of a screenshot was
  serialised inside the web handler, blocking the main loop for as long as
  twelve seconds. The frame is copied once into PSRAM and streamed from there,
  with the panel free to run between chunks.
- **The QR code on the failure screen had a white bar across its last row of
  modules** - the row a scanner needs. It was a border growing outward from the
  code; the quiet zone is a white card sized from the code itself now.

### Added

- **The NFC tester is a data sheet.** Every field the chip carries, labelled:
  UID, product id, type, brand, aspects, kind and diameter, nozzle and bed
  windows, drying, timestamp as a date, quantity and what is left of it, the
  second and third colours, the HueForge distance, the custom message - and the
  raw pages underneath, because the question this screen answers is "did each
  value come off the chip correctly".
- **A spool can be proved genuine with no network at all.** ECDSA-P256 over
  SHA-256 of the UID and the two identity words, against the public key that
  ships with the protocol version. It is the one row on that screen that gets a
  colour, because it is the one row that is a verdict rather than a value.
- **The device downloads its own reference tables.** The tables compiled into
  the firmware are the floor - what a new box knows offline on its first
  second - not the source. It fetches the current ones into its filesystem and
  prefers those, per table, so a corrupt brand file cannot throw away a good
  material file, and a failed download costs nothing. Aspects, types,
  diameters, units and protocol versions joined materials and brands; all of
  them are refreshed by `firmware/tools/tigertag_db/db_update.py` at build time
  and by the device itself every six hours.

### Changed

- **"Lecteur NFC" is "NFC Tester"** - it is for testing what a read returns,
  and the name now says so.
- **Cancelling a scan no longer wears the colour of Factory reset.** Restart
  wears the colour it has in the menu instead of an installer's green.
- **A result you can read.** A success clears itself after four seconds; a
  failure waits to be acknowledged, and only the failure says so.


## [1.30.0] - 2026-09-06

### Changed

- **The connection failure screen says one thing.** It listed four causes under
  the QR code, and the most useful of them - a Bambu that has run out of
  connection slots - was below the fold on a 240 x 320 panel. A screen that has
  to be scrolled to reach its point has no point. Title, QR, "Scan the QR
  code", and nothing else; the causes live on the wiki, where they can be
  corrected without shipping firmware.
- **Wi-Fi strength is the TigerScale's own icon now, not colour.** The glyph
  used to go red, then orange, then green as the signal improved, so a
  perfectly usable -70 dBm looked like a fault - orange means "this needs your
  attention" everywhere else on this device. It is two stacked copies of
  LV_SYMBOL_WIFI now: a dimmed one showing the whole shape, and a lit one
  inside a container whose height is the signal. The arcs that light up are not
  objects, they are what a mask lets through of one glyph - which is why three
  arcs drawn by hand could never have matched. The dBm arithmetic came across
  with it, and the portal's network picker moved onto the same arithmetic, so
  one network is now described by one number in three places.

  The rule this settles: **colour carries a state, length carries a quantity.**
  Green connected, red no network - and nothing in between, so nobody has to
  wonder whether a yellow means "middling" or "look out".

### Added

- **Connecting to a printer now looks like something is happening, and says
  what.** A turning ring replaces the grid of empty cells that made an
  unreachable printer look like a broken box, and the line under it names the
  step: reading the account first - the address may be what was wrong - then
  "Attempt 1/3", "2/3", "3/3". A spinner says the device is busy; a counter
  says it has a plan and how much of it is left. Retrying spends three attempts
  rather than five: someone who has just pressed the button is standing there
  watching, and forty seconds of that is long enough to walk away from.

### Fixed

- **The scan, review and result screens no longer carry status dots.** Getting
  there is already the proof: the slot grid is only reachable through a printer
  that answered, and the tap that opened the screen came off that grid. Two
  green dots repeating it spent the top of the panel telling the user what they
  had just done.
- **A Creality status line came out empty after a send to the external
  holder.** That slot's name is deliberately null - it is a word, not a
  position, and comes from the translation table - and concatenating it made
  Arduino invalidate the whole string.
- **Tapping a slot did nothing.** The cell was a button, but the coloured block
  inside it covers almost all of it and a bare LVGL object is clickable by
  default - so every press aimed at a spool landed on the block and stopped
  there. The grid could only be operated by hitting the three millimetres of
  text above or below the colour, which reads as a screen that ignores you. The
  scan screen behind it was there the whole time.
- **The whole interface froze for five seconds at a time while connecting to a
  printer.** Measured on the bench: `backend->loop()` took 5006 ms per attempt,
  and the main loop ran once in that span. The WebSocket library opens its TCP
  connection with a blocking connect, from the same loop that draws the screen,
  and its default timeout is five seconds - which is what an unreachable
  printer costs, every attempt. Capped at 1200 ms, measured again at 1203 ms:
  a printer on the same network answers in tens of milliseconds, so this only
  shortens the wait for one that is never going to answer.
- **The spinner on the Wi-Fi connecting screen did not turn at all.** Two
  reasons at once, and either alone was enough. The screen rebuilt itself on
  every pass to update its countdown, destroying the spinner and creating a new
  one at zero; and the loop around it slept 250 ms between frames. Built once
  and written into now, and LVGL runs at full rate while the join is waited on.
  This is the first screen a cold boot shows.
- **The slot screen was building a new retry button every frame.** The
  unchanged path - the one taken on almost every loop - re-ran the header code
  meant for a rebuild, stacking a fresh button on the previous one for as long
  as the screen was up.


## [1.29.0] - 2026-09-06

### Added

- **A connection failure now explains itself.** After the five attempts, the
  screen says so and lists what to check — is the printer on, is it on this
  network, are its settings right in Tiger Studio Manager, and whether it is
  simply out of connection slots, which is routine on Bambu A1, A1 Mini, A2L,
  P1 and P1S. A QR code goes to `wiki.tigersystem.io` for the long version: a
  page can be corrected the day a new printer joins that list, and a string
  compiled into firmware cannot.

### Fixed

- **Switching printers could leave the previous one's filament on screen.** The
  session is held open now, and the link check saw the old printer still
  connected and declared itself up — so the header named one printer while the
  grid showed another's spools, and no reconnection was ever attempted. The
  link hangs up before it changes printer.


## [1.28.0] - 2026-09-06

### Changed

- **The printer connection is held open instead of being opened per screen.**
  It used to be opened by selecting a printer and closed the moment you pressed
  Back, so it existed in exactly one view and every return cost a reconnection.
  It now comes up as soon as there is a network and a chosen printer — before
  anyone navigates anywhere — and stays up.
- **It gives up after five attempts** and offers a retry instead of trying for
  ever. An indicator that spins indefinitely is one nobody believes, and a
  device that reconnects for ever to a printer that has been sold never stops
  using its radio. The retry re-reads the account first, because a changed IP
  is the most likely reason five attempts failed and retrying the same stale
  address five more times answers nothing.


## [1.27.0] - 2026-09-06

### Fixed

- **The interface no longer freezes while the network is busy.** Two blocking
  jobs were running inside the main loop, both on the home screen — the screen
  people spend their time on. Measured before and after rather than guessed:
  **600 to 1550 ms per pass, now 62 to 71 ms.**

  The reachability probe is a synchronous TCP connect with a 900 ms timeout,
  called every 1.2 seconds. One switched-off printer froze everything for most
  of every second.

  And the LAN sweep that finds a Creality whose address has changed probed four
  addresses per pass at 150 ms each — 600 ms a time, sixty times in a row,
  every time a Creality was unreachable.

  Both now run on their own task. What stayed in the loop is the half that
  rewrites the printer list, because the screens read that same array while it
  is being written. The sweep also went from four addresses per pass to
  sixteen: off the loop, the only cost is how long it takes.

### Changed

- **The slot grid puts the external spool on its own row**, with the CFS's four
  underneath on one line — they belong together and the external one is a
  different thing that happens to sit next to them. The rule generalises: first
  slot alone, the rest four to a line.
- The cards around each slot are gone, and the colour blocks are portrait
  rectangles. A block is already an object; wrapping it in a second rounded
  panel drew a box around a box.


## [1.26.0] - 2026-09-06

### Changed

- **The slot grid is laid out like the mobile app.** Slot name above, a
  coloured block with the material written inside it, the brand underneath —
  so somebody with both in front of them is reading one design rather than two.
  A block rather than a disc, because the material has to sit *inside* the
  colour and a word inside a circle either overflows it or shrinks it until the
  colour stops carrying. The text inside flips between black and white on the
  spool's perceived brightness, not on an arithmetic mean of its channels: the
  eye reads green as far brighter than blue, and a mean puts black on navy.
- Slots now carry the brand the printer reports, so the Creality vendor field
  reaches the screen instead of being written and never read back.

### Added

- **An NFC reader screen under Settings.** It reports the reader and then reads
  a tag: hold a spool against the box and it shows the colour, the material,
  the brand, the two temperature ranges and the product id. It answers "is my
  reader working" by working, rather than by claiming to — and it answers "what
  is actually on this spool", which nothing else did.

### Removed

- The reader dot from the slot screen. It was green on every screen, always,
  because the reader is always ready. A pixel that never varies reports
  nothing; what it used to claim is now provable on a screen that exercises it.
  Only the printer connection is still reported there, because that one varies.


## [1.25.0] - 2026-09-06

### Added

- **The home screen shows the account, next to the Wi-Fi.** The same person
  glyph the Account row in Settings uses, so the two are recognisably the same
  subject. Four states: red when no account is linked, blue while it is
  connecting, orange when the last exchange with the account failed, green when
  it succeeded.

  Green is a claim about the **last exchange**, not about holding a token. A
  device whose network dies stops claiming to be fine, instead of staying green
  until its token expires half an hour later — which is what the same indicator
  does on the TigerScale, and what its author recommended changing on both
  products. Blue exists so a healthy device does not announce "no account" for
  the few seconds of every boot before its token arrives.

### Changed

- `scripts/flash.sh` identifies the board by MAC before it writes anything, and
  refuses if the expected one is not plugged in. See the incident below.


## [1.24.0] - 2026-09-06

### Changed

- **Opening the printer picker refreshes the account.** It is the one screen
  where somebody is looking at the list and expecting it to match what they
  just did in Tiger Studio, so it is the one moment freshness is worth a
  request. The cached list draws immediately and the answer updates it when it
  arrives — nothing to wait for.


## [1.23.0] - 2026-09-05

### Added

- **The device works out which way up it is, on its first boot.** The board has
  an accelerometer; a device that has never been told its orientation now asks
  it once, before the language screen, so nobody has to read an upside-down
  question.
- **A rotate button on that first screen**, top right. The guess can be wrong —
  lying flat, gravity says nothing — and a device somebody cannot read is a
  device they cannot set up, so the correction is on the screen where the
  problem shows rather than three taps into Settings. Pressing it is a
  decision: automatic following goes off and stays off.
- **Orientation under Display is now Auto / 0° / 180°.** Auto is not a
  modifier on the choice, it is one of the choices, and one control removes the
  state where automatic is on, an angle is also selected, and neither explains
  the other. Auto follows the accelerometer while the device is running —
  checked once a second, and only on a reading the sensor is sure of.

### Fixed

- `docs/WIRING.md` said the accelerometer lives on GPIO6/7. It does not: a scan
  of this board answers at `0x6B` on the touch controller's bus, and finds
  nothing at all on GPIO6/7. The warning against putting the reader there is
  unchanged — the pull-ups are what corrupt a UART, and an empty bus still has
  them.


## [1.22.0] - 2026-09-05

### Fixed

- **A background sync no longer scrolls the Settings menu back to the top.**
  The menu never changes shape — eight rows, same order, always — so only the
  four values a sync can touch are written into it now: the printer count, the
  network, the account name and the waiting version, with their icon colours.
  Icons can be recoloured in place rather than rebuilt.


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
