# Review — shared state and printer identity

**Date:** 2026-09-03
**Scope:** `firmware/src/main.cpp`, `firmware/src/tigertag_cloud.cpp` — the
boundary between the UI loop and the two background tasks, and how a printer is
identified in storage. Plus what the panel actually shows.
**Method:** read-only. Findings 1 and 2 were found while mapping the code, not
while looking for bugs. Finding 3 was found by flashing this branch and looking
at the panel over `/screen.bmp`.

Findings 1 and 2 are **deferred**, with reasons below. Finding 3 was deferred
and then fixed in the same cycle — the reason for deferring it did not survive
contact with the observation that it was guardable. They are recorded here rather
than in `CODEMAP.md` because a landmine row explains a hazard to the next agent
but does not track it — and a documented bug reads as an accepted one.

## Quick wins

None. Neither of these is an hour's work; that is why they are deferred rather
than done.

## Findings

### 1. Shared state crosses a task boundary with no serialisation — deferred

**Where:** `tigertag_cloud.cpp` (`ttSync`, `ttPair`), `main.cpp` (`loadCfg`).

**What breaks.** `ttSync` (16 KB stack) and `ttPair` (12 KB) are the only threads
in an otherwise single-loop firmware. They are pinned to core 1 at priority 1 and
signal completion through bare `volatile bool`. There is no mutex, semaphore or
critical section anywhere in `firmware/src/`.

`ttSync` writes the `tigerspool` NVS namespace. The UI loop reads that same
namespace through `loadCfg()`, which reads six string keys per printer
(`p{i}t`, `p{i}n`, `p{i}h`, `p{i}s`, `p{i}c`, `p{i}v`) in sequence, with no
atomicity across the six.

**Failure scenario.** The user is on the home screen when a scheduled sync lands
— it runs every five minutes, so this needs no unusual timing. `ttSync` rewrites
printer 2 from an account whose entry for that slot has changed. `loadCfg()` is
midway through printer 2: it has already read the old `p2n` and now reads the new
`p2h`. The home screen renders a printer whose name is the old machine and whose
address is the new one. Touching it sends filament to a printer the user did not
select. Nothing logs an error, because from each side nothing went wrong.

`volatile` orders nothing across cores and is not a memory barrier; it only stops
the compiler caching the read.

**Why deferred.** The fix is a design decision, not a patch. Either NVS access is
funnelled through one owner, or the tasks stop writing storage and hand a result
back for the loop to commit. Both change how the account import works, and
choosing between them is not a Phase 3 question.

### 2. A printer's identity is its array index — FIXED 2026-09-10, without a migration

> **What happened.** The failure scenario below happened on the bench: Tiger
> Studio added a second AD5X document, every Bambu after it moved down one
> position, and X1C Home (Lan) took the A1's address and access code - rc=5
> until the device was reflashed - while the AD5X kept an address it had left.
>
> **The fix keeps the positional keys** and changes how the import uses them:
> every imported printer is matched to the stored entry that is the same
> machine (`samePrinter()`: type, serial, then address or mode), wherever it
> sat, and the switch and a discovered host move with it; `printerIdx` follows
> its printer. The account now wins for everything it owns, and for the host
> whenever it gives a new one (`p{i}a` remembers the last). Links reconnect
> when their printer's settings change under them (`cfgSig()`). No storage
> migration: a device on the old layout is matched on its first sync. What
> stays open is finding 1, the unsynchronised NVS access.

**Where:** `main.cpp` — `loadCfg()`, `saveCfg` paths, `savePrinterVisible()`;
`tigertag_cloud.cpp` — the import merge.

**What breaks.** Every stored field is keyed by position: `p0h`, `p1h`, `p2h`.
There is no stable identifier — not the serial, not a UUID. The array slot *is*
the identity. Two consequences compound:

- The merge that preserves locally-corrected values is gated on
  `if (i < n && nt == ct)` — same index **and** same type. When the type at an
  index changes, name, host, serial and access code are all taken from the import
  wholesale.
- `visible` is never written by sync at all, so it stays attached to a slot
  rather than to a printer.

**Failure scenario.** A user with eight printers hides seven and keeps the
FlashForge at index 3. They then delete a Bambu from their TigerTag account in
Tiger Studio. The next sync returns seven printers; everything after the deleted
one shifts up by one. Index 3 is now a Creality. Its type differs, so the merge
overwrites the FlashForge's host and access code with the Creality's, and the
`visible` flag stays on slot 3 — which is now a different machine. The user's
hand-corrected address is gone and the wrong printer is the only one on screen.
No error is reported; the sync did what it was written to do.

**Why deferred (at the time).** Fixing it means introducing a stable key and a migration for
devices that already store the positional form, which is exactly the kind of
storage change that has to be settled deliberately. It is also entangled with
finding 1: both are about who owns the printer list.

### 3. Settings and home are half-translated on screen — FIXED

**Where:** `ui/screen_settings.cpp`, `ui/screen_home.cpp`.

**What breaks.** Some user-visible labels go through `i18n::T()` and others are
English string literals in the source. Counted per screen: settings has 10
translated and 15 hardcoded; home has 2 and 1; the setup flow, by contrast, has
19 and 4.

**Failure scenario.** No timing needed — it is on screen right now. On a device
set to French, the settings menu reads *Reglages / Imprimantes / Wi-Fi / Compte
/ **Screen** / **Language** / **Update** / **Restart** / **Factory reset***, and
the account screen offers **Sign out** under a French heading. The home screen
is titled **Printers**. A user who chose their language during setup is shown a
product that half-forgot.

Verified by looking at the panel over `/screen.bmp` after flashing this branch,
not inferred from the source.

**Fixed**, and guarded. Nineteen keys added across eight languages, and
twenty-five literals replaced. It was first deferred as product work, which was
the wrong call: unlike findings 1 and 2 it is not design work, and it is
*guardable*. `scripts/check-ui-translated.py` now fails when a string literal
carrying a word appears in a UI source outside the three contexts that never
reach the panel. Translating them fixed it once; the guard makes it not come
back.

Note what the guard's first shape would have missed: watching call sites alone
let through five of the fifteen, because the settings menu keeps its labels in a
table and hands the table to `frame::row`.

## Finding 2 has a deadline, and it is a release rather than a date

**Read this before assuming "deferred" means "later is fine".**

Three facts about this product stop being decisions the moment someone else's
device holds them. They are un-fixable after that, not merely expensive:

| Frozen by the first public release | Why it cannot be changed afterwards |
|---|---|
| The **partition table** | A device cannot install a new one over the air. Changing it means every user reflashes over USB, or is stranded. |
| The **OTA manifest key layout** | Units in the field parse the shape they left with. An old one coming back after months must still be able to read what it is served. |
| **Printer identity being an array index** (finding 2 above) | `p{i}t/n/h/s/c/v` is in NVS on a paired device right now. Fixing it later means a storage migration or a reset — and a reset means every user re-pairs. |

The first two are recorded in `docs/OTA.md`. The third is the one that gets
missed, because it reads as ordinary technical debt and is filed here as
deferred design work. It is the same class as the other two: **a data-model fact
that becomes permanent when it leaves the building.**

**They share one deadline, and the deadline is the first public release, not a
sprint boundary.** Nothing forces the date. The release does.

> **Annotation, 2026-09-10.** The deadline passed with finding 2 open - public
> releases shipped from 1.x - and the fix turned out not to need the storage
> change this section feared. The keys stayed positional; what changed is that
> the import recognises each printer by its serial and moves its stored
> settings with it. A device on the old layout is matched on its first sync
> after updating, so nobody re-pairs. See finding 2.

At v0.1.0, with nothing published, all three are still free.

**Finding 1 does not share this deadline.** A missing mutex is a bug that gets
worse with load and can be fixed in any later version without anyone re-pairing
anything. It is more urgent in one sense and less final in another. Do not treat
the two as one pile: shipping makes finding 2 permanent, and only makes finding 1
more common.

## Decisions recorded, so they are not re-litigated

### Mojibake has no guard, deliberately — for now

`check-file-format.py` covers CRLF, byte-order marks, invalid UTF-8, invisible
and bidirectional controls, and missing final newlines. **Mojibake is none of
those.** A double-encoded sequence like `Ã©` is valid, visible UTF-8 and passes
every one of those checks. Nothing in the nine guards covers it.

So the position is *uncovered*, not *covered*, and the earlier claim that the
format guard subsumed it was wrong.

It is still not being written today, on narrower grounds: this repository has
never had the incident, and a guard that can find nothing is ceremony that
teaches people to skim output. But the provenance is exactly the risk profile —
a Portuguese prototype, hand-translated in bulk, with at least one commit made
through a web editor rather than a client index. If a single `Ã` ever appears in
a diff, write the guard that day rather than fixing the one occurrence.

### Restoring accents is one unit of work per font, not per language

On-device strings are written without diacritics — "Francais", "Espanol",
"Changer de reseau". That is the font constraint being respected, not a defect,
and `check-ui-fonts.py` now enforces it.

Anyone tempted to "fix" one accent will ship a blank box. The work is:

1. Generate a Latin-1 subset font. That unlocks French, German, Spanish,
   Italian and both Portuguese variants **at once**.
2. Generate Latin Extended-A on top of it for Polish, which needs `ł ą ę ż ź ć`
   and which Latin-1 does not cover.

So it is two font builds, not eight translation passes, and no accent should be
restored before the font that carries it exists.

### The guards depended on `firmware/.pio` — fixed

`scripts/font_range.py` reads the compiled font's real character range from
LVGL's generated font source, which lives under `firmware/.pio/libdeps`. It
raises rather than guessing when that is absent — correct, because a validator
that assumes a range accepts exactly the characters it exists to reject.

The cost is that `verify.sh --quick`, and therefore the pre-commit hook, cannot
run in a fresh clone until someone runs `pio pkg install`. A hook that does not
work out of the box is a hook that gets uninstalled.

**Done.** `scripts/gen-font-range.py` extracts the range into
`scripts/font_range.json`, registered with `scripts/check-generated.py` so CI
re-derives it from the LVGL source on every push. `check-ui-fonts.py` reads the
committed fact. A fresh clone now runs all nine guards green and names the one
item it could not re-derive. Third application of the same shape, after the
reference header and the device names.

## What was not looked at

- The four printer backends, beyond the interface they share.
- The LVGL screens, except where they read state.
- The captive portal and the legacy web page.
- The reader and the NFC decode path.
- Findings 1 and 2 on hardware. Both are read from the source; neither has been
  reproduced on a device, and the timing of finding 1 in particular has not been
  measured. Finding 3 was seen on the panel.
- The scan and result screens with a real tag. The two brand names whose
  no-break space was removed this cycle cannot be confirmed on screen without a
  Duramic 3D or Filament PM spool in front of the reader.
