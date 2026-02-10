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
- `bash profile <function>` - Profile function performance with benchmarks (see Performance Profiling section)

**Advanced (only when bash scripts don't provide needed functionality):**
- `uv run ci/wasm_compile.py examples/Blink --just-compile` - WASM compilation
- `uv run mcp_server.py` - Start MCP server

**🚨 FORBIDDEN (unless explicit evidence of fingerprint cache bug):**
- `uv run test.py --no-fingerprint` - **STRONGLY DISCOURAGED**: Disables fingerprint caching, makes builds 10-100x slower
  - Use `bash test --clean` instead if you suspect cache issues
  - Only use if you have concrete evidence that fingerprint caching itself is broken (extremely rare)

**NEVER use:** `uv run python test.py` (always use `bash test` or `uv run test.py`)

**Test disambiguation:**
- Path-based: `bash test tests/fl/async` or `bash test examples/Blink`
- Flag-based: `bash test async --cpp` or `bash test Async --examples`

**Docker Testing (Linux Environment):**

Run tests inside a Docker container for consistent Linux environment with ASAN/LSAN sanitizers:

```bash
# Run all C++ tests in Docker (implies --debug with sanitizers)
bash test --docker

# Run specific unit test in Docker
bash test --docker tests/fl/async

# Run with quick mode (no sanitizers, faster)
bash test --docker --quick

# Run with release mode (optimized)
bash test --docker --build-mode release

# Run only unit tests
bash test --docker --unit

# Run only examples
bash test --docker --examples
```

**Notes:**
- `--docker` implies `--debug` mode (ASAN/LSAN sanitizers enabled) unless `--quick` or `--build-mode` is specified
- Warning shown: `⚠️  --docker implies --debug mode (sanitizers enabled). Use --quick or --build-mode release for faster builds.`
- First run downloads Docker image and Python packages (cached for subsequent runs)
- Uses named volumes for `.venv` and `.build` to persist between runs

**⚠️ AI AGENTS: Avoid `bash test --docker` unless necessary** - Docker testing is slow (3-5 minutes per test). Use `uv run test.py` for quick local testing. Only use Docker when:
- You need Linux-specific sanitizers (ASAN/LSAN) that aren't working locally
- Reproducing CI failures that only occur in the Linux environment
- Testing cross-platform compatibility issues

**Other tools:**

### fbuild (Default for ESP32-S3 and ESP32-C6)
The project uses `fbuild` as the **default build system** for ESP32-S3 and ESP32-C6 (RISC-V) boards. fbuild provides:
- **Daemon-based compilation** - Background process handles builds, survives agent interrupts
- **Cached toolchains/frameworks** - Downloads and caches ESP32 toolchain, Arduino framework
- **Direct esptool integration** - Fast uploads without PlatformIO overhead

**Default behavior:**
- **ESP32-S3 / ESP32-C6**: fbuild is used automatically (no flag needed)
- **Other ESP32 variants**: PlatformIO is used by default

**Usage via debug_attached.py:**
```bash
# ESP32-S3: fbuild is the default (no --use-fbuild needed)
uv run ci/debug_attached.py esp32s3 --example Blink

# Force PlatformIO on esp32s3/esp32c6
uv run ci/debug_attached.py esp32s3 --example Blink --no-fbuild

# Explicitly use fbuild on other ESP32 variants
uv run ci/debug_attached.py esp32dev --use-fbuild --example Blink
```

**Build caching:** fbuild stores builds in `.fbuild/build/<env>/` and caches toolchains in `.fbuild/cache/`.

### Test Disambiguation
When multiple tests match a query, use these methods:

**Path-based queries:**
- `uv run test.py tests/fl/async` - Run unit test by path
- `uv run test.py examples/Blink` - Run example by path

**Flag-based filtering:**
- `uv run test.py async --cpp` - Filter to unit tests only
- `uv run test.py Async --examples` - Filter to examples only

### Example Compilation (Host-Based)
FastLED supports fast host-based compilation of `.ino` examples using Meson build system:

**Quick Mode (Default - Fast Iteration):**
- `uv run test.py --examples` - Compile and run all examples (quick mode, 80 examples in ~0.24s)
- `uv run test.py --examples Blink DemoReel100` - Compile specific examples (quick mode)
- `uv run test.py --examples --no-parallel` - Sequential compilation (easier debugging)
- `uv run test.py --examples --verbose` - Show detailed compilation output
- `uv run test.py --examples --clean` - Clean build cache and recompile

**Debug Mode (Full Symbols + Sanitizers):**
- `uv run test.py --examples --debug` - Compile all examples with debug symbols and sanitizers
- `uv run test.py --examples Blink --debug` - Compile specific example in debug mode
- `uv run test.py --examples Blink --debug --full` - Debug mode with execution
- `uv run python ci/util/meson_example_runner.py Blink --debug` - Direct invocation (debug)

**Release Mode (Optimized Production Builds):**
- `uv run test.py --examples --build-mode release` - Compile all examples optimized
- `uv run test.py --examples Blink --build-mode release` - Compile specific example (release)
- `uv run python ci/util/meson_example_runner.py Blink --build-mode release` - Direct invocation (release)

**Build Modes:**
- **quick** (default): Light optimization with minimal debug info (`-O1 -g1`)
  - Build directory: `.build/meson-quick/examples/`
  - Binary size: Baseline (e.g., Blink: 2.8M)
  - Use case: Fast iteration and testing

- **debug**: Full symbols and sanitizers (`-O0 -g3 -fsanitize=address,undefined`)
  - Build directory: `.build/meson-debug/examples/`
  - Binary size: 3.3x larger (e.g., Blink: 9.1M)
  - Sanitizers: AddressSanitizer (ASan) + UndefinedBehaviorSanitizer (UBSan)
  - Use case: Debugging crashes, memory issues, undefined behavior
  - Benefits: Detects buffer overflows, use-after-free, memory leaks, integer overflow, null dereference

- **release**: Optimized production build (`-O2 -DNDEBUG`)
  - Build directory: `.build/meson-release/examples/`
  - Binary size: Smallest, 20% smaller than quick (e.g., Blink: 2.3M)
  - Use case: Performance testing

**Mode-Specific Directories:**
- All three modes use separate build directories to enable caching and prevent flag conflicts
- Switching modes does not invalidate other mode's cache (no cleanup overhead)
- All modes can coexist simultaneously: `.build/meson-{quick,debug,release}/examples/`

**Performance Notes:**
- Host compilation is 60x+ faster than PlatformIO (2.2s vs 137s for single example)
- All 80 examples compile in ~0.24s (394 examples/second) with PCH caching in quick/release modes
- Debug mode is slower due to sanitizer instrumentation but maintains reasonable performance
- PCH (precompiled headers) dramatically speeds up compilation by caching 986 dependencies
- Examples execute with limited loop iterations (5 loops) for fast testing

**Direct Invocation:**
- `uv run python ci/util/meson_example_runner.py` - Compile all examples directly (quick mode)
- `uv run python ci/util/meson_example_runner.py Blink --full` - Compile and execute specific example
- `uv run python ci/util/meson_example_runner.py Blink --debug --full` - Debug mode with execution

### WASM Development Workflow
`bash run wasm <example>` - Compile WASM example and serve with live-server:

**Usage:**
- `bash run wasm AnimartrixRing` - Compile AnimartrixRing to WASM and serve with live-server
- `bash run wasm Blink` - Compile Blink to WASM and serve with live-server

**What it does:**
1. Compiles the example to WASM using `bash compile wasm <example>`
2. Serves the output directory (`examples/<example>/fastled_js/`) with live-server
3. Opens browser automatically for interactive testing

**Requirements:**
- Install live-server: `npm install -g live-server`

### Performance Profiling & Optimization
`bash profile <function>` - Generate profiler and run performance benchmarks:

**Quick Start:**
```bash
# Profile a function (local build, 20 iterations)
bash profile sincos16

# Profile in Docker (consistent environment, recommended)
bash profile sincos16 --docker

# More iterations for better statistics
bash profile sincos16 --docker --iterations 50

# With callgrind analysis (slower, detailed hotspots)
bash profile sincos16 --docker --callgrind
```

**What it does:**
1. **Generate profiler** - Creates `tests/profile/profile_<function>.cpp` from template
2. **Build binary** - Compiles profiler with optimization flags
3. **Run benchmarks** - Executes multiple iterations and collects timing data
4. **Analyze results** - Computes statistics (best/median/worst/stdev)
5. **Export for AI** - Creates `profile_<function>_results.ai.json` for AI consumption

**Options:**
- `--docker` - Run in Docker (consistent Linux environment, recommended)
- `--iterations N` - Number of benchmark runs (default: 20)
- `--build-mode MODE` - Build mode: quick, debug, release, profile (default: release)
- `--no-generate` - Skip test generation (use existing profiler)
- `--callgrind` - Run valgrind callgrind analysis (requires valgrind)

**Output Files:**
- `tests/profile/profile_<function>.cpp` - Generated profiler source (template, needs customization)
- `profile_<function>_results.json` - Raw benchmark data (all iterations)
- `profile_<function>_results.ai.json` - Statistical summary for AI analysis

**Example Workflow (AI-Driven):**
```
User: "Optimize sincos16 with profile-guided optimization"

AI Steps:
1. bash profile sincos16 --docker --iterations 30
2. [Reads profile_sincos16_results.ai.json]
3. [Analyzes: median 47.8 ns/call]
4. [Identifies: LUT bandwidth bottleneck via callgrind]
5. [Creates optimized variant with smaller LUT]
6. bash profile sincos16_optimized --docker
7. [Results: 38.1 ns/call, 20% speedup]
8. [Validates accuracy with generated tests]
9. [Recommends: Apply optimization]
```

**⚠️ IMPORTANT: Generated profilers are TEMPLATES**
- The generated code is a starting point that MUST be customized
- You need to add appropriate test inputs for your function
- Ensure the benchmark calls the target function correctly
- Edit `tests/profile/profile_<function>.cpp` after generation

**When to use:**
- Function is performance-critical (hot path in rendering/effects)
- User requests "optimize" or "profile-guided optimization"
- Making algorithmic changes that affect speed
- Comparing performance of different implementations

**Integration with test.py:**
Profile tests are built alongside regular tests but NOT executed by `uv run test.py`:
```bash
# Build profile tests
uv run test.py profile_sincos16 --cpp --build-mode release --build

# Run manually (use bash profile instead)
.build/meson-release/tests/profile_sincos16.exe baseline
```

### Git Historian (Code Search)
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
  - Exception: Compiler feature testing is allowed, but MUST use clang-tool-chain wrappers:
    - ✅ Use: `clang-tool-chain-clant test.cpp -o test.exe` (C++)
    - ✅ Use: `clang-tool-chain-lldb test.exe` (debugger)
    - ❌ NEVER: `clang++`, `clang`, `gcc`, `g++`, `lldb`, `gdb` (bare toolchain commands)
    - ❌ NEVER: `uv run clang-tool-chain clang++` (wrong invocation)
    - Rationale: clang-tool-chain wrappers ensure consistent compiler versions and settings
  - Rationale: FastLED build system handles configuration, caching, dependencies, and platform-specific setup
  - See: `docs/agents/build-system.md` for details
- **NEVER manually delete build caches**: Do NOT use `rm -rf .build/` or delete build directories
  - Use: `bash test --clean`, `bash compile --clean` instead of manual deletion
  - Never use: `rm -rf .build/meson-quick`, `rm -rf .build && bash test`
  - Rationale: Build system is self-healing and has special cache invalidation code
  - **HIGHLY DISCOURAGED**: The build system will revalidate and self-heal on its own
  - See: `docs/agents/build-system.md` for details
- **NEVER disable fingerprint caching**: Do NOT use `--no-fingerprint` flag
  - Use: `bash test --clean` if you suspect cache issues
  - Never use: `uv run test.py --no-fingerprint` (makes builds 10-100x slower)
  - Rationale: Fingerprint caching is critical performance optimization that tracks file changes correctly
  - **STRONGLY DISCOURAGED**: Only use if you have concrete evidence fingerprint caching itself is broken (extremely rare)
  - See: `docs/agents/build-system.md` for details
- **Platform compilation timeout**: Use minimum 15 minute timeout for platform builds (e.g., `bash compile --docker esp32s3`)
- **NEVER disable sccache**: Do NOT set `SCCACHE_DISABLE=1` or disable sccache in any way (see `docs/agents/build-system.md`)

### JavaScript Code Standards
- **After modifying any JavaScript files**: Always run `bash lint --js` to ensure proper formatting

### Code Review Rule
**ALL AGENTS: After making code changes, run `/code_review` to validate changes. This ensures compliance with project standards.**

### Memory Refresh Rule
**ALL AGENTS: Read the relevant AGENTS.md file before concluding work to refresh memory about current project rules and requirements.**
