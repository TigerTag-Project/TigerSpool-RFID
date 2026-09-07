# 3D-printable cases

> **Status: empty.** The directory structure and the rules are here. No model
> files exist yet.

---

## The rule that governs every model in this directory

**Only the shell changes. The electronics, the wiring and the connectors are
strictly identical across every model.**

Same Waveshare ESP32-S3-Touch-LCD-2. Same PN532. Same four wires on the same pins.
Same USB-C entry. A TigerSpool built for a Bambu Lab and one built for a Creality
are the same device in a different jacket, and either firmware image runs on
either one.

This is not a convention. It is what makes the project shippable:

- **One firmware.** No per-model build, no variant to pick in the installer, no
  way to flash the wrong image.
- **One bill of materials.** [../hardware/BOM.md](../hardware/BOM.md) is complete
  for every model.
- **One set of wiring instructions.** [../docs/WIRING.md](../docs/WIRING.md)
  is correct for every model.
- **Anyone can add a model** without touching a line of code.

A design that needs a different board, a different reader, extra components, or a
change to the pinout **is not a TigerSpool model**. It is a fork, and it is
welcome as one — under a different name.

---

## What each model has to do

Different only in how it mounts. Every one of them has to:

| Requirement | Why |
|---|---|
| Hold the PN532 antenna **2–4 cm** from where a spool naturally rests | That is the reader's entire range ([../docs/WIRING.md](../docs/WIRING.md)) |
| Keep the antenna **away from the display's metal back** and any ground plane | Both destroy what little range there is |
| Present the 2.0" screen at a readable, tappable angle | It is a touchscreen, used standing up |
| Leave the USB-C port reachable | Power, and recovery flashing |
| Print without supports where possible, and on a printer of the brand it is for | It is a case for that printer; it should print on it |
| Say clearly **where to hold the spool** | Moulded, embossed, or an obvious surface — not in a manual |

---

## Directory layout

```
models/
├── desktop/      free-standing, sits on a bench or desk — the universal one
├── bambulab/     mounts to a Bambu Lab printer or AMS
├── creality/     mounts to a Creality printer or CFS
├── snapmaker/    mounts to a Snapmaker
├── flashforge/   mounts to a FlashForge
├── elegoo/       mounts to an Elegoo
└── anycubic/     mounts to an Anycubic
```

**A directory here is about physical mounting, not firmware support.** `elegoo/`
and `anycubic/` exist because a case can be designed for a printer whose network
protocol is not implemented yet — those two are 🧪 in
[../docs/PRINTER-COMPATIBILITY.md](../docs/PRINTER-COMPATIBILITY.md), which is the
authority on what actually works.

### What goes in a model directory

| File | |
|---|---|
| `README.md` | Which printers it fits, print settings, assembly notes, photo |
| `*.3mf` | Preferred — carries orientation, supports and plate layout |
| `*.stl` | One per part, pre-oriented for printing |
| `*.step` | Editable source, if available. Please include it. |

`desktop/` is the reference. Start there when designing a new one.

---

## Contributing a model

1. Read the rule at the top. If your design needs different electronics, it is a
   fork, not a model.
2. **Print it and build one.** Fit, screen angle and read distance are not
   things a render tells you.
3. **Measure the read distance through the shell.** A case that pushes the
   antenna past ~4 cm produces a device that looks fine and reads nothing.
4. Add a `README.md` with print settings, a photo, and which printers it fits.
5. Include editable source (`.step` or `.3mf`) so the next person can adapt it.
6. Open a PR. See [../CONTRIBUTING.md](../CONTRIBUTING.md).

A model that has been printed and used beats a model that has been designed.

---

## Licensing

Model files contributed here are MIT-licensed like the rest of the repository
([../LICENSE](../LICENSE)). Do not contribute geometry derived from a
manufacturer's proprietary CAD, or from a model whose own license does not permit
redistribution.
