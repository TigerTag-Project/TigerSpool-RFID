# Roadmap

What is deliberately not done yet, why, and what has to be decided before it
can be. Everything here is a decision that was taken, not a thing forgotten —
an item leaves this file by being built or by being ruled out, never by
quietly rotting.

Anything already broken lives in [reviews/](reviews/), not here.

---

## The account: a heartbeat

**The device never writes to the account.** It reads printers, the profile and
the display name; there is no write path at all. So nothing in Tiger Studio can
show which TigerSpools exist, which are switched on, or when one was last seen.

The TigerScale does this with a periodic `PATCH` of telemetry it alone knows —
battery, charging state, Wi-Fi signal, mDNS hostname — every 30 s with the
screen lit, every 5 min with it dark, and forced immediately on an event a
person causes and then watches.

**Deferred on purpose.** It is not urgent, and it is the piece most likely to
be built wrong in a hurry.

**Decide before writing the first line: who owns each field.** This is the
sibling project's strongest advice on the subject and the reason it has no
write conflicts at all — the scale never writes a field Tiger Studio owns. It
*reads* `displayName` and never writes it. Once two writers share one field, no
arbitration rule is a good one: not last-write-wins, not merge, not a timestamp.
Partition the fields first and the conflict never exists.

**Ask the TigerScale agent for the specifics when this is picked up:** exactly
what it writes, to which document path, on what cadence, what forces an
immediate write, and how a failed write is handled. Its answer to the previous
question is in `_internal/TIGERSCALE-CLOUD-ANSWERS.md` and covers the shape;
the exact field list and paths were not asked for and should be.

Known limit to inherit knowingly: the scale has **no offline queue**. A failed
write is lost and the next one carries current values. That is right for
telemetry, where the newest value replaces the last — and wrong for anything
that must not be lost. If a TigerSpool ever writes an event rather than a state
("this spool went to this printer"), it needs a queue, and a queue does not get
added afterwards without rethinking the rest.

## The account: being told about changes

Nothing tells the device that an account changed. It polls every five minutes,
and refreshes when the printer picker opens — which covers the moment that
matters.

**Blocked on a question the firmware cannot answer: who writes the signal, and
when?** Every mechanism — a Firestore command queue, a realtime-database node
streamed over SSE — is inert until something in the backend writes to it. Half
of this feature lives outside this repository.

Worth building only when a specific need is named that genuinely cannot wait
five minutes. The test: *which screen would somebody sit and watch, waiting for
this to change?* On a scale that is the weight, and it does not come from the
network. On a spool reader there is no such screen yet.

## Firmware signing

The update connection is verified against the root store, so a device knows who
it is talking to — but not who produced the image. See
[OTA.md](OTA.md) for the reasoning and the condition for changing it. The key
must never touch CI, which is the part that makes this more than an afternoon.

## Accented text on the panel

The compiled font is ASCII plus degree and bullet, so on-screen French is
written without its accents on purpose. The fix is a generated Latin subset
attached as a fallback face, and Polish needs Latin Extended-A on top. The
sibling project has done exactly this and its recipe is in
`_internal/TIGERSCALE-UI-ICONS-2.md`.

Do it in one pass with the Font Awesome sun currently generated on its own by
`scripts/make-icon-font.sh`: `lv_font_conv` takes several `--font` in one call,
so that glyph costs no second face and no extra link in the fallback chain.
Delete that script when it does.

## Elegoo and Anycubic

Both protocols are documented and working in Tiger Studio; the firmware side is
not written. See [PRINTER-COMPATIBILITY.md](PRINTER-COMPATIBILITY.md).
