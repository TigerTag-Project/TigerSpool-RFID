#!/usr/bin/env python3
"""The translation tables are positional. Nothing in the compiler checks that.

firmware/src/i18n.cpp holds one row per string, all languages side by side, in
the column order of `enum Lang` in i18n.h. There is a static_assert, and it
checks the number of *rows* against the enum - nothing else.

So these all compile cleanly today:

  - A language block one column out of place. Every string in that language is
    silently wrong, in a language the person who edits it usually cannot read.
  - A row with one entry too few. The remainder is zero-filled by aggregate
    initialisation and a NULL pointer reaches lv_label_set_text.
  - An empty string, which draws as nothing at all and reads as a layout bug.
  - A /* S_KEY */ comment that no longer matches the row it labels, which sends
    the next reader to the wrong line.

Counting is not enough: order is the failure mode. This checks the comments
against the enum position by position.

There are two tables. The second is the ten-column set in webcfg.cpp used by
the legacy configuration page - a different axis from the device's eleven
languages, and checked on its own terms rather than against them.
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent


def enum_members(header: str, name: str, stop: str):
    """Members of `enum name`, in declaration order, excluding the count."""
    m = re.search(rf"enum\s+{name}[^{{]*\{{(.*?)\}}", header, re.S)
    if not m:
        return []
    body = re.sub(r"//[^\n]*", "", m.group(1))
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    out = []
    # Members are comma-separated, not line-separated: the web table's enum
    # packs five to a line, and reading one per line silently truncates it.
    for item in body.split(","):
        ident = re.match(r"\s*([A-Za-z_]\w*)", item)
        if ident and ident.group(1) != stop:
            out.append(ident.group(1))
    return out


def literals(block: str):
    """String literals in a table row, adjacent-concatenation collapsed."""
    return re.findall(r'"((?:[^"\\]|\\.)*)"', block)


def check_table(problems, label, path, rows, keys, width, width_label):
    if not rows:
        problems.append(f"{path}: found no rows in the {label} table - "
                        "its shape has changed and this check is checking nothing")
        return
    if len(rows) != len(keys):
        problems.append(f"{path}: {label} has {len(rows)} rows but the enum "
                        f"declares {len(keys)} keys")

    for i, (key, body) in enumerate(rows):
        entries = literals(body)
        if len(entries) != width:
            problems.append(f"{path}: {label} row {key} has {len(entries)} "
                            f"entries, expected {width} ({width_label}). "
                            "A short row zero-fills and reaches the UI as NULL.")
        for j, text in enumerate(entries):
            if text == "":
                problems.append(f"{path}: {label} row {key}, column {j} is empty")
        if i < len(keys) and key != keys[i]:
            problems.append(f"{path}: {label} row {i} is labelled {key} but the "
                            f"enum has {keys[i]} in that position - the table and "
                            "the enum have come apart, and every row after this "
                            "one is mislabelled")


def main() -> int:
    problems = []
    checked = 0

    # --- the device table: every string x every language in enum Lang ------
    h = (REPO / "firmware/src/i18n.h").read_text()
    c = (REPO / "firmware/src/i18n.cpp").read_text()
    langs = enum_members(h, "Lang", "LANG_N")
    keys = enum_members(h, "StrId", "S_COUNT")
    rows = re.findall(r"/\*\s*(S_\w+)\s*\*/\s*\{\{(.*?)\}\}", c, re.S)
    if not langs:
        problems.append("firmware/src/i18n.h: could not read enum Lang")
    else:
        checked += 1
        check_table(problems, "STR", "firmware/src/i18n.cpp", rows, keys,
                    len(langs), ", ".join(langs))

    # --- the legacy web table: a different, ten-column axis ------------------
    w = (REPO / "firmware/src/webcfg.cpp").read_text()
    wkeys = enum_members(w, "", "W_N") or []
    wkeys = [k for k in wkeys if k.startswith("W_")]
    m = re.search(r"WT\[W_N\]\[(\d+)\]", w)
    if m and wkeys:
        checked += 1
        # The lookahead has to step over line comments between rows. Without
        # that, a row followed by a `//` note simply stopped matching, the
        # table came up one row short, and the checker blamed the FIRST row
        # after the comment for a drift that did not exist - which is the
        # worst kind of failure for a guard whose whole job is to say where
        # two lists came apart. A table nobody may annotate is a table nobody
        # annotates.
        wrows = re.findall(
            r"/\*\s*(W_\w+)\s*\*/\s*\{(.*?)\}\s*,?\s*(?:(?://[^\n]*\n)\s*)*(?=/\*|\};)",
            w, re.S)
        check_table(problems, "WT", "firmware/src/webcfg.cpp", wrows, wkeys,
                    int(m.group(1)), "the legacy page's ten columns")

    if checked == 0:
        print("error: found no translation tables to check", file=sys.stderr)
        return 2

    for p in problems:
        print(f"error: {p}", file=sys.stderr)
    print(f"checked {checked} translation table(s), {len(problems)} violations")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
