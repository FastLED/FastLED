#!/usr/bin/env python3
"""Clang-tidy rule: enforce FL_NOEXCEPT on all functions in src/platforms/.

Embedded platforms compile with -fno-exceptions but toolchains may still
generate .eh_frame unwind tables (~26KB bloat on ESP32). Marking functions
FL_NOEXCEPT lets the compiler skip unwind info.

Uses clang-query (AST analysis) for zero false positives, then inserts FL_NOEXCEPT
at the correct position in function signatures.

Usage:
    uv run python ci/lint/clang_tidy/noexcept_platforms.py              # check only
    uv run python ci/lint/clang_tidy/noexcept_platforms.py --fix        # auto-fix
"""

import re
import shutil
import subprocess
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent.parent

# Translation unit that pulls in all platform headers via stub platform
_TU = "ci/tools/_noexcept_check_platforms_tu.cpp"

_FILE_REGEX = ".*src.platforms.*"

_COMPILER_ARGS = [
    "-std=c++17",
    "-Isrc",
    "-Isrc/platforms/stub",
    "-DSTUB_PLATFORM",
    "-DARDUINO=10808",
    "-DFASTLED_USE_STUB_ARDUINO",
    "-DFASTLED_STUB_IMPL",
    "-DFASTLED_TESTING",
    "-DFASTLED_NO_AUTO_NAMESPACE",
    "-fno-exceptions",
    # Make existing FL_NOEXCEPT visible as noexcept to clang-query
    "-DFL_NOEXCEPT=noexcept",
]

_NOEXCEPT_INCLUDE = '#include "fl/stl/noexcept.h"'


# ============================================================================
# Step 1: Find clang-query
# ============================================================================


def _find_clang_query() -> str:
    """Find clang-query binary."""
    # System LLVM install (Windows)
    system_path = Path("C:/Program Files/LLVM/bin/clang-query.exe")
    if system_path.exists():
        return str(system_path)

    # Project cache
    cache_path = (
        PROJECT_ROOT / ".cache" / "clang-tools" / "clang" / "bin" / "clang-query.exe"
    )
    if cache_path.exists():
        return str(cache_path)

    # PATH fallback
    found = shutil.which("clang-query")
    if found:
        return found

    return ""


# ============================================================================
# Step 2: AST query for functions missing noexcept
# ============================================================================


def _find_missing_noexcept(clang_query: str) -> list[tuple[str, int, str]]:
    """Run clang-query to find functions missing noexcept.

    Returns list of (filepath, line_number, line_text) tuples.
    """
    tu_path = PROJECT_ROOT / _TU
    if not tu_path.exists():
        print(f"ERROR: Translation unit not found: {_TU}")
        sys.exit(1)

    query = (
        f"set output diag\n"
        f"match functionDecl("
        f"unless(isNoThrow()), "
        f"unless(isDeleted()), "
        f"unless(isDefaulted()), "
        f"unless(isImplicit()), "
        f'isExpansionInFileMatching("{_FILE_REGEX}"))'
    )

    result = subprocess.run(
        [clang_query, _TU, "--"] + _COMPILER_ARGS,
        input=query,
        capture_output=True,
        text=True,
        cwd=str(PROJECT_ROOT),
        timeout=300,
    )

    output = result.stdout + result.stderr

    # Parse "file:line:col: note: "root" binds here"
    pattern = re.compile(r"(src[\\/]\S+):(\d+):\d+: note: .root. binds here")
    hits: list[tuple[str, int, str]] = []
    seen: set[tuple[str, int]] = set()

    for m in pattern.finditer(output):
        filepath = m.group(1).replace("\\", "/")
        line_num = int(m.group(2))
        key = (filepath, line_num)
        if key not in seen:
            seen.add(key)
            # Read the actual source line for display
            full_path = PROJECT_ROOT / filepath
            line_text = ""
            if full_path.exists():
                try:
                    lines = full_path.read_text(
                        encoding="utf-8", errors="replace"
                    ).splitlines()
                    if 0 < line_num <= len(lines):
                        line_text = lines[line_num - 1].strip()
                except KeyboardInterrupt as ki:
                    from ci.util.global_interrupt_handler import (
                        handle_keyboard_interrupt,
                    )

                    handle_keyboard_interrupt(ki)
                except Exception:
                    pass
            hits.append((filepath, line_num, line_text))

    return hits


# ============================================================================
# Step 3: Insert FL_NOEXCEPT into function signatures
# ============================================================================


def _insert_fl_noexcept(line: str) -> str | None:
    """Insert FL_NOEXCEPT into a function signature line.

    Returns modified line, or None if insertion is not possible.
    """
    if "FL_NOEXCEPT" in line or "noexcept" in line:
        return None

    code = line.rstrip()

    # Skip trailing return types and initializer lists
    if "->" in code:
        return None
    stripped = code.lstrip()
    if stripped.startswith(":") or stripped.startswith(","):
        return None

    # Find the balanced closing paren
    depth = 0
    close_pos = -1
    for i, ch in enumerate(code):
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                close_pos = i
                break

    if close_pos == -1:
        return None

    # Skip array subscripts after paren
    after_paren = code[close_pos + 1 :].lstrip()
    if after_paren.startswith("["):
        return None

    # Advance past ) and const/volatile qualifiers
    pos = close_pos + 1
    rest_raw = code[pos:]
    rest = rest_raw.lstrip()
    pos += len(rest_raw) - len(rest)

    if rest.startswith("const") and (len(rest) == 5 or not rest[5].isalnum()):
        pos += 5
        rest = code[pos:].lstrip()
        pos += len(code[pos:]) - len(rest)

    if rest.startswith("volatile") and (len(rest) == 8 or not rest[8].isalnum()):
        pos += 8
        rest = code[pos:].lstrip()
        pos += len(code[pos:]) - len(rest)

    before = code[:pos].rstrip()
    after = code[pos:].lstrip()

    # Skip operator() and destructors
    if re.search(r"\boperator\s*\(\s*\)\s*$", before):
        return None
    if re.search(r"~\w+\s*$", before):
        return None

    # Insert FL_NOEXCEPT at the correct position
    if not after:
        return before + " FL_NOEXCEPT"
    elif after[0] == ";":
        return before + " FL_NOEXCEPT;" + after[1:]
    elif after[0] == "{":
        return before + " FL_NOEXCEPT {" + after[1:]
    elif after.startswith("override") or after.startswith("final"):
        return before + " FL_NOEXCEPT " + after
    elif after.startswith("__attribute__"):
        return before + " FL_NOEXCEPT " + after
    else:
        return before + " FL_NOEXCEPT " + after


def _ensure_include(lines: list[str]) -> bool:
    """Add #include "fl/stl/noexcept.h" if not already present.

    Returns True if added.
    """
    content = "\n".join(lines)
    if _NOEXCEPT_INCLUDE in content:
        return False

    # Insert after the last existing #include
    last_idx = -1
    for i, line in enumerate(lines):
        if line.strip().startswith("#include"):
            last_idx = i

    if last_idx >= 0:
        lines.insert(last_idx + 1, _NOEXCEPT_INCLUDE)
    else:
        # Fallback: after #pragma once
        for i, line in enumerate(lines):
            if "#pragma once" in line:
                lines.insert(i + 1, _NOEXCEPT_INCLUDE)
                return True
        lines.insert(0, _NOEXCEPT_INCLUDE)

    return True


# ============================================================================
# Step 4: Apply fixes to files
# ============================================================================


def _apply_fixes(hits: list[tuple[str, int, str]], dry_run: bool) -> tuple[int, int]:
    """Apply FL_NOEXCEPT insertions to source files.

    Returns (changes_applied, files_modified).
    """
    # Group hits by file
    by_file: dict[str, list[int]] = {}
    for filepath, line_num, _ in hits:
        by_file.setdefault(filepath, []).append(line_num)

    total_changes = 0
    total_files = 0

    for filepath in sorted(by_file):
        full_path = PROJECT_ROOT / filepath
        if not full_path.exists():
            continue

        content = full_path.read_text(encoding="utf-8", errors="replace")
        lines = content.split("\n")
        file_changes = 0

        for line_num in sorted(by_file[filepath]):
            if line_num < 1 or line_num > len(lines):
                continue

            old_line = lines[line_num - 1]
            new_line = _insert_fl_noexcept(old_line)

            if new_line is not None and new_line != old_line:
                if dry_run:
                    print(f"  {filepath}:{line_num}")
                    print(f"    - {old_line.strip()}")
                    print(f"    + {new_line.strip()}")
                lines[line_num - 1] = new_line
                file_changes += 1

        if file_changes > 0:
            include_added = _ensure_include(lines)

            if not dry_run:
                full_path.write_text("\n".join(lines), encoding="utf-8")
                print(
                    f"  {filepath}: {file_changes} changes"
                    f"{' (+include)' if include_added else ''}"
                )

            total_changes += file_changes
            total_files += 1

    return total_changes, total_files


# ============================================================================
# Main
# ============================================================================


def main() -> int:
    import argparse

    parser = argparse.ArgumentParser(
        description=(
            "Clang-tidy rule: enforce FL_NOEXCEPT on platform functions. "
            "Uses clang-query AST analysis for zero false positives."
        )
    )
    parser.add_argument(
        "--fix",
        action="store_true",
        help="Auto-fix: insert FL_NOEXCEPT into source files (default: check only)",
    )
    args = parser.parse_args()

    clang_query = _find_clang_query()
    if not clang_query:
        print("ERROR: clang-query not found.")
        print("Install LLVM: https://github.com/llvm/llvm-project/releases")
        return 1

    print(f"Using: {clang_query}")
    print(f"Scope: src/platforms/")
    print(f"Mode:  {'fix' if args.fix else 'check'}")
    print()

    # AST analysis
    print("Running clang-query (AST analysis)...")
    hits = _find_missing_noexcept(clang_query)

    if not hits:
        print("\nAll functions have FL_NOEXCEPT. Nothing to do.")
        return 0

    # Group by file for display
    by_file: dict[str, list[tuple[int, str]]] = {}
    for filepath, line_num, line_text in hits:
        by_file.setdefault(filepath, []).append((line_num, line_text))

    print(
        f"\nFound {len(hits)} functions missing FL_NOEXCEPT in {len(by_file)} files:\n"
    )

    if args.fix:
        changes, files = _apply_fixes(hits, dry_run=False)
        print(f"\nApplied {changes} changes across {files} files.")
        return 0
    else:
        # Check-only mode: print violations and exit with error
        for filepath in sorted(by_file):
            print(f"{filepath}:")
            for line_num, line_text in sorted(by_file[filepath]):
                print(f"  Line {line_num}: {line_text}")
            print()

        # Also show what --fix would do
        print("--- Dry-run preview (use --fix to apply) ---\n")
        _apply_fixes(hits, dry_run=True)

        return 1


if __name__ == "__main__":
    sys.exit(main())
