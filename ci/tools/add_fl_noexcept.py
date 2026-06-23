#!/usr/bin/env python3
"""Add FL_NOEXCEPT to function declarations/definitions in C++ files.

Uses clang-query (AST analysis) to find the exact file:line of every function
missing noexcept, then inserts FL_NOEXCEPT at the correct position.

Usage:
    uv run python ci/tools/add_fl_noexcept.py --scope fl/stl            # dry-run
    uv run python ci/tools/add_fl_noexcept.py --scope fl/stl --apply    # apply
    uv run python ci/tools/add_fl_noexcept.py --scope platforms         # dry-run (all platforms)
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent


def _find_clang_query() -> Path:
    """Find clang-query binary."""
    # System LLVM install (Windows)
    system_path = Path("C:/Program Files/LLVM/bin/clang-query.exe")
    if system_path.exists():
        return system_path
    # Project cache
    cache_path = (
        PROJECT_ROOT / ".cache" / "clang-tools" / "clang" / "bin" / "clang-query.exe"
    )
    if cache_path.exists():
        return cache_path
    # Fallback
    import shutil

    found = shutil.which("clang-query")
    if found:
        return Path(found)
    return cache_path  # will fail with clear error message


CLANG_QUERY = _find_clang_query()
NOEXCEPT_INCLUDE = '#include "fl/stl/noexcept.h"'

# ============================================================================
# Step 1: Use clang-query to find functions missing noexcept (AST-accurate)
# ============================================================================


def run_clang_query(scope: str) -> list[tuple[str, int]]:
    """Run clang-query to find all functions missing noexcept in scope.

    Returns list of (filepath, line_number) tuples.
    """
    # Pick translation unit based on scope
    if "platforms" in scope:
        tu = "ci/tools/_noexcept_check_platforms_tu.cpp"
    else:
        tu = "src/fl/build/fl.system+.cpp"

    # Build the file matching regex from scope
    scope_regex = scope.replace("/", ".")
    query = (
        f"set output diag\n"
        f"match functionDecl("
        f"unless(isNoThrow()), "
        f"unless(isDeleted()), "
        f"unless(isDefaulted()), "
        f"unless(isImplicit()), "
        f'isExpansionInFileMatching(".*{scope_regex}.*"))'
    )

    compiler_args = [
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
        # Make FL_NOEXCEPT expand to noexcept under stub mode
        "-DFL_NOEXCEPT=noexcept",
    ]

    if not CLANG_QUERY.exists():
        print(f"ERROR: clang-query not found at {CLANG_QUERY}")
        sys.exit(1)

    result = subprocess.run(
        [str(CLANG_QUERY), tu, "--"] + compiler_args,
        input=query,
        capture_output=True,
        text=True,
        cwd=str(PROJECT_ROOT),
        timeout=300,
    )

    output = result.stdout + result.stderr

    # Parse "file:line:col: note: "root" binds here" lines
    pattern = re.compile(r"(src[\\/]\S+):(\d+):\d+: note: .root. binds here")
    hits: list[tuple[str, int]] = []
    seen: set[tuple[str, int]] = set()

    for m in pattern.finditer(output):
        filepath = m.group(1).replace("\\", "/")
        line_num = int(m.group(2))
        key = (filepath, line_num)
        if key not in seen:
            seen.add(key)
            hits.append(key)

    return hits


# ============================================================================
# Step 2: Insert FL_NOEXCEPT at the correct position
# ============================================================================


def insert_fl_noexcept_in_line(line: str) -> str | None:
    """Insert FL_NOEXCEPT into a function signature line.

    Returns modified line, or None if can't insert.
    """
    if "FL_NOEXCEPT" in line or "noexcept" in line:
        return None

    code = line.rstrip()

    # Skip lines with trailing return types (-> decltype)
    if "->" in code:
        return None

    # Skip constructor initializer list lines (start with : or ,)
    stripped = code.lstrip()
    if stripped.startswith(":") or stripped.startswith(","):
        return None

    # Find balanced closing paren
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

    # Skip if followed by [ (array subscript — variable declaration)
    after_paren = code[close_pos + 1 :].lstrip()
    if after_paren.startswith("["):
        return None

    # Skip past ) and any const/volatile
    pos = close_pos + 1
    rest_raw = code[pos:]
    rest = rest_raw.lstrip()
    skip = len(rest_raw) - len(rest)
    pos += skip

    if rest.startswith("const") and (len(rest) == 5 or not rest[5].isalnum()):
        pos += 5
        rest = code[pos:].lstrip()
        skip2 = len(code[pos:]) - len(rest)
        pos += skip2

    if rest.startswith("volatile") and (len(rest) == 8 or not rest[8].isalnum()):
        pos += 8
        rest = code[pos:].lstrip()
        skip2 = len(code[pos:]) - len(rest)
        pos += skip2

    before = code[:pos].rstrip()
    after = code[pos:].lstrip()

    # Don't insert on operator() — the closing ) is part of the name
    if re.search(r"\boperator\s*\(\s*\)\s*$", before):
        return None

    # Don't insert on destructors — implicitly noexcept
    if re.search(r"~\w+\s*$", before):
        return None

    # Build the new line
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


def ensure_include(lines: list[str]) -> bool:
    """Add #include "fl/stl/noexcept.h" if not present. Returns True if added."""
    content = "\n".join(lines)
    if NOEXCEPT_INCLUDE in content:
        return False

    # Find last #include line
    last_idx = -1
    for i, line in enumerate(lines):
        if line.strip().startswith("#include"):
            last_idx = i

    if last_idx >= 0:
        lines.insert(last_idx + 1, NOEXCEPT_INCLUDE)
    else:
        # After #pragma once
        for i, line in enumerate(lines):
            if "#pragma once" in line:
                lines.insert(i + 1, NOEXCEPT_INCLUDE)
                return True
        lines.insert(0, NOEXCEPT_INCLUDE)

    return True


# ============================================================================
# Main
# ============================================================================


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Add FL_NOEXCEPT to functions using clang-query AST analysis"
    )
    parser.add_argument(
        "--scope",
        required=True,
        help="Path scope to match (e.g., fl/stl, platforms/esp/32)",
    )
    parser.add_argument(
        "--apply", action="store_true", help="Apply changes (default: dry-run)"
    )
    args = parser.parse_args()

    print(
        f"{'APPLYING' if args.apply else 'DRY RUN'}: "
        f"Adding FL_NOEXCEPT to functions in {args.scope}"
    )
    print()

    # Step 1: Get AST-accurate list
    print("Running clang-query (AST analysis)...")
    hits = run_clang_query(args.scope)
    print(f"Found {len(hits)} functions missing noexcept")
    print()

    if not hits:
        print("Nothing to do!")
        return 0

    # Group by file
    by_file: dict[str, list[int]] = {}
    for filepath, line_num in hits:
        by_file.setdefault(filepath, []).append(line_num)

    # Step 2: Process each file
    total_changes = 0
    total_skipped = 0

    for filepath in sorted(by_file):
        full_path = PROJECT_ROOT / filepath
        if not full_path.exists():
            continue

        content = full_path.read_text(encoding="utf-8", errors="replace")
        lines = content.split("\n")

        file_changes = 0
        file_skipped = 0

        for line_num in sorted(by_file[filepath]):
            if line_num < 1 or line_num > len(lines):
                continue

            old_line = lines[line_num - 1]
            new_line = insert_fl_noexcept_in_line(old_line)

            if new_line is not None and new_line != old_line:
                if not args.apply:
                    print(f"  {filepath}:{line_num}")
                    print(f"    - {old_line.strip()}")
                    print(f"    + {new_line.strip()}")
                lines[line_num - 1] = new_line
                file_changes += 1
            else:
                file_skipped += 1

        if file_changes > 0:
            # Add include if needed
            include_added = ensure_include(lines)

            if args.apply:
                full_path.write_text("\n".join(lines), encoding="utf-8")
                print(
                    f"  {filepath}: {file_changes} changes"
                    f"{' (+include)' if include_added else ''}"
                )

            total_changes += file_changes

        total_skipped += file_skipped

    print()
    print(
        f"{'APPLIED' if args.apply else 'WOULD APPLY'}: "
        f"{total_changes} changes across {len(by_file)} files "
        f"({total_skipped} skipped — already have noexcept or can't insert)"
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
