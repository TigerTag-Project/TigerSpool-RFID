# The account data model

What TigerSpool reads from a TigerTag account, and what it writes back.

This describes the shape of `users/{uid}/printers/{brand}/devices/{deviceId}`,
observed on a real account. Every value below is a placeholder — no real
credential appears in this repository.

---

## Why this document exists

TigerSpool's premise is that **your printers are already in your account**, put
there by [Tiger Studio](https://github.com/TigerTag-Project/TigerTag-Studio-Manager)
on a computer with a keyboard ([ACCOUNT-PAIRING.md](ACCOUNT-PAIRING.md)). This is
the shape of what it finds.

Two things make it more interesting than a list of IP addresses:

1. **The account already carries a normalised, brand-agnostic slot model.** The
   abstraction TigerSpool needs is not something it has to invent — it exists.
2. **The account is written to, not only read.** A spool scanned on the device
   should become visible in Studio and on a phone.

---

## A printer document

```jsonc
{
  "id":             "<deviceId>",
  "printerName":    "Workshop U1",
  "printerModelId": "1",            // catalog id — model name and photo in UI
  "ip":             "192.0.2.10",
  "updatedAt":      1788337589686,

  // Brand-specific credentials — see the table below. The field NAMES differ
  // between brands. This is the important part.

  "discovery": { … },               // how it was found; absent if added by hand
  "units":     { … }                // the slot model — see below
}
```

### Credentials are not one shape

This is the single most consequential fact for the firmware. Observed, per brand:

| Brand | Reachability | Fields that matter |
|---|---|---|
| **Snapmaker** | LAN | `ip` only — Moonraker needs no authentication |
| **Klipper** | LAN | `ip`, as `host` or `host:port` when Moonraker is not on 7125; optional `apiKey` |
| **Creality** | LAN | `ip`, `discovery.transport: "ws-9999"` |
| **FlashForge** | LAN | `ip`, `serialNumber`, `checkCode` *(also mirrored as `password`)* |
| **Bambu Lab** | LAN | `ip`, serial, access code |
| **Elegoo** | LAN | `ip`, `sn`, **`mqttPassword`** |
| **Anycubic** (LAN) | LAN | `ip`, `deviceId`, `username`, `password`, `acuModelId` |
| **Anycubic** (cloud) | cloud | `mode: "cloud"`, `cloudPrinterId`, `machineType`, `acuModelId`, `key`, `cloudToken`, `cloudEmail` — **no `ip`** |

Six brands, six different credential vocabularies, and one of them
(`mqttPassword`) shares no spelling with any other. A fixed
`{host, serial, accessCode}` struct cannot express this — which is why the
firmware models credentials as a named bag the account layer fills and each
backend reads by name ([ARCHITECTURE.md](ARCHITECTURE.md#configuration-and-storage)).

> **Known gap in the prototype.** Its import looks for an access code under
> `password`, `dev_access_code`, `accessCode`, `access_code` and `checkCode` —
> **not** `mqttPassword`. An Elegoo printer is therefore imported without its
> password and silently dropped. Fix this when porting.

### LAN or cloud is a property of the document

A printer can be in the account and **not reachable from TigerSpool at all**.
Cloud-mode documents carry `mode: "cloud"` and no local address: the printer
keeps no open local ports, and it is driven through the vendor's cloud instead.

TigerSpool must tell these apart and **say so on screen**. "Cloud printer — not
reachable from this device" is a different message from "offline", and a user who
sees the wrong one goes looking for a network fault that does not exist.

> Cloud sessions expire — the Anycubic token observed carried roughly a
> three-month lifetime, and Bambu Lab's behaves similarly. **Tiger Studio renews
> them; the firmware only consumes them.** An expired token is a "sign in again
> in Tiger Studio" message, not an error code.

---

## `units` — the slot model the ecosystem already agreed on

Every brand's document normalises its slots into the same structure:

```jsonc
"units": {
  "holder_0": {                     // a unit: an AMS/CFS/ACE box, or the toolhead
    "kind":    "holder",            // "holder" | "ext"
    "index":   0,
    "cols":    4,
    "rows":    1,
    "present": true,
    "hwId":    null,
    "slots": [
      {
        "index":    0,
        "label":    "1A",
        "material": "PLA",
        "subType":  "Silk",
        "color":    "#2981E6",
        "vendor":   "Generic",
        "loaded":   true,
        "seenAt":   1788142955641,
        "uids":     [],             // TigerTag UIDs bound to this slot
        "hw":       { … }           // the brand-specific address
      }
    ]
  },
  "ext_0": { "kind": "ext", … }     // external spool, when the printer has one
}
```

### `hw` is the brand's own addressing, kept out of everything else

| Brand | `hw` |
|---|---|
| Snapmaker | `{ "extruder": 0…3 }` |
| Elegoo | `{ "canvasId": 0, "trayId": 0…3 }` |
| FlashForge | `{ "slot": 1…4 }` or `{ "external": true }` |
| Anycubic | `{ "boxId": -1…N, "slotIndex": 0…3 }` |

**This is exactly the split the firmware's `PrinterBackend` makes** — a flat,
0-based slot list for the UI, with each backend translating to its printer's real
addressing ([ARCHITECTURE.md](ARCHITECTURE.md#printer-abstraction)). The design
was arrived at independently in the firmware and in Studio, which is decent
evidence it is the right one.

It also means the two can be aligned rather than merely compatible: a firmware
`SlotState` and an account `slots[]` entry describe the same thing, and `hw` is
the address the backend already computes.

> **Anycubic's `boxId: -1` is not a single external spool.** It is a full
> four-slot external box. A UI that collapses it to one cell loses three slots.

---

## Writing back

A scan is only half useful if it stays on the device. When TigerSpool assigns a
filament to a slot and the printer confirms it, it should update that slot in the
account:

| Field | Written |
|---|---|
| `material`, `subType`, `color`, `vendor` | from the tag |
| `uids` | the TigerTag UID that was scanned — this is the slot↔tag binding |
| `seenAt` | when |
| `loaded` | true |

That is what makes a spool scanned in the workshop visible in Tiger Studio and on
a phone, and it is why `uids` exists in the schema.

**Rules:**

- **Write only after the printer confirms.** Where a protocol can acknowledge a
  command it ignored — FlashForge does — confirm by re-reading before writing to
  the account. Reporting a change that did not happen is worse than reporting
  nothing.
- **Never for cloud-mode printers.** Their state comes from the vendor's cloud in
  real time and is already authoritative. Writing a second, staler copy creates a
  conflict with no resolution rule.
- **Never block the UI.** The write happens after the result screen, in the
  background, and a failure is not shown to the user — the filament is in the
  printer either way.

---

## Reading efficiently

Documents are large. A `creality/devices` collection was observed at **47 KB**,
and a Snapmaker document carries a complete Moonraker system dump under
`discovery.raw` — Python version, kernel version, SD card serial, CPU topology —
none of which the firmware will ever read.

The firmware parses these on a microcontroller, so:

- **Request only the fields needed.** Firestore REST accepts
  `mask.fieldPaths`; asking for the credential fields and `units` avoids pulling
  `discovery.raw` entirely.
- **Parse with a filter**, never into a full document tree.
- **Sync when idle**, never during a scan.

---

## Data-quality notes

Observed on a real account. Not TigerSpool's to fix, but the firmware has to
survive them:

- **Document ids do not indicate brand.** An Elegoo printer was found under id
  `snap_…`, created by a different add-flow. Trust the collection path, never the
  id.
- **A brand's documents are not uniformly shaped.** Some carry `discovery`, some
  do not. An import that requires it drops printers that are otherwise complete.
- **The same value appears under two names.** FlashForge's check code was present
  as both `checkCode` and `password`. Read defensively; prefer the specific name.
- **Literal dotted keys** such as `"widgets.units": true` were observed alongside
  a proper `widgets` map. Harmless here, but a parser that walks keys must not
  assume a dot means nesting.
