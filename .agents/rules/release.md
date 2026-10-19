# Releasing

**Rule.** A release is a human decision. Bump with `bash scripts/bump-version.sh X.Y.Z`, write the notes from `WORKLOG.md`, commit, tag `vX.Y.Z` (equal to `TIGERSPOOL_FW_VERSION` in `firmware/include/version.h`), push the branch, then the tag. Each of commit, tag and push needs asking for (`git-commit-push.md`).

**Why.** The release workflow publishes the binaries and the OTA manifest from the tag; a wrong tag ships to every device that checks for updates.

**How.**
- `bump-version.sh` edits the macro and moves `[Unreleased]` in `CHANGELOG.md` under the new version; it never commits, tags or pushes.
- Semantic versioning: a fix is a patch, a new language or feature is a minor.
- After the push: watch the `release` run, then confirm `https://tigertag-project.github.io/TigerSpool-RFID/version.json` names the new version and its `firmware_sha256` matches the published `.bin`.
- Tag/macro mismatch: `git ls-remote --tags origin` says whether the tag is public. Not public: delete and re-tag. Public: leave it and release the next version.
