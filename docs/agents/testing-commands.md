# Testing Commands

## 🚨 CRITICAL: Use Bash Wrapper Scripts

**ALWAYS use bash wrapper scripts. DO NOT run low-level build tools or Python scripts directly.**

```bash
# ✅ CORRECT - Use bash wrapper scripts
bash test                           # Run all tests
bash test <test_name>               # Build and run specific test
bash compile <platform>             # Compile for platforms
bash lint                           # Run linting
bash validate --parlio              # Hardware validation

# ⚠️ AVOID - Only when bash scripts don't provide needed functionality
uv run test.py <test_name>          # Direct Python script

# ❌ FORBIDDEN - Never use these
uv run python test.py               # WRONG - never "uv run python"
meson setup builddir                # WRONG - use bash scripts
ninja -C builddir                   # WRONG - use bash scripts
clang++ main.cpp -o main            # WRONG - use bash scripts
```

**Exceptions:** Runtime debugging (e.g., `lldb .build/runner.exe`) and compiler feature testing are allowed. See `docs/agents/build-system.md` for details.

## Test Timeouts and Hang Detection

**Tests automatically timeout after 10 seconds to detect infinite loops and deadlocks.**

### How It Works

1. **Test runs normally** - Internal crash handler provides stack traces if test crashes
2. **Test hangs** (no output for 10s) - Watchdog detects hang
3. **Automatic diagnosis** - lldb/gdb attaches and dumps all thread stacks
4. **Process killed** - Hung process terminated, failure reported with stack traces

### Example Output

```
TEST HUNG: test_name
Exceeded timeout of 10.0s
📍 Attaching lldb to hung process (PID 12345)...
THREAD STACK TRACES:
  * frame #0: infinite_loop() at test.cpp:67
🔪 Killed hung process
```

**What to do:**
- Check the stack trace to see where the test is stuck
- Look for infinite loops, deadlocks, or blocking operations
- Fix the issue and re-run the test

**Technical details:**
- Default timeout: 10 seconds (configured in `tests/meson.build`)
- Signal handler chaining allows both internal dumps AND external debugger access
- See `docs/signal-handler-chaining.md` and `docs/deadlock-detection.md` for details

## Clean Builds

**Use the `--clean` flag to rebuild from scratch. DO NOT manually delete build directories.**

```bash
# ✅ CORRECT - Use clean flag
bash test --clean                          # Clean and rebuild all tests
bash test --clean tests/fl/async           # Clean and rebuild specific test
bash compile --clean esp32s3               # Clean and rebuild platform

# ❌ FORBIDDEN - Never manually delete build caches
rm -rf .build/meson-quick                  # WRONG - use --clean instead
rm -rf .build && bash test                 # WRONG - use bash test --clean
```

**The build system is self-healing and revalidates automatically. Manual cache deletion is highly discouraged.** See `docs/agents/build-system.md` for details.

## Performance: Never Disable Fingerprint Caching

**DO NOT use `--no-fingerprint`. This flag disables critical performance optimizations and makes builds 10-100x slower.**

```bash
# ❌ FORBIDDEN - Never disable fingerprint caching
uv run test.py --no-fingerprint              # WRONG - extremely slow
bash test --no-fingerprint                   # WRONG - extremely slow

# ✅ CORRECT - Trust the fingerprint cache
bash test                                    # Fast, uses fingerprint cache
bash test --clean                            # Clean rebuild, preserves fingerprint cache
```

**Rationale**: Fingerprint caching tracks file changes and skips rebuilding unchanged files. Disabling it forces full rebuilds every time, which is unnecessary and extremely slow. If you suspect cache issues, use `--clean` instead.

**LAST RESORT ONLY**: Only use `--no-fingerprint` if you have concrete evidence that fingerprint caching itself is broken (not just a stale build). This should be extremely rare. See `docs/agents/build-system.md` for details.

## Docker Testing (Linux Environment)
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
- Warning shown: `--docker implies --debug mode (sanitizers enabled). Use --quick or --build-mode release for faster builds.`
- First run downloads Docker image and Python packages (cached for subsequent runs)
- Uses named volumes for `.venv` and `.build` to persist between runs

**AI AGENTS: Avoid `bash test --docker` unless necessary** - Docker testing is slow (3-5 minutes per test). Use `uv run test.py` for quick local testing. Only use Docker when:
- You need Linux-specific sanitizers (ASAN/LSAN) that aren't working locally
- Reproducing CI failures that only occur in the Linux environment
- Testing cross-platform compatibility issues

## fbuild (Default for ESP32-S3 and ESP32-C6)
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

## Example Compilation (Host-Based)
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

## WASM Development Workflow
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

## QEMU Commands
- `uv run ci/install-qemu.py` - Install QEMU for ESP32 emulation (standalone)
- `uv run test.py --qemu esp32s3` - Run QEMU tests (installs QEMU automatically)
- `FASTLED_QEMU_SKIP_INSTALL=true uv run test.py --qemu esp32s3` - Skip QEMU installation step
