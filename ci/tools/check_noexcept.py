#!/usr/bin/env python3
"""Check for functions missing FL_NOEXCEPT using clang-query AST analysis.

Uses clang-query to find function definitions missing noexcept — zero false
positives because it operates on the parsed AST, not text patterns.

Windows only (uses system LLVM install).

Usage:
    uv run python ci/tools/check_noexcept.py                         # check all scopes
    uv run python ci/tools/check_noexcept.py --scope platforms       # all platforms
    uv run python ci/tools/check_noexcept.py --scope fl              # fl/ core only
"""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent

# Translation units for different scopes
_TU_PLATFORMS = "ci/tools/_noexcept_check_platforms_tu.cpp"
_TU_FL = "src/fl/build/fl.system+.cpp"

# Scope → (translation_unit, file_matching_regex)
_SCOPES: dict[str, list[tuple[str, str]]] = {
    "platforms": [
        (_TU_PLATFORMS, ".*src.platforms.*"),
    ],
    "fl": [
        (_TU_FL, ".*src.fl.*"),
    ],
    "all": [
        (_TU_PLATFORMS, ".*src.platforms.*"),
        (_TU_FL, ".*src.fl.*"),
    ],
}

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
    # Critical: make FL_NOEXCEPT expand to noexcept under stub mode
    # so clang-query can see existing annotations
    "-DFL_NOEXCEPT=noexcept",
]


def _find_clang_query() -> str:
    """Find clang-query binary."""
    # Check system LLVM install (Windows)
    system_path = Path("C:/Program Files/LLVM/bin/clang-query.exe")
    if system_path.exists():
        return str(system_path)

    # Check project cache
    cache_path = (
        PROJECT_ROOT / ".cache" / "clang-tools" / "clang" / "bin" / "clang-query.exe"
    )
    if cache_path.exists():
        return str(cache_path)

    # Check PATH
    found = shutil.which("clang-query")
    if found:
        return found

    return ""


def _run_clang_query(
    clang_query: str, tu: str, file_regex: str
) -> list[tuple[str, int, str]]:
    """Run clang-query and return (filepath, line, function_text) tuples."""
    query = (
        f"set output diag\n"
        f"match functionDecl("
        f"unless(isNoThrow()), "
        f"unless(isDeleted()), "
        f"unless(isDefaulted()), "
        f"unless(isImplicit()), "
        f'isExpansionInFileMatching("{file_regex}"))'
    )

    result = subprocess.run(
        [clang_query, tu, "--"] + _COMPILER_ARGS,
        input=query,
        capture_output=True,
        text=True,
        cwd=str(PROJECT_ROOT),
        timeout=300,
    )

    output = result.stdout + result.stderr

    # Parse matches: "file:line:col: note: "root" binds here"
    pattern = re.compile(r"(src[\\/]\S+):(\d+):\d+: note: .root. binds here")
    hits: list[tuple[str, int, str]] = []
    seen: set[tuple[str, int]] = set()

    for m in pattern.finditer(output):
        filepath = m.group(1).replace("\\", "/")
        line_num = int(m.group(2))
        key = (filepath, line_num)
        if key not in seen:
            seen.add(key)
            # Read the actual line for display
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


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check for functions missing FL_NOEXCEPT (AST-accurate)"
    )
    parser.add_argument(
        "--scope",
        choices=list(_SCOPES.keys()),
        default="all",
        help="Which code scope to check (default: all)",
    )
    args = parser.parse_args()

    clang_query = _find_clang_query()
    if not clang_query:
        print("ERROR: clang-query not found.")
        print("Install LLVM: https://github.com/llvm/llvm-project/releases")
        return 1

    print(f"Using: {clang_query}")
    print(f"Scope: {args.scope}")
    print()

    all_hits: list[tuple[str, int, str]] = []
    for tu, file_regex in _SCOPES[args.scope]:
        tu_path = PROJECT_ROOT / tu
        if not tu_path.exists():
            print(f"WARNING: Translation unit not found: {tu}")
            continue

        print(f"Analyzing {tu} ...")
        hits = _run_clang_query(clang_query, tu, file_regex)
        all_hits.extend(hits)

    if not all_hits:
        print("\nAll functions have FL_NOEXCEPT. Nothing to do.")
        return 0

    # Group by file
    by_file: dict[str, list[tuple[int, str]]] = {}
    for filepath, line_num, line_text in all_hits:
        by_file.setdefault(filepath, []).append((line_num, line_text))

    # Print results
    print(
        f"\nFound {len(all_hits)} functions missing FL_NOEXCEPT in {len(by_file)} files:\n"
    )
    for filepath in sorted(by_file):
        print(f"{filepath}:")
        for line_num, line_text in sorted(by_file[filepath]):
            print(f"  Line {line_num}: {line_text}")
        print()

    return 1


if __name__ == "__main__":
    sys.exit(main())
