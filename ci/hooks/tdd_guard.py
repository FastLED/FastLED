#!/usr/bin/env python3
"""
PreToolUse hook for Edit|Write: TDD guard — warn when editing src/ without tests.

When writing or editing files under src/ (implementation code), check whether
a corresponding test file exists under tests/. If no test file exists, emit
a WARNING (not a block) reminding the agent to write tests first.

This is a soft guard — it warns but does not block, because:
1. Some src/ changes don't need tests (comments, formatting, config)
2. The existing check_test_file_pairing hook handles test file creation
3. Blocking would be too disruptive for routine maintenance

Exit codes:
- 0: Allow the operation (always — this is advisory only)
"""

import json
import os
import sys


def main():
    try:
        input_data = json.load(sys.stdin)
    except json.JSONDecodeError:
        sys.exit(0)

    tool_name = input_data.get("tool_name", "")
    if tool_name not in ("Write", "Edit"):
        sys.exit(0)

    tool_input = input_data.get("tool_input", {})
    file_path = tool_input.get("file_path", "")
    if not file_path:
        sys.exit(0)

    # Normalize to forward slashes
    file_path = file_path.replace("\\", "/")

    # Find project root
    project_root = os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ).replace("\\", "/")

    # Get path relative to project root
    if file_path.startswith(project_root):
        rel_path = file_path[len(project_root) :].lstrip("/")
    else:
        norm_file = os.path.normpath(file_path).replace("\\", "/")
        norm_root = os.path.normpath(project_root).replace("\\", "/")
        if norm_file.startswith(norm_root):
            rel_path = norm_file[len(norm_root) :].lstrip("/")
        else:
            sys.exit(0)

    # Only check files under src/
    if not rel_path.startswith("src/"):
        sys.exit(0)

    # Only check .h, .hpp, .cpp files (not .md, .txt, etc.)
    if not any(rel_path.endswith(ext) for ext in (".h", ".hpp", ".cpp", ".cpp.hpp")):
        sys.exit(0)

    # Skip platform-specific files (often untestable on host)
    skip_prefixes = (
        "src/platforms/",
        "src/fl/third_party/",
    )
    if any(rel_path.startswith(p) for p in skip_prefixes):
        sys.exit(0)

    # Extract relative path within src/ e.g. "fl/async.h"
    inner_path = rel_path[len("src/") :]

    # Strip extension to get stem e.g. "fl/async"
    for ext in (".cpp.hpp", ".hpp", ".cpp", ".h"):
        if inner_path.endswith(ext):
            stem = inner_path[: -len(ext)]
            break
    else:
        sys.exit(0)

    # Check for corresponding test file in any tier
    candidates = [f"tests/{tier}/{stem}.cpp" for tier in ("unit", "integration")]
    if any(os.path.exists(os.path.join(project_root, c)) for c in candidates):
        # Test exists, all good
        sys.exit(0)
    test_path = candidates[0]

    # No test file — emit advisory warning (NOT blocking)
    # Print to stdout so it appears as a note, not stderr (which would block)
    warning = (
        f"TDD REMINDER: Editing '{rel_path}' but no test found at '{test_path}'.\n"
        f"Consider writing a test first (Red-Green-Refactor). Use /tdd for guidance."
    )
    print(warning)
    sys.exit(0)


if __name__ == "__main__":
    main()
