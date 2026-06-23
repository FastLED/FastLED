# FL_HAS_INCLUDE Auto-Fixer

## Problem

Files that use the `FL_HAS_INCLUDE()` macro need to include `"fl/stl/has_include.h"`, otherwise the macro evaluates to 0 (undefined), causing incorrect behavior.

**Example Bug:** `tests/crash_handler.h` was using `FL_HAS_INCLUDE(<execinfo.h>)` without the include, causing it to select the wrong crash handler implementation and resulting in test failures.

## Solution

`ci/fix_has_include.py` is a manual auto-fix tool — find and fix files missing the `fl/stl/has_include.h` include.

**Usage:**
```bash
# Dry-run mode (just reports issues)
uv run python ci/fix_has_include.py

# Fix mode (automatically adds missing includes)
uv run python ci/fix_has_include.py --fix

# Help
uv run python ci/fix_has_include.py --help
```

**Options:**
- `--fix` - Automatically add missing includes (default: dry-run)
- `--macro NAME` - Search for different macro (default: FL_HAS_INCLUDE)
- `--include PATH` - Check for different include (default: fl/stl/has_include.h)

**Features:**
- Scans all C++ files (`.h`, `.hpp`, `.cpp`, `.ino`)
- Detects macro usage vs definition
- Finds optimal insertion point for includes
- Adds IWYU pragma to prevent removal
- Handles indentation matching

The standalone `ci/fix_has_include.py` tool remains as a one-shot manual fixer; the CI lint rule it once paired with was retired in #3293 and has not been ported to the Rust crate — see issue #3308 acceptance criterion for `fix_has_include.py`. Add a Rust checker under `ci/lint_cpp_rs/src/checkers/preprocessor.rs` if the rule needs CI enforcement again — see `agents/docs/linter-architecture.md` for the workflow.

## How It Works

### Detection Algorithm

1. **Find files using FL_HAS_INCLUDE:**
   - Search for pattern `FL_HAS_INCLUDE\s*\(`
   - Exclude `#define FL_HAS_INCLUDE` lines (macro definition)
   - Exclude files in `.git`, `.build`, `.venv`, etc.
   - Exclude `src/fl/stl/has_include.h` itself (defines the macro)

2. **Check for include:**
   - Search for `#include "fl/stl/has_include.h"` or `#include <fl/stl/compiler_control.h>`
   - Pattern: `^\s*#\s*include\s+[<"].*fl/stl/has_include.h[">]`

3. **Report difference:**
   - Files using macro but missing include = **FILES TO FIX**

### Insertion Point Strategy

When adding the include, the script uses this priority order:

1. **After header guard** (preferred)
   ```cpp
   #ifndef MYHEADER_H
   #define MYHEADER_H

   #include "fl/stl/has_include.h"  // IWYU pragma: keep  <-- INSERTED HERE
   ```

2. **Before first #include**
   ```cpp
   #pragma once

   #include "fl/stl/has_include.h"  // IWYU pragma: keep  <-- INSERTED HERE
   #include "other_header.h"
   ```

3. **After any #define**

4. **After #ifndef**

5. **Beginning of file** (fallback)

## Statistics

**Current Status (2026-02-14):**
- **48 files** use `FL_HAS_INCLUDE`
- **48 files** include `fl/stl/has_include.h` ✅
- **0 files** missing the include ✅

**Files Fixed:**
1. `tests/crash_handler.h` (manual fix)
2. `src/fl/system/serial.h` (auto-fix)
3. `src/fl/undef.h` (auto-fix)

## IWYU Pragma

The script adds `// IWYU pragma: keep` to prevent Include-What-You-Use from removing the include:

```cpp
#include "fl/stl/has_include.h"  // IWYU pragma: keep
```

This is necessary because IWYU doesn't understand that preprocessor macros need their defining headers.

## Future Improvements

1. **Add a Rust checker for CI enforcement** — port the original include-presence check to `ci/lint_cpp_rs/src/checkers/preprocessor.rs` (see `agents/docs/linter-architecture.md`).
2. **Pre-commit hook** - Prevent commits with missing includes
3. **Generic macro checker** - Extend to other project-specific macros
4. **IDE integration** - VSCode/clangd diagnostics

## Related Files

- `src/fl/stl/has_include.h` - Defines `FL_HAS_INCLUDE` macro
- `ci/lint_cpp_rs/src/checkers/basic.rs` — `BannedMacrosChecker` (bans raw `__has_include`)
- `ci/lint_cpp_rs/src/checkers/runtime.rs` — `AttributeChecker` (similar attribute-pattern enforcement)

## See Also

- Rust banned-macro checker: `ci/lint_cpp_rs/src/checkers/basic.rs`
- IWYU pragma documentation: https://github.com/include-what-you-use/include-what-you-use/blob/master/docs/IWYUPragmas.md
