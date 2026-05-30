# Workflow Orchestration

Guidelines for how agents should approach work in the FastLED project.

## 1. Plan Mode Default
- Enter plan mode for ANY non-trivial task (3+ steps or architectural decisions)
- If something goes sideways, STOP and re-plan immediately — don't keep pushing
- Use plan mode for verification steps, not just building
- Write detailed specs upfront to reduce ambiguity

## 2. Subagent Strategy
- Use subagents liberally to keep main context window clean
- Offload research, exploration, and parallel analysis to subagents
- For complex problems, throw more compute at it via subagents
- One task per subagent for focused execution
- **Orchestrated sub-agents must NOT each run `bash test`** — running the full test suite N times across N steps is pure waste. Per-step sub-agents run `bash lint` only; the orchestrator runs `bash test --cpp` ONCE at the end (the `Stop` hook does this for free when files changed). The orchestrator MUST explicitly suspend the test mandate in the dispatch prompt. Full rule: `agents/tests.md` → "Orchestrated Sub-Agent Carve-Out".

## 3. Self-Improvement Loop
- After ANY correction from the user: update `agents/tasks/lessons.md` with the pattern
- Write rules for yourself that prevent the same mistake
- Ruthlessly iterate on these lessons until mistake rate drops
- Review lessons at session start for relevant project

## 4. Verification Before Done
- Never mark a task complete without proving it works
- Diff behavior between main and your changes when relevant
- Ask yourself: "Would a staff engineer approve this?"
- Run tests, check logs, demonstrate correctness

## 5. Demand Elegance (Balanced)
- For non-trivial changes: pause and ask "is there a more elegant way?"
- If a fix feels hacky: "Knowing everything I know now, implement the elegant solution"
- Skip this for simple, obvious fixes — don't over-engineer
- Challenge your own work before presenting it

## 6. Autonomous Bug Fixing
- When given a bug report: just fix it. Don't ask for hand-holding
- Point at logs, errors, failing tests — then resolve them
- Zero context switching required from the user
- Go fix failing CI tests without being told how

## CodeRabbit Review Loop

When CodeRabbit posts review comments on a PR, run `/address-reviews` before any `gh pr merge`. The skill fetches open comments, classifies each, applies fixes for valid findings, and replies to threads. The pre-merge hook blocks merge while unresolved CodeRabbit threads remain. Security-critical findings are routed to a human — never auto-fixed. Max 3 iterations per PR; if CodeRabbit keeps asking after three rounds, stop and flag a human reviewer.

## CI Wait Loops

When polling CI on a PR (e.g. inside `/clud-pr-merge` or before `gh pr merge`):

- **FastLED's slowest platform builds take 20–30 minutes** on GitHub Actions runners — one observed: 26m7s on `build / build`. A `until <no pending>; do sleep 30; done` poll therefore sits silent for that long. Tell the user upfront ("this can take up to 30 minutes") so they don't read the silence as a hung shell.
- **The Claude Code harness blocks leading `sleep N; <cmd>` chains.** Use `until <condition>; do sleep N; done` (sleep inside the loop body), or pass `run_in_background: true` to the Bash tool. For waits longer than ~5 minutes, prefer `ScheduleWakeup` so the conversation isn't held open.
- **`gh pr checks` returns exit code 1 when any check failed**, even with `--json`. A polled background task that ends with exit 1 may still have produced complete output — read the output file before concluding the poll itself failed.
- **The `sync` check fails on every PR.** It belongs to the `project-automation` workflow on `pull_request_target` events; the failure is a 404 on "Generate GitHub App token" (the GitHub App isn't installed on the `FastLED` user/org). It does not run on `push` to master, so it won't appear in master's check-runs. Treat it as a known pre-existing infrastructure failure, never a regression.

## Task Management

1. **Plan First**: Write plan to `agents/tasks/todo.md` with checkable items
2. **Verify Plan**: Check in before starting implementation
3. **Track Progress**: Mark items complete as you go
4. **Explain Changes**: High-level summary at each step
5. **Document Results**: Add review section to `agents/tasks/todo.md`
6. **Capture Lessons**: Update `agents/tasks/lessons.md` after corrections
