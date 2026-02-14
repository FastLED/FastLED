#!/usr/bin/env python3
"""
Claude Code hook script for linting source files on save.

Supports C++, Python, and JavaScript/TypeScript files.
Delegates to lint.py in single-file mode which auto-detects file type.
Python files always use --strict mode (pyright) since single-file is fast.

Usage: Receives JSON on stdin from Claude Code PostToolUse hook.
Exit codes:
  0 - Success (no violations or unsupported file type)
  2 - Violations found (stderr fed back to Claude)
"""

import json
import os
import subprocess
import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent

# Supported file extensions (must match lint.py's extension sets)
CPP_EXTENSIONS = {".cpp", ".h", ".hpp", ".hxx", ".hh", ".ino"}
PYTHON_EXTENSIONS = {".py"}
JS_EXTENSIONS = {".js", ".ts"}
SUPPORTED_EXTENSIONS = CPP_EXTENSIONS | PYTHON_EXTENSIONS | JS_EXTENSIONS


def get_file_path_from_hook_input() -> str | None:
    """Extract file path from Claude Code hook JSON input."""
    try:
        hook_data = json.load(sys.stdin)
        # PostToolUse provides tool_input with file_path
        tool_input = hook_data.get("tool_input", {})
        return tool_input.get("file_path")
    except (json.JSONDecodeError, KeyError):
        return None


def is_supported_file(file_path: str) -> bool:
    """Check if file is a supported type for linting."""
    return Path(file_path).suffix.lower() in SUPPORTED_EXTENSIONS


def main() -> int:
    """Main entry point for the hook."""
    file_path = get_file_path_from_hook_input()

    if not file_path:
        return 0

    # Resolve to absolute path
    if not os.path.isabs(file_path):
        file_path = os.path.join(PROJECT_ROOT, file_path)

    # Skip unsupported file types
    if not is_supported_file(file_path):
        return 0

    # Skip if file doesn't exist (might have been deleted)
    if not os.path.exists(file_path):
        return 0

    # Delegate to lint.py in single-file mode
    # Always use --strict for Python files (pyright on single file is fast)
    cmd = ["uv", "run", "ci/lint.py", "--strict", file_path]

    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        cwd=str(PROJECT_ROOT),
    )

    if result.returncode != 0:
        # Send lint output to stderr (fed back to Claude)
        rel_path = os.path.relpath(file_path, PROJECT_ROOT)
        print(f"Lint violations in {rel_path}:", file=sys.stderr)
        # Include both stdout and stderr from lint.py
        if result.stdout.strip():
            print(result.stdout.strip(), file=sys.stderr)
        if result.stderr.strip():
            print(result.stderr.strip(), file=sys.stderr)
        return 2

    return 0


if __name__ == "__main__":
    sys.exit(main())
