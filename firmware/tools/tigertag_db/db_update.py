#!/usr/bin/env python3
"""Refresh the TigerTag reference tables beside this file from the public API.

    python3 firmware/tools/tigertag_db/db_update.py          # only what changed
    python3 firmware/tools/tigertag_db/db_update.py --force  # everything

Then it runs gen_db.py, so the committed header and the JSON never disagree.

WHY THIS EXISTS. The tables were copied in by hand from another checkout. That
works exactly once: new filaments, brands and finishes are added upstream every
few weeks, and a device whose tables are behind shows "#37" where a person
expects "Matt" - or "brand#48804" on a spool whose brand has existed for
months. Nothing fails, nothing logs, and the tag is fine; the firmware simply
does not know the word. A hand copy has no way to tell you it is stale.

WHAT IT DOES NOT DO. It does not commit. A reference table changing is a change
to what the product says, and that is a decision somebody makes on purpose -
run it, read the diff, decide. It also never touches the generated header
directly: that is gen_db.py's output and hand edits there revert silently.

WHAT IT REFUSES. A 200 is not proof of a good payload. An API mid-migration can
answer `[]`, a soft failure can answer an object, and a captive proxy can answer
HTML - all of which would otherwise overwrite live reference data with rubbish.
The bar is crude and absolute: a JSON array, and not empty. Files are replaced
atomically, so a killed process leaves the old file whole rather than half a
new one.

The endpoints and the last_update handshake are the ones Tiger Studio Manager
uses; this is a second reader of the same public API, not a second source of
truth.
"""

import argparse
import json
import os
import pathlib
import subprocess
import sys
import urllib.error
import urllib.request

API_BASE = "https://api.tigertag.io/api:tigertag"
HTTP_TIMEOUT = 30

HERE = pathlib.Path(__file__).resolve().parent
SRC = HERE                       # the tables live next to their two tools
LAST_UPDATE = SRC / "last_update.json"

# last_update key -> (endpoint, filename, compiled into the firmware?)
#
# Every one of these is compiled into the firmware. The unit table needed one
# concession to get there: two of its labels use a superscript, which the panel
# has no glyph for, and gen_db.py flattens them to "m2" and "m3".
DATASETS = {
    "versions":           ("version/get/all",           "id_version.json",      True),
    "types":              ("type/get/all",              "id_type.json",         True),
    "brands":             ("brand/get/all",             "id_brand.json",        True),
    "filament_diameters": ("diameter/filament/get/all", "id_diameter.json",     True),
    "filament_materials": ("material/get/all",          "id_material.json",     True),
    "aspects":            ("aspect/get/all",            "id_aspect.json",       True),
    "measure_units":      ("measure_unit/get/all",      "id_measure_unit.json", True),
}


def get_json(url):
    req = urllib.request.Request(url, headers={"Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=HTTP_TIMEOUT) as r:
        return json.loads(r.read().decode("utf-8"))


def write_atomic(path: pathlib.Path, text: str):
    """Write via a temp file and a rename.

    open(path, "w") truncates BEFORE writing, so a crash between those two
    moments leaves a truncated file that nothing can parse. os.replace is
    atomic, so the file on disk is either entirely the old one or entirely the
    new one.
    """
    tmp = path.with_suffix(path.suffix + ".tmp")
    try:
        tmp.write_text(text, encoding="utf-8")
        os.replace(tmp, path)
    finally:
        if tmp.exists():
            tmp.unlink(missing_ok=True)


def fetch(endpoint, filename):
    """Download one dataset, and refuse anything that would poison the tree.

    A 200 is not proof of a good payload. An API mid-migration answers `[]`, a
    soft failure answers an object, a captive portal answers HTML, and a
    truncated transfer answers half an array - each of which would otherwise
    replace live reference data with something the firmware cannot use. Every
    check below is cheap and absolute, and each one has a failure it prevents.
    """
    data = get_json(f"{API_BASE}/{endpoint}")

    if not isinstance(data, list):
        raise RuntimeError(
            f"{filename}: expected a JSON array, got {type(data).__name__}"
            " - refusing to overwrite")
    if not data:
        raise RuntimeError(f"{filename}: the API returned an EMPTY array"
                           " - refusing to overwrite")
    # Every row must be an object carrying an integer id and at least one of the
    # label fields the generator reads. A list of strings, or of objects with a
    # renamed key, parses as JSON and then fails in gen_db.py with a KeyError
    # that names nothing useful - here it names the file and the row.
    for i, row in enumerate(data):
        if not isinstance(row, dict):
            raise RuntimeError(f"{filename}: row {i} is {type(row).__name__},"
                               " not an object - refusing to overwrite")
        if "id" not in row:
            raise RuntimeError(f"{filename}: row {i} has no 'id'"
                               " - refusing to overwrite")
        try:
            int(row["id"])
        except (TypeError, ValueError):
            raise RuntimeError(f"{filename}: row {i} has a non-numeric id"
                               f" {row['id']!r} - refusing to overwrite")
        if not any(k in row for k in ("label", "name", "version")):
            raise RuntimeError(f"{filename}: row {i} has no label/name field"
                               " - refusing to overwrite")

    text = json.dumps(data, ensure_ascii=False, indent=2) + "\n"
    # Parse back what is about to be written. It costs microseconds and it is
    # the only check that covers the serializer itself.
    json.loads(text)

    write_atomic(SRC / filename, text)

    # And read the file back off the disk. write_atomic renames a fully written
    # temp file, so this should never fail - which is exactly why it is worth
    # asserting: if it ever does, the tree is broken now, in front of somebody,
    # rather than at the next build in front of somebody else.
    try:
        json.loads((SRC / filename).read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError) as e:
        raise RuntimeError(f"{filename}: written file does not read back: {e}")

    return len(data)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--force", action="store_true",
                    help="download every dataset, ignoring last_update")
    args = ap.parse_args()

    try:
        remote = get_json(f"{API_BASE}/all/last_update")
    except (urllib.error.URLError, OSError, ValueError) as e:
        print(f"error: cannot reach {API_BASE}: {e}", file=sys.stderr)
        return 1

    local = {}
    if LAST_UPDATE.exists():
        try:
            local = json.loads(LAST_UPDATE.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            local = {}

    changed = []
    for key, (endpoint, filename, compiled) in DATASETS.items():
        stale = args.force or remote.get(key) != local.get(key) \
            or not (SRC / filename).exists()
        if not stale:
            print(f"  {filename:24s} up to date")
            continue
        try:
            n = fetch(endpoint, filename)
        except (urllib.error.URLError, OSError, ValueError, RuntimeError) as e:
            print(f"error: {e}", file=sys.stderr)
            return 1
        print(f"  {filename:24s} updated, {n} entries"
              f"{'' if compiled else '   (not compiled in)'}")
        changed.append(filename)

    # last_update.json is the whole point of the handshake, and it is written
    # LAST and only after every download has succeeded. It records what is on
    # disk; stamping it earlier would make the next run skip a dataset it never
    # actually fetched, and the tree would sit one version behind for ever with
    # nothing to say so. It is committed alongside the tables for the same
    # reason - a clone with no stamp re-downloads everything, which is correct
    # but wasteful, and a clone with a WRONG stamp downloads nothing.
    write_atomic(LAST_UPDATE, json.dumps(remote, ensure_ascii=False, indent=2) + "\n")

    if not changed:
        print("nothing changed")
        return 0

    print("\nregenerating the header...")
    return subprocess.call([sys.executable, str(HERE / "gen_db.py")])


if __name__ == "__main__":
    sys.exit(main())
