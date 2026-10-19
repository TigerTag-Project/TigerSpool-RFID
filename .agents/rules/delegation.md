# Delegation

**Rule.** Delegate only to keep search cost out of the main context: `locator` answers where code is, `single-edit` makes one fully specified change (`.claude/agents/`). Never delegate a change spanning two subsystems, a new subsystem, a debugging session, the version decision, the guards, the final read-through or any git operation. No parallel fan-out by default.

**Why.** Each agent re-reads the same large files with its own system prompt; fanning out multiplies the cost without improving the result.

**How.**
- If a block of work is too large, hand the whole block to exactly one agent with a self-sufficient prompt.
