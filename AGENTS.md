# Working in this repository

TigerSpool RFID is ESP32-S3 firmware. It reads a TigerTag NFC chip and writes the
filament it names into a printer's slot. Printers come from the user's TigerTag
account; nothing is typed on the device.

This file is tool-agnostic and gives the layout. **The rules are in the index
in [CLAUDE.md](CLAUDE.md#rules)** - one line per rule, each pointing at its file
in `.agents/rules/`. Every agent, whatever its tool, follows that index; nothing
here repeats it.

## Layout

| Path | Holds |
|---|---|
| `firmware/src/` | The firmware. `main.cpp` is the state machine. |
| `firmware/src/ui/` | LVGL screens, the shared frame, the theme, the display port. |
| `firmware/src/net/` | `portal_page.h` — the captive portal, as one PROGMEM string. |
| `firmware/src/backend_*.{h,cpp}` | One printer brand each, behind `printer.h`. |
| `firmware/include/` | Board and build headers. `tigertag_db.h` is **generated**. |
| `firmware/lib/PN532/` | Vendored reader driver. Third party — leave it alone. |
| `firmware/tools/` | `gen_db.py` and its JSON inputs. |
| `scripts/` | Guards and tooling. Everything CI runs, runs here first. |
| `docs/` | Public documentation. Describes the product, not the sources. |
| `hardware/`, `Model3D/`, `installer/` | Wiring and BOM, printable cases, web installer. |
| `_internal/` | Maintainer working notes, French, **gitignored**. Never publish. |

[docs/ADOPTING-THIS-STANDARD.md](docs/ADOPTING-THIS-STANDARD.md) records what
this way of working is, which parts of it were dropped when it was adopted here
and why, and which guards this repository needed that no standard predicted.
Read it before proposing a change to how the guards work — and first, if you are
bringing another project up to this line.

`CODEMAP.md` describes what each source file owns and what must not be asked of
it. Read it before editing a file you have not edited before.

## Before you finish

`bash scripts/verify.sh` - guards plus a build, what CI runs - must pass before
work is reported done; `bash scripts/verify.sh --quick` is the loop while
working. Each guard names the file, the line and its own remedy, and
[CLAUDE.md](CLAUDE.md#when-a-check-goes-red) lists them all.

Once per clone, `bash scripts/install-hooks.sh` points git at the versioned
hooks, so the guards run on commit. They never compile: a build on every commit
teaches people to reach for `--no-verify`.
