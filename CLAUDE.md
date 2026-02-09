# FastLED AI Agent Guidelines

## Read the Right File for Your Task

**By what you're doing:**

| Task | Read |
|------|------|
| Writing/editing C++ code | `docs/agents/cpp-standards.md` |
| Writing/editing Python code | `docs/agents/python-standards.md` |
| Editing meson.build files | `docs/agents/build-system.md` |
| Running tests, Docker, WASM, QEMU | `docs/agents/testing-commands.md` |
| Hardware validation / `bash validate` | `docs/agents/hardware-validation.md` |
| Debugging a C++ crash | `docs/agents/debugging.md` |
| Creating a new C++ linter | `docs/agents/linter-architecture.md` |

**By what directory you're in:**

| Directory | Read |
|-----------|------|
| `src/`, `tests/` C++ files | `docs/agents/cpp-standards.md` |
| `ci/` Python files | `docs/agents/python-standards.md`, `ci/AGENTS.md` |
| `tests/` | `tests/AGENTS.md` |
| `examples/` .ino files | `examples/AGENTS.md` |
| `meson.build` (any) | `docs/agents/build-system.md` |

**Also see directory-specific guidelines:**
- **CI/Build Tasks**: `ci/AGENTS.md` - Python build system, compilation, MCP server tools
- **Testing**: `tests/AGENTS.md` - Unit tests, test execution, validation requirements, **test simplicity principles**
- **Examples**: `examples/AGENTS.md` - Arduino sketch compilation, .ino file rules

**When writing/updating tests, follow the Test Simplicity Principle in `tests/AGENTS.md`:**
- Keep tests as simple as possible
- Avoid mocks and helper classes unless absolutely necessary
- One focused test is better than many complex ones
- See `tests/fl/timeout.cpp` for an example of simple, effective testing

## Key Commands (Cheat Sheet)

**🚨 CRITICAL: Always use bash wrapper scripts (NOT direct Python invocation):**

- `bash test` - Run all tests
- `bash test --cpp` - C++ tests only
- `bash test TestName` - Specific test (e.g., `bash test xypath`)
- `bash lint` - Run code formatting/linting
- `bash compile uno --examples Blink` - Compile for specific platform
- `bash validate --parlio` - Live device testing (must specify driver)

**Advanced (only when bash scripts don't provide needed functionality):**
- `uv run test.py --no-fingerprint` - **LAST RESORT ONLY**: Disable fingerprint caching (very slow)
- `uv run ci/wasm_compile.py examples/Blink --just-compile` - WASM compilation
- `uv run mcp_server.py` - Start MCP server

**NEVER use:** `uv run python test.py` (always use `bash test` or `uv run test.py`)

**Test disambiguation:**
- Path-based: `bash test tests/fl/async` or `bash test examples/Blink`
- Flag-based: `bash test async --cpp` or `bash test Async --examples`

**Other tools:**
- `/git-historian keyword1 keyword2` - Search codebase and recent git history for keywords
- `/git-historian "error handling" config` - Search for multi-word phrases (use quotes)
- `/git-historian memory leak --paths src tests` - Limit search to specific directories
- Searches both current code AND last 10 commits' diffs in under 4 seconds
- `/code_review` - Run specialized code review checks on changes

## Core Rules (ALL AGENTS)

### Git and Code Publishing
- **NEVER run git commit**: Do NOT create commits - user will commit when ready
- **NEVER push code to remote**: Do NOT run `git push` or any command that pushes to remote repository
- **User controls all git operations**: All git commit and push decisions are made by the user

### Error Fixing Policy
- **ALWAYS fix encountered errors immediately**: When running tests or linting, fix ALL errors you encounter, even if they are pre-existing issues unrelated to your current task
- **Rationale**: Leaving broken tests or linting errors creates technical debt and makes the codebase less maintainable
- **Examples**:
  - If a test suite shows 2 failing tests, fix them before completing your work
  - If linting reveals errors in files you didn't modify, fix those errors
  - If compilation fails due to pre-existing bugs, investigate and fix them
- **Exception**: Only skip fixing errors if they are clearly outside the scope of the codebase (e.g., external dependency issues requiring upstream fixes)

### Command Execution
- **Build commands**: ALWAYS use bash wrapper scripts (`bash test`, `bash compile`, `bash lint`)
  - ✅ Use: `bash test`, `bash compile`, `bash lint`, `bash validate`
  - ⚠️ Avoid: `uv run test.py` (only when bash scripts don't provide needed functionality)
  - ❌ NEVER: `uv run python test.py` (always use bash scripts or `uv run test.py`)
- **Python scripts**: When directly invoking Python (not for builds):
  - Always use `uv run python script.py` (never just `python`)
  - For build tools, prefer `uv run <script>` over `uv run python <script>`
- **Stay in project root** - never `cd` to subdirectories
- **Git-bash compatibility**: Commands work in git-bash on Windows
- **NEVER use bare `pio` or `platformio` commands**: Direct PlatformIO commands are DANGEROUS and NOT ALLOWED
  - Use: `bash compile`, `bash validate`, or `bash debug` instead
  - Never use: `pio run`, `platformio run`, or any bare `pio`/`platformio` commands
  - Rationale: Bare PlatformIO commands bypass critical safety checks and daemon-managed package installation
- **NEVER use low-level build commands directly**: Do NOT run `meson`, `ninja`, `clang++`, or `gcc` for standard builds
  - Use: `bash test`, `bash compile`, `bash lint` instead
  - Never use: `meson setup builddir`, `ninja -C builddir`, `clang++ main.cpp -o main`
  - Exception: Runtime debugging of already-built executables (e.g., `lldb .build/runner.exe`) is allowed
  - Exception: Compiler feature testing (e.g., `clang++ -std=c++17 test.cpp`) is allowed
  - Rationale: FastLED build system handles configuration, caching, dependencies, and platform-specific setup
  - See: `docs/agents/build-system.md` for details
- **Platform compilation timeout**: Use minimum 15 minute timeout for platform builds (e.g., `bash compile --docker esp32s3`)
- **NEVER disable sccache**: Do NOT set `SCCACHE_DISABLE=1` or disable sccache in any way (see `docs/agents/build-system.md`)

### JavaScript Code Standards
- **After modifying any JavaScript files**: Always run `bash lint --js` to ensure proper formatting

### Code Review Rule
**ALL AGENTS: After making code changes, run `/code_review` to validate changes. This ensures compliance with project standards.**

### Memory Refresh Rule
**ALL AGENTS: Read the relevant AGENTS.md file before concluding work to refresh memory about current project rules and requirements.**
