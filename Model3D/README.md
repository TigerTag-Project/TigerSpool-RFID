# 3D-printable cases

**The [desktop stand](0.Desktop/) is the base model.** It stands beside any
printer, and it is the one to print if you are not sure. Every other model is a
**printer integration**: the same TigerSpool in a shell that clips onto one
printer's own screen.

<p align="center">
  <img src="../assets/tigerspool-desk-left-right.png" alt="The desktop stand, spool on the left and spool on the right" width="420">
</p>

| Model | Kind | MakerWorld | Files |
|---|---|---|---|
| Desktop stand, spool on the left | base, any printer | [print it](https://makerworld.com/models/3360492-tigerspool-desktop-stand-spool-on-the-left) | [`0.Desktop/`](0.Desktop/) |
| Desktop stand, spool on the right | base, any printer | [print it](https://makerworld.com/models/3360619-tigerspool-desktop-stand-spool-on-the-right) | [`0.Desktop/`](0.Desktop/) |
| Creality K2, K2 Pro | integration, clips onto the screen | [print it](https://makerworld.com/models/3361291-tigerspool-for-creality-k2-and-k2-pro) | [`2.Creality/K2-K2Pro/`](2.Creality/K2-K2Pro/) |
| FlashForge Creator 5, 5 Pro | integration, clips onto the screen | [print it](https://makerworld.com/models/3361301-tigerspool-rfid-for-flashforge-creator-5-and-5-pro) | [`4.FlashForge/Creator5-5Pro/`](4.FlashForge/Creator5-5Pro/) |
| Bambu Lab A1, A2L | integration, clips onto the screen | not yet | [`1.BambuLab/A1-A2/`](1.BambuLab/A1-A2/) |
| Anycubic Kobra 3, every model | integration, clips onto the screen | not yet | [`6.Anycubic/Kobra3-Series/`](6.Anycubic/Kobra3-Series/) |

Each brand directory lists its models. On MakerWorld every page carries the
full Bambu Studio project and the whole build guide; the bare parts, the other
slicers' projects and the Fusion sources are here.

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
Model3D/
├── TEMPLATE.md                the README every model starts from
├── 0.Desktop/                 free-standing, fits any printer - the universal one
├── 1.BambuLab/                one numbered directory per brand ...
│   ├── README.md              ... listing its models
│   └── X1C/                   ... and one directory per printer model
├── 2.Creality/
├── 3.Snapmaker/
├── 4.FlashForge/
├── 5.Elegoo/
└── 6.Anycubic/
```

Two levels, and only two: **brand, then printer model.** A case that fits a
whole range goes in the brand directory under the name of that range (`P1/`
for the P1P and the P1S); one that fits a single machine takes that machine's
name (`K2-Plus/`). A model that fits every printer is not a brand model at all:
it belongs in `0.Desktop/`, or next to it.

**A directory here is about physical mounting, not firmware support.** A case
can be designed for a printer before its network protocol works, so a directory
existing says nothing about whether the device talks to that printer.
[../docs/PRINTER-COMPATIBILITY.md](../docs/PRINTER-COMPATIBILITY.md) is the
authority on what actually works.

### What goes in a model directory

```
K2-Plus/
├── README.md                  from ../TEMPLATE.md
├── Images/                    what it looks like - a photo beats a render
├── Source/                    the editable design: .step, .f3d
├── OriginalFiles/             the parts as exported, one file each, no slicer settings
├── BambuStudio/               one directory per slicer the project is saved from
│   └── *.3mf
└── CrealityPrint/
    └── *.3mf
```

| What | Why |
|---|---|
| `README.md` | Which printers it fits, what to print and in what, how it goes together |
| `Images/` | Nobody downloads a case they cannot see. The slicer's own previews can be taken out of a 3MF (`Metadata/plate_1.png`, `top_1.png`) until there is a photo |
| `Source/` | So the next person can adapt it instead of starting again. `.step` opens everywhere; a native file (`.f3d`) is welcome beside it |
| `OriginalFiles/` | The bare parts, for any slicer and any printer |
| `<Slicer>/*.3mf` | A 3MF saved from a slicer carries the orientation, the supports and the plate for that slicer's printers, and is tied to it. One directory per slicer: `BambuStudio/`, `CrealityPrint/`, `FlashStudio/`, `OrcaSlicer/`, `PrusaSlicer/` |

Variants of one design (the desktop stand with the spool on the left or on the
right) are told apart by the file name, or by a `Left/` and `Right/` level where
there are several files per variant.

### Names

**Directories** are written to be read: capitals where they help (`BambuStudio`,
`OriginalFiles`, `K2-Plus`), a number in front of the top-level ones to set
their order. **Files** are lower case with hyphens:
`tigerspool-<model>-<variant>-<part>.3mf`, for example
`tigerspool-desk-left-display-cover.3mf`. **No spaces anywhere**: a space breaks
every link to the path in a README. `scripts/check-models.py` enforces all
three.

### Before a photo is committed

A phone photo records the phone, the time and **the GPS position it was taken
at**. Publish a copy with none of it, scaled to something a page can load:

```bash
ffmpeg -i IMG_1234.jpeg -vf "scale='min(2000,iw)':-2" -map_metadata -1 -q:v 3 \
    Model3D/<Brand>/<Model>/Images/tigerspool-<model>-photo-<view>.jpg
```

`-map_metadata -1` drops the metadata; ffmpeg applies the phone's rotation to
the pixels first, so the picture stays the right way up. Look at the
background too: a photo on a desk shows whatever else is on that desk.
`verify.sh` refuses a picture that still carries EXIF.

### Before a 3MF is committed

A slicer project records more than geometry. It keeps the **full path** of the
file each object was imported from - a user name and a folder layout, on a
public repository - and every object's name as the designer typed it.

```bash
python3 scripts/clean-3mf.py Model3D/<Brand>/<Model>/<Slicer>/*.3mf
```

cuts every source path down to its file name and prints every object name, so
you read them before anyone else does. `verify.sh` refuses a 3MF that still
carries a path.

## Contributing a model

1. Read the rule at the top. If your design needs different electronics, it is a
   fork, not a model.
2. **Print it and build one.** Fit, screen angle and read distance are not
   things a render tells you.
3. **Measure the read distance through the shell.** A case that pushes the
   antenna past ~4 cm produces a device that looks fine and reads nothing.
4. Copy [TEMPLATE.md](TEMPLATE.md) to your directory as `README.md` and fill
   it in: print settings, a photo, which printers it fits.
5. Include the editable source (`.step`, and the native file if you have one)
   so the next person can adapt it.
6. Run `scripts/clean-3mf.py` on your projects, then `bash scripts/verify.sh --quick`.
7. Add a row to the brand's README, and open a PR. See [../CONTRIBUTING.md](../CONTRIBUTING.md).

A model that has been printed and used beats a model that has been designed.

---

## Licensing

Model files contributed here are MIT-licensed like the rest of the repository
([../LICENSE](../LICENSE)). Do not contribute geometry derived from a
manufacturer's proprietary CAD, or from a model whose own license does not permit
redistribution.
