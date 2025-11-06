# FastLED AI Agent Guidelines

## Quick Reference

This project uses directory-specific agent guidelines. See:

- **CI/Build Tasks**: `ci/AGENTS.md` - Python build system, compilation, MCP server tools
- **Testing**: `tests/AGENTS.md` - Unit tests, test execution, validation requirements  
- **Examples**: `examples/AGENTS.md` - Arduino sketch compilation, .ino file rules

## Key Commands

- `uv run test.py` - Run all tests
- `uv run test.py --cpp` - Run C++ tests only
- `uv run test.py TestName` - Run specific C++ test (e.g., `uv run test.py xypath`)
- `uv run test.py --no-fingerprint` - Disable fingerprint caching (force rebuild/rerun)
- `bash lint` - Run code formatting/linting
- `uv run ci/ci-compile.py uno --examples Blink` - Compile examples for specific platform
- `uv run ci/wasm_compile.py examples/Blink --just-compile` - Compile Arduino sketches to WASM
- `uv run mcp_server.py` - Start MCP server for advanced tools

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