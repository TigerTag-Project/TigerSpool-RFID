# Wiring

Six wires. Getting two of them wrong is the single most common way to end up with
a board that boots, shows a screen, and never reads a tag.

Everything on this page was verified on a bench prototype against real hardware.
Where something is *not* verified it says so.

---

## Bill of connections

**Board:** Waveshare ESP32-S3-Touch-LCD-2 — ESP32-S3**R8**, 16 MB flash, 8 MB
octal PSRAM, 2.0" 240×320 IPS with a CST816S capacitive touch panel.

**Reader:** PN532 module (the common red V3 breakout), in **HSU** mode.

### Wiring Diagram

> [!NOTE]
> Four wires. They come with the PN532 module, so there is nothing to buy.

<p align="center">
  <a href="https://app.cirkitdesigner.com/project/7a6c0887-8e44-4303-81b3-be51aab4b40a">
    <img src="../assets/wiring-diagram.jpg" alt="Wiring diagram: PN532 to ESP32-S3-Touch-LCD-2 — VCC to 3V3, GND to GND, TXD to GPIO44, RXD to GPIO43" width="600">
  </a>
</p>
<p align="center">
  <sub><a href="https://app.cirkitdesigner.com/project/7a6c0887-8e44-4303-81b3-be51aab4b40a">Interactive schematic in Cirkit Designer</a></sub>
</p>

| PN532 pin | ESP32-S3 | Direction | Notes |
|---|---|---|---|
| `VCC` | **3V3** | — | **Not 5 V.** |
| `GND` | `GND` | — | Common ground. |
| `TXD` | **GPIO44** | PN532 → ESP32 RX | Crossed. |
| `RXD` | **GPIO43** | ESP32 TX → PN532 | Crossed. |
| `RSTO` | — | — | **Not connected.** The firmware defines no reset pin and never drives one; `config.h` declares the UART and nothing else. Leave it unwired. |
| `IRQ` | — | — | Not used. |

**Module DIP switches: both to `0` / OFF.** That selects HSU on the usual red V3
boards (`0 0` = HSU, `1 0` = I²C, `0 1` = SPI). Clones vary — read the table
silkscreened on your own module.

**Link settings:** UART1, 115200 baud, 8N1.

### The crossed-wires rule

`TXD` on the reader goes to the ESP32's **RX**, and `RXD` goes to the ESP32's
**TX**. Transmit talks to receive. If `getFirmwareVersion()` returns 0 at boot,
swap these two wires before you change anything else — it is the cause far more
often than not.

---

## ⚠️ Do not use GPIO6 and GPIO7

On this board GPIO6/7 are an **I²C bus** (camera header) and carry **pull-up
resistors**. The QMI8658 accelerometer is *not* there — a scan of this board
answers on the touch controller's bus, SDA 48 and SCL 47, at `0x6B`, and finds
nothing at all on GPIO6/7. The warning below is unchanged either way: the
pull-ups are what corrupt a UART, and an empty bus still has them.

Wiring the PN532's UART there produces a link that looks alive and is quietly
corrupt: random UIDs, `READ` commands that fail or return
garbage, tags that read once and never again.

GPIO6/7 appear in some early notes for this project and in the pin table of the
prototype's first-draft README. **That draft predates any hardware testing and is
wrong.** The bench-verified pins are 43 and 44.

### Why 43/44 are free

The ESP32-S3R8 on this board has fewer usable pins than the datasheet count
suggests:

| GPIO | Why it's unavailable |
|---|---|
| 0 | Strapping / BOOT button |
| 19, 20 | Native USB D−/D+ |
| 26–32 | SPI flash |
| 33–37 | Octal PSRAM |
| 45, 46 | Strapping |
| 47, 48 | Touch panel I²C (see below) |
| 6, 7 | On-board I²C with pull-ups — see above |

43 and 44 are the classic `U0TXD`/`U0RXD` pins. They are free here **because the
serial console runs over native USB-CDC**, not over UART0. This is why
`platformio.ini` must keep `-DARDUINO_USB_MODE=1` and
`-DARDUINO_USB_CDC_ON_BOOT=1` — dropping those reclaims 43/44 for the console
and breaks the reader.

---

## On-board peripherals (already wired, for reference)

You do not wire these — they are on the board. They are listed because they
explain which pins are unavailable.

### Display — SPI3

| Signal | GPIO |
|---|---|
| `SCLK` | 39 |
| `MOSI` | 38 |
| `MISO` | 40 |
| `DC` | 42 |
| `CS` | 45 |
| `BL` (backlight) | 1 |

Panel orientation is **rotation 2** — portrait, rotated 180°. Anything else and
the touch coordinates stop matching what is drawn.

### Touch — CST816S on I²C0

| Signal | GPIO |
|---|---|
| `SDA` | 48 |
| `SCL` | 47 |
| Address | `0x15` |

---

## What the reader can and cannot do

These are properties of the hardware, learned the hard way. Design around them
rather than trying to engineer them away.

**Range is 2–4 cm.** That is the PN532's honest range with the module's own
antenna. Keep the antenna away from ground planes and from the display's metal
back — proximity to either kills what little range there is. The enclosure
designs in [models/](../models/) exist partly to hold the antenna at a sane
distance.

**The module powers down between commands.** On these modules only the *first*
command after boot answers; every later one times out on the ACK. The symptom is
distinctive and misleading: the reader reports itself ready, and then reads zero
tags forever. The fix is to send a wake-up preamble before *every* command rather
than only at startup.

**Initialisation needs retries.** The first attempt fails almost every time. Retry
every 2 seconds until the firmware version comes back non-zero — and check it a
second time *after* `SAMConfig()`, because a module that answers once and then
goes quiet is exactly the failure above.

**Read pages `0x04`–`0x0B` only.** 32 bytes covers everything TigerSpool needs
(product id, material, brand, colour, temperatures). Reading them as two 16-byte
transactions instead of eight 4-byte ones is dramatically more reliable on a
marginal HSU link. Keep a 4-byte-per-page path as a fallback: when the link is
poor, smaller frames survive better than large ones.

**Read several times per tag presentation.** A single successful read is not
evidence of a good read. Validate what came back — a written TigerTag has very
few zero bytes in those pages, so a payload full of zeros means a truncated
response, not a blank tag.

**The stock Adafruit PN532 library's UART driver is not adequate here.** It
returned `FF 00 FF 00` on this hardware. The prototype uses the Seeed/elechouse
`PN532_HSU` driver with local patches; see
[docs/MIGRATION.md](MIGRATION.md#the-vendored-pn532-library) for what those
patches are and how they will be carried over.

---

## Bringing a new build up

Check in this order. Each step rules out the failure modes above.

1. **Power.** 3V3 on the PN532's VCC. Not 5 V.
2. **DIP switches.** Both at `0` / OFF.
3. **Wires.** PN532 `TXD` → GPIO44, `RXD` → GPIO43. Crossed.
4. **Idle bytes.** With the reader powered and nothing near it, the ESP32's RX
   line should be silent. Continuous garbage means TX/RX swapped, the DIPs in the
   wrong mode, a 5 V level, or the wrong UART.
5. **Firmware version.** Ask the PN532 for its firmware version. Non-zero once,
   then non-zero *again* after `SAMConfig()`. A single non-zero followed by
   timeouts is the power-down problem.
6. **A tag.** Present a TigerTag; the UID and 32 bytes of page data should come
   back. If the UID changes every read, you are on GPIO6/7.

<!-- TODO(images): wiring photo — docs/images/wiring-hsu.jpg -->

---

## Not yet verified

Listed here rather than left implied. These need bench work before they belong
in the sections above.

- **Enclosure antenna placement.** The 2–4 cm figure is measured with a bare
  module. Read distance through each printed shell in [models/](../models/) has
  not been characterised.
- **PN532 module variants.** Verified on the common red V3 breakout. Other
  form factors (e.g. the flat NFC antenna variants) are untested.
- **A shared reset line.** `RSTO` on GPIO4 helps init; whether it is sufficient
  to recover a wedged reader at runtime, without a power cycle, has not been
  tested.
