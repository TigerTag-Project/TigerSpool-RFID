# Printer compatibility

**"Any printer" is the ambition. This page is the truth.**

Marketing gets one line on the front page; this table gets the rest. If a printer
is not on it, TigerSpool does not support it. If it is on it at 🧪, that means
someone has to do work before it does.

> **Status of this page:** the firmware has not been migrated to this repository
> yet. Everything marked 🟢 *proven* below was proven **in the bench prototype**,
> on real printers, and is being carried over. Nothing here is claimed on the
> basis of documentation alone.

---

## The three levels

| | Level | What it means for you |
|---|---|---|
| ✅ | **Automatic** | Add the printer once in Tiger Studio. TigerSpool finds it, connects, and works. Nothing to configure on the device. |
| ⚙️ | **One setup step** | Works, but the printer needs one thing switched on or read off its own screen — usually LAN mode, sometimes an access code. Once. |
| 🧪 | **Experimental** | Not finished. May be partly implemented, may be unimplemented, may be broken by a printer firmware update. Do not buy hardware for this. |

---

## Support matrix

| Brand | Models | Level | Transport | Prototype status | What's needed |
|---|---|---|---|---|---|
| **Creality** | K2, K2 Plus | ⚙️ | WebSocket `:9999` | 🟢 proven on hardware | **LAN mode must be on** in the printer's network settings, or port 9999 refuses connections. |
| **FlashForge** | Creator 5 / 5 Pro, **AD5X** | ⚙️ | HTTP `:8898` | 🟢 proven on hardware | Serial number **and check code**, both from the printer's network info screen. Imported automatically if the printer is in your TigerTag account. |
| **Bambu Lab** | A1, A1 mini, A2L | ⚙️ | MQTT/TLS `:8883` | 🟢 proven on hardware | **LAN mode on**, plus the serial and the 8-character access code from the printer screen. Imported automatically from your account. |
| **Snapmaker** | Artisan, J1, J1s, U1 | ⚙️ | Moonraker WebSocket `:7125` | 🟢 proven on hardware | Nothing — Moonraker needs no authentication on the LAN. The printer's IP is all it takes. |
| **Bambu Lab (cloud)** | X1, P1, and any printer not on your LAN | 🧪 | MQTT/TLS to Bambu's broker | 🟡 partly implemented | Depends on a Bambu session token that **Tiger Studio** obtains and stores in your account; the device never signs in to Bambu itself. The token expires roughly every three months and has to be renewed in Studio. Not a finished experience. |
| **Elegoo** | Centauri Carbon 2 and others | 🧪 | MQTT `:1883` (plain TCP) | 🟡 backend written, **never run against a printer** | Serial number and the MQTT password (an "Access Code" on the printer). Imported automatically from your account. |
| **Anycubic (cloud)** | any Anycubic not in LAN mode | 🧪 | signed REST + MQTT to Anycubic's cloud | 🔵 protocol documented, firmware not written | Nothing on the printer — but it is a **second, heavier code path** than LAN, and whether it belongs in v1 is undecided. |

> **Anycubic TLS — the supposed blocker does not exist.** Tiger Studio's protocol
> notes say the broker must be pinned to TLS 1.2 because it requests an optional
> client certificate that a TLS 1.3 stack aborts on. Probed directly against a
> **Kobra X (modelId 20030)** in LAN mode, that is not what happens: the broker
> completes **both** TLS 1.3 (`TLS_AES_256_GCM_SHA384`) and TLS 1.2
> (`ECDHE-RSA-AES256-GCM-SHA384`), sends **no** certificate request at all, and
> accepts every mbedTLS-friendly suite tried — including
> `ECDHE-RSA-AES128-GCM-SHA256`, the ESP32's default. Its certificate is
> self-signed, as expected.
>
> This may vary by model or firmware, so it is recorded as an observation rather
> than a rule. But on this hardware the ESP32 will handshake, and the largest
> stated risk for the Anycubic backend is not present.
| **Anycubic** | Kobra 3 V2, Kobra X, ACE units | 🧪 | MQTT/TLS `:9883` | 🟡 backend written, **never run against a printer** | **The printer must be paired in AnycubicSlicerNext at least once** — its broker credentials exist nowhere else. Tiger Studio reads them from there into your account. |

**Legend for implementation status:** 🟢 implemented in the firmware prototype
and verified against a physical printer · 🟡 implemented, not fully verified ·
🔵 protocol fully documented and proven on real hardware **elsewhere**
([Tiger Studio](https://github.com/TigerTag-Project/TigerTag-Studio-Manager)), but
no firmware backend written yet · 🔴 nothing at all.

---

## Elegoo and Anycubic

Both are **targeted for v1**. Neither has a firmware backend yet — but unlike
every other protocol in this project, **neither needs to be reverse-engineered
either.** Both are already documented and working in
[Tiger Studio](https://github.com/TigerTag-Project/TigerTag-Studio-Manager), from
live captures of the vendors' own slicers against real printers:

- `renderer/printers/elegoo/PROTOCOL.md`
- `renderer/printers/anycubic/PROTOCOL.md`

That turns v1 support from open-ended research into a porting job. What remains is
real work — a JavaScript desktop client and an ESP32 firmware backend are not the
same thing — but the unknowns are now specific rather than total.

### What is already known

| | Elegoo | Anycubic |
|---|---|---|
| Transport | MQTT over **plain TCP**, port 1883 | MQTT over **TLS**, port 9883, self-signed |
| Auth | broker username `elegoo` + password, plus the serial number | `deviceId` + username + password |
| Read slots | method `2005` (with Canvas) / `1061` (single extruder) | `getInfo` on the `multiColorBox` topic |
| Write a slot | method `2003` (Canvas) / `1055` (single extruder) | `setInfo` on the same topic |
| Colour | `#RRGGBB`, uppercase | `[r, g, b]` array |
| Materials | a captured 50-entry `filament_code` table keyed by type × name | ~30 accepted names, sent verbatim |
| Discovery | UDP broadcast, port 52700 | `GET http://<ip>:18910/info`, no auth |

Elegoo is in some ways **simpler than Bambu Lab** — no TLS at all.

### The two things that will bite

**Anycubic's broker demands TLS 1.2.** It requests an *optional* client
certificate, and a TLS 1.3 stack aborts the handshake when none is offered. The
version has to be pinned. Whether the ESP32's TLS stack negotiates this correctly
is the single biggest open question for this backend, and it can only be answered
on hardware.

**Anycubic credentials cannot be obtained from the printer.** The `/info`
endpoint does not expose them and they cannot be derived from what it does expose
— that was established exhaustively, not assumed. They exist only in
AnycubicSlicerNext's own config file, which Tiger Studio decodes on the desktop
and writes to your TigerTag account.

For TigerSpool that is not an obstacle, it is the product working as designed:
the desktop does the part that is impossible on a 2.0" screen, and the device
imports the result. But it means **an Anycubic printer must have been paired in
AnycubicSlicerNext at least once**, and the documentation has to say so plainly
rather than letting a user discover it as a failure.

### LAN is written. Cloud is not, and it is not the same job.

An Anycubic printer is reachable **either** over the LAN **or** through Anycubic's
cloud, and the account says which. The LAN backend exists now. A cloud-mode
printer keeps no open local ports at all: there is nothing on the network to
connect to, and the only route is Anycubic's own service — signed REST requests
against a second broker, with signing secrets that live inside the closed slicer
and app binaries.

That is not a variant of the LAN backend with a flag set. It is a second client
against a different service, and the device cannot obtain its own credentials for
it any more than it can for the LAN one. The realistic shape is the Bambu cloud
pattern already in this firmware: **Tiger Studio holds the session and the
account carries a token the device presents.** Nothing in the firmware can be
built until that is decided, because it determines what the account has to store.

The same question already exists for Bambu Lab, whose cloud path is partly
implemented in the prototype.

### Still useful from the community

- **Other models.** The captures come from specific printers. A protocol that
  holds across a brand's range is worth far more than one that fits one machine.
- **Testing** once the backends exist.

## Slot names

A slot name is what the user reads on the box and matches against the machine in
front of them. It has to be the name the printer's own interface uses, and the
same one Tiger Studio shows — a slot called something else here is a slot the
user has to translate in their head.

These are taken from Tiger Studio's renderers, which are the reference:

| Brand | External | Units |
|---|---|---|
| **Bambu Lab** | `Ext.` | `A1`–`A4` for the first AMS, `B1`–`B4` for the second, and so on |
| **Creality** | `Ext.` | `1A`–`1D` for the first CFS box, `2A`–`2D` for the second |
| **FlashForge** | `Ext.` | `1A`–`1D` for the material station. `T1`–`T4` are something else: the Creator 5 Pro's tool-changer nozzles, not station slots. |
| **Elegoo** | `Ext.` | `S1`–`S4` |
| **Snapmaker** | — | `E1`–`E4`, one per extruder |
| **Anycubic** | see below | `A1`–`A4`, `B1`–`B4`, … one row per ACE unit |

**There is one external slot per printer.** Not one per CFS, AMS or ACE unit —
one for the machine. It belongs to the printer, and it is drawn once. That is why
Tiger Studio puts `Ext.` on the first row and an invisible spacer in the same
column on every row after it: the grid stays aligned without inventing an
external spool that does not exist.

Both backends here follow it: the external slot is index 0, written once, before
any unit.

**The letter and the digit swap places between brands.** Bambu is
unit-then-position (`A1`), Creality is box-then-position (`1A`). They are not
typos.

**Reported slots are not loadable colours.** A Kobra X has four inputs in its
head. One feeds a splitter serving four ACE units — sixteen colours — and the
other three take one spool each, so the machine holds **nineteen** colours. Four
ACE units are sixteen slots, not twenty.

### Anycubic, and what is actually known

**The protocol does not describe the wiring, and a backend must not try to.** You
receive the list of ACE units. You do not learn which head input each one hangs
off, and you do not need to: the printer resolves a box and slot to a physical
path itself. Modelling the topology would be inventing a fact the machine never
sends.

What the hardware does, from the bench:

- A **Kobra X** has four inputs on the print head. Each takes one spool directly,
  or an ACE unit. So the same machine can be four direct colours, or sixteen
  through four ACE units, or a mix — and the report looks the same shape either
  way.
- A **Kobra S1** and **S1 Max** have a slot the machine calls **"Holder"**. That
  is the `Ext.` equivalent, one per printer, consistent with the rule above.

**What is not settled is how box `-1` should be labelled.** Tiger Studio's
renderer builds a per-slot prefix of `E` for box `-1`, giving `E1`–`E4` — four
external slots, against the one-per-printer rule. The header comment in that same
file says `[Ext.] [A1] [A2] …` instead. The two disagree inside one file, and the
Kobra X reports four slots on box `-1` while the S1's "Holder" is plainly one.

Decide it against a machine, not against the report. Everything else in this
table is settled.

Slot **names** are decoupled from slot **indices**: renaming is safe, adding a
slot changes the protocol mapping and is not.

## What "supported" does and does not mean

**It means:** TigerSpool can write a material name, a colour and nozzle/bed
temperatures into a numbered slot, and can read back what is currently in each
slot to show it on screen.

**It does not mean:**

- **Loading or unloading filament.** TigerSpool sets metadata. It does not drive
  the extruder. No supported printer exposes load/unload the way it exposes
  slot configuration.
- **Any colour you like.** FlashForge accepts only its own 24-colour palette;
  a tag's colour is snapped to the nearest one. Bambu Lab accepts exact RGB.
- **Any material name you like.** Each printer has a fixed vocabulary. Materials
  the printer does not know are mapped to the closest one it does.

> **What that looks like in practice.** A verified end-to-end assignment on a
> FlashForge AD5X: the tag said **PLA High Speed, R3D, `#DC123F`**; the printer
> ended up showing **PLA, `#F82D29`**. The colour was snapped to the palette's
> nearest entry and the "High Speed" variant was dropped, because those are the
> only values the printer accepts. Nothing failed — but the device must **say**
> the colour was adapted, or a user will read a correct result as a bug.
- **Working during a print.** Changing slot configuration mid-print is not
  supported and may be refused by the printer.

---

## Why a printer that "should work" may not

**Printer firmware updates.** Every protocol here except Snapmaker's is
reverse-engineered from a vendor's private API. Vendors change them without
notice and owe nobody compatibility. A printer that worked last month can stop.

**LAN mode.** Creality, FlashForge and Bambu Lab all gate their local APIs behind
a mode that is off by default. This is the single most common reason a correctly
configured printer shows as offline.

**Network segmentation.** Guest networks, VLANs and client isolation on the
access point will all prevent TigerSpool from reaching a printer that is
otherwise perfectly configured. They must be on the same network segment.

**Silent acknowledgements.** At least one printer answers "success" to commands it
did not understand. Where the protocol allows it, TigerSpool re-reads slot state
after writing to confirm the change actually landed — but confirmation is not
possible on every protocol.

---

## Adding a printer brand

The [architecture](ARCHITECTURE.md#printer-abstraction) is built so that a new
brand is one class implementing one interface, and touches nothing else. What
that class has to provide:

1. **Connect and stay connected** over whatever transport the printer speaks,
   recovering on its own when the network drops.
2. **Report slots** — how many, what they are called, and what is currently in
   each one.
3. **Accept an assignment** — translate a decoded TigerTag into the printer's own
   material vocabulary and colour constraints, and send it.
4. **Tell the truth about failure** — including detecting the case where the
   printer claims success and does nothing.

See [CONTRIBUTING.md](../CONTRIBUTING.md).
