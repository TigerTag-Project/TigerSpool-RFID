#!/usr/bin/env bash
# Generate the UI faces: Montserrat with accents, plus the symbols LVGL draws.
#
#     bash scripts/make-ui-font.sh
#
# WHY THIS EXISTS. LVGL's built-in Montserrat faces are compiled with
# `-r 0x20-0x7F,0xB0,0x2022` - ASCII, the degree sign and a bullet. Everything
# else the eight languages of this product need is missing: every French accent,
# every German umlaut, the Spanish tilde, the Polish ogonek. LVGL draws a
# character it has no glyph for as a blank box and logs nothing, so the
# translations were written without accents rather than shipped broken - which
# is a French interface that reads as if nobody proof-read it.
#
# So the faces are generated here instead, from the same two source fonts LVGL
# uses and with the same symbol list, over a range wide enough for the languages
# this device speaks.
#
# WHAT IS IN THE RANGE, and why each part.
#   0x20-0x7F    ASCII.
#   0xA1-0x17F   Latin-1 Supplement from the inverted exclamation on, plus Latin
#                Extended-A. That covers French, German, Spanish, Italian and
#                Portuguese entirely, and Polish - which needs Extended-A for
#                its ogonek and stroke letters, and would otherwise be the one
#                language still written wrong.
#                0xA0 is DELIBERATELY EXCLUDED. It is the no-break space, and
#                leaving it without a glyph is what makes one visible: a copy
#                and paste artefact in reference data shows up as a box instead
#                of hiding as an ordinary space. Two brand names arrived with
#                one and this is how they were found.
#   0x2022       The bullet, used in lists.
#
# The FontAwesome list is LVGL's own, verbatim, so every LV_SYMBOL_* keeps
# working - dropping one would be a missing icon nobody notices until a screen
# is opened. 61829 (0xF185, the sun) is appended: it is the Display row's icon,
# which used to need a whole second face of its own.
#
# The source fonts come from the installed LVGL package rather than a download:
# they are what LVGL itself generated from, so the glyphs are identical, and
# pinning the library pins them. Nothing font-shaped is committed.
#
# Licence: Montserrat is SIL OFL 1.1, the Font Awesome files are OFL 1.1 for the
# fonts and CC BY 4.0 for the artwork. Both are cited in THIRD_PARTY_LICENSES.md.
set -euo pipefail
cd "$(dirname "$0")/.."

SIZES="12 14 16 20 24"
TEXT_RANGE="0x20-0x7F,0xA1-0x17F,0x2022"

# LVGL's own symbol list, plus the sun.
SYMBOLS="61441,61448,61451,61452,61452,61453,61457,61459,61461,61465,61468,61473,61478,61479,61480,61502,61507,61512,61515,61516,61517,61521,61522,61523,61524,61543,61544,61550,61552,61553,61556,61559,61560,61561,61563,61587,61589,61636,61637,61639,61641,61664,61671,61674,61683,61724,61732,61787,61829,61931,62016,62017,62018,62019,62020,62087,62099,62212,62189,62810,63426,63650"

command -v npx >/dev/null || { echo "note: npx not on PATH - cannot regenerate here"; exit 3; }

FONTDIR="$(ls -d firmware/.pio/libdeps/*/lvgl/scripts/built_in_font 2>/dev/null | head -1 || true)"
if [ -z "$FONTDIR" ]; then
    # Distinctly 3, not 1: "I could not check" is a different answer from
    # "it does not match", and check-generated.py reports them differently.
    echo "note: LVGL package not installed - run 'pio pkg install' in firmware/" >&2
    exit 3
fi

TTF="$FONTDIR/Montserrat-Medium.ttf"
AWESOME="$FONTDIR/FontAwesome5-Solid+Brands+Regular.woff"

for S in $SIZES; do
    OUT="firmware/src/ui/font_ui_${S}.c"
    npx --yes lv_font_conv@1.5.3 \
        --no-compress --no-prefilter --bpp 4 --size "$S" \
        --font "$TTF" -r "$TEXT_RANGE" \
        --font "$AWESOME" -r "$SYMBOLS" \
        --format lvgl -o "$OUT" --force-fast-kern-format
    # lv_font_conv writes the absolute path of whatever it was handed into the
    # banner, which differs between machines and would make the generated file
    # fail its own regeneration check. Reduce it to the file name.
    python3 - "$OUT" <<'PY'
import pathlib, re, sys
p = pathlib.Path(sys.argv[1]); s = p.read_text()
s = re.sub(r"--font \S*/([^/\s]+)", r"--font \1", s)
s = re.sub(r"-o \S*/([^/\s]+)", r"-o \1", s)
p.write_text(s)
PY
    echo "OK -> $OUT"
done
