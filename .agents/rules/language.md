# Language

**Rule.** Everything committed is English: code, comments, identifiers, commit messages, documentation, log lines and user-visible source strings. The conversation with the maintainer may be in French; the repository is not a conversation.

**Why.** Settled from the first commit; `check-text-english.py` was added on 2026-09-03 after French comments and strings kept slipping in.

**How.**
- Translations of user-visible text live in the translation tables (see `i18n-keys.md`), never as French literals in code.
- `_internal/` holds French working notes and is gitignored; it is the only place French belongs on disk.
- If `check-text-english.py` flags a word, translate it. Never add the word to the guard's list to make it pass.
