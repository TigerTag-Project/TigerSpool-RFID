# Translation keys

**Rule.** English is the reference language. Every user-visible string goes through a translation table, never a literal; a new key is added in every language of its table in the same commit. The tables: `firmware/src/i18n.cpp` (screen, 11 languages, order of `enum Lang` in `firmware/src/i18n.h`), the `WT` table in `firmware/src/webcfg.cpp` (sign-in and account pages), `firmware/src/net/portal_page.h` (Wi-Fi page) and `installer/i18n.js` (web installer).

**Why.** 2026-09-04: a French device showed half its settings menu in English because labels were literals (`check-ui-translated.py`). 2026-10-19: the maintainer asked for the sign-in pages in the same eleven languages as Tiger Studio Manager, and for no key missing in any language.

**How.**
- A string you cannot translate yet: put the English text in that column and say so in the commit; never leave a cell empty (an empty string draws as nothing).
- Languages are appended at the end of `enum Lang`, never inserted, so a stored language keeps its index; reordering or removing one means bumping `LANG_SCHEMA` in `i18n.cpp`.
- A new character outside the compiled font draws as a blank box: see `ui-fonts.md`.
- Keep panel strings short: the screen is 240 px wide.
- Guards: `check-i18n.py` (screen and `WT` tables, column by column), `check-ui-translated.py` (no literal on the panel), `check-agent-rules.py` (portal and installer keys).
- Diagnostic API replies (`/api/tap`, `/api/memtest`) are not user text and stay English.
