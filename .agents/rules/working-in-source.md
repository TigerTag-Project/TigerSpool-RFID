# Working in the source

**Rule.** Read, grep, slice, smallest edit: `CODEMAP.md` says which file owns the thing; grep for the symbol to get real line numbers; read the slice around it including its callers; make the smallest edit; verify.

**Why.** Reading whole large files to change one function wastes the session's context and misses the callers.

**How.**
- If `CODEMAP.md` and the grep disagree, the map is stale: say so and fix it.
- Vendored code in `firmware/lib/PN532/` is third party: leave it alone.
