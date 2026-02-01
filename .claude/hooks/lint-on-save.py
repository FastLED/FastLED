#!/usr/bin/env python3
"""
Claude Code hook script for linting C++ files on save.

This hook runs after Edit/Write operations and checks the modified file
against the project's C++ linting rules.

Usage: Receives JSON on stdin from Claude Code PostToolUse hook.
Exit codes:
  0 - Success (no violations or non-C++ file)
  2 - Violations found (stderr fed back to Claude)
"""

import json
import os
import sys
from pathlib import Path

# Add project root to path for imports
SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

from ci.lint_cpp.run_all_checkers import create_checkers
from ci.util.check_files import FileContent, is_excluded_file


def get_file_path_from_hook_input() -> str | None:
    """Extract file path from Claude Code hook JSON input."""
    try:
        hook_data = json.load(sys.stdin)
        # PostToolUse provides tool_input with file_path
        tool_input = hook_data.get("tool_input", {})
        return tool_input.get("file_path")
    except (json.JSONDecodeError, KeyError):
        return None


def is_cpp_file(file_path: str) -> bool:
    """Check if file is a C++ file that should be linted."""
    cpp_extensions = (".cpp", ".h", ".hpp", ".ino", ".cpp.hpp", ".hxx", ".hh")
    return file_path.endswith(cpp_extensions)


def get_file_scope(file_path: str) -> list[str]:
    """Determine which checker scopes apply to this file."""
    scopes = []
    rel_path = os.path.relpath(file_path, PROJECT_ROOT)
    normalized = rel_path.replace("\\", "/")

    if normalized.startswith("src/"):
        scopes.append("src")
        scopes.append("global")
        if normalized.startswith("src/fl/"):
            scopes.append("fl")
            if file_path.endswith((".h", ".hpp", ".hxx", ".hh")):
                scopes.append("fl_headers")
        if normalized.startswith("src/lib8tion/"):
            scopes.append("lib8tion")
        if normalized.startswith("src/fx/"):
            scopes.append("fx_sensors_platforms_shared")
        if normalized.startswith("src/sensors/"):
            scopes.append("fx_sensors_platforms_shared")
        if normalized.startswith("src/platforms/"):
            scopes.append("platforms")
            scopes.append("platforms_banned")
            scopes.append("fl_platforms_include_paths")
            if normalized.startswith("src/platforms/shared/"):
                scopes.append("fx_sensors_platforms_shared")
        if normalized.startswith("src/third_party/"):
            scopes.append("third_party")
        # Check if it's in src/ root (not a subdirectory)
        if "/" not in normalized[4:]:  # After "src/"
            scopes.append("src_root")
    elif normalized.startswith("examples/"):
        scopes.append("examples")
        scopes.append("examples_banned")
        scopes.append("global")
    elif normalized.startswith("tests/"):
        scopes.append("tests")
        scopes.append("global")

    return scopes


def check_single_file(file_path: str) -> list[str]:
    """Run all applicable checkers on a single file."""
    violations = []

    # Read file content
    try:
        with open(file_path, "r", encoding="utf-8", errors="replace") as f:
            content = f.read()
    except OSError as e:
        return [f"Could not read file: {e}"]

    file_content = FileContent(path=file_path, content=content, lines=content.splitlines())

    # Get applicable scopes
    scopes = get_file_scope(file_path)
    if not scopes:
        return []  # File not in a checked directory

    # Create all checkers
    checkers_by_scope = create_checkers()

    # Run applicable checkers
    checked_checkers = set()
    for scope in scopes:
        scope_checkers = checkers_by_scope.get(scope, [])
        for checker in scope_checkers:
            # Avoid running same checker twice
            checker_id = id(checker)
            if checker_id in checked_checkers:
                continue
            checked_checkers.add(checker_id)

            # Check if this checker wants to process the file
            if not checker.should_process_file(file_path):
                continue

            # Run the checker
            checker.check_file_content(file_content)

            # Collect violations
            checker_violations = getattr(checker, "violations", {})
            if file_path in checker_violations:
                for line_num, content in checker_violations[file_path]:
                    checker_name = checker.__class__.__name__
                    violations.append(f"[{checker_name}] Line {line_num}: {content}")

    return violations


def main() -> int:
    """Main entry point for the hook."""
    file_path = get_file_path_from_hook_input()

    if not file_path:
        # No file path in input, skip silently
        return 0

    # Resolve to absolute path
    if not os.path.isabs(file_path):
        file_path = os.path.join(PROJECT_ROOT, file_path)

    # Skip non-C++ files
    if not is_cpp_file(file_path):
        return 0

    # Skip excluded files
    if is_excluded_file(file_path):
        return 0

    # Skip if file doesn't exist (might have been deleted)
    if not os.path.exists(file_path):
        return 0

    # Run linting
    violations = check_single_file(file_path)

    if violations:
        # Output violations to stderr (will be fed back to Claude with exit 2)
        rel_path = os.path.relpath(file_path, PROJECT_ROOT)
        print(f"C++ lint violations in {rel_path}:", file=sys.stderr)
        for v in violations:
            print(f"  {v}", file=sys.stderr)
        return 2  # Exit 2 = blocking error, stderr fed back to Claude

    return 0


if __name__ == "__main__":
    sys.exit(main())
