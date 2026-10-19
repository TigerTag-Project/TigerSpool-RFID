# Contributing

Thanks for being here. The firmware is released and updates itself over the
air; six printer brands are supported. Code is welcome, and so is what code
cannot produce: tests on real printers, cases, and translations.

Talk to us on **[Discord](https://discord.gg/3Qv5TSqnJH)**.

---

## What would help most, in order

### 1. Test on a printer we support, especially one we have not seen

A report saying "Creality K2, firmware 1.1.3.13, works" or "stopped working
after the September update" is worth a lot: every protocol here except
Snapmaker's is reverse-engineered, vendors change them without notice, and each
backend was proven on specific machines only.
[docs/PRINTER-COMPATIBILITY.md](docs/PRINTER-COMPATIBILITY.md) says which.

### 2. Read the architecture and argue with it

[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) describes how the firmware is
divided and why. Open an issue if something does not hold up.

### 3. Design a case

[Model3D/](Model3D/) has the rules, the layout and a README template. The electronics are
identical for every model — only the shell changes. A case that has been printed
and used beats one that has been designed.

---

## Firmware contributions

Open an issue before a large change, so nobody builds the same thing twice.
Read [AGENTS.md](AGENTS.md) and [CODEMAP.md](CODEMAP.md) first: they say where
each thing lives and which constants only look arbitrary. Run
`bash scripts/verify.sh` before opening the pull request - it is what CI runs.

**Rules that are not negotiable:**

- **No credentials in the repository.** Not Wi-Fi, not access codes, not API keys
  with authority. Everything comes from NVS at runtime. See [SECURITY.md](SECURITY.md).
- **English everywhere** — comments, identifiers, log messages, commits.
  User-facing text goes through i18n.
- **No text entry on the device.** There is no keyboard and there will not be one.
  A feature that needs typing needs a phone. See [docs/ONBOARDING.md](docs/ONBOARDING.md).
- **A hardware constant gets a comment saying what breaks without it.**
- **One copy of a vendored library**, with its patches documented.

**A new printer brand** is one class implementing `PrinterBackend` plus a
registration line. It touches nothing else. If your change needs to modify the UI
or the state machine, say so in the PR — it may mean the interface is wrong,
which is worth knowing.

---

## Pull requests

1. Fork, branch from `main`.
2. One concern per PR. A rename and a behaviour change in one diff cannot be
   reviewed properly.
3. Say **what you tested it on**. For firmware, that means a real board and,
   for a backend, a real printer with its model and firmware version.
4. Keep it building. CI runs on every PR.

Commits: a short imperative subject line, and a body that says *why* when the
reason is not obvious from the diff.

---

## Issues

**Bugs** — the templates ask for board, firmware version, printer model and
printer firmware version. All four matter: most failures are one of a bad PN532
link, a vendor protocol change, or LAN mode being off.

**Before opening a reader bug**, walk the bring-up checklist in
[docs/WIRING.md](docs/WIRING.md#bringing-a-new-build-up). It resolves most of them,
and it tells us something either way.

**Printer support requests** — say which brand, which model, and whether you can
capture traffic. Without a capture or existing protocol notes, a request is a
wish rather than something anyone can act on.

---

## Translations

The firmware speaks eleven languages - the nine of
[TigerScale V3](https://github.com/TigerTag-Project/Tiger-Scale-V3), plus Russian
and Dutch, which Tiger Studio also speaks. Every string lives in a table, one
row per string and one column per language: `firmware/src/i18n.cpp` for the
screen, the `WT` table in `firmware/src/webcfg.cpp` for the sign-in pages,
`firmware/src/net/portal_page.h` for the Wi-Fi page and `installer/i18n.js` for
the web installer. Correcting a translation is a genuinely useful first
contribution.

Two constraints: the device screen is 240 px wide, so a string that is twice as
long in your language will be cut off; and the fonts are generated from the
character set actually used, so a new script means regenerating them.

---

## Licensing

Contributions are MIT-licensed like the rest of the repository
([LICENSE](LICENSE)). Do not contribute code, geometry or documentation you do
not have the right to relicense — including anything decompiled from a vendor's
software or derived from proprietary CAD.

The **TigerTag** and **TigerSpool** names have conditions:
[TRADEMARK.md](TRADEMARK.md). Forks are welcome under a different name.

---

## Code of conduct

[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Be decent to each other.
