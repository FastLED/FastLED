# FastLED AI Agent Guidelines

## Read the Right File for Your Task

**By what you're doing:**

| Task | Read |
|------|------|
| Writing/editing C++ code | `agents/docs/cpp-standards.md` |
| Creating an API wrapper type | `agents/docs/cpp-standards.md` → "API Object Pattern" |
| Writing/editing Python code | `agents/docs/python-standards.md` |
| Editing meson.build files | `agents/docs/build-system.md` |
| Running tests, Docker, WASM, QEMU | `agents/docs/testing-commands.md` |
| Test-Driven Development (TDD) | Use `/tdd` or `/tdd-implement` skills |
| Hardware autoresearch / `bash autoresearch` | `agents/docs/hardware-autoresearch.md` |
| Debugging a C++ crash | `agents/docs/debugging.md` |
| Creating a new C++ linter | `agents/docs/linter-architecture.md` |
| Detailed command reference | `agents/docs/commands-reference.md` |
| Workflow and task management | `agents/docs/workflow.md` |

**By directory:** `src/`/`tests/` → `agents/docs/cpp-standards.md` | `ci/` → `agents/docs/python-standards.md`, `agents/ci.md` | `tests/` → `agents/tests.md` | `examples/` → `agents/examples.md` | `meson.build` → `agents/docs/build-system.md`

## Key Commands

**CRITICAL: Always use bash wrapper scripts (NOT direct Python invocation):**

- `bash test` / `bash test --cpp` / `bash test TestName` — Run tests
- `bash lint` — Code formatting/linting
- `bash compile wasm --examples Blink` — Compile example (WASM is default target)
- `bash compile <platform> --examples Blink` — Compile for specific hardware (only when explicitly requested)
- `bash autoresearch --parlio` — Live device testing (must specify driver)
- `bash profile <function>` — Performance profiling

**NEVER use:** `uv run python test.py` — use `bash test` or `uv run test.py`
**FORBIDDEN:** `--no-fingerprint` (use `bash test --clean`), bare `pio`/`platformio`, bare `meson`/`ninja`/`clang++`

See `agents/docs/commands-reference.md` for Docker, fbuild, WASM, profiling, example compilation, and override mechanism.
See `agents/docs/build-system.md` for full command execution rules and forbidden patterns.

## Core Rules (ALL AGENTS)

### Git and Code Publishing
- **Default mindset: finish the job.** Agents should not leave uncommitted changes dangling on `master`/`main`. If you made edits on `master`, the correct end-state is a feature branch + pushed PR — not a dirty working tree.
- **Feature branches — full autonomy, no user consent required.** Agents may freely create branches, commit, push, and open PRs against any branch that is NOT `master`/`main`. Do this proactively when work is complete.
- **`master`/`main` — extra caution required.**
  - NEVER commit directly to `master`/`main`.
  - NEVER push directly to `master`/`main`.
  - NEVER force-push to `master`/`main` (or any branch with an open PR others may be reviewing).
  - If changes exist on `master`, move them: `git checkout -b feat/<topic>` carries the working-tree changes to a feature branch, then commit + push + open a PR there.
- **Recovery pattern for uncommitted changes on `master`:**
  1. `git status` — confirm scope
  2. `git checkout -b <descriptive-branch>` — changes follow to new branch
  3. `git add <specific files>` + `git commit` (conventional commit format)
  4. `git push -u origin <branch>` and `gh pr create`
  5. `git status` again to confirm clean tree

### Hook Error Policy
- **ALWAYS stop and fix Write/Edit hook errors immediately** before writing the next file
- **IWYU errors may be deferred** when laying down multiple new files in a batch

### Test Failure Debug Policy
- **When ANY test fails in quick mode, you MUST immediately re-run it in debug mode**: `bash test <TestName> --debug`
- Debug mode enables ASAN/LSAN/UBSAN sanitizers that catch memory errors, undefined behavior, and leaks
- Quick mode failures without debug re-run are INCOMPLETE — the root cause is often only visible with sanitizers
- Do NOT attempt to fix the code based only on quick-mode output — always get debug output first

### Error Fixing Policy
- **Fix ALL encountered errors immediately**, even pre-existing ones unrelated to your current task

### Command Execution
- **Always use bash wrapper scripts** (`bash test`, `bash compile`, `bash lint`, `bash autoresearch`)
- **Stay in project root** — never `cd` to subdirectories
- **Python scripts**: Always use `uv run python script.py` (never bare `python`)
- **Platform compilation timeout**: 15 minutes minimum for platform builds
- **Override**: `FL_AGENT_ALLOW_ALL_CMDS=1` prefix bypasses forbidden command checks
- See `agents/docs/build-system.md` for full rules

### Code Standards
- **C++**: See `agents/docs/cpp-standards.md` (span convention, DMA patterns, naming, macros)
- **JavaScript**: Run `bash lint --js` after modifying JS files

### Code Review Rule
**ALL AGENTS: Run `/code-review` after making code changes.**

### Memory Refresh Rule
**ALL AGENTS: Read the relevant agents doc before concluding work.**

## Workflow

See `agents/docs/workflow.md` for full workflow orchestration and task management.

- **Plan first** for non-trivial tasks (3+ steps) — use plan mode
- **Subagents** for research, exploration, and parallel analysis
- **Self-improvement**: Update `agents/tasks/lessons.md` after corrections
- **Verify before done** — prove it works with tests, logs, demonstrations
- **Test simplicity**: Keep tests simple, avoid mocks. See `agents/tests.md`
- **TDD for features/bugs**: Use `/tdd` (guided cycle) or `/tdd-implement` (full feature). Write tests FIRST.

## Core Principles

- **Simplicity First**: Make every change as simple as possible. Minimal code impact.
- **No Laziness**: Find root causes. No temporary fixes. Senior developer standards.
- **Minimal Impact**: Changes should only touch what's necessary. Avoid introducing bugs.
