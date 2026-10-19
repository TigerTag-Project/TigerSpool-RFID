# Local and internal notes

**Rule.** Machine-specific facts (device name, IP, serial port, bench MAC) go in `LOCAL.md`, which is gitignored and never committed. `_internal/` holds French maintainer notes, is gitignored and must never publish. Reach the device by mDNS, not by IP.

**Why.** No committed file may carry a network address, a serial port or an absolute path.

**How.**
- If `LOCAL.md` is stale, correct it there.
- `scripts/check-docs.py` fails if `_internal/` is ever tracked; then ask, and `git rm -r --cached _internal`.
