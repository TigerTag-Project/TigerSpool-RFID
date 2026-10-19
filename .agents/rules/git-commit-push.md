# Commit, tag and push

**Rule.** Never commit, tag or push unless the maintainer asks, each time. Committing, tagging and pushing are three separate permissions; none implies the next. A pushed `v*` tag is irreversible: the release workflow acts on it.

**Why.** Repository rule since the bootstrap. A tag cannot be taken back once the release workflow has published from it.

**How.**
- Ask for the commit, then for the tag, then for the push, unless the request names them.
- Merging a pull request is a push to `main`: review it first and have the maintainer approve the merge.
- Never `git push --force` to `main`, never re-point a published tag. If a published tag disagrees with the version macro, release the next version instead (`release.md`).
- Interactive git (`rebase -i`, `add -i`) is not available in agent sessions.
