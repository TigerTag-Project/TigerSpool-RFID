# The connection budget

How many printers one TigerSpool can hold open at once, how that is decided,
and every measurement it rests on. Written to be come back to: when a new brand
is added, when a number looks wrong, or when the budget is tuned per use case.

The code is [`firmware/src/printer_budget.h`](../firmware/src/printer_budget.h).
If this document and that file ever disagree, the file is what runs - fix
whichever is wrong, in the same change.

---

## The idea: load slots, like seats on a bus

The limit was never a **number** of printers. It is **internal RAM**, and how
much of it a printer's connection takes depends almost entirely on one thing:
whether it speaks TLS. A Bambu or an Anycubic costs 40-48 KB; everything else
costs 2-6 KB. Ten Elegoos fit where three Bambus do not, so a limit written as
"N printers" would be wrong for nearly every account.

So every printer takes the **load slots** its connection actually costs, and
the device has a fixed number of them. **One load slot is one kilobyte of
internal RAM.**

> **"Load slot", never plain "slot".** In this codebase a *slot* is a place a
> spool sits - an AMS tray, a station bay (`SlotState`, `slotCount`). The two
> must not be confused in code, in logs or in conversation.

A printer is refused at the moment somebody **switches it on**, when they can
see why - not later, as a dot that stays red with no explanation.

---

## The numbers

### The budget: 160 load slots

| | |
|---|---|
| Internal RAM free at boot, before any printer | **198 - 203 KB** (varies between boots) |
| Survival floor - below this the device closes a link to stay alive | **32 KB** |
| Usable for printers | **~166 KB** |
| **Budget** | **160 load slots** |
| Reserve kept for what the model does not count | **~6 KB** |

The reserve covers what the budget does not model: a web page being served, a
JSON document being parsed, a screen being built.

**Why 160 and not 200.** 200 is roughly what is free before anything runs, but
the device cannot spend all of it: 32 KB is the survival floor below which
`main.cpp` starts closing background links, and a few more are needed for the
device's own work. That leaves ~166.

**Why 160 and not 166.** It was 150 at first, keeping a 16 KB reserve. It was
raised to 160 deliberately, trading ten kilobytes of that reserve for room for
one more printer - a LAN Bambu beside a full bench, or several light ones.

**Why the thin reserve is safe.** The budget is not what stops a crash. The
32 KB floor and the *sustained* low-memory closer in `main.cpp` are: if free
memory stays under the floor for three seconds, the last background link is
closed. At worst a peak costs a printer its connection for a moment. The budget
exists to make the choice visible and predictable, not to guard the heap.

### Load slots per printer

Each is the **worst cost measured** for that connection, rounded up to the next
kilobyte. The margin is not spread across the brands - it lives once, in the
reserve above.

| Printer | Measured (worst of 5 passes) | Load slots | What the cost is |
|---|---|---|---|
| **First Bambu Lab in cloud mode** | 48.5 KB | **50** | opens the TLS session every cloud Bambu on the account shares |
| **Each further Bambu Lab in cloud mode** | 3.6 KB | **4** | only subscribes to the shared session |
| **Bambu Lab in LAN mode** | *not measured* | **50** | its own broker, its own TLS session |
| **Anycubic** | 40.4 KB | **41** | TLS |
| **Creality** | 5.6 KB | **6** | WebSocket |
| **Snapmaker** | 4.9 KB | **5** | WebSocket |
| **Elegoo** | 2.5 KB | **3** | plain MQTT, no TLS |
| **FlashForge** | 2.0 KB | **2** | HTTP; nothing held between requests |

**The one assumption left: Bambu in LAN mode.** Both LAN Bambus on the bench
were switched off at every measurement. It is charged 50 because it opens
exactly what a cloud printer opens - the same TLS client against a broker - but
that is reasoning, not a reading. The first bench with one switched on should
replace it.

### Per connection, not per printer

Only the cloud Bambu depends on what else is open. Every Bambu in cloud mode on
one account talks to the same regional broker with the same credentials, so
they share **one** TLS session ([`bambu_cloud.cpp`](../firmware/src/bambu_cloud.cpp)):
the first pays for it, each further one is a subscription on it.

| Bench example | Load slots |
|---|---|
| Three cloud Bambus | 50 + 4 + 4 = **58** |
| The same three before the session was shared | ~141 KB measured |

Every other printer's cost is its own and does not depend on the others.

### Worked example: the bench account

| Printer | Load slots |
|---|---|
| Creality Ender-3 | 6 |
| Bambu A1 (first cloud) | 50 |
| Bambu X1C (further cloud) | 4 |
| Snapmaker U1 | 5 |
| Elegoo Centauri Carbon 2 | 3 |
| Anycubic Kobra X | 41 |
| **Total** | **109 of 160 - shown as 68%** |

Measured the same evening: those six printers took **103.2 KB**. The model is
6 KB on the cautious side for this set.

---

## What the user sees

A bar in **Settings > Printers** and on the first-boot **Choose printers**
screen, above the list, labelled **Load** with a **percentage** - "68%", not
"109 / 160". Load slots are how the device counts; a person needs to know how
full it is, and a percentage says that without knowing what the 160 is.

- The accent colour while there is room, **orange from 85%**.
- A switch that would overflow the budget **does not move**, the bar turns
  **red** and the label reads **"Not enough room"** for two and a half seconds.
- Switching a printer **off** is always allowed.

The raw counts are in the serial log, in the form
`[budget] '<printer>' refused: <total it would reach> of 160 load slots`.

---

## What the model does not see

**Fragmentation.** A new TLS session needs one contiguous piece of about 40 KB,
and after three TLS sessions the largest free block has been measured as low as
18 KB - while the total said there was plenty. The budget adds totals and does
not see that shape.

Today the two agree, by arithmetic rather than by design: 160 load slots caps
TLS sessions at three, which is also the ceiling observed. The cheapest four -
one cloud Bambu and three Anycubics - come to 173, over the budget before a
single light printer.

A **one-off** TLS request - an account sync, an update check - is kept possible
by a separate mechanism, the TLS stand-down in `main.cpp`: background links step
aside for the second or two the request needs and come straight back. The
budget does not provide for those.

**The link limit.** Independently of memory, the code holds at most ten
links (`MAX_LINKS` in `main.cpp`). A printer that is switched on but powered off
still holds one while it retries. With the shared cloud session, the bench
account reached **ten links with 133 KB still free** - for this account the
binding limit is now the link count, not the RAM.

---

## How the numbers were measured

`/api/memtest` on the device's web server. It closes every link, holds the
account sync off, and opens printers **one at a time**, reading internal RAM
and PSRAM after each one has been up for ten settled seconds. The report goes
to the serial console.

    http://<device>/api/memtest          the printers switched on
    http://<device>/api/memtest?all=1    every printer on the account

Why it had to be built: numbers taken in normal running were not facts about
any printer. Links opened in groups and an account sync allocated a TLS session
in the middle, so the per-link figures added up to 193 KB against 135 KB
measured on the whole.

**Why the MQTT buffers are not in the table.** This framework is compiled with
`CONFIG_SPIRAM_USE_MALLOC` and a 4 KB internal threshold, so every allocation
over 4 KB - the 50 KB Bambu receive buffer included - lands in PSRAM, of which
8 MB is idle. What stays in internal RAM is mbedTLS, pinned there by
`CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC`. That is why a printer's cost is, to within
a few kilobytes, whether it speaks TLS.

Five passes, 2026-09-10, on one Waveshare ESP32-S3-Touch-LCD-2:

| Pass | Setup | Notes |
|---|---|---|
| 1 | per-printer sessions, active printers | the first clean numbers |
| 2 | per-printer sessions, whole account | three TLS sessions fit, a fourth did not |
| 3 | shared cloud session, active printers | second cloud Bambu 3.2 KB |
| 4 | shared cloud session, active printers | repeat, evening |
| 5 | shared cloud session, whole account | three cloud Bambus on one session; stopped on link slots, not RAM |

---

## When to revisit

- **A new brand**: measure it with `/api/memtest` and add its worst case to the
  table and to `loadSlotsFor()`. Do not estimate.
- **A LAN Bambu is switched on at the bench**: measure it and replace the
  assumed 50.
- **The framework is updated**: re-run all five passes. A change to the
  PSRAM malloc threshold or to where mbedTLS allocates changes every number.
- **Tuning per use case** - planned: the numbers here are the floor any tuning
  starts from.
- **More than three TLS printers are wanted**: see
  [ROADMAP.md](ROADMAP.md#more-tls-printers-at-once). The two ways past it are
  mbedTLS in PSRAM, or sharing more sessions.
