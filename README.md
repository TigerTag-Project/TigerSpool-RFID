<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)"  srcset="assets/logo-tigertag-head-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="assets/logo-tigertag-head.svg">
    <img src="assets/logo-tigertag-head.svg" alt="TigerTag" width="260">
  </picture>
</p>

<h1 align="center">TigerSpool RFID</h1>

<p align="center">
  <strong>Tap a spool on the box. The filament lands in the right printer slot.</strong><br>
  A 2.0" touchscreen, a PN532 reader, and no typing.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT">
  <img src="https://img.shields.io/badge/Platform-ESP32--S3-blue.svg" alt="Platform: ESP32-S3">
  <img src="https://img.shields.io/badge/Build-PlatformIO-orange.svg" alt="Build: PlatformIO">
  <img src="https://img.shields.io/badge/UI-LVGL%208.4-6c3.svg" alt="UI: LVGL 8.4">
  <img src="https://img.shields.io/badge/Languages-8-informational.svg" alt="8 languages">
</p>

<p align="center">
  <a href="https://tigertag-project.github.io/TigerSpool-RFID/">
    <img src="assets/install-button.svg" alt="Install TigerSpool from your browser" width="420">
  </a>
</p>

<p align="center">
  <a href="#what-it-is">What it is</a>
  &nbsp;&middot;&nbsp;
  <a href="#before-you-start">Before you start</a>
  &nbsp;&middot;&nbsp;
  <a href="#quick-start">Quick start</a>
  &nbsp;&middot;&nbsp;
  <a href="#build-it-yourself">Build one</a>
</p>

<p align="center">
  <img src="assets/Hero-TigerSystem-ecosystem.png" alt="The TigerTag system: a TigerPOD reader, Tiger Studio Manager on a desktop, and the TigerTag app on a phone" width="720">
</p>

<p align="center">
  <sub>The TigerTag system. A TigerSpool is the device that takes a spool's
  identity and puts it into a printer's slot.</sub>
</p>

---

**TigerSpool RFID** is open-source firmware for a small box that sits next to
your 3D printer. Hold a spool carrying a TigerTag NFC chip against it, pick a
slot on the touchscreen, and the box writes the filament into that slot on the
printer — material, brand, colour and temperatures. No app, no keyboard, no
retyping what the tag already knows.

MIT licensed. Built with PlatformIO for the ESP32-S3.

---

## What it is

Your printer already has a slot list. Your filament already carries its own
identity. TigerSpool is the thirty centimetres between them.

- **Tap, pick, done.** Hold the spool to the box, tap a slot, confirm. The
  assignment reaches the printer over its own protocol.
- **Your printers come from your account.** They are configured once, in Tiger
  Studio Manager, and every TigerSpool you own reads the same list.
- **It speaks eight languages** and asks which one before anything else.
- **It updates itself.** Over the air, verified, from this repository's releases.

## Before you start

**This is the part that catches people out, so it is first.**

A TigerSpool has no keyboard and no way to type a printer's address, and that is
deliberate — it reads your printers from your TigerTag account instead. Which
means two things have to exist before the box is useful:

### 1. A TigerTag account, created in Tiger Studio Manager

**[Tiger Studio Manager](https://github.com/TigerTag-Project/TigerTag-Studio-Manager)**
is the desktop application for the ecosystem. Install it and create an account
there. That account is what the TigerSpool signs in to — by e-mail, or with
Google through a QR code, so you never type a password on a 2" screen.

Without an account, the box gets through Wi-Fi setup and then has nothing to
sign in to.

<p align="center">
  <img src="assets/tiger-studio-manager.png" alt="Tiger Studio Manager showing a filament library and a printer's slots" width="680">
</p>

<p align="center">
  <sub>Tiger Studio Manager. Your filaments and your printers live here — the box
  reads this list, it does not build it.</sub>
</p>

### 2. Your printers, added in Tiger Studio Manager

Adding a printer means Tiger Studio finds it on your network, or you enter its
address and access code, and stores it against your account.

**If you have not done this, the printer list on the box will be empty.** That is
not a fault and there is nothing to fix on the device — it is showing you exactly
what your account contains. Add the printer in Tiger Studio, and it appears on
the box at the next sync.

The same is true for the details: the access code, the IP address, the brand and
model all come from there. TigerSpool reads that list; it does not build it.

> **In short:** Tiger Studio Manager → create an account → add your printers →
> *then* set up the box.

## Quick start

### Step 1 — install from your browser

**Plug the board into your computer → click Install → wait a minute.** Use the
button at the top of this page. It writes the bootloader, the partition table,
the boot selector and the firmware.

Chrome, Edge or Opera, on a desktop or laptop. Safari and Firefox do not
implement WebSerial and no mobile browser does — the page says so rather than
failing quietly.

### Step 2 — set it up on the box

1. **Pick your language.** Eight of them, before anything else.
2. **Join Wi-Fi.** The box shows a QR code. Scan it with your phone, and a page
   opens listing the networks it can see. Pick yours, type the password on your
   phone, and the box joins — no reboot, nothing typed on the small screen.
3. **Sign in.** E-mail and password, or Google through a second QR code.
4. **Your printers arrive.** From your account, as configured in Tiger Studio.

Then hold a tagged spool against the box, tap the slot you want, and confirm.

From then on it updates itself over the air.

### Build it yourself

```bash
git clone https://github.com/TigerTag-Project/TigerSpool-RFID.git
cd TigerSpool-RFID

bash scripts/flash.sh --monitor
```

That builds the firmware, flashes it over USB and opens the serial console.
Requires [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/).

An ordinary flash **keeps your Wi-Fi credentials, your account session and your
printers** — nothing it writes touches the NVS partition. `--erase` wipes the
chip and is how you get a genuine first-boot again.

## Hardware

One board, one reader, four wires. The electronics are identical for every
printer brand — only the 3D-printed shell changes.

| Qty | Component | |
|---|---|---|
| 1 | Waveshare ESP32-S3-Touch-LCD-2 — 240×320 IPS touch, ESP32-S3**R8**, 16 MB flash, 8 MB octal PSRAM | [buy](https://link.amazon/B0c5hr3uf) |
| 1 | PN532 **V3** NFC module — DIP switches, set to **HSU (UART)**: both switches `0` / OFF | [buy](https://link.amazon/B0dyEfwKa) |
| 1 | USB-C to USB-A cable that carries data — a charge-only cable makes a working board look dead, no serial port ever appears | [buy](https://link.amazon/B00Xg3WT4) |
| — | Four jumper wires | ships with the PN532 |
| 1 | 3D-printed case | [models/](models/) — one per printer brand, plus a desktop stand |

**About 40 €** in total, plus filament.

Full parts list: **[hardware/BOM.md](hardware/BOM.md)** ·
Wiring: **[docs/WIRING.md](docs/WIRING.md)** and **[hardware/pinout.md](hardware/pinout.md)**

<p align="center">
  <a href="docs/WIRING.md">
    <img src="assets/wiring-diagram.jpg" alt="Wiring diagram: ESP32-S3-Touch-LCD-2 to PN532 — 3V3, GND, TX to SDA, RX to SCL" width="600">
  </a>
</p>
<p align="center">
  <sub><a href="https://app.cirkitdesigner.com/project/7a6c0887-8e44-4303-81b3-be51aab4b40a">Interactive schematic in Cirkit Designer</a></sub>
</p>

> **The reader goes on GPIO43/44.** Not GPIO6/7 — that pair is an I²C bus with
> pull-ups on this board. A PN532 wired there powers up, answers, and returns
> random UIDs with failing reads. It looks like a bad tag or a bad antenna. It is
> neither, and it costs a day. Follow [docs/WIRING.md](docs/WIRING.md) exactly.

## Printer support

**"Any printer" is the goal, not a claim about today.** The current state of
every brand is in **[docs/PRINTER-COMPATIBILITY.md](docs/PRINTER-COMPATIBILITY.md)**,
which grades each on three levels — ✅ automatic, ⚙️ one setup step,
🧪 experimental. Read it before buying parts for a specific printer.

| Brand | Firmware support | Transport |
|---|---|---|
| **Creality** | ✅ implemented, proven on hardware | WebSocket |
| **FlashForge** | ✅ implemented, proven on hardware | HTTP |
| **Bambu Lab** | ✅ implemented, proven on hardware | MQTT over TLS |
| **Snapmaker** | ✅ implemented, proven on hardware | Moonraker over WebSocket |
| **Elegoo** | ✗ not implemented | protocol documented in Tiger Studio |
| **Anycubic** | ✗ not implemented | protocol documented in Tiger Studio |

Slot names match the ones the printer and Tiger Studio use — `Ext.` plus
`1A`–`1D` on Creality and FlashForge, `A1`–`A4` then `B1`–`B4` on Bambu,
`E1`–`E4` on Snapmaker. The table and its two traps are in the compatibility
document.

## Documentation

| | |
|---|---|
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Layering, the state machine, why a backend knows nothing about the screen |
| [ONBOARDING.md](docs/ONBOARDING.md) | The first-boot journey, screen by screen |
| [WIFI-PROVISIONING.md](docs/WIFI-PROVISIONING.md) | The QR code and the captive portal |
| [ACCOUNT-PAIRING.md](docs/ACCOUNT-PAIRING.md) | E-mail and Google sign-in, and why RFC 8628 was rejected |
| [ACCOUNT-DATA.md](docs/ACCOUNT-DATA.md) | The shape a printer arrives in |
| [OTA.md](docs/OTA.md) | Partitions, the manifest, and what is settled before the first release |
| [PRINTER-COMPATIBILITY.md](docs/PRINTER-COMPATIBILITY.md) | Per-brand status and slot naming |
| [WIRING.md](docs/WIRING.md) | The four wires |

Contributors and agents start at **[AGENTS.md](AGENTS.md)** and
**[CODEMAP.md](CODEMAP.md)**.

## Known limitations

Written down rather than discovered.

- **The firmware is not signed.** Its update connection is verified against the
  root certificate store, so the box knows who it is talking to — but not who
  produced the image. The reasoning and the condition for changing that are in
  [docs/OTA.md](docs/OTA.md).
- **No Elegoo or Anycubic backend.** Both protocols are documented and working in
  Tiger Studio; the firmware side is not written.
- **On-screen text carries no accents.** The compiled font is ASCII plus degree
  and bullet, so "Francais" is spelled without its cedilla on purpose. Restoring
  them needs a generated Latin subset font, and Polish needs Latin Extended-A on
  top of that.
- **A printer is identified by its position in your account's list.** Reordering
  it in Tiger Studio can move a per-printer setting to the wrong machine. Tracked
  in [docs/reviews/](docs/reviews/).

## Part of the TigerTag ecosystem

TigerTag is an open NFC identification standard for 3D-printing materials. A
spool carries its own identity, and every device in the system reads the same
one.

- **[Tiger Studio Manager](https://github.com/TigerTag-Project/TigerTag-Studio-Manager)** — desktop printer and filament manager. **Start here**: it is where an account is created and printers are declared.
- **[TigerTag-RFID-Guide](https://github.com/TigerTag-Project/TigerTag-RFID-Guide)** — the protocol specification and public registry
- **[TigerSystem-Docs](https://github.com/TigerTag-Project/TigerSystem-Docs)** — ecosystem source of truth
- **[Tiger-Scale-V3](https://github.com/TigerTag-Project/Tiger-Scale-V3)** — the connected filament scale
- **[TigerPOD](https://github.com/TigerTag-Project/TigerPOD)** — open desktop NFC reader/writer
- **SDKs** — [Python](https://github.com/TigerTag-Project/TigerTag-SDK-Python) · [JavaScript](https://github.com/TigerTag-Project/TigerTag-SDK-JS)

## Contributing

Issues and pull requests are welcome. [CONTRIBUTING.md](CONTRIBUTING.md) has the
conventions; the short version is that `bash scripts/verify.sh` passes before
anything is reported as done, and everything committed is in English.

Security reports: [SECURITY.md](SECURITY.md).

## License

[MIT](LICENSE) — build it, sell it, fork it.

Third-party components keep their own licenses; see
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). "TigerTag" and "TigerSpool"
are project names, not a license to imply endorsement — the terms for using them
on a product you sell are in [TRADEMARK.md](TRADEMARK.md).
