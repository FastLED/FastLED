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

## Background Watchers — The Silent-Stall Trap

`until grep -qE "..." <log>; do sleep N; done` armed via Bash + `run_in_background: true` is the workhorse for "wait for some marker in a log." It silently stalled for **13 hours** in one observed session. Three independent failures had to compound for that to happen — and any one of them can happen again. Defend against all three.

**Failure 1: regex drift.** A watcher built from a remembered prior-run log line will silently mismatch the current run when wording changes by one word. Observed: `BUILD\+FLASH FAIL Process` (from one run) vs `BUILD+FLASH FAIL timeout` (current run) — no match, loop polls forever.
- Mitigation: **before arming any long watcher, run the grep against the live log file once** to confirm at least the success path matches. Five-second check, 13-hour saving.
- Mitigation: **build the alternation from the actual log structure**, not from memory. Use the wrapper script's source if available (`grep -nR "FAIL\|completed" path/to/wrapper.py`).
- Cover failure modes too, not just success. If your filter only matches `✅ passed`, a crash or hang produces zero events and the silence is indistinguishable from "still running."

**Failure 2: `timeout` is silently ignored for background bash tasks.** Verified empirically: `Bash({command: "sleep 70", timeout: 5000, run_in_background: true})` ran the full 70 s and exited 0 — the 5 s `timeout` was a no-op. Background tasks run until natural exit; the harness does not enforce the `timeout` parameter.
- Mitigation: **wrap with the POSIX `timeout` binary inside the command itself** — `timeout 1800 bash -c 'until ...; do sleep N; done'`. That kill switch fires from the OS and the task completes (with exit 124), triggering a task-notification.
- Mitigation: emit a sentinel on exit so you can distinguish "loop matched" from "OS killed it" — e.g. `... && echo "MATCHED" || echo "TIMEOUT $(date)"`.

**Failure 3: no model heartbeat while a background task is still running.** The harness re-invokes the model only on (a) user prompts, (b) task-notifications fired when a task **completes**, or (c) ScheduleWakeup deadlines. A still-running infinite-loop background task generates none of these — the model sits idle indefinitely waiting for a notification that will never come.
- Mitigation: **for any wait that may exceed 5–10 minutes, pair the watcher with a ScheduleWakeup heartbeat** (1200–1800 s) so the model is re-invoked and can read the log even if no task-completion event arrives. Don't rely solely on task-notification.
- Mitigation: outside a `/loop` skill turn there's no implicit re-entry — explicitly schedule one if the work crosses a goal boundary.

**Recovery when you suspect a stalled watcher.** Symptom: a background task ID hasn't notified for an unexpectedly long time. Diagnostics: `TaskOutput({task_id, block: false})` returns `<status>running</status>` indefinitely; the watched log file mtime is fresh but doesn't match your grep; pgrep/Get-Process shows the upstream producer has exited. Action: `TaskStop` the watcher, dump the actual log lines containing your terms (`grep -nE "BUILD|TEST|FAIL|ERROR" <log>`), compare against your regex, then re-arm with a corrected pattern + OS-level `timeout` wrapper + ScheduleWakeup heartbeat.

## Task Management

1. **Plan First**: Write plan to `agents/tasks/todo.md` with checkable items
2. **Verify Plan**: Check in before starting implementation
3. **Track Progress**: Mark items complete as you go
4. **Explain Changes**: High-level summary at each step
5. **Document Results**: Add review section to `agents/tasks/todo.md`
6. **Capture Lessons**: Update `agents/tasks/lessons.md` after corrections
