# Smallest diff

**Rule.** Make the smallest edit that does the job and match the surrounding naming, idiom and comment density. A comment states the reason, not a restatement of the code.

**Why.** Opportunistic cleanup and drive-by reformatting make a diff unreviewable and hide the real change.

**How.**
- Mention what you noticed but did not touch, instead of fixing it in passing.
- Do not read a 900-line file in full to change one function (`working-in-source.md`).
