#!/usr/bin/env python3
"""Line endings, byte-order marks, invisible controls, final newlines.

The incident: a pull request titled "translate three comment blocks" arrived
carrying +16034 / -16034 on one file. Nine of those lines were real; the rest
was the whole file converted to CRLF. Merging it would have reassigned git
blame for every line and made every later diff on that file unreviewable.

.gitattributes already asks for LF, and that did not stop it. The
normalisation it describes happens in a client's index on `git add`; a commit
made through the web editor or the API never passes through one. The attribute
is the intention. This is the enforcement.

Also checked here because they are the same class of invisible damage:

  - Byte-order marks, which break `#include` and shell shebangs in ways that
    read as a corrupt file rather than a stray three bytes.
  - Bidirectional and invisible control characters, which make source display
    to a reviewer in an order the compiler does not use.
  - A missing final newline, which makes the next edit to the last line show up
    as two changed lines instead of one.

Scope: every text file git tracks, vendored code included. None of these checks
asks anyone to restyle third-party code - a driver we do not touch still must
not arrive in CRLF - so nothing is excluded.
"""

import pathlib
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent

# Characters that change how text is displayed without changing what it means.
# The bidirectional overrides are the "Trojan Source" family; the zero-width and
# no-break spaces are the ones that reach source through copy-paste.
INVISIBLE = {
    0x00A0: "NO-BREAK SPACE",
    0x200B: "ZERO WIDTH SPACE",
    0x200C: "ZERO WIDTH NON-JOINER",
    0x200D: "ZERO WIDTH JOINER",
    0x200E: "LEFT-TO-RIGHT MARK",
    0x200F: "RIGHT-TO-LEFT MARK",
    0x202A: "LEFT-TO-RIGHT EMBEDDING",
    0x202B: "RIGHT-TO-LEFT EMBEDDING",
    0x202C: "POP DIRECTIONAL FORMATTING",
    0x202D: "LEFT-TO-RIGHT OVERRIDE",
    0x202E: "RIGHT-TO-LEFT OVERRIDE",
    0x2066: "LEFT-TO-RIGHT ISOLATE",
    0x2067: "RIGHT-TO-LEFT ISOLATE",
    0x2068: "FIRST STRONG ISOLATE",
    0x2069: "POP DIRECTIONAL ISOLATE",
    0xFEFF: "ZERO WIDTH NO-BREAK SPACE",
}

# Files whose bytes are data, not text, whatever their extension says.
BINARY_SUFFIXES = {".png", ".jpg", ".jpeg", ".ico", ".pdf", ".bin", ".gz",
                   ".zip", ".stl", ".3mf", ".woff", ".woff2", ".ttf"}


def tracked_text_files():
    out = subprocess.run(["git", "ls-files", "-z"], cwd=REPO,
                         capture_output=True, text=True, check=True).stdout
    for name in out.split("\0"):
        if not name:
            continue
        path = REPO / name
        if path.suffix.lower() in BINARY_SUFFIXES or not path.is_file():
            continue
        raw = path.read_bytes()
        if b"\0" in raw[:8000]:          # NUL early is the reliable binary tell
            continue
        yield name, raw


def main() -> int:
    problems = []
    scanned = 0

    for name, raw in tracked_text_files():
        scanned += 1

        if raw.startswith(b"\xef\xbb\xbf"):
            problems.append(f"{name}:1: byte-order mark at the start of the file")

        if b"\r\n" in raw:
            n = raw.count(b"\r\n")
            problems.append(f"{name}: {n} CRLF line ending(s) - this file must be LF")

        if raw and not raw.endswith(b"\n"):
            problems.append(f"{name}: no newline at end of file")

        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError as e:
            problems.append(f"{name}: not valid UTF-8 ({e.reason} at byte {e.start})")
            continue

        # The one exemption, and it is narrow on purpose.
        #
        # firmware/tools/tigertag_db/ is a MIRROR of the public TigerTag API, kept
        # current by db_update.py. Two brand names arrive from it with a
        # no-break space between the words. Editing them here would be undone by
        # the next download, so this guard would fail on the day someone runs
        # the updater and pass again only if they hand-edited data back out of
        # sync with its source - which is the opposite of what a mirror is for.
        #
        # The hazard is still covered, downstream and by a checker that can
        # actually act on it: gen_db.py validates every label against the
        # compiled font, repairs the no-break space to an ordinary one, and
        # names each entry it repaired so the upstream defect stays visible.
        # Nothing invisible reaches the panel, and nothing invisible reaches a
        # C string literal.
        if name.startswith("firmware/tools/tigertag_db/"):
            continue

        for lineno, line in enumerate(text.splitlines(), 1):
            for ch in line:
                if ord(ch) in INVISIBLE:
                    problems.append(
                        f"{name}:{lineno}: U+{ord(ch):04X} {INVISIBLE[ord(ch)]} "
                        "- invisible here, and not what it looks like")

    if scanned == 0:
        # Zero files scanned means the scan is broken, whatever the violation
        # count says. Zero violations across real files is the pass.
        print("error: scanned no files at all - this check is checking nothing",
              file=sys.stderr)
        return 2

    for p in problems:
        print(f"error: {p}", file=sys.stderr)
    print(f"scanned {scanned} files, {len(problems)} violations")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
