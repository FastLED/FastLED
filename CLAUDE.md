# FastLED AI Agent Guidelines

## Quick Reference

This project uses directory-specific agent guidelines. See:

- **CI/Build Tasks**: `ci/AGENTS.md` - Python build system, compilation, MCP server tools
- **Testing**: `tests/AGENTS.md` - Unit tests, test execution, validation requirements, **test simplicity principles**
- **Examples**: `examples/AGENTS.md` - Arduino sketch compilation, .ino file rules

**⚠️ IMPORTANT: When writing/updating tests, follow the Test Simplicity Principle in `tests/AGENTS.md`:**
- Keep tests as simple as possible
- Avoid mocks and helper classes unless absolutely necessary
- One focused test is better than many complex ones
- See `tests/fl/timeout.cpp` for an example of simple, effective testing

## Key Commands

### Testing & Building
- `uv run test.py` - Run all tests
- `uv run test.py --cpp` - Run C++ tests only
- `uv run test.py TestName` - Run specific C++ test (e.g., `uv run test.py xypath`)
- `uv run test.py --no-fingerprint` - Disable fingerprint caching (force rebuild/rerun) ⚠️ **Avoid unless necessary - very slow**
- `bash lint` - Run code formatting/linting
- `bash validate` - 🎯 **AI agents: Use this for live device testing** (hardware-in-the-loop validation)
- `uv run ci/ci-compile.py uno --examples Blink` - Compile examples for specific platform
- `uv run ci/wasm_compile.py examples/Blink --just-compile` - Compile Arduino sketches to WASM
- `uv run mcp_server.py` - Start MCP server for advanced tools

### Docker Testing (Linux Environment)
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

### Git Historian (Code Search)
- `/git-historian keyword1 keyword2` - Search codebase and recent git history for keywords
- `/git-historian "error handling" config` - Search for multi-word phrases (use quotes)
- `/git-historian memory leak --paths src tests` - Limit search to specific directories
- Searches both current code AND last 10 commits' diffs in under 4 seconds
- Returns file locations with matching lines + historical context from commits

### QEMU Commands
- `uv run ci/install-qemu.py` - Install QEMU for ESP32 emulation (standalone)
- `uv run test.py --qemu esp32s3` - Run QEMU tests (installs QEMU automatically)
- `FASTLED_QEMU_SKIP_INSTALL=true uv run test.py --qemu esp32s3` - Skip QEMU installation step

### Live Device Testing (AI Agents)
**🎯 PRIMARY TOOL: `bash validate`** - Hardware-in-the-loop validation framework:

The `bash validate` command is the **preferred entry point** for AI agents doing live device testing. It provides a complete validation framework with pre-configured expect/fail patterns designed for hardware testing.

**⚠️ MANDATORY: Driver Selection Required**

You **must** specify at least one LED driver to test using one of these flags:
- `--parlio` - Test parallel I/O driver
- `--rmt` - Test RMT (Remote Control) driver
- `--spi` - Test SPI driver
- `--uart` - Test UART driver
- `--i2s` - Test I2S LCD_CAM driver (ESP32-S3 only)
- `--all` - Test all drivers (equivalent to `--parlio --rmt --spi --uart --i2s`)

**Usage Examples:**
```bash
# Test specific driver (MANDATORY - must specify at least one)
bash validate --parlio                    # Auto-detect environment
bash validate esp32s3 --parlio            # Specify esp32s3 environment
bash validate --rmt
bash validate --spi
bash validate --uart
bash validate --i2s                       # ESP32-S3 only

# Test multiple drivers
bash validate --parlio --rmt              # Auto-detect environment
bash validate esp32dev --parlio --rmt     # Specify esp32dev environment
bash validate --spi --uart

# Test all drivers
bash validate --all                       # Auto-detect environment
bash validate esp32s3 --all               # Specify esp32s3 environment

# Combined with other options
bash validate --parlio --skip-lint
bash validate esp32s3 --rmt --timeout 120
bash validate --all --skip-lint --timeout 180
```

**Common Options:**
- `--skip-lint` - Skip linting for faster iteration
- `--timeout <seconds>` - Custom timeout (default: 120s)
- `--help` - See all options

**Strip Size Configuration:**
Configure LED strip sizes for validation testing via JSON-RPC:
```bash
# Use strip size presets
bash validate --parlio --strip-sizes small         # 100, 500 LEDs
bash validate --rmt --strip-sizes medium           # 300, 1000 LEDs
bash validate --spi --strip-sizes large            # 500, 3000 LEDs

# Use custom strip sizes (comma-separated LED counts)
bash validate --parlio --strip-sizes 100,300       # Test with 100 and 300 LED strips
bash validate --rmt --strip-sizes 100,300,1000     # Test with 100, 300, and 1000 LED strips
bash validate --i2s --strip-sizes 500              # Test with single 500 LED strip

# Combined configuration
bash validate --all --strip-sizes 100,500,3000
```

**Strip Size Presets:**
- `tiny` - 10, 100 LEDs
- `small` - 100, 500 LEDs (default)
- `medium` - 300, 1000 LEDs
- `large` - 500, 3000 LEDs
- `xlarge` - 1000, 5000 LEDs (high-memory devices only)

**Lane Configuration:**
Configure number of lanes for validation testing via JSON-RPC:
```bash
# Test with specific lane count
bash validate --parlio --lanes 2                   # Test with exactly 2 lanes
bash validate --i2s --lanes 4                      # Test with exactly 4 lanes

# Test with lane range
bash validate --rmt --lanes 1-4                    # Test with 1 to 4 lanes (tests all combinations)
bash validate --spi --lanes 2-8                    # Test with 2 to 8 lanes

# Set per-lane LED counts (NEW)
bash validate --i2s --lane-counts 100,200,300      # 3 lanes with 100, 200, 300 LEDs per lane
bash validate --parlio --lane-counts 50,100        # 2 lanes with 50 and 100 LEDs per lane

# Combined with strip sizes
bash validate --i2s --lanes 2 --strip-sizes 100,300  # 2 lanes, strips of 100 and 300 LEDs
```

**Default:** 1-8 lanes (firmware default)

**Color Pattern Configuration:**
Configure custom RGB color pattern for validation testing:
```bash
# Custom color patterns (hex RGB format)
bash validate --parlio --color-pattern ff00aa      # Pink color (RGB: 255, 0, 170)
bash validate --rmt --color-pattern 0x00ff00       # Green color (RGB: 0, 255, 0)
bash validate --i2s --color-pattern 112233         # Dark blue (RGB: 17, 34, 51)

# Combined with lane configuration
bash validate --parlio --lane-counts 100,200 --color-pattern ff0000  # 2 lanes, red color
```

**Note:** Custom color patterns require firmware support via the `setSolidColor` RPC command. This command may need to be implemented in the firmware if not already available.

**Error Handling:**
If you run `bash validate` without specifying a driver, you'll get a helpful error message:
```
ERROR: No LED driver specified. You must specify at least one driver to test.

Available driver options:
  --parlio    Test parallel I/O driver
  --rmt       Test RMT (Remote Control) driver
  --spi       Test SPI driver
  --uart      Test UART driver
  --i2s       Test I2S LCD_CAM driver (ESP32-S3 only)
  --all       Test all drivers

Example commands:
  bash validate --parlio
  bash validate esp32s3 --parlio
  bash validate --rmt --spi
  bash validate --all
```

**What it does:**
1. **Lint** → Catches ISR errors and code quality issues (skippable with --skip-lint)
2. **Compile** → Builds for attached device
3. **Upload** → Flashes firmware to device
4. **Monitor** → Validates output with smart pattern matching
5. **Report** → Exit 0 (success) or 1 (failure) with detailed feedback

**Runtime Driver Selection:**
Driver selection happens at runtime via JSON-RPC (no recompilation needed). You can instantly switch between drivers or test multiple drivers without rebuilding firmware.

**Why use `bash validate` instead of `bash debug`:**
- ✅ Pre-configured patterns catch hardware failures automatically
- ✅ Designed for automated testing and AI feedback loops
- ✅ Prevents false positives from ISR hangs and device crashes
- ✅ Comprehensive test matrix (48 test cases across drivers/lanes/sizes)
- ✅ Runtime driver selection (no recompilation needed)

**When to use `bash debug` (advanced):**
For custom sketches requiring manual pattern configuration. See examples below.

### Device Debug (Manual Testing)
`bash debug <sketch>` - Low-level tool for custom device workflows:

**Simple example:**
```bash
bash debug MySketch --expect "READY"
```

**Complex example:**
```bash
bash debug MySketch --expect "INIT OK" --expect "TEST PASS" --fail-on "PANIC" --timeout 2m
```

For full documentation, see `uv run ci/debug_attached.py --help`

### JSON-RPC Validation Protocol (Advanced)

**🔧 JSON-RPC AS A SCRIPTING LANGUAGE**: The validation system uses JSON-RPC as a **scripting language** for hardware testing with fail-fast semantics:

**Fail-Fast Model:**
- All test orchestration happens via JSON-RPC commands and responses
- Text output (FL_PRINT, FL_WARN) is for human diagnostics ONLY
- Python code never greps serial output for control flow decisions
- Tests exit immediately on pass/fail (no timeout waiting)

**Example JSON-RPC Test Script:**
```python
from ci.rpc_client import RpcClient

with RpcClient("/dev/ttyUSB0") as client:
    # Pre-flight check
    result = client.send("testGpioConnection", args=[1, 2])
    if not result["connected"]:
        exit(1)  # Fail-fast: hardware not connected

    # Configure and run test in single call (NEW consolidated format)
    result = client.send("runTest", args={
        "drivers": ["PARLIO", "RMT"],
        "laneRange": {"min": 1, "max": 4},
        "stripSizes": [100, 300]
    })
    exit(0 if result["success"] else 1)
```

**🔧 EXTENSIBLE TESTING**: The validation system uses bidirectional JSON-RPC over serial for hardware-in-the-loop testing. This protocol enables AI agents to:
- Test LED drivers without recompilation (runtime driver selection)
- Add new test functionality by extending the JSON-RPC API
- Configure variable lane sizes and test patterns programmatically

**Python Agent (`ci/validation_agent.py`)**:
```python
from validation_agent import ValidationAgent, TestConfig

# Connect to device
with ValidationAgent("COM18") as agent:
    # Health check
    agent.ping()  # Returns: {timestamp, uptimeMs, frameCounter}

    # Get available drivers
    drivers = agent.get_drivers()  # Returns: ["PARLIO", "RMT", "SPI", "UART"]

    # Configure test: 100 LEDs, 1 lane, PARLIO driver
    config = TestConfig(
        driver="PARLIO",
        lane_sizes=[100],  # Per-lane LED counts
        pattern="MSB_LSB_A",
        iterations=1
    )
    agent.configure(config)

    # Run test and get results
    result = agent.run_test()
    print(f"Passed: {result.passed_tests}/{result.total_tests}")

    # Reset between tests
    agent.reset()
```

**JSON-RPC Commands** (sent over serial with `REMOTE:` response prefix):

| Command | Args | Description |
|---------|------|-------------|
| `ping` | - | Health check, returns timestamp and uptime |
| `drivers` | - | List available LED drivers with enabled status |
| `getState` | - | Query current device state and configuration |
| `runTest` | `{drivers, laneRange?, stripSizes?}` | **NEW**: Configure and execute test in single call with named arguments (recommended) |
| `setDrivers` | `[driver_names...]` | Legacy: Set which drivers to test (use `runTest` with config instead) |
| `setLaneRange` | `min, max` | Legacy: Set lane count range (use `runTest` with config instead) |
| `setStripSizes` | `[sizes...]` | Legacy: Set strip sizes array (use `runTest` with config instead) |
| `configure` | `{driver, laneSizes, pattern, iterations, shortStripSize?, longStripSize?, testSmallStrips?, testLargeStrips?}` | Set up test parameters (extended with strip size config) |
| `setLaneSizes` | `[sizes...]` | Set per-lane LED counts directly |
| `setLedCount` | `count` | Set uniform LED count for all lanes |
| `setPattern` | `name` | Set test pattern (MSB_LSB_A, SOLID_RGB, etc.) |
| `setShortStripSize` | `size` | Set short strip LED count |
| `setLongStripSize` | `size` | Set long strip LED count |
| `setStripSizeValues` | `short, long` | Set both strip sizes at once |
| `reset` | - | Reset device state |

**NEW: Consolidated `runTest` with Named Arguments** (recommended):
```json
// Device emits ready event after setup:
RESULT: {"type":"ready","ready":true,"setupTimeMs":2340,"testCases":48,"drivers":3}

// NEW: Single call with config (replaces setDrivers + setLaneRange + setStripSizes + runTest)
// Send:
{"function":"runTest","args":{"drivers":["PARLIO","RMT"],"laneRange":{"min":2,"max":4},"stripSizes":[100,300]}}

// Receive (streaming JSONL):
REMOTE: {"success":true,"streamMode":true}
RESULT: {"type":"test_start","testCases":16}
RESULT: {"type":"test_complete","passed":true,"totalTests":64,"passedTests":64,"durationMs":8230}
```

**Legacy Multi-Call Format** (still supported for backward compatibility):
```json
// Send (4 separate calls):
{"function":"setDrivers","args":["PARLIO"]}
{"function":"setLaneRange","args":[2,4]}
{"function":"setStripSizes","args":[[100,300]]}
{"function":"runTest","args":[]}

// Receive:
REMOTE: {"success":true,"streamMode":true}
RESULT: {"type":"test_start","driver":"PARLIO","totalLeds":100}
RESULT: {"type":"test_complete","passed":true,"totalTests":4,"passedTests":4,"durationMs":2830}
```

**Variable Lane Configurations**:
```python
# Uniform: all lanes same size
config = TestConfig.uniform("RMT", led_count=100, lane_count=4, pattern="MSB_LSB_A")

# Asymmetric: different sizes per lane
config = TestConfig(driver="PARLIO", lane_sizes=[300, 200, 100, 50], pattern="SOLID_RGB")
```

**Strip Size Configuration (NEW)**:
```python
# Option 1: Individual RPC commands
agent.set_strip_size_values(short=100, long=500)  # Set both sizes at once
agent.set_strip_sizes_enabled(small=True, large=True)  # Enable both strip sizes

# Option 2: Via TestConfig
config = TestConfig(
    driver="PARLIO",
    lane_sizes=[100, 100],
    pattern="MSB_LSB_A",
    short_strip_size=300,      # Override default short strip size
    long_strip_size=1000,      # Override default long strip size
    test_small_strips=True,    # Enable small strip testing
    test_large_strips=True,    # Enable large strip testing (requires sufficient memory)
)
agent.configure(config)
```

**Extending the Protocol**:
The JSON-RPC architecture allows agents to easily add new test functionality:
1. Add new RPC handler in `examples/Validation/ValidationRemote.cpp`
2. Add corresponding Python method in `ci/validation_agent.py`
3. No firmware recompilation needed for client-side extensions

**Files**:
- `ci/validation_agent.py` - Python ValidationAgent class
- `ci/validation_loop.py` - Test orchestration with CLI
- `examples/Validation/ValidationRemote.cpp` - Firmware RPC handlers

### Package Installation Daemon Management
`bash daemon <command>` - Manage the singleton daemon that handles PlatformIO package installations:

**Commands:**
- `bash daemon status` - Show daemon status and health
- `bash daemon stop` - Stop the daemon gracefully
- `bash daemon logs` - View daemon log file (last 50 lines)
- `bash daemon logs-tail` - Follow daemon logs in real-time (Ctrl+C to exit)
- `bash daemon start` - Start the daemon (usually automatic)
- `bash daemon restart` - Stop and start the daemon
- `bash daemon clean` - Remove all daemon state (force fresh start)

**What is the daemon:**
The package installation daemon is a singleton background process that ensures PlatformIO package installations complete atomically, even if an agent is interrupted mid-download. It prevents package corruption by:
- Surviving agent termination (daemon continues independently)
- Preventing concurrent package installations system-wide
- Providing progress feedback to waiting clients
- Auto-shutting down after 12 hours of inactivity

**Note:** The daemon starts automatically when needed by `bash compile` or `bash debug`. Manual management is typically not required.

### Code Review
- `/code_review` - Run specialized code review checks on changes
  - Reviews src/, examples/, and ci/ changes against project standards
  - Detects try-catch blocks in src/, evaluates new *.ino files for quality
  - Enforces type annotations and KeyboardInterrupt handlers in Python

## Core Rules

### Git and Code Publishing (ALL AGENTS)
- **🚫 NEVER run git commit**: Do NOT create commits - user will commit when ready
- **🚫 NEVER push code to remote**: Do NOT run `git push` or any command that pushes to remote repository
- **User controls all git operations**: All git commit and push decisions are made by the user

### Error Fixing Policy (ALL AGENTS)
- **✅ ALWAYS fix encountered errors immediately**: When running tests or linting, fix ALL errors you encounter, even if they are pre-existing issues unrelated to your current task
- **Rationale**: Leaving broken tests or linting errors creates technical debt and makes the codebase less maintainable
- **Examples**:
  - If a test suite shows 2 failing tests, fix them before completing your work
  - If linting reveals errors in files you didn't modify, fix those errors
  - If compilation fails due to pre-existing bugs, investigate and fix them
- **Exception**: Only skip fixing errors if they are clearly outside the scope of the codebase (e.g., external dependency issues requiring upstream fixes)

### Command Execution (ALL AGENTS)
- **Python**: Always use `uv run python script.py` (never just `python`)
- **Stay in project root** - never `cd` to subdirectories
- **Git-bash compatibility**: Prefix commands with space: `bash test`
- **🚫 NEVER use bare `pio` or `platformio` commands**: Direct PlatformIO commands are DANGEROUS and NOT ALLOWED
  - ✅ Use: `bash compile`, `bash validate`, or `bash debug` instead
  - ❌ Never use: `pio run`, `platformio run`, or any bare `pio`/`platformio` commands
  - Rationale: Bare PlatformIO commands bypass critical safety checks and daemon-managed package installation
- **Platform compilation timeout**: Use minimum 15 minute timeout for platform builds (e.g., `bash compile --docker esp32s3`)
- **Compiler requirement (Windows)**: The project requires clang for Windows builds. When configuring meson:
  ```bash
  CXX=clang-tool-chain-sccache-cpp CC=clang-tool-chain-sccache-c meson setup builddir
  ```
  - GCC 12.2.0 on Windows has `_aligned_malloc` errors in ESP32 mock drivers
  - Clang 21.1.5 provides proper MSVC compatibility layer and resolves these issues
  - test.py automatically uses clang on Windows (can override with `--gcc` flag)
  - The clang-tool-chain wrappers include sccache integration for fast builds
- **Unified GNU Build (Windows ≈ Linux)**: clang-tool-chain provides a uniform GNU-style build environment across all platforms:
  - **Same link flags**: `-fuse-ld=lld`, `-pthread`, `-static`, `-rdynamic` work identically on Windows and Linux
  - **Same libraries**: Libraries like `libunwind` are available on Windows via DLL injection - no platform-specific code needed
  - **Meson configuration**: The root `meson.build` defines shared `runner_link_args`, `runner_cpp_args`, and `dll_link_args` used by both tests and examples
  - **Platform differences are minimal**: Only `dbghelp`/`psapi` (Windows debug libs) and macOS static linking restrictions require platform checks
  - **Benefits**: Write platform-agnostic build logic; avoid `if is_windows` conditionals for most cases
- **🚫 NEVER disable sccache**: Do NOT set `SCCACHE_DISABLE=1` or disable sccache in any way
  - ✅ If sccache fails: Investigate and fix the root cause (e.g., `sccache --stop-server` to reset)
  - ❌ Never use: `SCCACHE_DISABLE=1` as a workaround for sccache errors
  - Rationale: sccache provides critical compilation performance optimization - disabling it dramatically slows down builds

### C++ Code Standards
- **Use `fl::` namespace** instead of `std::`
- **If you want to use a stdlib header like <type_traits>, look check for equivalent in `fl/type_traits.h`
- **Vector type usage**: Use `fl::vector<T>` for dynamic arrays. The implementation is in `src/fl/stl/vector.h`.
- **Platform dispatch headers**: FastLED uses dispatch headers in `src/platforms/` (e.g., `int.h`, `io_arduino.h`) that route to platform-specific implementations via coarse-to-fine detection. See `src/platforms/README.md` for details.
- **Platform-specific headers (`src/platforms/**`)**: Header files typically do NOT need platform guards (e.g., `#ifdef ESP32`). Only the `.cpp` implementation files require guards. When the `.cpp` file is guarded from compilation, the header won't be included. This approach provides better IDE code assistance and IntelliSense support.
  - ✅ Correct pattern:
    - `header.h`: No platform guards (clean interface)
    - `header.cpp`: Has platform guards (e.g., `#ifdef ESP32 ... #endif`)
  - ❌ Avoid: Adding `#ifdef ESP32` to both header and implementation files (degrades IDE experience)
- **Automatic span conversion**: `fl::span<T>` has implicit conversion constructors - you don't need explicit `fl::span<T>(...)` wrapping in function calls. Example:
  - ✅ Correct: `verifyPixels8bit(output, leds)` (implicit conversion)
  - ❌ Verbose: `verifyPixels8bit(output, fl::span<const CRGB>(leds, 3))` (unnecessary explicit wrapping)
- **Prefer passing and returning by span**: Use `fl::span<T>` or `fl::span<const T>` for function parameters and return types unless a copy of the source data is required:
  - ✅ Preferred: `fl::span<const uint8_t> getData()` (zero-copy view)
  - ✅ Preferred: `void process(fl::span<const CRGB> pixels)` (accepts arrays, vectors, etc.)
  - ❌ Avoid: `std::vector<uint8_t> getData()` (unnecessary copy)
  - Use `fl::span<const T>` for read-only views to prevent accidental modification
- **Macro definition patterns - Two distinct types**:

  **Type 1: Platform/Feature Detection Macros (defined/undefined pattern)**

  **Platform Identification Naming Convention:**
  - MUST follow pattern: `FL_IS_<PLATFORM><_OPTIONAL_VARIANT>`
  - ✅ Correct: `FL_IS_STM32`, `FL_IS_STM32_F1`, `FL_IS_STM32_H7`, `FL_IS_ESP_32S3`
  - ❌ Wrong: `FASTLED_STM32_F1`, `FASTLED_STM32` (missing `FL_IS_` prefix)
  - ❌ Wrong: `FL_STM32_F1`, `IS_STM32_F1` (incorrect prefix pattern)

  **Detection and Usage:**
  - Platform defines like `FL_IS_ARM`, `FL_IS_STM32`, `FL_IS_ESP32` and their variants
  - Feature detection like `FASTLED_STM32_HAS_TIM5`, `FASTLED_STM32_DMA_CHANNEL_BASED` (not platform IDs)
  - These are either **defined** (set) or **undefined** (unset) - NO values
  - ✅ Correct: `#ifdef FL_IS_STM32` or `#ifndef FL_IS_STM32`
  - ✅ Correct: `#if defined(FL_IS_STM32)` or `#if !defined(FL_IS_STM32)`
  - ✅ Define as: `#define FL_IS_STM32` (no value)
  - ❌ Wrong: `#if FL_IS_STM32` or `#if FL_IS_STM32 == 1`
  - ❌ Wrong: `#define FL_IS_STM32 1` (do not assign values)

  **Type 2: Configuration Macros with Defaults (0/1 or numeric values)**
  - Settings that have a default when undefined (e.g., `FASTLED_USE_PROGMEM`, `FASTLED_ALLOW_INTERRUPTS`)
  - Numeric constants (e.g., `FASTLED_STM32_GPIO_MAX_FREQ_MHZ 100`, `FASTLED_STM32_DMA_TOTAL_CHANNELS 14`)
  - These MUST have explicit values: 0, 1, or numeric constants
  - ✅ Correct: `#define FASTLED_USE_PROGMEM 1` or `#define FASTLED_USE_PROGMEM 0`
  - ✅ Correct: `#define FASTLED_STM32_GPIO_MAX_FREQ_MHZ 100`
  - ✅ Check as: `#if FASTLED_USE_PROGMEM` or `#if FASTLED_USE_PROGMEM == 1`
  - ❌ Wrong: `#define FASTLED_USE_PROGMEM` (missing value - ambiguous default behavior)
- **Use proper warning macros** from `fl/compiler_control.h`
- **Debug and Warning Output**:
  - **Use `FL_DBG("message" << var)`** for debug prints (easily stripped in release builds)
  - **Use `FL_WARN("message" << var)`** for warnings (persist into release builds)
  - **Avoid `fl::printf`, `fl::print`, `fl::println`** - prefer FL_DBG/FL_WARN macros instead
  - Note: FL_DBG and FL_WARN use stream-style `<<` operator, NOT printf-style formatting
- **Function-local statics and Teensy 3.x `__cxa_guard` conflicts**:
  - **Problem**: Function-local statics with non-trivial constructors generate implicit `__cxa_guard_*` function calls. If Teensy's `<new.h>` is included after the compiler sees the static, the signatures conflict.
  - **Preferred Solution (`.cpp.hpp` files)**: Include `<new.h>` early on Teensy 3.x to declare the guard functions before use:
    ```cpp
    // Teensy 3.x compatibility: Include new.h before function-local statics
    #if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
        #include <new.h>
    #endif
    ```
    - ✅ Example: `src/fl/async.cpp.hpp` (includes `<new.h>` early to prevent conflicts)
    - This ensures the compiler uses the correct `__guard*` signature
  - **Alternative Solution (header files)**: Move static initialization to corresponding `.cpp` file
    - Example: See `src/platforms/shared/spi_hw_1.{h,cpp}` for the correct pattern
  - **Exception**: Statics inside template functions are allowed (each template instantiation gets its own static, avoiding conflicts)
  - **Linter**: Enforced by `ci/lint_cpp/test_no_static_in_headers.py` for critical directories (`src/platforms/shared/`, `src/fl/`, `src/fx/`)
  - **Suppression**: Add `// okay static in header` comment if absolutely necessary (use sparingly)
- **Member variable naming**: All member variables in classes and structs MUST use mCamelCase (prefix with 'm'):
  - ✅ Correct: `int mCount;`, `fl::string mName;`, `bool mIsEnabled;`
  - ❌ Wrong: `int count;`, `fl::string name;`, `bool isEnabled;`
  - The 'm' prefix clearly distinguishes member variables from local variables and parameters
- **Follow existing code patterns** and naming conventions

### Debugging C++ Crashes
- **When a mysterious, non-trivial C++ crash appears**:
  1. **Recompile with debug flags**: Build the program with `-g3` (full debug info) and `-O0` (no optimization)
  2. **Run under GDB**: Execute the program using `gdb <program>` or `gdb --args <program> <args>`
  3. **Reproduce the crash**: Run the program until the crash occurs (`run` command in GDB)
  4. **Inspect crash state**: When the crash happens, examine:
     - Stack trace: `bt` (backtrace) or `bt full` (with local variables)
     - Current frame variables: `info locals`
     - Specific variable values: `print <variable_name>`
     - Register state: `info registers`
     - Memory contents: `x/<format> <address>`
  5. **Report findings**: Document the exact crash location, variable states, and any suspicious values
  - This systematic approach often reveals null pointers, uninitialized variables, buffer overflows, or memory corruption
  - GDB commands: `run`, `bt`, `bt full`, `frame <n>`, `print <var>`, `info locals`, `info args`

### Python Code Standards
- **Type Annotations**: Use modern PEP 585 and PEP 604 type hints (Python 3.11+ native syntax):
  - Use `list[T]`, `dict[K, V]`, `set[T]` instead of `List[T]`, `Dict[K, V]`, `Set[T]`
  - Use `X | Y` for unions instead of `Union[X, Y]`
  - Use `T | None` instead of `Optional[T]`
  - No need for `from __future__ import annotations` (works natively in Python 3.11+)

- **Structured Return Types**: Prefer `@dataclass` over tuples and dicts for function return types:
  - ✅ Correct: Return a `@dataclass` with named fields (e.g., `CompileResult(success=True, error_output="")`)
  - ❌ Avoid: Returning `tuple[bool, str]` or `dict[str, Any]` — unnamed fields are error-prone and hard to read
  - Named fields are self-documenting, support IDE autocomplete, and make refactoring safer
  - Exception: Single-value returns or well-established conventions (e.g., `divmod`) don't need a dataclass

- **Exception Handling - KeyboardInterrupt**:
  - **CRITICAL**: NEVER silently catch or suppress `KeyboardInterrupt` exceptions
  - When catching broad `Exception`, ALWAYS explicitly re-raise `KeyboardInterrupt` first:
    ```python
    try:
        operation()
    except KeyboardInterrupt:
        raise  # MANDATORY: Always re-raise KeyboardInterrupt
    except Exception as e:
        logger.error(f"Operation failed: {e}")
    ```
  - **Thread-aware KeyboardInterrupt handling**: When handling KeyboardInterrupt in worker threads, use the global interrupt handler:
    ```python
    from ci.util.global_interrupt_handler import notify_main_thread

    try:
        # Worker thread operation
        operation()
    except KeyboardInterrupt:
        print("KeyboardInterrupt: Cancelling operation")
        notify_main_thread()  # Propagate interrupt to main thread
        raise
    ```
  - `notify_main_thread()` calls `_thread.interrupt_main()` to ensure the main thread receives the interrupt signal
  - This is essential for multi-threaded applications to properly coordinate shutdown across all threads
  - KeyboardInterrupt (Ctrl+C) is a user signal to stop execution - suppressing it creates unresponsive processes
  - This applies to ALL exception handlers, including hook handlers, cleanup code, and background tasks
  - Ruff's **BLE001** (blind-except) rule can detect this but is NOT active by default
  - Consider enabling BLE001 with `--select BLE001` for automatic detection

### JavaScript Code Standards
- **After modifying any JavaScript files**: Always run `bash lint --js --no-fingerprint` to ensure proper formatting

### Meson Build System Standards
**Critical Information for Working with meson.build Files**:

1. **NO Embedded Python Scripts** - Meson supports inline Python via `run_command('python', '-c', '...')`, but this creates unmaintainable code
   - ❌ Bad: `run_command('python', '-c', 'import pathlib; print(...)')`
   - ✅ Good: Extract to standalone `.py` file in `ci/` or `tests/`, then call via `run_command('python', 'script.py')`
   - Rationale: External scripts can be tested, type-checked, and debugged independently

2. **Avoid Dictionary Subscript Assignment in Loops** - Meson does NOT support `dict[key] += value` syntax
   - ❌ Bad: `category_files[name] += files(...)`
   - ✅ Good: `existing = category_files.get(name); updated = existing + files(...); category_files += {name: updated}`
   - Limitation: Meson's assignment operators work on variables, not dictionary subscripts

3. **Configuration as Data** - Hardcoded values should live in Python config files, not meson.build
   - ❌ Bad: Lists of paths, patterns, or categories scattered throughout meson.build
   - ✅ Good: Define in `*_config.py`, import in Python scripts, call from Meson
   - Example: `tests/test_config.py` defines test categories and patterns

4. **Extract Complex Logic to Python** - Meson is a declarative build system, not a programming language
   - Meson is great for: Defining targets, dependencies, compiler flags
   - Python is better for: String manipulation, file discovery, categorization, conditional logic
   - Pattern: Use `run_command()` to call Python helpers that output Meson-parseable data
   - Example: `tests/organize_tests.py` outputs `TEST:name:path:category` format

5. **NO Global Error Suppression** - Meson/build errors MUST NOT be globally suppressed
   - ❌ FORBIDDEN: Filtering out all error messages matching a pattern (e.g., `if "error:" in line: continue`)
   - ✅ REQUIRED: Collect errors in a validation list with diagnostics about WHY they were suppressed
   - ✅ REQUIRED: Show suppressed errors when self-healing or fallback mechanisms trigger
   - Pattern: Store errors with context, cap at 5 entries, display only when needed for debugging
   - Example: `ci/meson/compile.py` stores "Can't invoke target" errors during quiet fallback, shows them only when ALL targets fail
   - Rationale: Silent error suppression hides critical diagnostic information needed to debug build failures

**Current Architecture** (after refactoring):
- `meson.build` (root): Source discovery + library compilation (⚠️ still has duplication - needs refactoring)
- `tests/meson.build`: Uses `organize_tests.py` for test discovery (✅ refactored)
- `examples/meson.build`: PlatformIO target registration (✅ clean)

### C++ Linter Architecture (ci/lint_cpp/)
**IMPORTANT: All C++ linters MUST use the centralized dispatcher for performance.**

- **Central dispatcher**: `ci/lint_cpp/run_all_checkers.py`
  - Loads each file **ONCE** and runs all applicable checkers on it
  - 10-100x faster than running standalone scripts on each file
  - All checkers run in a single pass per scope (src/, examples/, tests/)

- **Creating a new C++ linter**:
  1. Create a checker class in `ci/lint_cpp/your_checker.py` inheriting from `FileContentChecker`
  2. Implement `should_process_file(file_path: str) -> bool` (filter which files to check)
  3. Implement `check_file_content(file_content: FileContent) -> list[str]` (check logic)
  4. Add checker instance to appropriate scope in `run_all_checkers.py` (see `create_checkers()`)
  5. ✅ Checker now runs automatically via `bash lint --cpp`

- **DO NOT**:
  - ❌ Create standalone `test_*.py` scripts that scan files independently
  - ❌ Call standalone scripts from the `lint` bash script
  - ❌ Load files multiple times for different checks

- **Reference examples**:
  - `ci/lint_cpp/serial_printf_checker.py` - Simple pattern matching checker
  - `ci/lint_cpp/using_namespace_fl_in_examples_checker.py` - Regex-based checker
  - `ci/lint_cpp/static_in_headers_checker.py` - Complex multi-condition checker

### Code Review Rule
**🚨 ALL AGENTS: After making code changes, run `/code_review` to validate changes. This ensures compliance with project standards.**

### Memory Refresh Rule
**🚨 ALL AGENTS: Read the relevant AGENTS.md file before concluding work to refresh memory about current project rules and requirements.**

---

*This file intentionally kept minimal. Detailed guidelines are in directory-specific AGENTS.md files.*
