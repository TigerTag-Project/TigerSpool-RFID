#!/usr/bin/env python3
"""The agent rules stay indexed, and what they name exists.

The rules for working here are one file each in .agents/rules/, and CLAUDE.md
holds the index - one line per rule, the line an agent acts on. Both halves
drift on their own:

  - A rule file nobody indexed is a rule nobody reads.
  - An index line pointing at a deleted file reads as a rule that still holds.
  - A path named in CLAUDE.md, AGENTS.md or a rule that no longer exists sends
    the next reader to nothing, and is the commonest lying comment: a file was
    moved and the prose was not.

The same guard checks the two translation tables nothing else watches - the
Wi-Fi portal page and the web installer - so a key added in English and
forgotten in another language fails here instead of showing English on a phone.
(The screen and the sign-in pages are checked by check-i18n.py.)

Gitignored paths (LOCAL.md, .pio/, _internal/) are not required to exist: a
fresh clone and CI do not have them. Globs and placeholders are skipped.
"""

import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
RULES = REPO / ".agents" / "rules"
ENTRY_POINTS = ["CLAUDE.md", "AGENTS.md"]
SPAN = re.compile(r"`([^`]+)`")
PATH_TOKEN = re.compile(r"^(?:[A-Za-z0-9_.][A-Za-z0-9_.-]*/[A-Za-z0-9_./-]*|[A-Za-z0-9_-]+\.md)$")


def ignored(rel: str) -> bool:
    return subprocess.run(["git", "check-ignore", "-q", rel], cwd=REPO).returncode == 0


def path_exists(rel: str, base: pathlib.Path) -> bool:
    rel = rel.rstrip("/")
    for root in (REPO, base, REPO / "firmware", REPO / "firmware" / "src"):
        if (root / rel).exists():
            return True
    return ignored(rel)


def check_paths(problems, doc: pathlib.Path):
    for lineno, line in enumerate(doc.read_text().splitlines(), 1):
        # Every word of a code span, so `bash scripts/verify.sh` is checked too.
        tokens = [t.strip(",;:()") for span in SPAN.findall(line) for t in span.split()]
        for rel in filter(PATH_TOKEN.match, tokens):
            if any(c in rel for c in "*<>{}") or "X.Y.Z" in rel or rel.startswith(("http", "~")):
                continue
            if rel.endswith(".local") or rel.startswith("/"):
                continue
            # "GPIO6/7" or "RX/TX" is not a path: check a slashed word only when
            # it starts at a real directory or ends in a file name.
            first, last = rel.split("/")[0], rel.rstrip("/").split("/")[-1]
            if "/" in rel and "." not in last and not path_exists(first, doc.parent):
                continue
            if not path_exists(rel, doc.parent):
                problems.append(f"{doc.relative_to(REPO)}:{lineno}: names `{rel}`, "
                                f"which does not exist")


def js_keys(block: str):
    return set(re.findall(r"(?:^|[,{]\s*)([A-Za-z_]\w*)\s*:", block, re.M))


def check_portal(problems):
    src = (REPO / "firmware/src/net/portal_page.h").read_text()
    m = re.search(r"var L=\{(.*?)\}\};", src, re.S)
    if not m:
        return 0
    langs = dict(re.findall(r"(?:^|\},)\s*(\w+):\{(.*?)(?=\}(?:,\s*\w+:\{|$))",
                            m.group(1) + "}", re.S))
    return compare(problems, "firmware/src/net/portal_page.h", langs)


def check_installer(problems):
    src = (REPO / "installer/i18n.js").read_text()
    blocks = re.findall(r'^(\w[\w-]*|"[\w-]+"):\s*\{(.*?)^\},?', src, re.S | re.M)
    langs = {k.strip('"'): v for k, v in blocks}
    return compare(problems, "installer/i18n.js", langs)


def compare(problems, name, langs):
    if "en" not in langs:
        return 0
    ref = js_keys(langs["en"])
    for lang, block in langs.items():
        keys = js_keys(block)
        for k in sorted(ref - keys):
            problems.append(f"{name}: '{lang}' has no '{k}' (English has it)")
        for k in sorted(keys - ref):
            problems.append(f"{name}: '{lang}' has '{k}', which English does not")
    return len(langs)


def main() -> int:
    problems = []
    rules = sorted(RULES.glob("*.md"))
    if not rules:
        print("error: no rule files in .agents/rules/", file=sys.stderr)
        return 2

    index = (REPO / "CLAUDE.md").read_text()
    indexed = set(re.findall(r"\.agents/rules/([\w-]+\.md)", index))
    if not indexed:
        print("error: CLAUDE.md has no rules index", file=sys.stderr)
        return 2
    for r in rules:
        if r.name not in indexed:
            problems.append(f".agents/rules/{r.name} is not in the index in CLAUDE.md")
    for name in sorted(indexed):
        if not (RULES / name).exists():
            problems.append(f"CLAUDE.md indexes .agents/rules/{name}, which does not exist")

    docs = [REPO / d for d in ENTRY_POINTS] + rules
    for d in docs:
        check_paths(problems, d)

    tables = check_portal(problems) and check_installer(problems)
    if not tables:
        print("error: could not read the portal or installer translation tables",
              file=sys.stderr)
        return 2

    for p in problems:
        print(f"error: {p}", file=sys.stderr)
    print(f"checked {len(rules)} rules, {len(docs)} documents and 2 web translation "
          f"tables, {len(problems)} violations")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
