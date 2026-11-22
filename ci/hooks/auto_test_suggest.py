#!/usr/bin/env python3
"""
PostToolUse hook for Edit|Write: suggest running affected tests after src/ edits.

When a C++ file under src/ is edited, identifies the corresponding test file
under tests/ and prints an advisory message suggesting the agent run that test.

This is advisory only (exit 0 + stdout) — it does NOT run the test or block.
The agent can then choose to run `bash test TestName` at an appropriate time.

Mapping: src/fl/foo.h → tests/unit/fl/foo.cpp (test name: "fl/foo")

Exit codes:
- 0: Always (advisory only)
"""

import json
import os
import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent

# C++ file extensions that might have corresponding tests
CPP_EXTENSIONS = {".h", ".hpp", ".cpp", ".cpp.hpp"}


def get_file_path_from_hook_input() -> str | None:
    """Extract file path from Claude Code hook JSON input."""
    try:
        hook_data = json.load(sys.stdin)
        tool_input = hook_data.get("tool_input", {})
        return tool_input.get("file_path")
    except (json.JSONDecodeError, KeyError):
        return None


def normalize_path(file_path: str) -> str | None:
    """Get path relative to project root, normalized with forward slashes."""
    file_path = file_path.replace("\\", "/")
    project_root = str(PROJECT_ROOT).replace("\\", "/")

    if file_path.startswith(project_root):
        return file_path[len(project_root) :].lstrip("/")

    norm_file = os.path.normpath(file_path).replace("\\", "/")
    norm_root = os.path.normpath(project_root).replace("\\", "/")
    if norm_file.startswith(norm_root):
        return norm_file[len(norm_root) :].lstrip("/")

    return None


def get_stem(inner_path: str) -> str | None:
    """Strip C++ extension to get stem path."""
    for ext in (".cpp.hpp", ".hpp", ".cpp", ".h"):
        if inner_path.endswith(ext):
            return inner_path[: -len(ext)]
    return None


def main() -> int:
    file_path = get_file_path_from_hook_input()
    if not file_path:
        return 0

    rel_path = normalize_path(file_path)
    if not rel_path:
        return 0

    # Only check files under src/
    if not rel_path.startswith("src/"):
        return 0

    # Only check C++ files
    if not any(rel_path.endswith(ext) for ext in CPP_EXTENSIONS):
        return 0

    # Skip platform-specific files (often untestable on host)
    skip_prefixes = ("src/platforms/", "src/fl/third_party/")
    if any(rel_path.startswith(p) for p in skip_prefixes):
        return 0

    # Extract stem: src/fl/foo.h → fl/foo
    inner_path = rel_path[len("src/") :]
    stem = get_stem(inner_path)
    if not stem:
        return 0

    # Check if corresponding test exists in any tier
    for tier in ("unit", "integration"):
        test_path = f"tests/{tier}/{stem}.cpp"
        if os.path.exists(os.path.join(str(PROJECT_ROOT), test_path)):
            break
    else:
        return 0

    # bash test uses the test file stem as argument
    test_name = Path(stem).name
    print(
        f"AFFECTED TEST: '{test_path}' corresponds to edited '{rel_path}'. "
        f"Run: bash test {test_name}"
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
