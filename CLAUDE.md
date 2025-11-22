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
- `uv run ci/ci-compile.py uno --examples Blink` - Compile examples for specific platform
- `uv run ci/wasm_compile.py examples/Blink --just-compile` - Compile Arduino sketches to WASM
- `uv run mcp_server.py` - Start MCP server for advanced tools

### Example Compilation (Host-Based)
FastLED supports fast host-based compilation of `.ino` examples using Meson build system:

- `uv run test.py --examples` - Compile and run all examples for host (fast, 96 examples in ~0.2s)
- `uv run test.py --examples Blink DemoReel100` - Compile and run specific examples
- `uv run test.py --examples --no-parallel` - Sequential compilation (easier debugging)
- `uv run test.py --examples --verbose` - Show detailed compilation output
- `uv run test.py --examples --clean` - Clean build cache and recompile
- `uv run test.py --examples --no-pch` - Disable precompiled headers (ignored - PCH always enabled)

**Performance Notes:**
- Host compilation is 60x+ faster than PlatformIO (2.2s vs 137s for single example)
- All 96 examples compile in ~0.24s (394 examples/second) with PCH caching
- PCH (precompiled headers) dramatically speeds up compilation by caching 986 dependencies
- Examples execute with limited loop iterations (5 loops) for fast testing

**Direct Invocation:**
- `uv run python ci/util/meson_example_runner.py` - Compile all examples directly
- `uv run python ci/util/meson_example_runner.py Blink --full` - Compile and execute specific example

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

### Device Debug Workflow (Compile → Upload → Monitor)
`bash debug` or `uv run ci/debug_attached.py` - Three-phase workflow for attached device development:

**Phase 1: Compile** - Build PlatformIO project for target environment
**Phase 2: Upload** - Upload firmware with automatic port conflict resolution (kills lingering monitors)
**Phase 3: Monitor** - Attach to serial monitor, capture output, detect failure keywords

**Usage:**
- `bash debug` - Auto-detect environment
- `bash debug esp32dev` - Specific environment
- `bash debug --upload-port COM3` - Specific serial port
- `bash debug --timeout 120` - Monitor timeout in seconds (default: 80s)
- `bash debug --timeout 2m` - Monitor timeout with time suffix (2 minutes)
- `bash debug --timeout 5000ms` - Monitor timeout in milliseconds (5 seconds)
- `bash debug --fail-on PANIC` - Exit 1 if keyword found in output
- `bash debug --fail-on ERROR --fail-on CRASH` - Multiple failure keywords

**Timeout Formats:**
- Plain number: `120` (assumes seconds)
- Seconds: `120s`
- Minutes: `2m`
- Milliseconds: `5000ms`

**Exit Codes:**
- 0: Success (normal timeout or clean exit)
- 1: Failure (compile/upload error, process crash, or keyword match)
- 130: User interrupt (Ctrl+C)

**Note:** Normal timeout is considered success (exit 0). Only keyword matches (`--fail-on`) or actual failures cause exit 1.

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
- **Platform compilation timeout**: Use minimum 15 minute timeout for platform builds (e.g., `bash compile --docker esp32s3`)

### C++ Code Standards
- **Use `fl::` namespace** instead of `std::`
- **If you want to use a stdlib header like <type_traits>, look check for equivalent in `fl/type_traits.h`
- **Platform dispatch headers**: FastLED uses dispatch headers in `src/platforms/` (e.g., `int.h`, `io_arduino.h`) that route to platform-specific implementations via coarse-to-fine detection. See `src/platforms/README.md` for details.
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
