# Bench-only code

**Rule.** Diagnostics that must not ship go inside `#if TIGERSPOOL_BENCH` or through `BENCH_LOG()` (`firmware/src/bench.h`), never loose in the release build.

**Why.** Added 2026-09-28: a payload dump or a counter left in the release runs on every device in the field, on a board with a few hundred kilobytes of internal RAM.

**How.**
- `check-bench-code.py` fails on a `BENCH:`, `TEMP:` or `DO NOT SHIP` marker outside such a block. Never remove the marker to make it pass.
- Build the bench variant with `pio run -e tigerspool-bench`.
