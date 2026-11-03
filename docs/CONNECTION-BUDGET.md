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
whether it speaks TLS - or did, while TLS lived in internal RAM: a Bambu or an
Anycubic cost 40-50 KB and everything else 2-6 KB. Since mbedTLS allocates in
PSRAM (`tls::usePsram()`, 2026-10-05) a TLS printer costs 5-9 KB, and the
binding limit for most accounts is the link count rather than the RAM. The
budget stays per printer all the same: costs still differ, and a limit written
as "N printers" says nothing about why one more does not fit.

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

### The budget: 120 load slots

| | |
|---|---|
| Internal RAM free at boot, before any printer | **183 KB** (TLS in PSRAM, 2026-10-05) |
| Survival floor - below this the device closes a link to stay alive | **32 KB** |
| Usable for printers | **~151 KB** |
| **Budget** | **120 load slots** |
| Reserve kept for what the model does not count | **~30 KB** |

The reserve covers what the budget does not model: a web page being served, a
JSON document being parsed, a screen being built, and a one-off TLS request's
16 KB task stack - its buffers are in PSRAM, its stack is not.

**Why the reserve is wide.** The figures below come from one pass of
`/api/memtest`, not five. 120 is still twice what ten typical printers take, so
a wide reserve costs nothing anybody would notice; narrow it once the passes
have been repeated.

**The budget does not stop a crash.** The 32 KB floor and the *sustained*
low-memory closer in `main.cpp` do: if free memory stays under the floor for
three seconds, the last background link is closed. The budget exists to make
the choice visible and predictable, not to guard the heap.

### Load slots per printer

Each is the **worst cost measured** for that connection, rounded up to the next
kilobyte. Where an earlier measurement with TLS in internal RAM was higher for
a printer that does not speak TLS, the earlier one is kept.

| Printer | Measured, TLS in PSRAM | Load slots | Before (TLS internal) |
|---|---|---|---|
| **First Bambu Lab in cloud mode** | 8.3 KB | **9** | 50 |
| **Each further Bambu Lab in cloud mode** | 5.2 KB | **6** | 4 |
| **Bambu Lab in LAN mode** | 5.3 KB | **6** | 50, never measured |
| **First Anycubic in cloud mode** | 5.3 KB | **6** | 50 |
| **Anycubic in LAN mode** | *not measured* | **6** | 41 |
| **Each further Anycubic in cloud mode** | *not measured* ([#19](https://github.com/TigerTag-Project/TigerSpool-RFID/issues/19)) | **6** | 4 |
| **Creality** | 4.5 KB | **6** | 6 (5.6 KB) |
| **Snapmaker** | 4.7 KB | **5** | 5 |
| **Klipper** | *not measured* - the same WebSocket client and Moonraker as Snapmaker | **5** | - |
| **Elegoo** | 0.7 KB | **3** | 3 (2.5 KB) |
| **FlashForge** | 0.1 KB | **2** | 2 (2.0 KB) |

**Two assumptions left: an Anycubic in LAN mode, and a further cloud
Anycubic.** Neither has been on the bench. Both are charged as the TLS printers
measured beside them; the first bench with one switched on should replace them.

### Per connection, not per printer

Only the cloud printers depend on what else is open - Bambu and Anycubic
alike, each with a session of its own brand. Every Bambu in cloud mode on one
account talks to the same regional broker with the same credentials, so they
share **one** TLS session ([`bambu_cloud.cpp`](../firmware/src/bambu_cloud.cpp)),
and so do cloud Anycubics ([`anycubic_cloud.cpp`](../firmware/src/anycubic_cloud.cpp)):
the first pays for it, each further one is a subscription on it.

### Worked example: the bench account

| Printer | Load slots |
|---|---|
| Creality Ender-3 V4 | 6 |
| FlashForge AD5X | 2 |
| Bambu X1C (LAN) | 6 |
| Bambu P1P (first cloud) | 9 |
| Bambu A1 (further cloud) | 6 |
| Snapmaker U1 | 5 |
| Elegoo Centauri Carbon 2 | 3 |
| Anycubic Kobra X (first cloud) | 6 |
| **Total** | **43 of 120 - shown as 36%** |

Measured the same night: those eight printers took **34 KB**, all eight up at
once, 149 KB still free.

---

## What the user sees

A bar in **Settings > Printers** and on the first-boot **Choose printers**
screen, above the list, labelled **Load** with a **percentage** - "68%", not
"43 / 120". Load slots are how the device counts; a person needs to know how
full it is, and a percentage says that without knowing what the 120 is.

- The accent colour while there is room, **orange from 85%**.
- A switch that would overflow the budget **does not move**, the bar turns
  **red** and the label reads **"Not enough room"** for two and a half seconds.
- Switching a printer **off** is always allowed.

The raw counts are in the serial log, in the form
`[budget] '<printer>' refused: <total it would reach> of 120 load slots`.

---

## What the model does not see

**Fragmentation**, which used to be the real ceiling. A new TLS session needed
one contiguous piece of about 40 KB of internal RAM, and after three sessions
the largest free block fell as low as 18 KB - so the fourth failed while the
total said there was room, and so did the presence beat beside them. With the
buffers in PSRAM the largest internal block stays above 130 KB with eight
printers up, and a new session needs a few kilobytes of it.

A **one-off** TLS request - an account sync, an update check - still has the
TLS stand-down in `main.cpp` behind it: background links step aside for the
second or two the request needs. With the buffers in PSRAM it is rarely
triggered; the presence beat does not use it and no longer needs to.

**The link limit.** Independently of memory, the code holds at most ten
links (`MAX_LINKS` in `main.cpp`). A printer that is switched on but powered off
still holds one while it retries. With the shared cloud session, the bench
account reached **ten links with 133 KB still free**, before TLS moved to
PSRAM. It is the binding limit for nearly every account now: ten typical
printers are about 60 load slots of 120.

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
8 MB is idle. What stayed in internal RAM was mbedTLS, pinned there by
`CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC` - until `tls::usePsram()` redirected its
allocator at boot, which the core allows at run time
(`mbedtls_platform_set_calloc_free`) without rebuilding it.

Six passes on one Waveshare ESP32-S3-Touch-LCD-2 - the first five on 2026-09-10
with TLS in internal RAM, the sixth on 2026-10-05 with it in PSRAM:

| Pass | Setup | Notes |
|---|---|---|
| 1 | per-printer sessions, active printers | the first clean numbers |
| 2 | per-printer sessions, whole account | three TLS sessions fit, a fourth did not |
| 3 | shared cloud session, active printers | second cloud Bambu 3.2 KB |
| 4 | shared cloud session, active printers | repeat, evening |
| 5 | shared cloud session, whole account | three cloud Bambus on one session; stopped on link slots, not RAM |
| 6 | TLS in PSRAM, whole account | all eight up, 34 KB taken, 149 KB free; the figures in the table above |

---

## When to revisit

- **A new brand**: measure it with `/api/memtest` and add its worst case to the
  table and to `loadSlotsFor()`. Do not estimate.
- **A LAN Bambu is switched on at the bench**: measure it and replace the
  assumed 50.
- **The framework is updated**: re-run the passes. A change to the PSRAM
  malloc threshold or to where mbedTLS allocates changes every number - moving
  mbedTLS to PSRAM took a TLS printer from 50 load slots to 6-9.
- **Tuning per use case** - planned: the numbers here are the floor any tuning
  starts from.
- **More than ten printers are wanted**: `MAX_LINKS` in `main.cpp`. Memory
  allows it now; what to measure first is the main loop, which pumps every
  link on every pass.
