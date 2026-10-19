# Secrets

**Rule.** No credential in the repository or the firmware binary: no Wi-Fi password, access code, token or key with authority. Everything secret comes from NVS at runtime and is never logged. The Firebase `apiKey` in `firmware/src/tigertag_cloud.cpp` is public client configuration, not a secret.

**Why.** The published binary is byte-identical for every user; a build is not a place to put a password. See `SECURITY.md`.

**How.**
- Anycubic cloud tokens and certificates are held in RAM only and refilled by each account sync.
- Never print a credential in a log line, a report or a chat message.
- Personal device data (IP, serial port) goes in `LOCAL.md`, which is gitignored (`local-notes.md`).
