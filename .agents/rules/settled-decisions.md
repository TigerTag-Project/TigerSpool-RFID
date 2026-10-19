# Settled decisions

**Rule.** Do not re-propose a settled decision:
- **Wi-Fi provisioning is in the main firmware.** The device raises its own access point, answers the captive-portal probes and joins without rebooting; there is no separate provisioning firmware and no second flash.
- **Account sign-in is email/password or Google pairing**, a code-and-QR flow served by the TigerTag Cloud Functions. RFC 8628 device flow was rejected: Google publishes no `verification_uri_complete` (a QR cannot carry the code), polling needs a client secret the device must not hold, and it covers Google only.
- **No Wi-Fi Easy Connect (DPP).** A QR pointing at the device's own access point works from any phone; DPP does not.
- **No text entry on the device.** There is no keyboard; anything that needs typing happens on a phone (`docs/ONBOARDING.md`).

**Why.** Each was decided once and cost something to decide; re-litigating it costs that again. The reason for each is given with it above.

**How.**
- Another settled point lives in its own rule: frozen partitions (`flash-safety.md`), reader pins (`reader-pins.md`), LVGL only (`lvgl-only.md`), state ownership (`state-ownership.md`), secrets (`secrets.md`).
