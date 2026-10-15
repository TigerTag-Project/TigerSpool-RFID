# Adopting this standard

This repository was brought up to a working standard written from another
project's scars. Most of it transferred. Some of it did not, and a few things it
never mentioned turned out to matter more than half of what it did.

**This document is the part worth reading first.** A standard that is only ever
added to becomes ceremony: rules nobody can trace to a consequence, followed
because they are there. The list below is how it stays a standard — by recording
what was dropped, and why, in enough detail that the next project can tell
whether the same reason applies to it.

The ten principles behind it are in [AGENTS.md](../AGENTS.md) and
[CLAUDE.md](../CLAUDE.md). This is about the instances.

---

## Part 1 — What did not transfer

### The line-number map machinery

**The rule:** map each source file with line numbers, and ship a checker that
fails when they drift.

**Why not here:** the reference project's firmware is one file of about 12 500
lines, where "which of the four `handleTouch` blocks" is a real question and a
line number is the only cheap answer. This firmware is ~8 300 lines across 47
files, and `CODEMAP.md` plus a grep gets you there in one step.

So no line numbers were recorded at all — and therefore no checker was needed.
That is the cleaner reading of the rule than shipping numbers and a checker for
them: **a number nothing verifies is a number that lies**, and the cheapest way
to satisfy that is to record none.

**When it would transfer:** the moment one file grows past roughly two thousand
lines and stops having an obvious internal structure.

### A separate mojibake guard

**The rule:** check for double-encoded text; the reference firmware once carried
218 such sequences, and the real damage was second-order — a generator had been
*changed to match the damaged bytes*, so the corruption looked normal and spread.

**Why not here:** the incident has never happened in this repository. A guard
that can find nothing is ceremony, and worse, it trains people to skim guard
output — which is how the guard that *would* have caught something gets skimmed
too.

**Read the reasoning that was nearly used instead**, because it was wrong: the
first argument was that `check-file-format.py` already covered it. It does not.
That guard covers CRLF, byte-order marks, invalid UTF-8, invisible and
bidirectional controls. A double-encoded sequence like `Ã©` is valid, visible
UTF-8 and passes every one of those. The position is *uncovered*, not *covered*,
and the decision to leave it uncovered has to be made on that basis.

**When it would transfer:** the day a single stray `Ã` appears in a diff. The
note in [the review](reviews/2026-09-03-concurrency-and-identity.md) says to
write the guard that day rather than fixing the one occurrence.

### The path-to-consequence table, as a table of paths

**The rule:** a table saying which paths trigger which CI work, so it is obvious
before pushing what a change will set off.

**Why not literally:** the table exists in `CLAUDE.md`, but every row for a code
or documentation change says the same thing — guards *and* the firmware build,
on every push, on every branch. There is no path filtering.

That was a decision, not an omission. The firmware job costs two minutes.
Skipping it on a documentation change means maintaining a list of paths that
decides whether the build runs, and **that list is a second source of truth
about what a change affects.** It goes wrong quietly, in the direction of not
building something that needed building.

**When it would transfer:** when the build costs enough that two minutes becomes
twenty.

### A build matrix

**The rule, implied:** the reference project builds several environments, and CI
covers them.

**Why not here:** there is one board and one environment that ships,
`tigerspool`. `tigerspool-bench` only adds bench diagnostics and is never
released; building it in CI would test code no user runs. A matrix of one is a
matrix-shaped way of writing a single build.

### `docs/release-notes/vX.Y.Z.md`

**The rule:** either keep release notes in the CHANGELOG or move them to
per-version files.

**Why the first:** `release.yml` already extracts the section for the tagged
version out of `CHANGELOG.md`. Adding per-version files would mean two places a
release note can live, and therefore one that goes stale. The offered choice was
real; taking it would have violated the principle underneath it.

### "Check that every translation file carries the same key set"

**The rule:** if a second translation set appears, check all its files share one
key set.

**Why it needed rewriting:** a second set does exist — the legacy configuration
page's table in `webcfg.cpp`. But it has a *different axis*: ten columns where
the device speaks eleven languages. Comparing their key sets would produce a permanent,
meaningless failure.

`check-i18n.py` therefore checks each table against **its own** enum, on its own
terms. The transferable part of the rule was "a positional table needs its order
checked", not "all tables must match each other".

### Reading enabled font faces from `lv_conf.h`

**The rule:** read the enabled faces from `lv_conf.h` and the ranges from the
generated font headers.

**Why only half was needed:** all 23 generated Montserrat sizes carry the same
`-r` range, so the union across them is the same as the range of any one, and
which faces `lv_conf.h` enables cannot change the answer. `gen-font-range.py`
verifies that assumption rather than relying on it — it fails loudly if the sizes
ever disagree, because a string drawable at 14 px and a blank box at 20 would be
worse than either.

---

## Part 2 — What the standard did not anticipate

Five of the eleven guards are not in the standard at all. They exist because this
repository drifted in ways the reference project had not, and they are the
strongest argument that a guard suite has to be grown from a specific tree rather
than copied.

| Guard | The drift it was written for |
|---|---|
| `check-text-english.py` | A commit titled "Every comment in English" that left 17 Portuguese and French comments behind. It found 66, because it reads string literals too — including two that reached users. |
| `check-device-names.py` | Six documents describing a device name the firmware had stopped building, one of them offering a Wi-Fi QR payload the device does not emit. |
| `check-wiring.py` | Not new in intent, but rebuilt in shape. See below. |
| `check-ui-translated.py` | A settings menu that read half in French and half in English on a paired device. Found by looking at the panel, not by reading the source. |
| `check-release-notes.py` | `WORKLOG.md` had no consumer, and a file nothing consumes stops being maintained. |

### The one guard the standard named, and got the shape of wrong

The standard said: keep the existing GPIO6/7 check and move it into `scripts/`.

The existing check searched prose for a pin number near a signal name on the same
line. **It cannot tell a wiring instruction from a sentence warning against one**
— it failed on a document explaining what it checks, and its author wrote around
it, which is exactly how a guard becomes ceremony.

Moving it would have preserved the fault. It was rebuilt to ask a different
question: read `PN532_UART_RX` and `PN532_UART_TX` from `config.h`, and check
every markdown *wiring table* against them. Prose is where you warn about
GPIO6/7, and warning there is correct. Tables and pin definitions are where you
prescribe, and that is all it looks at.

**The generalisable form**, which turned out to be the most reusable idea in the
whole exercise: *the code owns the fact, and the guard proves the prose still
agrees with it.* Four guards have that shape — the reference header against its
generator, the documentation against the device-name formats, the wiring tables
against the pin macros, and the font range against LVGL's own font source. Reach
for it whenever a document restates something the source computes.

### Two smaller surprises

**A guard suite that never embarrasses its author has not been tested.** Three
guards were caught being wrong by their own failure tests: the i18n checker read
one enum member per line and reported nine violations that did not exist; the
font checker skipped the single most important file in its scan and printed a
confident zero; the English checker read the `//` in `https://` as a comment and
reported a Google API domain as Portuguese. All three were the same mistake —
reading C++ with a regular expression per line — and produced `cxx_scan.py`.

**"An empty input set is a hard error" is about files scanned, not violations
found.** Easy to over-apply into a guard that fails when it finds nothing, which
is the opposite of what is wanted. Every guard here prints both counts
separately — `scanned 125 files, 0 violations` — so the difference is visible in
a log rather than inferred.

---

## Part 3 — What transferred unchanged, and earned it

Recorded because a list of exceptions with no baseline reads as though the
standard mostly failed. It mostly worked.

- **"Things that are settled — do not re-litigate."** The highest-value table in
  `AGENTS.md`, and the cheapest to write.
- **One command that runs everything CI runs.** With one honest exception, named
  in `CLAUDE.md`: CI re-derives the font range, a bench without `.pio` cannot.
- **Every rule carries the incident that earned it.** Every guard's header names
  what it exists to prevent. It is the difference between a rule someone deletes
  and a rule someone understands.
- **Reports are permanent and annotated.** Already paid for itself once: a
  finding filed as deferred was re-read, found to be guardable, and fixed in the
  same cycle.
- **One commit per property, green at each.** Fourteen commits, each carrying a
  guard and the fix for what it found, each verified green on its own in a
  detached worktree.
- **Verify the published result, not the workflow's green check.** Not yet
  exercised — there is nothing published — but it is the reason
  `verify-published-site.py` is specified in [OTA.md](OTA.md) before the
  installer exists.

## What is still unproven here

- **The two agent definitions.** `locator` and `single-edit` are written and
  committed, and were not used across seven phases. Their contracts are
  reasoned, not tested.
- **Everything in Phase 5 beyond the decisions.** No installer, no manifest, no
  published site.
- **`flash.sh --fs`.** The LittleFS partition exists and nothing puts anything in
  it.
- **The pre-commit hook's regeneration check.** It fingerprints the generated
  files before and after, but no guard currently regenerates anything — the
  generator check restores the file before judging. It is a net under a fall that
  cannot yet happen.
