# Architecture

What the firmware is made of, and why it is divided the way it is.

[CODEMAP.md](../CODEMAP.md) maps it file by file.
[MIGRATION.md](MIGRATION.md) records how it was carried over from the prototype.

---

## The one idea

A printer backend knows one thing: **how to put a material, a colour and two
temperatures into slot _n_ of one specific printer.** It knows nothing about NFC,
nothing about the screen, and nothing about Wi-Fi.

Everything above it — the UI, the reader, the account — is written once and works
for every printer. Everything below it is per-brand and stays contained. Adding
Elegoo means writing one class, not touching the application.

---

## Layers

```
┌───────────────────────────────────────────────────────────────┐
│  ui/            screens, touch handling, rendering            │
│                 knows: state machine, layout                  │
│                 knows nothing about: NFC, MQTT, HTTP, Firebase│
├───────────────────────────────────────────────────────────────┤
│  app/           state machine, device orchestration           │
│                 owns the transitions and the selected printer │
├───────────────┬──────────────┬────────────────┬───────────────┤
│  reader/      │  printers/   │  account/      │  net/         │
│  PN532 HSU    │  backends    │  TigerTag      │  Wi-Fi, portal│
│  TigerTag     │  + registry  │  session,      │  mDNS, OTA,   │
│  decode       │              │  printers in,  │  web config   │
│               │              │  slots out     │               │
├───────────────┴──────────────┴────────────────┴───────────────┤
│  platform/      NVS, display driver, touch, time, logging     │
└───────────────────────────────────────────────────────────────┘
```

Each layer may call downwards. Nothing calls upwards; a backend that wants to
tell the user something returns a status, it does not draw.

### Why `net/` is one layer and not per-feature

Wi-Fi provisioning, the configuration web server, mDNS and OTA all share the same
scarce resource — the radio and one TCP stack — and all of them have to behave
when the network goes away mid-operation. Keeping them adjacent is what stops
four independent reconnection policies from fighting each other.

---

## Printer abstraction

### The interface

Every printer implements the same small surface. The prototype's version, which
has survived four protocols unchanged, is the starting point:

```cpp
class PrinterBackend {
public:
    virtual void begin(const PrinterCfg& cfg) = 0;  // open the connection
    virtual void loop()                       = 0;  // pump the transport
    virtual void stop()                       = 0;  // close, free the heap
    virtual bool connected()                  = 0;

    virtual int         slotCount()           = 0;  // how many slots exist
    virtual const char* slotLabel(int i)      = 0;  // "Ext", "1A", "T3"…
    virtual const SlotState& slot(int i)      = 0;  // what's in slot i now
    virtual bool assign(int i, const TagInfo& t) = 0;  // put this filament there

    virtual String status()                   = 0;
    virtual void   refresh()                  = 0;  // re-read slot state
};
```

Three things make this work across protocols as different as MQTT and HTTP
polling:

**Slots are a flat 0-based list to the UI.** Real printers do not agree on
addressing — Creality has box+slot pairs, Bambu has AMS unit + tray with special
values for the external spool, FlashForge has 1-based slot ids, Snapmaker has
extruder indices, Anycubic has box+slot with a four-slot box numbered `-1`.
Every one of those mappings lives *inside* its backend. The grid on screen
iterates `0 … slotCount()-1` and never learns the difference.

> The TigerTag account already models slots this way, normalised across brands,
> each entry carrying a `hw` object holding the printer's own address. The
> firmware and Tiger Studio arrived at the same split independently, which is
> decent evidence it is the right one — and it means the two can be aligned
> rather than merely compatible.
> [ACCOUNT-DATA.md](ACCOUNT-DATA.md#units--the-slot-model-the-ecosystem-already-agreed-on)

**`slotCount()` is dynamic.** A Bambu Lab printer's slot count is not known until
its first status report arrives — one AMS unit or four, plus the external spool.
The UI paginates whatever number it is told.

**`assign()` takes a decoded tag, not a protocol payload.** Translating "PETG,
Polymaker, #1A5C0C, 230/240 °C" into whatever the printer will accept is the
backend's entire job, and it is where the per-brand ugliness lives — see below.

### Transport vs. protocol

These are separated because they vary independently. Two printers can share a
transport and share nothing else — Bambu Lab, Elegoo and Anycubic all speak MQTT
and agree on nothing above it, including whether there is any TLS.

| Backend | Transport | Protocol |
|---|---|---|
| Creality | WebSocket, port 9999 | proprietary JSON, `get`/`set` envelopes |
| FlashForge | HTTP POST, port 8898 | JSON with credentials in every request body |
| Bambu Lab | MQTT over TLS, port 8883 | JSON commands on `device/{SN}/request` |
| Snapmaker | WebSocket, port 7125 | Moonraker JSON-RPC 2.0, no auth |
| Elegoo | MQTT, port 1883, **plain TCP** | numbered JSON methods on `elegoo/{sn}/…` topics |
| Anycubic | MQTT over TLS, port 9883 | JSON actions on an `anycubic/…/multiColorBox` topic |

The transport layer answers: how do bytes get there, how does the connection
recover, what does authentication look like. The protocol layer answers: what
does "set the filament in slot 2" spell out to.

### The part nobody expects: material and colour translation

Backends are not thin. The tag says exactly what the filament is; printers accept
only what they already know about, and each is differently stubborn:

- **Bambu Lab** wants a `tray_info_idx` code (`GFL99` for generic PLA, `GFG99`
  for PETG…) and accepts **any** RGB colour.
- **FlashForge Creator 5** accepts only the **24 colours in its own palette** and
  a material name from **its own list of 21**. Send anything else and it answers
  "success" while silently resetting the slot to white. Colour has to be snapped
  to the nearest palette entry, material to the nearest known name.
- **Creality** wants a 7-digit `#0RRGGBB` colour — a `0` after the `#`, not a
  6-digit hex.
- **Snapmaker** takes G-code arguments, so any space in a vendor or material name
  has to become an underscore or the command is silently mis-parsed.
- **Elegoo** wants a `filament_code` looked up from a 50-entry table keyed by
  material type *and* variant name, and a `filament_type` stripped back to its
  base token — `"PLA+ Silk"` has to become `"PLA"`.
- **Anycubic** takes a `[r, g, b]` array and accepts only `{index, type, color}`;
  richer fields are acknowledged and silently dropped. Pure black `[0,0,0]`
  renders as *empty* on the ACE display and has to be nudged to `[1,1,1]`.

This translation is a first-class part of every backend, not an afterthought.
Each one needs its own tests, and each is a place where a printer firmware update
can break us.

> **A lesson worth writing down:** on FlashForge, an unknown command is
> acknowledged with `{code:0}` anyway. **Success is not evidence of effect.** Any
> backend whose protocol can lie must confirm by re-reading state after a write.

### The registry

Backends register themselves; the application never contains a `switch` over
brands. Adding one means adding a file and a registration line, and that is what
keeps a new brand from touching the UI.

---

## State machine

```
                    ┌──────────┐
     first boot ───▶│   LANG   │  pick a language, once
                    └────┬─────┘
                         ▼
                    ┌──────────┐  credentials in NVS?
              ┌────▶│   WIFI   │──── no / fails ──┐
              │     └────┬─────┘                  ▼
              │          │ connected      ┌──────────────┐
              │          │                │  PROVISION   │  built-in captive
              │          │                │              │  portal + Wi-Fi QR
              │          │                └──────┬───────┘
              │          │                       │ saved
              │          │◀──────────────────────┘  (reboot)
              │          ▼
              │     ┌──────────┐  no account linked?
              │     │  ACCOUNT │──────────────────┐
              │     └────┬─────┘                  ▼
              │          │ linked          ┌──────────────┐
              │          │                 │   PAIRING    │  QR + short code,
              │          │                 │              │  poll until approved
              │          │                 └──────┬───────┘
              │          │◀───────────────────────┘
              │          ▼
              │     ┌──────────┐  the printers imported from the account
              │     │ PRINTERS │  paginated list, online/offline dots
              │     └────┬─────┘
              │          │ tap a printer  ──▶ backend->begin()
              │          ▼
              │     ┌──────────┐  slot grid, colours and materials read live
              └─────│   GRID   │◀────────────────────────────┐
             back   └────┬─────┘                             │
                         │ tap a slot                        │
                         ▼                                   │
                    ┌──────────┐  "hold the spool to the box" │
                    │   SCAN   │──── cancel ─────────────────▶│
                    └────┬─────┘                              │
                         │ tag decoded                        │
                         ▼                                    │
                    ┌──────────┐  colour swatch, material,    │
                    │  REVIEW  │  brand, temperatures         │
                    └────┬─────┘  [ No ]        [ Send ] ─────┤
                         │ Send → backend->assign(slot, tag)  │
                         ▼                                    │
                    ┌──────────┐  OK / error + reason         │
                    │  RESULT  │──── tap, or 2.5 s ──────────▶┘
                    └──────────┘
```

**Design rules for the state machine:**

- **The screen never blocks.** Anything that can take seconds — a network scan, an
  account sync, an OTA download — either runs incrementally across `loop()` calls
  or gets its own screen with progress. A frozen 2.0" screen is indistinguishable
  from a crashed device.
- **Every failure state names a way out.** "Printer offline" is not an error
  message; "Printer offline — check it is on the same network" is.
- **`PROVISION` and `PAIRING` are states, not separate firmwares.** See
  [WIFI-PROVISIONING.md](WIFI-PROVISIONING.md) and
  [ACCOUNT-PAIRING.md](ACCOUNT-PAIRING.md).
- **`RESULT` writes back to the account, in the background.** A confirmed
  assignment updates the slot in the user's account — material, colour, vendor,
  and the scanned tag's UID — so a spool scanned in the workshop is visible in
  Tiger Studio and on a phone. It happens after the result screen, never blocks
  it, and never runs for a cloud-mode printer whose state the vendor's cloud
  already owns. [ACCOUNT-DATA.md](ACCOUNT-DATA.md#writing-back)

---

## Configuration and storage

Everything persistent lives in NVS under the namespace **`tigerspool`**.

| Key group | Holds |
|---|---|
| `ssid`, `pass` | Wi-Fi credentials |
| `lang` | selected language |
| `p0…p7` | up to eight printers: type, name, host, serial, access code |
| `psel` | which printer is selected |
| account | refresh token, uid, email — encrypted, see below |

**Printers are imported, not typed.** The account sync writes `p0…p7`; the web
form exists to correct an import or to add a printer the account does not know
about, and it merges rather than overwrites, so a value a user corrected by hand
is not clobbered by the next sync.

**A printer's credentials are not a fixed shape, and the config must stop
pretending they are.** The prototype models every printer as
`{type, name, host, sn, cc}` — enough for a serial and one access code, which
covers Creality, FlashForge, Bambu Lab, Snapmaker and Elegoo. **Anycubic does
not fit**: it needs a broker `deviceId`, a `username`, a `password` and a numeric
`acuModelId` that forms part of the MQTT topic. Cloud-mode printers carry
different fields again.

Bolting `sn2`/`cc2` onto the struct is how this ends up unreadable by the third
brand. The credential set belongs to the backend that consumes it — a small
key/value bag the account layer fills and the backend reads by name — so adding a
brand with unusual authentication stays a change inside that brand.

The observed field names, per brand, are in
[ACCOUNT-DATA.md](ACCOUNT-DATA.md#credentials-are-not-one-shape). Six brands use
six different vocabularies, and one of them shares no spelling with any other.

**The account token is stored in encrypted NVS.** A refresh token is a
long-lived credential to someone's account. It does not sit in plaintext flash
where a `esptool read_flash` recovers it. See [SECURITY.md](../SECURITY.md).

**No credentials are ever compiled in.** Not Wi-Fi, not printer access codes, not
API keys with any authority. The published firmware binary is identical for every
user.

---

## Identifiers

Fixed now so that nothing has to be renamed later. The prototype's `k2`/`tigertag`
names belong to a Creality-only ancestor and do not survive.

| | Value |
|---|---|
| mDNS hostname | `tigerspool-xxxx.local` |
| Setup AP SSID | `TigerSpool-Setup-XXXX` |
| Firmware binary | `tigerspool-v1.0.0.bin` |
| PlatformIO env | `tigerspool` |
| MQTT client id | `tigerspool-<mac6>` |
| NVS namespace | `tigerspool` |

---

## Directory layout

The source tree is mapped in [CODEMAP.md](../CODEMAP.md): what each file owns,
what it must not be asked to do, and the landmines to read before editing it.
It is kept there rather than here so there is one description of the layout
rather than two that drift.

**One copy of the PN532 library**, vendored under `firmware/lib/PN532/`. The
prototype carried three, which had already drifted apart from each other. See
[MIGRATION.md](MIGRATION.md#the-vendored-pn532-library).

---

## Open questions

Recorded here rather than answered wrongly.

- **Recovering a wedged reader at runtime.** The reset line helps at init. Whether
  it can recover a reader that stops responding after hours, without a power
  cycle, is untested.
- **Backend memory ceiling.** Bambu Lab's full status report can reach ~50 KB and
  needs a large MQTT buffer. Only one backend is active at a time, which is what
  makes this fit; whether that constraint should be enforced by the registry
  rather than by convention is undecided.
- **Behaviour during a print.** Reassigning a slot mid-print is at best ignored
  and at worst harmful on some printers. Whether the firmware should detect and
  refuse it, and how it learns the printer is printing, is not designed yet.
