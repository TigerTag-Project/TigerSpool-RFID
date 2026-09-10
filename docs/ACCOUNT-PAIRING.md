# Linking a TigerTag account

How a device with no keyboard gets signed in — and why that sign-in is what makes
the whole product work.

---

## Why the device signs in at all

Because this is where TigerSpool stops being another spool scanner.

Your printers are already in your TigerTag account: you added them in
**[Tiger Studio](https://github.com/TigerTag-Project/TigerTag-Studio-Manager)**,
on a computer, with a keyboard, once. Names, addresses, serial numbers, access
codes.

When TigerSpool signs into that account it reads `users/{uid}/printers/*` and
configures itself. **The user never types a printer's IP address or access code
into a 2.0" touchscreen, because they never have to type it at all.** Add a
printer to your account next month and TigerSpool picks it up on its own, on its
next sync.

The account is not a nice-to-have wrapped around a local product. It is the
product's core, and the architecture assumes it.

---

## Two ways in, both without a device keyboard

Same as [TigerScale](https://github.com/TigerTag-Project/Tiger-Scale-V3). Users
arrive with accounts created either way, so both paths stay.

### 1. Email and password — typed on the phone

Reached from the configuration web page, at `http://tigerspool-xxxx.local` on the LAN
or through the setup portal. The user types their email and password **on their
phone's keyboard**, with their password manager available, on a page the device
serves.

The device exchanges them for a session and stores the refresh token. It never
stores the password.

### 2. Google — a QR code on the device screen

Accounts created with Google have **no password to type**, so there is nothing a
form can ask for. These sign in by scanning a QR code shown on the device itself.

---

## The QR pairing flow

```
  TigerSpool                    TigerTag cloud                  owner's phone
      |                                |                              |
      |--- POST pair/start ----------->|                              |
      |<-- code + verify_url + poll_token                             |
      |                                |                              |
  [ draws a QR of verify_url ]         |        <---- scans it -------|
  [ prints the code beneath it ]       |                              |
      |                                |<--- opens verify_url --------|
      |                                |     (signs in with Google
      |                                |      if not already)
      |                                |<--- approves ----------------|
      |                                |  [ mints a custom token ]    |
      |                                |                              |
      |--- POST pair/poll ------------>|                              |
      |<-- status: approved, custom_token                             |
      |                                |                              |
      |--- signInWithCustomToken ----->|  (Identity Toolkit)          |
      |<-- idToken + refreshToken                                     |
      |                                |                              |
  [ stores the refresh token, reads the account's printers ]
```

**The URL in the QR already carries the code.** The user scans something that is
physically in front of them and taps approve on a phone where they are usually
already signed in. Nothing is read off one screen and typed into another.

### Why not RFC 8628

[RFC 8628](https://datatracker.ietf.org/doc/html/rfc8628) is the OAuth device
authorization grant, and it exists for exactly this shape of device. It was
examined and rejected — the same conclusion, for the same reasons, as
[TigerScale V3](https://github.com/TigerTag-Project/Tiger-Scale-V3/blob/main/docs/ACCOUNT-PAIRING.md):

- **The QR cannot carry the code.** Google's device-code response has no
  `verification_uri_complete`. It returns a bare URL and an eight-character code
  the user must retype. The best achievable experience becomes "scan this, then
  copy a code across from a screen 40 cm away" — which is the friction this whole
  design exists to remove.
- **Polling requires a `client_secret`**, which a public firmware binary must not
  carry. It could be provisioned at runtime, but that is machinery in service of
  a flow that is worse anyway.
- **It authenticates against Google only.** Apple, or any provider added later,
  would each need a separate path.

Pairing through TigerTag's own cloud has none of those properties, because both
ends belong to the project.

### Why the code and the poll token are different secrets

The **code** is short because a human may read it aloud or type it. The **poll
token** is long because whoever holds it receives the credential.

If one value did both jobs, anyone who glimpsed the screen from across the room
could poll for the token themselves. They are separate on purpose.

### What the approval page must show

The page reached by the QR has to name **which device is asking** — its name and
the last four of its MAC address — and **which account** it would be linked to.

The risk of a user approving someone else's pairing code is low here, because
they are looking at the physical screen the QR came from. But a page that says
only "Approve?" trains people to approve anything, and that habit is the actual
vulnerability.

---

## On-device screens

| Screen | Shows |
|---|---|
| **Account** | Signed-in email, or an invitation to link an account. |
| **Pairing** | The QR code, large. The short code in plain text beneath it, for any phone that will not scan. A countdown to expiry. |
| **Approved** | Confirmation and the account's email, then straight to importing printers. |
| **Denied / expired** | Says which of the two it was, and offers a fresh code. Never a bare error. |

The pairing flow **must be reachable from the device screen**, not only from the
web page. A user standing in front of the box with a phone in their hand should
not have to find a URL on another device first.

---

## Endpoints

These are the endpoints **TigerScale V3 actually calls in shipped firmware**, and
the prototype calls the same ones. They are the deployed, working surface.

| | |
|---|---|
| Start | `POST https://us-central1-tigertag-connect.cloudfunctions.net/pairStart` |
| Poll | `POST https://us-central1-tigertag-connect.cloudfunctions.net/pairPoll` |
| Sign in | `POST https://identitytoolkit.googleapis.com/v1/accounts:signInWithCustomToken` |
| Refresh | `POST https://securetoken.googleapis.com/v1/token` |
| Printers | Firestore REST, `users/{uid}/printers/*` |

> **On `/api/device/pair/*`.** TigerScale V3's own documentation describes a
> first-party API path rather than raw Cloud Function URLs. Its firmware does not
> use it — the shipped code hardcodes the two Cloud Function URLs above. Treat
> `/api/device/pair/*` as an intended future surface, not something to code
> against today. If it is ever built, moving to it is a two-constant change.

### `pair/start`

Unauthenticated: the device has no identity yet. That is what the rate limit and
the short expiry are for.

Request:

```json
{ "device": "tigerspool-a1b2", "model": "TigerSpool RFID", "kind": "bridge", "fw": "1.0.0" }
```

Response:

```json
{
  "code":       "K7QF-3M2P",
  "verify_url": "https://tigersystem.io/pair?c=K7QF3M2P",
  "poll_token": "<32 bytes, base64url>",
  "expires_in": 600,
  "interval":   5
}
```

### `pair/poll`

```json
{ "poll_token": "<the value from start>" }
```

| Response | Meaning |
|---|---|
| `{"status":"pending"}` | Nobody has approved yet. Poll again after `interval` seconds. |
| `{"status":"approved","custom_token":"<jwt>","email":"…"}` | Sign in with it. Single use — the record is consumed. |
| `{"status":"denied"}` | The owner refused. Stop and say so on screen. |
| `{"status":"expired"}` | Older than `expires_in`. Start again with a fresh code. |

### Requirements on the cloud side

- Rate-limit `start` per IP and `poll` per token; reject a poll faster than
  `interval` with the same back-off the OAuth device flow uses.
- Codes are single-use and expire in ten minutes.
- Store the pending record keyed by a **hash** of the poll token, not the token.

---

## Importing printers

Once signed in, the device reads the account's printers and writes them to NVS
as `p0`, `p1`, … - up to `MAX_PRINTERS`, 24.

**What it can reach.** Printers on the LAN are imported, and so are Bambu Lab
printers in cloud mode, read-only: the device reads their slots through Bambu's
broker but cannot write to them. A cloud-only printer of any other brand is
skipped, with the reason in the log, rather than imported as if it were on the
network.

**Sync is periodic and non-blocking.** The account is re-read every few minutes
on a background task, whatever screen is up. The result is applied only on the
printer screens - never mid-scan, and never under the slot grid someone is
using.

**The account is the source of truth for what it holds.** Name, serial, access
code and address are taken from the account at every sync, so a printer that
moves or gets a new code in Tiger Studio is followed. Two exceptions, both
deliberate: a field the account leaves empty keeps the device's value rather
than being blanked, and a K2 address found by LAN discovery stands while the
account keeps giving the address it gave before. What the account does not
hold - which printers are switched on - belongs to the device.

**A printer is recognised by its serial, not its position.** Adding or removing
a printer in the account does not hand anyone else's address, code or switch to
the printers after it, and two documents for one printer are merged, the newer
winning. The rules are `samePrinter()` in `tigertag_cloud.cpp`.

---

## Security

**The refresh token is a long-lived credential to someone's account.** It is
stored in **encrypted NVS**, never in plaintext flash. See [SECURITY.md](../SECURITY.md).

**TLS certificates must be validated.** The prototype disables verification on
every HTTPS call, which was acceptable on a bench and is not acceptable shipped.
A pinned root CA bundle, and a plan for rotating it, is required before release.
This is tracked as a release blocker.

**Signing out must actually revoke.** "Forget this device" has to clear the local
session *and* be reflected in the account, so that a user who sells or loses a
device can cut it off from a phone. A local-only wipe is not a revocation.

**The device never signs in to a printer vendor's cloud.** Where a vendor session
is needed — Bambu Lab's, for instance — Tiger Studio obtains it and the account
carries it. The firmware only consumes it.
