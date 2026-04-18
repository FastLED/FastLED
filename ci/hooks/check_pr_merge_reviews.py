#!/usr/bin/env python3
"""
PreToolUse hook: block `gh pr merge` when CodeRabbit has unresolved review comments.

Reads the tool-use payload from stdin (Claude Code hook contract). If the
Bash command looks like a PR merge, defers to coderabbit_addressor.py --check
to decide whether to allow or block.

Exits:
    0 — allow
    2 — block with message on stderr (Claude Code treats as deny)
    anything else — treated as soft failure, allow-through with warning
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from typing import Any, cast

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


MERGE_RE = re.compile(r"\bgh\s+pr\s+merge\b")


def main() -> int:
    try:
        payload = cast(dict[str, Any], json.load(sys.stdin))
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        return 0

    tool_name = cast(str, payload.get("tool_name", ""))
    if tool_name != "Bash":
        return 0

    tool_input = cast(dict[str, Any], payload.get("tool_input", {}))
    command = cast(str, tool_input.get("command", ""))
    if not MERGE_RE.search(command):
        return 0

    try:
        result = subprocess.run(
            ["uv", "run", "python", "ci/tools/coderabbit_addressor.py", "--check"],
            capture_output=True,
            text=True,
            timeout=30,
        )
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return 0

    if result.returncode == 0:
        return 0

    sys.stderr.write(result.stderr or "")
    sys.stderr.write(
        "\n[check_pr_merge_reviews] Blocking merge — run /address-reviews first.\n"
    )
    return 2


if __name__ == "__main__":
    sys.exit(main())
