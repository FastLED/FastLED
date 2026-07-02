# Commands Reference

Detailed reference for all build, test, and development commands. For quick reference, see the Key Commands section in `CLAUDE.md`.

## Docker Testing (retired)

`bash test --docker` was retired along with the host `docker/unit-tests` image.
C++ tests run natively via Meson — `bash test --debug` still enables
ASAN/LSAN/UBSAN on any host that ships them. To reproduce a Linux-specific CI
failure on Windows, use WSL2 rather than Docker.

## fbuild (Default for Board Builds)

The project uses `fbuild` as the build system for all board compiles. New board targets must take the fbuild path by default; do not add board allowlists or PlatformIO fallbacks for board compatibility issues. Fix those in fbuild instead.

File board build compatibility problems at https://github.com/FastLED/fbuild/issues.

fbuild provides:
- **Daemon-based compilation** — Background process handles builds, survives agent interrupts
- **Cached toolchains/frameworks** — Downloads and caches ESP32 toolchain, Arduino framework
- **Direct esptool integration** — Fast uploads without PlatformIO overhead

**Default behavior:**
- **All boards**: fbuild is used by default

**Usage via debug_attached.py:**
```bash
# All board builds use fbuild
uv run ci/debug_attached.py esp32s3 --example Blink

# Compatibility flag only; fbuild is still used
uv run ci/debug_attached.py esp32s3 --use-fbuild --example Blink
```

**Build caching:** fbuild stores builds in `.fbuild/build/<env>/` and caches toolchains in `.fbuild/cache/`.

## Test Disambiguation

When multiple tests match a query, use these methods:

**Path-based queries:**
- `bash test tests/fl/async` — Run unit test by path
- `bash test examples/Blink` — Run example by path

**Flag-based filtering:**
- `bash test async --cpp` — Filter to unit tests only
- `bash test Async --examples` — Filter to examples only

## Example Compilation (Host-Based)

FastLED supports fast host-based compilation of `.ino` examples using Meson build system:

**Quick Mode (Default - Fast Iteration):**
- `bash test --examples` — Compile and run all examples (quick mode, 80 examples in ~0.24s)
- `bash test --examples Blink DemoReel100` — Compile specific examples
- `bash test --examples --no-parallel` — Sequential compilation (easier debugging)
- `bash test --examples --verbose` — Show detailed compilation output
- `bash test --examples --clean` — Clean build cache and recompile

**Debug Mode (Full Symbols + Sanitizers):**
- `bash test --examples --debug` — Compile all examples with debug symbols and sanitizers
- `bash test --examples Blink --debug` — Compile specific example in debug mode
- `bash test --examples Blink --debug --full` — Debug mode with execution

**Release Mode (Optimized Production Builds):**
- `bash test --examples --build-mode release` — Compile all examples optimized
- `bash test --examples Blink --build-mode release` — Compile specific example (release)

**Build Modes:**
- **quick** (default): `-O1 -g1` — Build dir: `.build/meson-quick/examples/`
- **debug**: `-O0 -g3 -fsanitize=address,undefined` — Build dir: `.build/meson-debug/examples/`
- **release**: `-O2 -DNDEBUG` — Build dir: `.build/meson-release/examples/`

All three modes use separate build directories (no cache conflicts, can coexist simultaneously).

**Performance Notes:**
- Host compilation is 60x+ faster than PlatformIO (2.2s vs 137s for single example)
- All 80 examples compile in ~0.24s with PCH caching in quick/release modes
- Examples execute with limited loop iterations (5 loops) for fast testing

**Direct Invocation:**
- `uv run python ci/util/meson_example_runner.py` — Compile all examples directly (quick mode)
- `uv run python ci/util/meson_example_runner.py Blink --full` — Compile and execute specific example
- `uv run python ci/util/meson_example_runner.py Blink --debug --full` — Debug mode with execution

## WASM Development Workflow

`bash run wasm <example>` — Compile WASM example and serve with live-server:

```bash
bash run wasm AnimartrixRing   # Compile and serve with live-server
bash run wasm Blink            # Opens browser automatically
```

**What it does:**
1. Compiles the example to WASM using `bash compile wasm <example>`
2. Serves the output directory (`examples/<example>/fastled_js/`) with live-server
3. Opens browser automatically for interactive testing

**Requirements:** Install live-server: `npm install -g live-server`

## Performance Profiling & Optimization

`bash profile <function>` — Generate profiler and run performance benchmarks:

```bash
# Profile a function (native local build, 20 iterations)
bash profile sincos16

# More iterations for better statistics
bash profile sincos16 --iterations 50

# With callgrind analysis (slower, detailed hotspots; Linux/WSL2 only)
bash profile sincos16 --callgrind
```

The `--docker` flag was retired along with the host `fastled-unit-tests` image
in the platform-Docker sweep — profiling is native. Callgrind still requires a
Linux host (WSL2 works on Windows).

**What it does:**
1. **Generate profiler** — Creates `tests/profile/profile_<function>.cpp` from template
2. **Build binary** — Compiles profiler with optimization flags
3. **Run benchmarks** — Executes multiple iterations and collects timing data
4. **Analyze results** — Computes statistics (best/median/worst/stdev)
5. **Export for AI** — Creates `profile_<function>_results.ai.json` for AI consumption

**Options:**
- `--iterations N` — Number of benchmark runs (default: 20)
- `--build-mode MODE` — Build mode: quick, debug, release, profile (default: release)
- `--no-generate` — Skip test generation (use existing profiler)
- `--callgrind` — Run valgrind callgrind analysis (requires valgrind on Linux/WSL2)

**Output Files:**
- `tests/profile/profile_<function>.cpp` — Generated profiler source (template, needs customization)
- `profile_<function>_results.json` — Raw benchmark data (all iterations)
- `profile_<function>_results.ai.json` — Statistical summary for AI analysis

**IMPORTANT: Generated profilers are TEMPLATES** — The generated code is a starting point that MUST be customized. Add appropriate test inputs and ensure the benchmark calls the target function correctly.

**When to use:**
- Function is performance-critical (hot path in rendering/effects)
- User requests "optimize" or "profile-guided optimization"
- Making algorithmic changes that affect speed
- Comparing performance of different implementations

## Git Historian (Code Search)

- `/git-historian keyword1 keyword2` — Search codebase and recent git history for keywords
- `/git-historian "error handling" config` — Search for multi-word phrases (use quotes)
- `/git-historian memory leak --paths src tests` — Limit search to specific directories
- Searches both current code AND last 10 commits' diffs in under 4 seconds

## Override Mechanism for Forbidden Commands

All forbidden commands can be bypassed using `FL_AGENT_ALLOW_ALL_CMDS=1`:

```bash
FL_AGENT_ALLOW_ALL_CMDS=1 clang++ test.cpp -o test.exe    # Compiler feature testing
FL_AGENT_ALLOW_ALL_CMDS=1 ninja --version                  # Build system debugging
FL_AGENT_ALLOW_ALL_CMDS=1 meson introspect .build/meson-quick
```

**When to use:** Compiler feature testing, build system debugging, investigating specific build failures.
**NOT for regular development** — always prefer bash wrapper scripts.

See `ci/hooks/README.md` for complete hook documentation.
