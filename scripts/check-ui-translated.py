#!/usr/bin/env python3
"""Text drawn on the panel must come from the translation table, not a literal.

The incident, found by flashing the firmware and reading the panel back rather
than by reading the source: the settings menu on a French device rendered

    Reglages / Imprimantes / Wi-Fi / Compte / Screen / Language / Update /
    Restart / Factory reset

and offered "Sign out" under a French heading, while the home screen was titled
"Printers". Some labels went through i18n::T() and others were English string
literals a few lines away. Fifteen hardcoded against ten translated in one file.
Nothing failed, nothing logged, and it compiled perfectly - a user who chose
their language during setup was simply shown a product that half-forgot.

Translating them once fixes it once. This makes it not come back: every call
that puts text on the panel must be handed something that came from the table.

WHAT COUNTS AS A VIOLATION. Any string literal in a UI source that contains a
word - two or more letters in a row. Not only the ones passed directly to a
draw call: the settings menu keeps its labels in a table and hands the table to
frame::row, so a guard watching call sites alone would have missed five of the
fifteen. Format skeletons like "%d/%d" and separators carry no language and are
let through.

THREE CONTEXTS ARE NOT DRAWN, and are skipped: #include lines, Serial.* logging
(English on the wire is correct, and check-text-english.py covers it), and the
Wi-Fi QR payload, which is a protocol string a phone parses.

WHAT IS ALLOWED, and why each one is. Proper nouns and technical identifiers
are the same in every language this product speaks, and routing them through the
table would mean eight identical rows and a key nobody can name. They are listed
explicitly rather than pattern-matched, so adding one is a decision somebody
makes on purpose.
"""

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import cxx_scan  # noqa: E402

REPO = pathlib.Path(__file__).resolve().parent.parent
UI = REPO / "firmware" / "src" / "ui"

# Lines whose literals never reach the panel.
INCLUDE = re.compile(r"^\s*#\s*include")
SERIAL = re.compile(r"\bSerial\s*\.\s*\w+\s*\(")

# Format specifiers are not words. Without stripping them, "%s  %ds" reads as
# containing "ds" and a skeleton gets reported as untranslated prose.
FORMAT = re.compile(r"%[-+ #0]*[\d.*]*[hlLqjzt]*[a-zA-Z]")

# Protocol strings a machine parses, not prose a person reads.
PROTOCOL = re.compile(r"^WIFI:|^https?:|^[%\s\d./:;=-]*$")

# Same in every language the product speaks.
ALLOWED = {
    "Wi-Fi", "MAC", "IP", "OK", "TigerSpool", "TigerTag", "Google",
    "DHCP", "mDNS", "AP", "SSID", "NFC", "USB", "LAN", "QR", "PN532",
    "ID",
    # The chip's own serial. UID is what every NFC tool, datasheet and spec
    # calls it, in every language - a translated one would be harder to match
    # against a reader log, which is the entire point of showing it.
    "UID",
    # A unit symbol, not a word: it is dBm in every language this device
    # speaks, and translating it would be inventing something.
    "dBm",
    # Millimetres. Same argument as dBm: it is the symbol, not the word, and it
    # is written mm in every language this device speaks.
    "mm",
}

WORD = re.compile(r"[A-Za-z]{2,}")


def unescape(literal: str) -> str:
    """Decode C escape sequences before looking for words.

    The literal arrives as it is written in the source, so "\\xC2\\xB0" - a
    degree sign spelled in bytes to keep the source ASCII - reaches the word
    test as the characters x, C, 2, x, B, 0. "xC" is two letters, so it looked
    like a word, and the guard reported a symbol as untranslated text.

    Same shape as the two false positives this checker family has already had:
    reading the source form of something instead of what it means.
    """
    try:
        return literal.encode("latin-1", "backslashreplace").decode("unicode_escape")
    except (UnicodeDecodeError, UnicodeEncodeError):
        return literal


def serial_spans(text: str):
    """Character ranges covered by Serial.* calls, brackets balanced.

    Matched over the whole file rather than per line: a printf whose format
    string is split across two lines has "Serial.printf(" only on the first.
    """
    spans = []
    for m in SERIAL.finditer(text):
        depth, i = 0, m.end() - 1
        while i < len(text):
            if text[i] == "(":
                depth += 1
            elif text[i] == ")":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        spans.append((m.start(), i))
    return spans


def main() -> int:
    files = sorted(UI.glob("*.cpp"))
    if not files:
        print("error: found no UI sources - this check is checking nothing",
              file=sys.stderr)
        return 2

    problems = []
    scanned = 0

    for path in files:
        rel = path.relative_to(REPO).as_posix()
        text = path.read_text(encoding="utf-8")
        lines = text.splitlines()
        spans = serial_spans(text)
        scanned += 1

        for kind, lineno, literal in cxx_scan.scan(text):
            if kind != "string":
                continue
            if literal in ALLOWED or PROTOCOL.match(literal):
                continue
            # Format specifiers are placeholders, not text. What is left after
            # removing them is judged word by word rather than whole, so
            # "%d dBm" passes on the strength of dBm being a unit while
            # "Signal %d dBm" still fails on "Signal" - which is the point.
            residue = FORMAT.sub(" ", unescape(literal))
            words = WORD.findall(residue)
            if not words or all(w in ALLOWED for w in words):
                continue
            line = lines[lineno - 1] if lineno <= len(lines) else ""
            if INCLUDE.search(line):
                continue
            at = text.find(f'"{literal}"')
            if any(a <= at <= b for a, b in spans):
                continue
            problems.append(
                f"{rel}:{lineno}: the literal \"{literal[:60]}\" reaches the "
                "panel - drawn text must come from i18n::T(), or the screen "
                "half-forgets the language the user chose")

    if scanned == 0:
        print("error: scanned no UI sources - this check is checking nothing",
              file=sys.stderr)
        return 2

    for p in problems:
        print(f"error: {p}", file=sys.stderr)
    print(f"scanned {scanned} UI files, {len(problems)} violations")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
