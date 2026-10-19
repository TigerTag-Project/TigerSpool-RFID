# How these rules are kept

**Rule.** One rule per file in `.agents/rules/`, each with Rule, Why and How. `CLAUDE.md` holds the index: one self-sufficient line per rule pointing at its file. A changed rule is rewritten in place - no "superseded" notes, git keeps the history.

**Why.** 2026-10-19, maintainer order: one rule = one file, an index, zero lying comments, and an automatic guard.

**How.**
- `scripts/check-agent-rules.py` (run by `verify.sh`) fails if a rule file is missing from the index, the index names a missing file, or a path cited in `CLAUDE.md`, `AGENTS.md` or a rule does not exist.
- Adding a rule: write the file, add its index line, run `bash scripts/verify.sh --quick`.
- `AGENTS.md` and other entry points link to the index; they never copy a rule.
