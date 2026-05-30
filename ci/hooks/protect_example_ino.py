#!/usr/bin/env python3
"""
PreToolUse hook for Write: prevent spurious creation of new example .ino files.

The examples/ tree should stay minimal. Agents tend to spin up new one-off
.ino sketches to test functionality; instead, new functionality should be
exercised inside the existing examples/AutoResearch/AutoResearch.ino sketch
(the canonical scratch target for live/device testing).

This hook blocks the creation of a NEW .ino file anywhere under examples/.
Editing an existing .ino (the file already exists on disk) is always allowed.

Override (FL_* directive, same convention as the other FastLED hooks):
  1. In-content directive — prepend a comment containing FL_AGENT_ALLOW_NEW_EXAMPLE
     to the file you are writing. This documents the deliberate intent in the file
     itself. Example first line:
         // FL_AGENT_ALLOW_NEW_EXAMPLE
  2. Environment variable — launch Claude with FL_AGENT_ALLOW_NEW_EXAMPLE=1.

Exit codes:
- 0: Allow the write
- 2: Block the write (error message sent to agent)
"""

import json
import os
import sys


# Doubles as an environment variable AND an in-content directive token
# (the Write tool has no command string to prepend the env var to).
OVERRIDE_ENV_VAR = "FL_AGENT_ALLOW_NEW_EXAMPLE"


def main() -> int:
    try:
        input_data = json.load(sys.stdin)
    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt

        handle_keyboard_interrupt(ki)
        raise
    except json.JSONDecodeError as exc:
        print(
            f"protect_example_ino: invalid hook payload: {exc}",
            file=sys.stderr,
        )
        return 2

    tool_name = input_data.get("tool_name", "")
    if tool_name != "Write":
        return 0

    tool_input = input_data.get("tool_input", {})
    file_path = tool_input.get("file_path", "")
    if not file_path:
        return 0

    # Environment override (human escape hatch, consistent with other hooks).
    if os.environ.get(OVERRIDE_ENV_VAR) in ("1", "true", "True", "TRUE"):
        return 0

    # In-content directive override: agent prepends the FL_* directive as a
    # leading comment. Restrict to the first non-empty line (and require it to
    # be a comment) so the token cannot accidentally bypass the hook by merely
    # appearing somewhere in the file body.
    content = tool_input.get("content", "") or ""
    first_non_empty_line = ""
    for line in content.splitlines():
        if line.strip():
            first_non_empty_line = line.strip()
            break
    if (
        first_non_empty_line.startswith("//")
        and OVERRIDE_ENV_VAR in first_non_empty_line
    ):
        return 0

    # Normalize to forward slashes.
    file_path = file_path.replace("\\", "/")

    # Project root = directory two levels above this hook (ci/hooks/ -> root).
    project_root = os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ).replace("\\", "/")

    if file_path.startswith(project_root):
        rel_path = file_path[len(project_root) :].lstrip("/")
    else:
        norm_file = os.path.normpath(file_path).replace("\\", "/")
        norm_root = os.path.normpath(project_root).replace("\\", "/")
        if norm_file.startswith(norm_root):
            rel_path = norm_file[len(norm_root) :].lstrip("/")
        else:
            # Outside the project root — not our concern.
            return 0

    # Only guard .ino files under examples/.
    if not rel_path.startswith("examples/") or not rel_path.endswith(".ino"):
        return 0

    # If the file already exists, this is an edit/overwrite — always allowed.
    full_path = os.path.join(project_root, rel_path)
    if os.path.exists(full_path):
        return 0

    error_lines = [
        f"BLOCKED: Creating a new example sketch '{rel_path}'.",
        "",
        "The examples/ tree is kept intentionally minimal. New one-off .ino",
        "sketches should NOT be created to test functionality.",
        "",
        "What to do instead:",
        "  Exercise new functionality inside the existing scratch sketch:",
        "      examples/AutoResearch/AutoResearch.ino",
        "  That is the canonical target for testing new functionality.",
        "",
        "If you genuinely need a brand-new example, force creation with the FL_* directive:",
        f"  1. Prepend a comment containing {OVERRIDE_ENV_VAR} to the file content, e.g.:",
        f"         // {OVERRIDE_ENV_VAR}",
        f"  2. Or launch Claude with the env var: {OVERRIDE_ENV_VAR}=1",
    ]
    print("\n".join(error_lines), file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
