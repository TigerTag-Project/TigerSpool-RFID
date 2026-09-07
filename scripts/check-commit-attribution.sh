#!/usr/bin/env bash
# Refuse a push whose commit messages credit an AI as an author.
#
#     bash scripts/check-commit-attribution.sh [BASE]
#
# The commit-msg hook is the first line and the cheap one: it stops the line
# being written at all. This is the second, for a clone that never ran
# install-hooks.sh and for a tool that was told mid-session to override the
# repository's rules - which is exactly how 43 commits carrying
# "Co-Authored-By: Claude" reached main under a CLAUDE.md that forbade them.
#
# It checks only the commits a push ADDS. Rewriting the ones already published
# would mean re-pointing thirty-six release tags whose binaries are installed on
# people's devices, so the existing history is a decision for the repository
# owner, not something a guard gets to force.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

PATTERN='^[[:space:]]*(co-authored-by|signed-off-by|assisted-by)[[:space:]]*:.*(claude|anthropic|copilot|chatgpt|gpt-[0-9]|gemini|cursor|codex)'

# What this push adds, or the last commit outside CI.
BASE="${1:-${GITHUB_EVENT_BEFORE:-}}"
if [ -z "$BASE" ] || [ "$BASE" = "0000000000000000000000000000000000000000" ] \
   || ! git cat-file -e "$BASE^{commit}" 2>/dev/null; then
    RANGE="HEAD~1..HEAD"
    git cat-file -e "HEAD~1^{commit}" 2>/dev/null || RANGE="HEAD"
else
    RANGE="$BASE..HEAD"
fi

bad=0
for sha in $(git rev-list "$RANGE"); do
    if git log -1 --format=%B "$sha" | grep -qiE "$PATTERN"; then
        echo "error: $(git log -1 --format='%h %s' "$sha")" >&2
        git log -1 --format=%B "$sha" | grep -inE "$PATTERN" | sed 's/^/    /' >&2
        bad=1
    fi
done

if [ "$bad" = 1 ]; then
    echo >&2
    echo "No AI attribution anywhere - not in commit messages, not in pull" >&2
    echo "request bodies, not in comments, not in contributor lists. It is a" >&2
    echo "repository rule and tooling does not get a vote on it; see the hard" >&2
    echo "rules in CLAUDE.md." >&2
    echo >&2
    echo "Run 'bash scripts/install-hooks.sh' so this is caught before the" >&2
    echo "commit exists rather than after it is pushed." >&2
    exit 1
fi
echo "checked $(git rev-list --count "$RANGE") new commit message(s), 0 violations"
