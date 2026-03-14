# Build System Standards

## 🚨 CRITICAL: Use Bash Wrapper Scripts

**ALWAYS use bash wrapper scripts for all build operations. DO NOT run low-level tools directly.**

### Use These Commands

```bash
# ✅ CORRECT - Use bash wrapper scripts
bash test                            # Run all tests
bash test <test_name>                # Run specific test
bash compile <platform>              # Compile for platforms
bash lint                            # Run linting
bash validate --parlio               # Hardware validation

# ⚠️ AVOID - Only when bash scripts don't provide needed functionality
uv run test.py <test_name>           # Direct Python script
uv run ci/ci-compile.py <platform>   # Direct Python script

# ❌ FORBIDDEN - Never use these
uv run python test.py                # WRONG - never "uv run python"
meson setup builddir                 # WRONG - use bash scripts
ninja -C builddir                    # WRONG - use bash scripts
clang++ main.cpp -o main             # WRONG - use bash scripts
CXX=... meson setup builddir         # WRONG - use bash scripts
```

### Exceptions (When Low-Level Commands Are Allowed)

**ONLY use low-level build commands in these specific scenarios:**

1. **Runtime Debugging** - When the build is already complete:
   ```bash
   # ✅ ALLOWED - debugging already-built executables
   uv run clang-tool-chain-lldb .build/meson-debug/tests/runner.exe
   gdb .build/meson-debug/examples/example-Blink.exe
   ```

2. **Compiler Feature Testing** - Manual builds to test specific compiler features:
   ```bash
   # ✅ ALLOWED - testing specific compiler behavior
   clang++ -std=c++17 test_feature.cpp -o test && ./test
   ```

3. **Build System Development** - When explicitly modifying the build system itself:
   ```bash
   # ✅ ALLOWED - only when working on build system internals
   meson introspect builddir --targets
   ninja -C builddir -t commands
   ```

### Rationale

- **Consistency**: FastLED build system handles configuration, caching, dependencies
- **Safety**: Direct meson/ninja calls bypass critical setup and validation
- **Complexity**: Build system manages zccache, compiler selection, platform-specific flags
- **Reliability**: test.py/compile scripts ensure correct build environment

**If you see example commands like `CXX=clang-tool-chain-cpp meson setup builddir` in documentation, these are for REFERENCE ONLY showing what the build system does internally - do NOT execute them directly.**

## 🚨 CRITICAL: Never Manually Delete Build Caches

**DO NOT manually delete build directories. The build system is self-healing and will revalidate on its own.**

### ❌ FORBIDDEN - Never Do This

```bash
# ❌ WRONG - Never manually delete build caches
rm -rf .build/meson-quick
rm -rf .build/meson-debug
rm -rf .build/meson-release
rm -rf .build/pio
rm -rf .fbuild

# ❌ WRONG - Never combine cache deletion with commands
rm -rf .build/meson-quick && uv run test.py tests/fl/stl/move
rm -rf .build && bash test
```

### ✅ CORRECT - Use Clean Flags Instead

```bash
# ✅ CORRECT - Use clean flag to rebuild from scratch
bash test --clean                          # Clean and rebuild all tests
bash test --clean tests/fl/stl/move        # Clean and rebuild specific test
bash compile --clean esp32s3               # Clean and rebuild platform

# ✅ CORRECT - Just run the command (build system self-heals)
bash test tests/fl/stl/move                # Build system revalidates automatically
```

### Rationale: Why Manual Deletion Is Discouraged

1. **Self-Healing**: The build system automatically detects stale builds and revalidates
2. **Special Code**: We have dedicated cache invalidation logic that handles edge cases
3. **Safety**: Manual deletion can corrupt build state or remove important artifacts
4. **Performance**: The `--clean` flag selectively cleans only what's needed, preserving zccache
5. **Correctness**: `--clean` ensures proper cleanup order and dependency tracking

**The build system is designed to be robust. Trust it to self-heal. Only use `--clean` if you specifically need a guaranteed clean rebuild.**

## 🚨 CRITICAL: Never Disable Fingerprint Caching

**DO NOT use `--no-fingerprint`. Fingerprint caching is a critical performance optimization.**

### ❌ FORBIDDEN - Never Do This

```bash
# ❌ WRONG - Never disable fingerprint caching
uv run test.py --no-fingerprint
bash test --no-fingerprint
uv run test.py tests/fl/async --no-fingerprint

# ❌ WRONG - This makes builds extremely slow
uv run test.py --no-fingerprint --cpp
```

### ✅ CORRECT - Trust the Build System

```bash
# ✅ CORRECT - Let fingerprint caching work
bash test
bash test tests/fl/async
bash test --cpp

# ✅ CORRECT - Use --clean if you suspect cache issues
bash test --clean                    # Clean rebuilds, but keeps fingerprint cache
bash test --clean tests/fl/async     # Selective clean rebuild
```

### Rationale: Why --no-fingerprint Is Forbidden

1. **Performance**: Fingerprint caching makes builds 10-100x faster by skipping unchanged files
2. **Reliability**: The fingerprint system is battle-tested and automatically detects file changes
3. **Self-Healing**: If fingerprints are stale, `--clean` will fix it while preserving cache
4. **Correctness**: Fingerprint cache tracks source files, headers, and compiler flags correctly
5. **No Benefit**: Disabling fingerprints only adds massive build time without fixing real issues

**The fingerprint cache is NOT the problem. If you suspect cache issues, use `--clean` instead, which rebuilds but keeps the fingerprint system working.**

**LAST RESORT ONLY**: If you have concrete evidence that fingerprint caching itself is broken (not just a stale build), then `--no-fingerprint` may be used for debugging. This should be extremely rare (< 0.01% of builds).

### Command Hierarchy Summary

**Tier 1: Bash Scripts (ALWAYS USE)** - Primary interface:
- `bash test`, `bash compile`, `bash lint`, `bash validate`

**Tier 2: Direct Python (AVOID)** - Only when bash doesn't provide functionality:
- `uv run test.py` (use `bash test` instead when possible)
- `uv run ci/ci-compile.py` (use `bash compile` instead when possible)

**Tier 3: Low-Level Tools (FORBIDDEN)** - Never for standard builds:
- `uv run python test.py` (NEVER - always wrong)
- `meson`, `ninja`, `clang++`, `gcc` (only for exceptions above)

## Compiler Requirements
The project uses clang-tool-chain for all builds. The build system internally configures meson with:
```bash
# REFERENCE ONLY - DO NOT RUN THIS DIRECTLY
# The build system (test.py/compile) handles this automatically
CXX=clang-tool-chain-cpp CC=clang-tool-chain-c meson setup builddir
```
- Clang 21.1.5 provides proper MSVC compatibility layer and cross-platform support
- test.py automatically uses clang-tool-chain (no alternative compilers supported)
- The clang-tool-chain wrappers include zccache integration for fast builds

## Unified GNU Build (Windows = Linux)
clang-tool-chain provides a uniform GNU-style build environment across all platforms:
- **Same link flags**: `-fuse-ld=lld`, `-pthread`, `-static`, `-rdynamic` work identically on Windows and Linux
- **Same libraries**: Libraries like `libunwind` are available on Windows via DLL injection - no platform-specific code needed
- **Meson configuration**: The root `meson.build` defines shared `runner_link_args`, `runner_cpp_args`, and `dll_link_args` used by both tests and examples
- **Platform differences are minimal**: Only `dbghelp`/`psapi` (Windows debug libs) and macOS static linking restrictions require platform checks
- **Benefits**: Write platform-agnostic build logic; avoid `if is_windows` conditionals for most cases

## zccache
- **NEVER disable zccache**: Do NOT set `ZCCACHE_DISABLE=1` or disable zccache in any way
  - If zccache fails: Investigate and fix the root cause (e.g., `zccache --stop-server` to reset)
  - Never use: `ZCCACHE_DISABLE=1` as a workaround for zccache errors
  - Rationale: zccache provides critical compilation performance optimization - disabling it dramatically slows down builds

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
