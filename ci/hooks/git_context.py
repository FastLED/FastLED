#!/usr/bin/env python3
"""
UserPromptSubmit hook: provide context about recent git changes.

When the user submits a prompt, this hook checks for recent uncommitted changes
and recent commits, printing a brief summary to stdout. This gives the agent
awareness of the current repository state without the agent needing to run
git commands explicitly.

The output is advisory (stdout, exit 0) — it appears as context for the agent
but does not block the user's prompt.

Performance: runs `git status --porcelain` and `git log --oneline -5` which
are both fast (<100ms typically).

Exit codes:
- 0: Always (advisory only)
"""

import json
import sys
from pathlib import Path

from running_process import RunningProcess, TimeoutExpired


SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent


def run_cmd(cmd: list[str], timeout: int = 5) -> str:
    """Run a command and return stdout, or empty string on failure."""
    try:
        result = RunningProcess.run(
            cmd,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            cwd=str(PROJECT_ROOT),
            timeout=timeout,
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except (TimeoutExpired, FileNotFoundError, OSError, RuntimeError):
        pass
    return ""


def get_uncommitted_summary() -> str:
    """Get a brief summary of uncommitted changes."""
    status = run_cmd(["git", "status", "--porcelain"])
    if not status:
        return ""

    lines = status.splitlines()
    modified = sum(1 for l in lines if l.startswith(" M") or l.startswith("M "))
    added = sum(1 for l in lines if l.startswith("A ") or l.startswith("??"))
    deleted = sum(1 for l in lines if l.startswith(" D") or l.startswith("D "))

    parts: list[str] = []
    if modified:
        parts.append(f"{modified} modified")
    if added:
        parts.append(f"{added} added/untracked")
    if deleted:
        parts.append(f"{deleted} deleted")

    if not parts:
        return f"{len(lines)} changes"
    return ", ".join(parts)


def get_changed_files_brief() -> list[str]:
    """Get list of changed file paths (max 10)."""
    status = run_cmd(["git", "status", "--porcelain"])
    if not status:
        return []

    files: list[str] = []
    for line in status.splitlines()[:10]:
        # Format: "XY filename" or "XY filename -> newname"
        if len(line) >= 3:
            fname = line[3:].strip()
            if " -> " in fname:
                fname = fname.split(" -> ")[1]
            files.append(fname)
    return files


def main() -> int:
    # Read hook input (we don't use it, but must consume stdin)
    try:
        json.load(sys.stdin)
    except (json.JSONDecodeError, EOFError):
        pass

    # Get uncommitted changes summary
    summary = get_uncommitted_summary()
    if not summary:
        # Clean working tree — no context needed
        return 0

    # Get changed files
    changed_files = get_changed_files_brief()

    # Get current branch
    branch = run_cmd(["git", "rev-parse", "--abbrev-ref", "HEAD"])

    # Build context message
    parts = [f"GIT CONTEXT: Branch '{branch}' has uncommitted changes: {summary}"]
    if changed_files:
        file_list = ", ".join(changed_files[:5])
        if len(changed_files) > 5:
            file_list += f" (+{len(changed_files) - 5} more)"
        parts.append(f"  Changed: {file_list}")

    print("\n".join(parts))
    return 0


if __name__ == "__main__":
    sys.exit(main())
