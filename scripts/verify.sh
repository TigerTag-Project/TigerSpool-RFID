#!/usr/bin/env bash
# One command that runs everything CI runs.
#
#   bash scripts/verify.sh          guards, then build the reference environment
#   bash scripts/verify.sh --quick  guards only, no compiling
#   bash scripts/verify.sh --fix    regenerate what is regenerable, then verify
#   bash scripts/verify.sh --all    exactly what CI runs
#
# CI and the bench must not be able to disagree about what "passing" means, so
# the workflow calls this file rather than carrying its own copy of the checks.
# A check that only exists inside a workflow can only be run by pushing.
#
# Every guard runs even when an earlier one fails: stopping at the first turns
# one session into five. Failures are counted and reported once at the end.

set -uo pipefail
cd "$(dirname "$0")/.."

MODE="build"
case "${1:-}" in
  --quick) MODE="quick" ;;
  --fix)   MODE="fix" ;;
  --all)   MODE="build" ;;
  "")      ;;
  *) echo "unknown option: $1"; sed -n '3,7p' "$0"; exit 2 ;;
esac

# Not every bench has a system python3, but every bench that can build this
# firmware has PlatformIO's own interpreter.
PY=""
for candidate in python3 python "$HOME/.platformio/penv/bin/python"; do
  if command -v "$candidate" >/dev/null 2>&1; then PY="$candidate"; break; fi
done
if [ -z "$PY" ]; then
  echo "error: no python interpreter found (tried python3, python, PlatformIO's penv)"
  exit 2
fi

FAILED=0
FAILED_NAMES=""

step() {
  local name="$1"; shift
  printf '\n\033[1m== %s\033[0m\n' "$name"
  local out
  if out=$("$@" 2>&1); then
    echo "$out" | tail -3
    return 0
  fi
  # The first few real error lines, not just a non-zero exit. A red check that
  # makes you re-run it by hand to find out what it said has failed twice.
  echo "$out" | head -12
  local n; n=$(echo "$out" | wc -l | tr -d ' ')
  [ "$n" -gt 12 ] && echo "   ... $((n - 12)) more lines"
  FAILED=$((FAILED + 1))
  FAILED_NAMES="$FAILED_NAMES\n  - $name"
  return 0
}

if [ "$MODE" = "fix" ]; then
  printf '\n\033[1m== regenerating\033[0m\n'
  "$PY" firmware/tools/tigertag_db/gen_db.py || { echo "generator failed - fix its input first"; exit 1; }
fi

step "file format: line endings, marks, invisible characters" "$PY" scripts/check-file-format.py
step "documentation: internal notes unpublished, links resolve" "$PY" scripts/check-docs.py
step "agent rules indexed, named paths exist, web tables complete" "$PY" scripts/check-agent-rules.py
step "documented reader wiring matches config.h" "$PY" scripts/check-wiring.py
step "documented device names match the ones the firmware builds" "$PY" scripts/check-device-names.py
step "printable cases: layout, names, no local paths" "$PY" scripts/check-models.py
step "bench-only code stays out of the release build" "$PY" scripts/check-bench-code.py
step "committed comments and strings are English" "$PY" scripts/check-text-english.py
step "drawn text comes from the translation table" "$PY" scripts/check-ui-translated.py
step "every drawn string is in a compiled font" "$PY" scripts/check-ui-fonts.py
step "the device and the portal agree on signal strength" "$PY" scripts/check-signal-scale.py
step "translation tables agree with their enums" "$PY" scripts/check-i18n.py
step "generated files match their generator" "$PY" scripts/check-generated.py
step "the version macro is the single source of truth" bash scripts/check-version.sh
step "the current version has release notes" "$PY" scripts/check-release-notes.py

if [ "$MODE" != "quick" ]; then
  step "firmware builds" bash -c 'cd firmware && pio run -e tigerspool'
fi

echo
if [ "$FAILED" -gt 0 ]; then
  printf '\033[31m%d check(s) failed:\033[0m' "$FAILED"
  printf "$FAILED_NAMES\n"
  exit 1
fi
printf '\033[32mall checks passed\033[0m\n'
