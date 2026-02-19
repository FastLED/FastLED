#!/usr/bin/env python3
"""
Claude Code hook script that runs 'bash lint' at the end of each session.

This hook fires on the Stop event (when Claude finishes responding / session ends).
It runs the full project lint to catch any issues introduced during the session.

Usage: Receives JSON on stdin from Claude Code Stop hook.
Exit codes:
  0 - Success (lint passed or lint not applicable)
  2 - Lint violations found (stderr fed back to Claude)
"""

import subprocess
import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent


def main() -> int:
    """Run bash lint from the project root."""
    result = subprocess.run(
        ["uv", "run", "ci/lint.py"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        cwd=str(PROJECT_ROOT),
    )

    if result.returncode != 0:
        print("Lint failed at end of session:", file=sys.stderr)
        if result.stdout.strip():
            print(result.stdout.strip(), file=sys.stderr)
        if result.stderr.strip():
            print(result.stderr.strip(), file=sys.stderr)
        return 2

    return 0


if __name__ == "__main__":
    sys.exit(main())
