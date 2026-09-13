#!/usr/bin/env python3
"""
Claude Code hook script for compiling examples after edits.

When an example file in examples/ is edited, runs `fastled --just-compile`
on the example directory to verify it still compiles.

Usage: Receives JSON on stdin from Claude Code PostToolUse hook.
Exit codes:
  0 - Success (compiled OK or not an example file)
  1 - Compilation failed (stderr fed back to Claude)
"""

import json
import os
import sys
from pathlib import Path

from running_process import PIPE, RunningProcess


SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent


def get_file_path_from_hook_input() -> str | None:
    """Extract file path from Claude Code hook JSON input."""
    try:
        hook_data = json.load(sys.stdin)
        tool_input = hook_data.get("tool_input", {})
        return tool_input.get("file_path")
    except (json.JSONDecodeError, KeyError):
        return None


def get_example_dir(file_path: str) -> str | None:
    """Extract example directory name if file is inside examples/.

    Returns the example directory path (e.g. 'examples/Blink') or None.
    """
    rel_path = os.path.relpath(file_path, PROJECT_ROOT).replace("\\", "/")
    parts = rel_path.split("/")
    if len(parts) >= 2 and parts[0] == "examples":
        example_dir = os.path.join(PROJECT_ROOT, "examples", parts[1])
        if os.path.isdir(example_dir):
            return str(example_dir)
    return None


def main() -> int:
    """Main entry point for the hook."""
    file_path = get_file_path_from_hook_input()

    if not file_path:
        return 0

    # Resolve to absolute path
    if not os.path.isabs(file_path):
        file_path = os.path.join(PROJECT_ROOT, file_path)

    # Skip if file doesn't exist
    if not os.path.exists(file_path):
        return 0

    # Check if file is inside an example directory
    example_dir = get_example_dir(file_path)
    if not example_dir:
        return 0

    example_name = os.path.basename(example_dir)
    print(f"Compiling example: {example_name}", file=sys.stderr)

    result = RunningProcess.run(
        ["fastled", "--just-compile", "--no-interactive", example_dir],
        stdout=PIPE,
        stderr=PIPE,
        encoding="utf-8",
        errors="replace",
        cwd=str(PROJECT_ROOT),
    )

    if result.returncode != 0:
        print(
            f"Compilation FAILED for example '{example_name}':",
            file=sys.stderr,
        )
        if result.stdout.strip():
            # Limit output to last 80 lines to avoid flooding
            lines = result.stdout.strip().splitlines()
            if len(lines) > 80:
                print(f"... ({len(lines) - 80} lines omitted)", file=sys.stderr)
                lines = lines[-80:]
            print("\n".join(lines), file=sys.stderr)
        if result.stderr.strip():
            lines = result.stderr.strip().splitlines()
            if len(lines) > 80:
                print(f"... ({len(lines) - 80} lines omitted)", file=sys.stderr)
                lines = lines[-80:]
            print("\n".join(lines), file=sys.stderr)
        return 1

    print(f"Example '{example_name}' compiled OK.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
