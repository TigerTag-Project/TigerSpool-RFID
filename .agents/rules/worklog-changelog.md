# Worklog, changelog and the pre-commit sweep

**Rule.** Append to `WORKLOG.md` the moment a change is done; `CHANGELOG.md` is written from it at release. Before every commit, sweep: every README, the worklog, i18n (every new or changed string in every language), `CODEMAP.md`, `CLAUDE.md` and the rules, `docs/ROADMAP.md`, then `bash scripts/verify.sh`.

**Why.** 2026-09-10, maintainer order: check every README before each commit, update the changelog each time, check i18n and every other important element, and the CODEMAP too - the README had drifted.

**How.**
- The worklog describes the end state, not the journey; a reverted change disappears from it.
- Say in the report which docs were touched or checked.
