# FastLED AI Agent Guidelines

## Read the Right File for Your Task

**By what you're doing:**

| Task | Read |
|------|------|
| Writing/editing C++ code | `agents/docs/cpp-standards.md` |
| Creating an API wrapper type | `agents/docs/cpp-standards.md` → "API Object Pattern" |
| Adding a global setting / configuration knob | `agents/docs/cpp-standards.md` → "Public Settings Pattern" (new setters go on `CFastLED`, not as bare `fl::set_*` free functions) |
| Writing/editing Python code | `agents/docs/python-standards.md` |
| Editing meson.build files | `agents/docs/build-system.md` |
| Running tests, Docker, WASM, QEMU | `agents/docs/testing-commands.md` |
| Test-Driven Development (TDD) | Use `/tdd` or `/tdd-implement` skills |
| Hardware autoresearch / `bash autoresearch` | `agents/docs/hardware-autoresearch.md` |
| Debugging a C++ crash | `agents/docs/debugging.md` |
| Investigating binary size / flash bloat | Run `bash bloat <board>` and read "Binary Size Analysis" below |
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
- `bash bloat <board>` — Per-symbol flash/RAM bloat report (see "Binary Size Analysis" below)

**NEVER use:** `uv run python test.py` — use `bash test` or `uv run test.py`
**FORBIDDEN:** `--no-fingerprint` (use `bash test --clean`), bare `pio`/`platformio`, bare `meson`/`ninja`/`clang++`

See `agents/docs/commands-reference.md` for Docker, fbuild, WASM, profiling, example compilation, and override mechanism.
See `agents/docs/build-system.md` for full command execution rules and forbidden patterns.

## Binary Size Analysis

When investigating "why is the firmware so big" or "what symbol regressed in this PR", use `bash bloat <board>` instead of running `nm`/`size`/`xtensa-esp32s3-elf-nm` by hand. The wrapper hides every choice that was previously a per-invocation footgun.

### Quick start

```bash
# Default — analyzes the latest Blink build, prints top-10 flash symbols
bash bloat esp32s3

# Different example
bash bloat esp32s3 --example FxFire

# Deeper top-N
bash bloat esp32s3 --top 25

# Rebuild before analyzing (chains `bash compile`)
bash bloat esp32s3 --build

# JSON + MD artifact only, no stdout table
bash bloat esp32s3 --no-summary
```

Supported boards live in `ci/bloat.py::BOARD_CHIP_MAP` (esp32, esp32s2, esp32s3, esp32c3, esp32c6, esp32h2). Adding a new board is a one-line entry mapping the board id to its architecture + chip prefix; the script resolves the cross-toolchain `nm` from the PIO packages directory.

### Artifacts

Every run writes BOTH files side by side under `.build/symbols/<board>/`:

| File | What's in it |
|---|---|
| `report.json` | Machine-readable: `{ symbols: [...], sections: [...], total_flash, total_ram, ... }`. Per-symbol rows carry `archive`, `object`, `output_section`, `source`, `region`, demangled `demangled` name. Suitable for diffing two builds. |
| `report.md` | Human-readable GitHub-style tables: top FLASH symbols, top RAM symbols, per-archive flash roll-up. Renders inline on PRs. |

### Lessons baked in (do not re-discover)

These are the gotchas the wrapper handles for you. They are documented here so they survive an agent rewrite of `ci/bloat.py`:

1. **fbuild release lag.** `fbuild symbols` (the underlying subcommand) was merged to fbuild#main after the v2.2.18 wheel was tagged. The released wheel **does NOT carry it**. The wrapper's `assert_fbuild_has_symbols()` detects this and fails fast with the upgrade instructions. Until fbuild >= 2.2.19 publishes, rebuild fbuild from main (`cargo build --release -p fbuild-cli` inside the dev checkout) and drop the binary into `.venv/Scripts/fbuild.exe`.

2. **Map-derived synthesis is what makes the report useful.** fbuild PR #427 parses `.rodata.<owner>.str1.<N>` input-section names and attributes those bytes to the owning function. Without it, the single biggest contributor on ESP32-S3 Blink (the NEOPIXEL chipset ctor's `FL_WARN`/`FL_LOG` string pool, ~58 KB / 15 %) appears as anonymous bytes against `main.cpp.o` and there's no way to chase it. The `source: "map-derived"` field on each symbol tags rows whose attribution came from this synthesis pass; treat them with the same trust as `source: "nm"` rows.

3. **The dominant flash lever on ESP32-S3 is `FASTLED_LOG_VERBOSITY=0`.** FastLED PR #2791 introduced this build-time knob. Setting it (define before `#include <FastLED.h>`) collapses ~43-58 KB of FL_WARN string pool with zero behaviour change for users who don't need release-mode logging. **Always check whether this knob is set before chasing other optimisations — it's a single define that dominates everything else.**

4. **`--nm` is still required.** fbuild's `build_info.json` does not yet carry toolchain paths (`nm_path` / `cppfilt_path`). The wrapper resolves them from PIO packages. fbuild issue #428 tracks the migration to build-info-driven resolution; when that lands, drop the explicit `--nm` path in `ci/bloat.py::run_fbuild_symbols`.

5. **Diff two builds with the existing diff script.** Save two `report.json` files and run `uv run python .claude/symbolaudit/diff.py <old.json> <new.json>` for a per-symbol delta table (added / removed / grew / shrunk). The wrapper does NOT do this automatically; ship a follow-up PR if you need it inline.

### Don'ts

- **Don't run `xtensa-esp32s3-elf-nm` or `xtensa-esp32s3-elf-size` directly.** The wrapper subsumes both. Direct toolchain invocations have caused every prior bloat audit to wire up nm/c++filt/map by hand and miss the map-derived synthesis pass.
- **Don't shell out to `fbuild symbols` with a hardcoded ELF path.** Use the wrapper — it discovers the latest ELF, picks the right toolchain, defaults the output directory, and prints the summary table in one call.
- **Don't write a new Python aggregator under `.claude/symbolaudit/`.** The wrapper's `_AggBucket` summary plus the existing `diff.py` cover the per-symbol and per-archive views needed for both "what's in this build" and "what changed between two builds". Extend `ci/bloat.py` if a new view is required.

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

### Examples Policy
- **Keep the `examples/` tree minimal.** Do NOT create new one-off `.ino` sketches to try out functionality.
- **Test new functionality in `examples/AutoResearch/AutoResearch.ino`** — that is the canonical scratch target for live/device testing.
- A `PreToolUse` hook (`ci/hooks/protect_example_ino.py`) **blocks creation of any new `.ino` under `examples/`**. Editing an existing `.ino` is always allowed.
- **Override (only when a genuinely new example is required):** prepend a comment containing the `FL_AGENT_ALLOW_NEW_EXAMPLE` directive to the file (e.g. `// FL_AGENT_ALLOW_NEW_EXAMPLE`), or launch with the `FL_AGENT_ALLOW_NEW_EXAMPLE=1` env var.

### Command Execution
- **Always use bash wrapper scripts** (`bash test`, `bash compile`, `bash lint`, `bash autoresearch`)
- **Stay in project root** — never `cd` to subdirectories
- **Python scripts**: Always use `uv run python script.py` (never bare `python`)
- **Platform compilation timeout**: 15 minutes minimum for platform builds
- **Override**: `FL_AGENT_ALLOW_ALL_CMDS=1` prefix bypasses forbidden command checks
- See `agents/docs/build-system.md` for full rules

### Code Standards
- **C++**: See `agents/docs/cpp-standards.md` (span convention, DMA patterns, naming, macros)
- **C++ public settings**: New global setters MUST go on `CFastLED` (`FastLED.setX()`), not as bare `fl::set_*` free functions — see `agents/docs/cpp-standards.md` → "Public Settings Pattern"
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
- **On-device / hardware tests go through `examples/AutoResearch/AutoResearch.ino`** — prefer reusing or augmenting its existing test harness (e.g. `AutoResearchSimd.h`, run via `bash autoresearch <board> --simd`/`--parlio`/etc.) rather than creating a new `.ino` in `examples/`. New example sketches bloat the CI compile matrix; AutoResearch already has the RPC/serial plumbing.
- **TDD for features/bugs**: Use `/tdd` (guided cycle) or `/tdd-implement` (full feature). Write tests FIRST.
- **Orchestrated sub-agents skip `bash test`** — when a sub-agent is one step of a multi-step plan, the orchestrator runs `bash test --cpp` once at the end (the `Stop` hook covers this for free). Per-step sub-agents run `bash lint` only. See `agents/tests.md` → "Orchestrated Sub-Agent Carve-Out". The orchestrator must say so explicitly in the dispatch prompt.

## Core Principles

- **Simplicity First**: Make every change as simple as possible. Minimal code impact.
- **No Laziness**: Find root causes. No temporary fixes. Senior developer standards.
- **Minimal Impact**: Changes should only touch what's necessary. Avoid introducing bugs.
