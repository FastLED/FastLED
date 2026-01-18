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
- `uv run test.py --no-fingerprint` - Disable fingerprint caching (force rebuild/rerun)
- `bash lint` - Run code formatting/linting
- `bash validate` - 🎯 **AI agents: Use this for live device testing** (hardware-in-the-loop validation)
- `uv run ci/ci-compile.py uno --examples Blink` - Compile examples for specific platform
- `uv run ci/wasm_compile.py examples/Blink --just-compile` - Compile Arduino sketches to WASM
- `uv run mcp_server.py` - Start MCP server for advanced tools

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
- **quick** (default): Fast compilation with minimal debug info (`-O0 -g1`)
  - Build directory: `.build/meson-quick/examples/`
  - Binary size: Baseline (e.g., Blink: 2.8M)
  - Use case: Fast iteration and testing

- **debug**: Full symbols and sanitizers (`-O0 -g3 -fsanitize=address,undefined`)
  - Build directory: `.build/meson-debug/examples/`
  - Binary size: 3.3x larger (e.g., Blink: 9.1M)
  - Sanitizers: AddressSanitizer (ASan) + UndefinedBehaviorSanitizer (UBSan)
  - Use case: Debugging crashes, memory issues, undefined behavior
  - Benefits: Detects buffer overflows, use-after-free, memory leaks, integer overflow, null dereference

- **release**: Optimized production build (`-O0 -g1`)
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
| `configure` | `{driver, laneSizes, pattern, iterations}` | Set up test parameters |
| `setLaneSizes` | `[sizes...]` | Set per-lane LED counts directly |
| `setLedCount` | `count` | Set uniform LED count for all lanes |
| `setPattern` | `name` | Set test pattern (MSB_LSB_A, SOLID_RGB, etc.) |
| `runTest` | - | Execute configured test, returns streaming results |
| `reset` | - | Reset device state |

**Raw JSON-RPC Example** (for custom integrations):
```json
// Send:
{"function":"configure","args":[{"driver":"PARLIO","laneSizes":[100],"pattern":"MSB_LSB_A","iterations":1}]}

// Receive:
REMOTE: {"success":true,"config":{"driver":"PARLIO","laneSizes":[100],"totalLeds":100}}

// Send:
{"function":"runTest","args":[]}

// Receive (streaming JSONL):
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
- **No function-local statics in headers** (Teensy 3.0 compatibility):
  - **DO NOT** use function-local static variables in header files (causes `__cxa_guard` linkage errors on Teensy 3.0/3.1/3.2)
  - **Instead**: Move static initialization to corresponding `.cpp` file
  - **Example**: See `src/platforms/shared/spi_hw_1.{h,cpp}` for the correct pattern
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

**Current Architecture** (after refactoring):
- `meson.build` (root): Source discovery + library compilation (⚠️ still has duplication - needs refactoring)
- `tests/meson.build`: Uses `organize_tests.py` for test discovery (✅ refactored)
- `examples/meson.build`: PlatformIO target registration (✅ clean)

### Code Review Rule
**🚨 ALL AGENTS: After making code changes, run `/code_review` to validate changes. This ensures compliance with project standards.**

### Memory Refresh Rule
**🚨 ALL AGENTS: Read the relevant AGENTS.md file before concluding work to refresh memory about current project rules and requirements.**

---

*This file intentionally kept minimal. Detailed guidelines are in directory-specific AGENTS.md files.*
