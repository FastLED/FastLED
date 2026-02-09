# Build System Standards

## Compiler Requirements (Windows)
The project requires clang for Windows builds. When configuring meson:
```bash
CXX=clang-tool-chain-sccache-cpp CC=clang-tool-chain-sccache-c meson setup builddir
```
- GCC 12.2.0 on Windows has `_aligned_malloc` errors in ESP32 mock drivers
- Clang 21.1.5 provides proper MSVC compatibility layer and resolves these issues
- test.py automatically uses clang on Windows (can override with `--gcc` flag)
- The clang-tool-chain wrappers include sccache integration for fast builds

## Unified GNU Build (Windows = Linux)
clang-tool-chain provides a uniform GNU-style build environment across all platforms:
- **Same link flags**: `-fuse-ld=lld`, `-pthread`, `-static`, `-rdynamic` work identically on Windows and Linux
- **Same libraries**: Libraries like `libunwind` are available on Windows via DLL injection - no platform-specific code needed
- **Meson configuration**: The root `meson.build` defines shared `runner_link_args`, `runner_cpp_args`, and `dll_link_args` used by both tests and examples
- **Platform differences are minimal**: Only `dbghelp`/`psapi` (Windows debug libs) and macOS static linking restrictions require platform checks
- **Benefits**: Write platform-agnostic build logic; avoid `if is_windows` conditionals for most cases

## sccache
- **NEVER disable sccache**: Do NOT set `SCCACHE_DISABLE=1` or disable sccache in any way
  - If sccache fails: Investigate and fix the root cause (e.g., `sccache --stop-server` to reset)
  - Never use: `SCCACHE_DISABLE=1` as a workaround for sccache errors
  - Rationale: sccache provides critical compilation performance optimization - disabling it dramatically slows down builds

## Meson Build System Standards
**Critical Information for Working with meson.build Files**:

1. **NO Embedded Python Scripts** - Meson supports inline Python via `run_command('python', '-c', '...')`, but this creates unmaintainable code
   - Bad: `run_command('python', '-c', 'import pathlib; print(...)')`
   - Good: Extract to standalone `.py` file in `ci/` or `tests/`, then call via `run_command('python', 'script.py')`
   - Rationale: External scripts can be tested, type-checked, and debugged independently

2. **Avoid Dictionary Subscript Assignment in Loops** - Meson does NOT support `dict[key] += value` syntax
   - Bad: `category_files[name] += files(...)`
   - Good: `existing = category_files.get(name); updated = existing + files(...); category_files += {name: updated}`
   - Limitation: Meson's assignment operators work on variables, not dictionary subscripts

3. **Configuration as Data** - Hardcoded values should live in Python config files, not meson.build
   - Bad: Lists of paths, patterns, or categories scattered throughout meson.build
   - Good: Define in `*_config.py`, import in Python scripts, call from Meson
   - Example: `tests/test_config.py` defines test categories and patterns

4. **Extract Complex Logic to Python** - Meson is a declarative build system, not a programming language
   - Meson is great for: Defining targets, dependencies, compiler flags
   - Python is better for: String manipulation, file discovery, categorization, conditional logic
   - Pattern: Use `run_command()` to call Python helpers that output Meson-parseable data
   - Example: `tests/organize_tests.py` outputs `TEST:name:path:category` format

5. **NO Global Error Suppression** - Meson/build errors MUST NOT be globally suppressed
   - FORBIDDEN: Filtering out all error messages matching a pattern (e.g., `if "error:" in line: continue`)
   - REQUIRED: Collect errors in a validation list with diagnostics about WHY they were suppressed
   - REQUIRED: Show suppressed errors when self-healing or fallback mechanisms trigger
   - Pattern: Store errors with context, cap at 5 entries, display only when needed for debugging
   - Example: `ci/meson/compile.py` stores "Can't invoke target" errors during quiet fallback, shows them only when ALL targets fail
   - Rationale: Silent error suppression hides critical diagnostic information needed to debug build failures

**Current Architecture** (after refactoring):
- `meson.build` (root): Source discovery + library compilation (still has duplication - needs refactoring)
- `tests/meson.build`: Uses `organize_tests.py` for test discovery (refactored)
- `examples/meson.build`: PlatformIO target registration (clean)
