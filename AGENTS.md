# Working in this repository

TigerSpool RFID is ESP32-S3 firmware. It reads a TigerTag NFC chip and writes the
filament it names into a printer's slot. Printers come from the user's TigerTag
account; nothing is typed on the device.

This file is tool-agnostic. Claude sessions additionally read
[CLAUDE.md](CLAUDE.md), which is a superset of this one.

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
| `hardware/`, `models/`, `installer/` | Wiring and BOM, printable cases, web installer. |
| `_internal/` | Maintainer working notes, French, **gitignored**. Never publish. |

[docs/ADOPTING-THIS-STANDARD.md](docs/ADOPTING-THIS-STANDARD.md) records what
this way of working is, which parts of it were dropped when it was adopted here
and why, and which guards this repository needed that no standard predicted.
Read it before proposing a change to how the guards work — and first, if you are
bringing another project up to this line.

`CODEMAP.md` describes what each source file owns and what must not be asked of
it. Read it before editing a file you have not edited before.

## Before you finish

```
bash scripts/verify.sh --quick
```

One command, and it is what CI runs. If it is red, the work is not done. Each
guard names the file, the line and its own remedy. A guard exiting `2` did not
find a violation — it could not run, and is checking nothing.

Once per clone, `bash scripts/install-hooks.sh` points git at the versioned
hooks, so the guards run on commit. They never compile: a build on every commit
teaches people to reach for `--no-verify`.

## Conventions

- **Everything committed is English.** Code, comments, commit messages,
  documentation, log lines, user-visible strings. Conversation may be in any
  language; the repository is not a conversation.
- **Smallest diff that does the job.** No opportunistic cleanup, no drive-by
  reformatting. Match the surrounding naming, idiom and comment density.
- **A comment carries the reason, not the restatement.** `// increment i` is
  noise. A comment that contradicts the code is worse than no comment, because
  the next reader believes it.
- **No AI attribution anywhere** — not in commit messages, not in pull request
  bodies, not in comments, not in contributor lists.
- **Never commit, tag or push unless asked.** A pushed tag is the one action no
  later edit undoes: the release workflow acts on it.
- **Generated files are never hand-edited.** `firmware/include/tigertag_db.h`
  comes from `firmware/tools/gen_db.py`. Change the JSON input, re-run the
  generator, commit its output.

## Things that are settled — do not re-litigate

Each of these was decided once and cost something to decide. Re-proposing one
costs that again.

| Settled | The reason it is settled |
|---|---|
| A screen is never rebuilt to reflect a value. | Rebuilding throws away the scroll position, the focus and any animation in flight. Build once, then write changing values into the widgets that hold them - only a change in what the screen CONTAINS justifies rebuilding it. Cost of ignoring it: a printer toggle threw the list back to the top, and the OTA ring restarted from zero a hundred times per download. |
| **The reader is on GPIO43/44.** | GPIO6/7 is an I²C bus on this board, with pull-ups, shared with the IMU and camera header. A reader wired there enumerates, answers, and returns random UIDs — it looks alive. |
| **Wi-Fi provisioning is in the main firmware.** | The device raises its own access point, answers the captive-portal probes and joins without rebooting. There is no separate provisioning firmware, and no second flash. |
| **Account sign-in is email/password or Google pairing.** | The Google path is a code-and-QR pairing served by the TigerTag Cloud Functions. RFC 8628 device flow was evaluated and rejected: Google publishes no `verification_uri_complete`, so a QR cannot carry the code; polling requires a client secret the device must not hold; and it covers Google only. |
| **The partition table is two 4 MB OTA slots, and it is frozen.** | Changing it after the first public release forces a USB reflash on every device in the field. |
| **Wi-Fi Easy Connect (DPP) is not used.** | Evaluated and rejected. A QR pointing at the device's own access point works from any phone; DPP does not. |
| **The language set is eight, and Chinese is not among them.** | The compiled font carries no CJK glyphs. Offering a language that renders as empty boxes is worse than not offering it. |
| **Accents are not written into on-device strings.** | The compiled font carries ASCII only. See the font row in [CLAUDE.md](CLAUDE.md#hardware-facts-that-bite) before touching this. |
| **The firmware carries no credentials, ever.** | Every secret comes from NVS at runtime. The published binary is byte-identical for every user; a build is not a place to put a password. |
| **The Firebase `apiKey` in `tigertag_cloud.cpp` is not a secret.** | It is public client configuration, served without authentication from the project's own `init.json`. It identifies the project and authorises nothing. Removing it breaks the build and protects no one. |
| **Every screen is LVGL.** | Raw pixel drawing was removed. Do not add any back, and do not draw outside the shared frame in `ui/frame.cpp`. |
