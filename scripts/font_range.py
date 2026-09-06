"""Which characters the compiled UI font can actually draw.

One source of truth for a fact that two places need: `firmware/tools/tigertag_db/gen_db.py`
validating its input, and `scripts/check-ui-fonts.py` validating every string
that reaches the panel. Writing the range out twice means that the day a Latin
subset font ships, one of them is wrong and nothing says so.

The range is not our choice — it is recorded in the generated font source that
LVGL ships, on the `Opts:` line the font converter writes:

    Opts: ... --font Montserrat-Medium.ttf -r 0x20-0x7F,0xB0,0x2022 ...

That source lives under firmware/.pio/libdeps, which a fresh clone does not
have. So scripts/gen-font-range.py extracts it once into font_range.json and
this reads the committed file. The fact does not go stale unwatched:
scripts/check-generated.py re-runs the generator and fails if the committed
value disagrees, so CI proves it on every push while the pre-commit hook needs
no build tree.

LVGL draws a character it has no glyph for as a blank box and logs nothing,
which is why this needs checking at all: nothing fails until a user sees it.
"""

import json
import pathlib


class FontRangeUnavailable(RuntimeError):
    """The font source could not be found, so the range is unknown.

    Raised rather than guessed. A checker that assumes a range it did not read
    passes on characters the panel cannot draw, which is the failure it exists
    to prevent.
    """


def compiled_range(repo_root: pathlib.Path):
    """Return (codepoints, human_description) for the compiled UI font."""
    committed = pathlib.Path(__file__).resolve().parent / "font_range.json"
    if not committed.exists():
        raise FontRangeUnavailable(
            f"{committed.name} is missing - regenerate it with "
            "'python3 scripts/gen-font-range.py'")

    spec = json.loads(committed.read_text()).get("spec", "")
    if not spec:
        raise FontRangeUnavailable(f"{committed.name} carries no range")

    allowed: set[int] = set()
    for part in spec.split(","):
        if "-" in part:
            lo, hi = part.split("-", 1)
            allowed.update(range(int(lo, 16), int(hi, 16) + 1))
        else:
            allowed.add(int(part, 16))
    return allowed, spec


def offending(text: str, allowed: set[int]):
    """Characters in `text` the font cannot draw, as (index, codepoint)."""
    return [(i, ord(c)) for i, c in enumerate(text) if ord(c) not in allowed]


def describe(codepoint: int) -> str:
    import unicodedata
    try:
        name = unicodedata.name(chr(codepoint))
    except ValueError:
        name = "unnamed control character"
    return f"U+{codepoint:04X} {name}"
