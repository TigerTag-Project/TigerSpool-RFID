# Docs and comments follow behaviour

**Rule.** Every change of behaviour corrects the comment, doc or rule that describes it in the same commit. No comment, doc or rule may contradict the code, the config or a newer rule; when a rule turns out to be wrong, change it in the same session.

**Why.** 2026-10-19: the maintainer asked for zero lying comments after an audit found several (a header saying Chinese was absent, a contributing guide saying the firmware was not written, a broken `verify.sh --fix` path).

**How.**
- Before committing, grep for what the change makes false: language counts, file paths, commands, "never X".
- A historical document says so at its top instead of reading as current.
- When you cannot tell which of two statements is true, ask the maintainer rather than guessing.
