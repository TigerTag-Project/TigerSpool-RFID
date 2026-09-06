#!/usr/bin/env python3
"""Generate firmware/include/tigertag_db.h from the TigerTag reference data.

    python3 firmware/tools/tigertag_db/gen_db.py

Reads the id_*.json reference tables beside it and writes the id-to-label tables
the reader uses, so a scan needs no network.

Every field the chip stores as a number needs one of these, or the NFC tester
shows an id where a person expects a word. Aspect, type and diameter are small
enough to carry whole; the catalogue is not, and is deliberately absent.

The output is verified: scripts/check-generated.py re-runs this and fails if the
committed header differs. That is what makes the header's own "do NOT edit by
hand" banner true rather than aspirational.

Labels from this data are drawn on the panel, so the input is validated against
what the compiled font can actually draw. See scripts/font_range.py — the
allowed set is read from the font, never restated here, so it widens by itself
the day a Latin subset font ships.
"""

import json
import os
import pathlib
import sys

# The generator lives WITH its data: one directory holds the reference tables,
# the script that downloads them and the script that compiles them. Splitting
# the two meant a reader had to know both places to understand either.
SRC = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(SRC)))

# The compiled header lives where the compiler looks for it: platformio.ini
# passes -I include, which resolves to firmware/include. Writing anywhere else
# produces a second copy that nothing builds, while the built one goes stale.
OUT = os.path.join(REPO, "firmware", "include", "tigertag_db.h")

sys.path.insert(0, os.path.join(REPO, "scripts"))
import font_range  # noqa: E402


# The one character that is repaired rather than rejected.
#
# Two brand names arrive from the API with a NO-BREAK SPACE between the words -
# "Duramic\u00a03D", "Filament\u00a0PM" - which is a copy-paste artefact, not a
# name. Rejecting them stops the build over an upstream typo nobody here can fix
# before the next download puts it straight back; drawing them puts a blank box
# on the panel in the middle of a brand. Turning it into the ordinary space it
# was meant to be changes nothing a reader can see, and the warning below keeps
# the defect visible instead of quietly absorbing it.
#
# Nothing else is repaired. Every other character the font cannot draw is a real
# question about what the panel should show, and guessing at those is how a
# product ends up with silent mojibake.
NBSP = "\u00a0"

# Superscripts in unit symbols. "m³" is the correct typography and the panel has
# no glyph for it; "m3" is what everybody types when they cannot type the real
# one, and it is unambiguous. Rewriting it is a rendering decision, not a change
# of meaning - unlike, say, dropping an accent from a brand name, which changes
# what the name is. Reported like the no-break space, so it stays visible.
SUPERSCRIPT = {"\u00b2": "2", "\u00b3": "3"}


def normalise(label, ident, source, repaired):
    if NBSP in label:
        repaired.append(f"  {source} id {ident}: {label!r} -> no-break space replaced")
        label = label.replace(NBSP, " ")
    for sup, plain in SUPERSCRIPT.items():
        if sup in label:
            repaired.append(f"  {source} id {ident}: {label!r} -> superscript flattened")
            label = label.replace(sup, plain)
    return label


def load(name):
    with open(os.path.join(SRC, name), encoding="utf-8") as f:
        return json.load(f)


def validate(entries, source):
    """Reject labels the panel cannot render, naming the entry and the character.

    esc() below escapes backslash and double quote and nothing else, so anything
    else in the JSON goes straight into a C string literal. A newline breaks the
    build, which is safe. A non-breaking space, a NUL byte or a bidirectional
    override all compile - and produce a blank glyph on the panel, a silently
    truncated string, or source that displays to a reviewer in the wrong order.
    The dangerous inputs are exactly the ones that compile, so they are stopped
    here rather than downstream.
    """
    try:
        allowed, spec = font_range.compiled_range(pathlib.Path(REPO))
    except font_range.FontRangeUnavailable as e:
        # Never guess the range: a validator that assumes a font it did not
        # read accepts characters the panel cannot draw, which is the exact
        # failure it exists to prevent.
        raise SystemExit(f"error: cannot validate labels: {e}")
    problems = []
    for ident, label in entries:
        for _, cp in font_range.offending(label, allowed):
            problems.append(
                f"  {source} id {ident}: {label!r} contains "
                f"{font_range.describe(cp)}, which lv_font_montserrat "
                f"(range {spec}) cannot draw"
            )
    return problems


def main():
    repaired = []

    def rows(filename, key):
        out = []
        for e in load(filename):
            ident = int(e["id"])
            out.append((ident, normalise(str(e[key]), ident, filename, repaired)))
        return out

    materials = rows("id_material.json", "label")
    brands = rows("id_brand.json", "name")
    aspects = rows("id_aspect.json", "label")
    types = rows("id_type.json", "label")
    diameters = rows("id_diameter.json", "label")
    units = rows("id_measure_unit.json", "label")
    # The protocol id on page 0x04. Its "name" is what identifies the tag
    # family - TigerTag, TigerTag+, uninitialised - and it is a u32, unlike
    # every other id here, so it gets a table of its own.
    versions = rows("id_version.json", "name")

    if repaired:
        print("note: reference labels rewritten for the panel:", file=sys.stderr)
        print("\n".join(repaired), file=sys.stderr)
        print("  a no-break space is an upstream defect and should be fixed in\n"
              "  the TigerTag database; a flattened superscript is this panel's\n"
              "  font and nothing to fix.", file=sys.stderr)

    problems = (validate(materials, "id_material.json")
                + validate(brands, "id_brand.json")
                + validate(aspects, "id_aspect.json")
                + validate(types, "id_type.json")
                + validate(diameters, "id_diameter.json")
                + validate(units, "id_measure_unit.json")
                + validate(versions, "id_version.json"))
    if problems:
        print("error: reference data contains characters the UI font cannot draw:",
              file=sys.stderr)
        print("\n".join(problems), file=sys.stderr)
        print("\nfix the JSON, not the generated header.", file=sys.stderr)
        return 1

    for table in (materials, brands, aspects, types, diameters, units, versions):
        table.sort(key=lambda x: x[0])

    # The public keys, straight from id_version.json and keyed by the same id.
    # A signature can only be checked against the key for the tag family that
    # signed it, so the two travel together or neither is any use.
    keys = {int(v["id"]): str(v.get("public_key") or "") for v in load("id_version.json")}

    def esc(s):
        return (s.replace("\\", "\\\\").replace('"', '\\"')
                 .replace("\n", "\\n").replace("\r", ""))

    with open(OUT, "w", encoding="utf-8") as f:
        f.write("// GENERATED by gen_db.py - do NOT edit by hand.\n")
        f.write("// TigerTag reference tables (id -> label), ordered by id.\n")
        f.write("#pragma once\n#include <Arduino.h>\n\n")
        f.write("struct TTEntry { uint16_t id; const char* label; };\n")
        f.write("struct TTEntry32 { uint32_t id; const char* label; };\n")
        f.write("struct TTKey { uint32_t id; const char* pem; };\n\n")

        f.write("static const TTEntry32 TT_VERSIONS[] = {\n")
        for ident, label in versions:
            f.write(f'  {{ {ident}u, "{esc(label)}" }},\n')
        f.write("};\n")
        f.write(f"static const size_t TT_VERSIONS_N = {len(versions)};\n\n")

        f.write("static const TTKey TT_KEYS[] = {\n")
        for ident, _ in versions:
            pem = keys.get(ident, "")
            f.write(f'  {{ {ident}u, "{esc(pem)}" }},\n')
        f.write("};\n")
        f.write(f"static const size_t TT_KEYS_N = {len(versions)};\n\n")

        for name, rows in (("TT_MATERIALS", materials), ("TT_BRANDS", brands),
                           ("TT_ASPECTS", aspects), ("TT_TYPES", types),
                           ("TT_DIAMETERS", diameters), ("TT_UNITS", units)):
            f.write(f"static const TTEntry {name}[] = {{\n")
            for ident, label in rows:
                f.write(f'  {{ {ident}, "{esc(label)}" }},\n')
            f.write("};\n")
            f.write(f"static const size_t {name}_N = {len(rows)};\n\n")

        f.write("""static inline const char* tt_lookup(const TTEntry* t, size_t n, uint16_t id) {
  size_t lo = 0, hi = n;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    if (t[mid].id == id) return t[mid].label;
    if (t[mid].id < id) lo = mid + 1; else hi = mid;
  }
  return nullptr;
}
static inline const char* tt_lookup32(const TTEntry32* t, size_t n, uint32_t id) {
  size_t lo = 0, hi = n;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    if (t[mid].id == id) return t[mid].label;
    if (t[mid].id < id) lo = mid + 1; else hi = mid;
  }
  return nullptr;
}
static inline const char* tt_material(uint16_t id) { return tt_lookup(TT_MATERIALS,  TT_MATERIALS_N,  id); }
static inline const char* tt_brand(uint16_t id)    { return tt_lookup(TT_BRANDS,     TT_BRANDS_N,     id); }
static inline const char* tt_aspect(uint16_t id)   { return tt_lookup(TT_ASPECTS,    TT_ASPECTS_N,    id); }
static inline const char* tt_type(uint16_t id)     { return tt_lookup(TT_TYPES,      TT_TYPES_N,      id); }
static inline const char* tt_diameter(uint16_t id) { return tt_lookup(TT_DIAMETERS,  TT_DIAMETERS_N,  id); }
static inline const char* tt_unit(uint16_t id)     { return tt_lookup(TT_UNITS,      TT_UNITS_N,      id); }
static inline const char* tt_public_key(uint32_t id) {
  for (size_t i = 0; i < TT_KEYS_N; i++) if (TT_KEYS[i].id == id) return TT_KEYS[i].pem;
  return nullptr;
}
static inline const char* tt_version(uint32_t id)  { return tt_lookup32(TT_VERSIONS, TT_VERSIONS_N,   id); }
""")

    print(f"OK -> {OUT}   ({len(materials)} materials, {len(brands)} brands, "
          f"{len(aspects)} aspects, {len(types)} types, "
          f"{len(diameters)} diameters, {len(units)} units, "
          f"{len(versions)} versions)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
