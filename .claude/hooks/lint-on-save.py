#!/usr/bin/env python3
"""
Claude Code hook script for linting C++ files on save.

This hook runs after Edit/Write operations and checks the modified file
against the project's C++ linting rules using the unified single-file linter.

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

from ci.lint_cpp.run_all_checkers import (
    create_checkers,
    run_checkers_on_single_file,
)
from ci.util.check_files import is_excluded_file


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

    # Run linting using the unified single-file linter
    checkers_by_scope = create_checkers()
    results = run_checkers_on_single_file(str(Path(file_path).resolve()), checkers_by_scope)

    if results:
        # Format violations
        rel_path = os.path.relpath(file_path, PROJECT_ROOT)
        print(f"C++ lint violations in {rel_path}:", file=sys.stderr)

        for checker_name, checker_results in sorted(results.items()):
            if checker_results.has_violations():
                for file_path_key, file_violations in checker_results.violations.items():
                    for violation in file_violations.violations:
                        print(
                            f"  [{checker_name}] Line {violation.line_number}: {violation.content}",
                            file=sys.stderr,
                        )

        return 2  # Exit 2 = blocking error, stderr fed back to Claude

    return 0


if __name__ == "__main__":
    sys.exit(main())
