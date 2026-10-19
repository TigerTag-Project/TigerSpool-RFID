# Verify before reporting done

**Rule.** Run `bash scripts/verify.sh` (guards plus a real build) and see it pass before reporting a code change done. `bash scripts/verify.sh --quick` is the fast loop while working, not the thing to report on.

**Why.** It is what CI runs (guards and the firmware build on every push). A push should not be what reveals that an index drifted or a build broke.

**How.**
- The pre-commit hook runs `--quick` only, deliberately: a build on every commit teaches people to use `--no-verify`.
- `--fix` regenerates `tigertag_db.h` first, then verifies.
- A guard exiting `2` found nothing to check: fix the guard before trusting the tree.
- The symptom-to-remedy table for every guard is in `CLAUDE.md` ("When a check goes red").
- Two PlatformIO builds at once on one machine share `~/.platformio/packages` and can swap `tool-esptoolpy` under each other; wait for the other build to finish.
