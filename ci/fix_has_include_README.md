# FL_HAS_INCLUDE Auto-Fixer

## Problem

Files that use the `FL_HAS_INCLUDE()` macro need to include `"fl/has_include.h"`, otherwise the macro evaluates to 0 (undefined), causing incorrect behavior.

**Example Bug:** `tests/crash_handler.h` was using `FL_HAS_INCLUDE(<execinfo.h>)` without the include, causing it to select the wrong crash handler implementation and resulting in test failures.

## Solution

Two scripts have been created to prevent this issue:

### 1. `ci/fix_has_include.py` - Auto-Fixer (Manual Tool)

**Purpose:** Find and fix files missing the `fl/has_include.h` include.

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
- `--include PATH` - Check for different include (default: fl/has_include.h)

**Features:**
- Scans all C++ files (`.h`, `.hpp`, `.cpp`, `.ino`)
- Detects macro usage vs definition
- Finds optimal insertion point for includes
- Adds IWYU pragma to prevent removal
- Handles indentation matching

### 2. `ci/lint_cpp/has_include_checker.py` - CI Linter

**Purpose:** Prevent the issue from happening again by checking in CI.

**Usage:**
```bash
# Run the checker
uv run python ci/lint_cpp/has_include_checker.py

# Returns 0 if all files are OK
# Returns 1 if any files are missing the include
```

**Integration:** This checker can be integrated into the unified C++ linting system.

## How It Works

### Detection Algorithm

1. **Find files using FL_HAS_INCLUDE:**
   - Search for pattern `FL_HAS_INCLUDE\s*\(`
   - Exclude `#define FL_HAS_INCLUDE` lines (macro definition)
   - Exclude files in `.git`, `.build`, `.venv`, etc.
   - Exclude `src/fl/has_include.h` itself (defines the macro)

2. **Check for include:**
   - Search for `#include "fl/has_include.h"` or `#include <fl/has_include.h>`
   - Pattern: `^\s*#\s*include\s+[<"].*fl/has_include.h[">]`

3. **Report difference:**
   - Files using macro but missing include = **FILES TO FIX**

### Insertion Point Strategy

When adding the include, the script uses this priority order:

1. **After header guard** (preferred)
   ```cpp
   #ifndef MYHEADER_H
   #define MYHEADER_H

   #include "fl/has_include.h"  // IWYU pragma: keep  <-- INSERTED HERE
   ```

2. **Before first #include**
   ```cpp
   #pragma once

   #include "fl/has_include.h"  // IWYU pragma: keep  <-- INSERTED HERE
   #include "other_header.h"
   ```

3. **After any #define**

4. **After #ifndef**

5. **Beginning of file** (fallback)

## Statistics

**Current Status (2026-02-14):**
- **48 files** use `FL_HAS_INCLUDE`
- **48 files** include `fl/has_include.h` ✅
- **0 files** missing the include ✅

**Files Fixed:**
1. `tests/crash_handler.h` (manual fix)
2. `src/fl/serial.h` (auto-fix)
3. `src/fl/undef.h` (auto-fix)

## IWYU Pragma

The script adds `// IWYU pragma: keep` to prevent Include-What-You-Use from removing the include:

```cpp
#include "fl/has_include.h"  // IWYU pragma: keep
```

This is necessary because IWYU doesn't understand that preprocessor macros need their defining headers.

## Future Improvements

1. **Integrate into unified C++ linter** - Add to `ci/lint_cpp/run_all_checkers.py`
2. **Pre-commit hook** - Prevent commits with missing includes
3. **Generic macro checker** - Extend to other project-specific macros
4. **IDE integration** - VSCode/clangd diagnostics

## Related Files

- `src/fl/has_include.h` - Defines `FL_HAS_INCLUDE` macro
- `ci/lint_cpp/banned_macros_checker.py` - Bans raw `__has_include` usage
- `ci/lint_cpp/attribute_checker.py` - Similar pattern checker for attributes

## See Also

- Python banned macro linter: `ci/lint_cpp/banned_macros_checker.py`
- IWYU pragma documentation: https://github.com/include-what-you-use/include-what-you-use/blob/master/docs/IWYUPragmas.md
