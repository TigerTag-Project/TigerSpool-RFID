# Bill of materials

Everything needed to build one TigerSpool. **The electronics are identical for
every printer brand** — only the printed shell changes ([models/](../models/)).

> **Prices are indicative** and were not re-checked at the time of writing.
>
> Some links here are Amazon affiliate links: buying through them pays the
> project a small commission at no cost to you. Nothing on this list is chosen
> because it is affiliated — the parts came first, and any equivalent module
> works. Where a link is missing, the part is named precisely enough to search
> for.

## Three things to buy

| # | Part | Why this one | ~Price |
|---|---|---|---|
| 1 | **[Waveshare ESP32-S3 2inch Capacitive Touch Display Development Board](https://link.amazon/B0c5hr3uf)** — 240×320 IPS, LX7 dual-core to 240 MHz, Wi-Fi and Bluetooth. (Listed with an OV5640 camera header; the camera is not included and TigerSpool does not use it.) | 2.0" 240×320 IPS with CST816S capacitive touch, ESP32-S3**R8**, 16 MB flash, 8 MB octal PSRAM. The screen, the touch panel and the MCU are one board — there is no separate display to wire. The 16 MB is what makes two OTA slots comfortable. | ~25 € |
| 2 | **PN532 NFC module**, V3 breakout with DIP switches — e.g. [this two-pack](https://link.amazon/B0dyEfwKa) | Reads the NTAG21x chips TigerTag uses. It **must** support **HSU/UART** mode; the DIP-switch V3 boards do, and both switches go to `0` / OFF. A two-pack costs barely more than one, and the spare settles "is it the module or my wiring?" in a minute. | ~9 € the pair |
| 3 | **A USB-A to USB-C cable that carries data** — e.g. [this one](https://link.amazon/B00Xg3WT4) | Powers the board and flashes it. **Speed is irrelevant**: the ESP32-S3's native USB is full-speed, so any USB 2.0 data cable is enough. What matters is that it carries data at all. | ~5-10 € |

Plus a **3D-printed case** from [models/](../models/), about 30 g of filament.

**The four jumper wires come with the PN532** — 3V3, GND, TX and RX, and that is
the whole harness. There is nothing else to order.

**Indicative total: ~40 €** plus filament.

> **The cable is not a detail.** Plenty of USB-C cables carry power and nothing
> else — the ones bundled with phone chargers usually do. With a charge-only
> cable the board lights up, the screen works, and no computer ever sees a serial
> port: the web installer finds nothing to install to and `scripts/flash.sh`
> reports no device. It reads exactly like a dead board. If a cable you know
> transfers files works, use that one.

## What is deliberately not on this list

**No separate display, no microcontroller board, no touch controller.** The
Waveshare board is all three. It costs more than a bare ESP32-S3 and removes the
entire class of display wiring problems, which on a product aimed at
non-technical users is the right trade.

**No level shifters.** The PN532 runs at 3V3 — the same as the ESP32-S3. Do not
power it from 5 V. See [../docs/WIRING.md](../docs/WIRING.md).

**No battery.** TigerSpool sits next to a printer that is already plugged in.

## The tags

TigerSpool reads **TigerTag** NFC chips, which are not included:

- **TigerTag store** — <https://tigertag.io>
- Filament from partner brands ships with them already attached.

The tag protocol is open and documented:
[TigerTag-RFID-Guide](https://github.com/TigerTag-Project/TigerTag-RFID-Guide).

## Board variants — read this before ordering

**Waveshare sells several similar boards.** `ESP32-S3-Touch-LCD-2` is a 2.0"
240×320 panel. The 1.28", 1.69" and 3.5" boards in the same family have different
panel controllers and different pin assignments, and this firmware will not run
on them correctly. Check the silkscreen, not the listing title.

**PN532 clones vary.** The DIP switch mode table is silkscreened on the module
itself and the mapping is not always the same between clones. Read your own
module rather than trusting a table found online — including the one in
[../docs/WIRING.md](../docs/WIRING.md).

## Assembly

1. Wire the PN532 to the board exactly as in [../docs/WIRING.md](../docs/WIRING.md).
   **Four wires, two of them crossed.**
2. Set both DIP switches on the PN532 to `0` / OFF.
3. Flash the firmware from the browser — [../installer/](../installer/).
4. Verify the reader responds *before* closing the case: bring-up checklist in
   [../docs/WIRING.md](../docs/WIRING.md#bringing-a-new-build-up).
5. Mount the reader antenna away from the display's metal back and any ground
   plane. Read range is only 2–4 cm and both will eat it.
6. Close the case.

<!-- TODO(images): assembled build photo — docs/images/ -->
