# Testing Commands

## 🚨 CRITICAL: Use Bash Wrapper Scripts

**ALWAYS use bash wrapper scripts. DO NOT run low-level build tools or Python scripts directly.**

**WASM is the default compilation target.** Only use hardware platforms when the user explicitly requests it.

```bash
# ✅ CORRECT - Use bash wrapper scripts
bash test                           # Run all tests
bash test <test_name>               # Build and run specific test
bash compile wasm --examples Blink  # Compile for WASM (default target)
bash compile <platform> --examples X # Only when user requests specific platform
bash lint                           # Run linting
bash autoresearch --parlio              # Hardware validation

# ⚠️ AVOID - Only when bash scripts don't provide needed functionality
uv run test.py <test_name>          # Direct Python script

# ❌ FORBIDDEN - Never use these
uv run python test.py               # WRONG - never "uv run python"
meson setup builddir                # WRONG - use bash scripts
ninja -C builddir                   # WRONG - use bash scripts
clang++ main.cpp -o main            # WRONG - use bash scripts
```

**Exceptions:** Runtime debugging (e.g., `lldb .build/runner.exe`) and compiler feature testing are allowed. See `agents/docs/build-system.md` for details.

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
bash compile --clean wasm --examples Blink  # Clean and rebuild WASM

# ❌ FORBIDDEN - Never manually delete build caches
rm -rf .build/meson-quick                  # WRONG - use --clean instead
rm -rf .build && bash test                 # WRONG - use bash test --clean
```

**The build system is self-healing and revalidates automatically. Manual cache deletion is highly discouraged.** See `agents/docs/build-system.md` for details.

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

**LAST RESORT ONLY**: Only use `--no-fingerprint` if you have concrete evidence that fingerprint caching itself is broken (not just a stale build). This should be extremely rare. See `agents/docs/build-system.md` for details.

## Docker Testing (retired)

`bash test --docker` was retired along with the host `docker/unit-tests` image
in the same sweep as the platform-Docker retirement. C++ tests run natively via
Meson — sanitizers (ASAN/LSAN/UBSAN) are available on any host that ships them
(`bash test --debug` opts in). To reproduce a Linux-specific CI failure on
Windows, use WSL2 rather than Docker.

## fbuild (Default for Board Builds)
The project uses `fbuild` as the build system for all board compiles. New board targets must take the fbuild path by default; do not add board allowlists or PlatformIO fallbacks for board compatibility issues. Fix those in fbuild instead.

File board build compatibility problems at https://github.com/FastLED/fbuild/issues.

fbuild provides:
- **Daemon-based compilation** - Background process handles builds, survives agent interrupts
- **Cached toolchains/frameworks** - Downloads and caches ESP32 toolchain, Arduino framework
- **Direct esptool integration** - Fast uploads without PlatformIO overhead

**Default behavior:**
- **All boards**: fbuild is used automatically (no flag needed)

**Usage via debug_attached.py:**
```bash
# fbuild is the default for all board builds
uv run ci/debug_attached.py esp32s3 --example Blink

# Deprecated compatibility flag; fbuild is still used
uv run ci/debug_attached.py esp32s3 --example Blink --no-fbuild

# Deprecated compatibility flag; fbuild is still used
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

## AVR8JS Emulator (Local, No Docker)

Run AVR firmware in the avr8js emulator locally using Node.js — no Docker needed.

**One-time setup:**
```bash
cd ci/docker_utils/avr8js && npm install && cd -
```

**Workflow: Compile → Run**
```bash
# 1. Compile sketch for UNO (uses PlatformIO, may use Docker for compilation)
bash compile uno --examples Blink --local

# 2. Run hex in avr8js emulator (pure Node.js, no Docker)
npx --prefix ci/docker_utils/avr8js tsx ci/docker_utils/avr8js/main.ts \
  .build/pio/uno/.pio/build/uno/firmware.hex --timeout 5 --verbose
```

**Options:**
- `--timeout <seconds>` — Simulated time limit (default: 30)
- `--verbose` — Show execution stats (wall time, cycles, performance %)

**Performance:** avr8js runs at ~140-180% of real 16MHz clock speed.

**Output:** Serial (UART) output prints to stdout. The emulator checks for "Test loop" in output by default (exit code 1 if missing — safe to ignore for non-test sketches).

**Supported boards:** uno (ATmega328P), nano_every (ATmega4809). ATtiny85/88 lack hardware UART so serial output won't work.

**Hex file location:** `.build/pio/<board>/.pio/build/<board>/firmware.hex`

**Files:**
- `ci/docker_utils/avr8js/main.ts` — Entry point
- `ci/docker_utils/avr8js/execute.ts` — AVR CPU emulator wrapper
- `ci/docker_utils/avr8js/package.json` — Node dependencies (avr8js, tsx)

## QEMU Commands

The Docker-based QEMU pipeline (`ci/install-qemu.py`, `bash test --qemu ...`) was retired along with the rest of the platform-Docker infrastructure. Local QEMU runs go through fbuild directly — the exact invocation matches what CI runs in `.github/workflows/qemu_docker_template.yml`:

```bash
# 1. Stage the sketch (produces .build/pio/<env>/ with firmware + partitions).
uv run ci/ci-compile.py esp32s3 \
    --examples Blink \
    --merged-bin \
    --defines FASTLED_ESP32_IS_QEMU \
    --verbose

# 2. Emulate with fbuild (auto-downloads the Espressif QEMU binary).
uv run fbuild test-emu \
    --emulator qemu \
    --environment esp32s3 \
    --timeout 120 \
    --halt-on-success "setup starting" \
    --halt-on-error "Guru Meditation|abort\\(\\)|Backtrace:" \
    .build/pio/esp32s3
```
