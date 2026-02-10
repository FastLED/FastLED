#!/usr/bin/env python3
"""
Pre-command hook to block direct invocation of build system commands.

This hook prevents Claude from running low-level build commands directly,
which can bypass FastLED's build system safety checks and caching.

Exit codes:
- 0: Allow command
- 2: Block command (error message sent to Claude)
"""

import json
import os
import re
import sys


# DRY_RUN: Set to True to test without blocking commands
DRY_RUN = False

FORBIDDEN_COMMANDS = [
    "ninja",
    "meson",
    "clang",
    "clang++",
    "gcc",
    "g++",
    "gdb",
    "lldb",
    "pio",
    "platformio",
]

# Specific recommendations for each forbidden command
COMMAND_RECOMMENDATIONS = {
    "ninja": "use 'bash test' instead (FastLED build system handles ninja invocation)",
    "meson": "use 'bash test' instead (FastLED build system handles meson configuration)",
    "clang": "use 'bash test' instead (build system uses clang-tool-chain internally)",
    "clang++": "use 'bash test' instead (build system uses clang-tool-chain internally)",
    "gcc": "GCC is NOT SUPPORTED by FastLED - project requires Clang 21.1.5",
    "g++": "G++ is NOT SUPPORTED by FastLED - project requires Clang 21.1.5",
    "gdb": "use 'clang-tool-chain-lldb' instead (FastLED's LLDB wrapper)",
    "lldb": "use 'clang-tool-chain-lldb' instead (FastLED's LLDB wrapper)",
    "pio": "use 'bash compile', 'bash debug', or 'bash validate' instead",
    "platformio": "use 'bash compile', 'bash debug', or 'bash validate' instead",
}

FORBIDDEN_PATTERNS = [
    # Cache deletion patterns
    (
        r"rm\s+-rf\s+\.build",
        "rm -rf .build is forbidden - use 'bash test --clean' instead",
    ),
    (
        r"rm\s+-rf\s+\.fbuild",
        "rm -rf .fbuild is forbidden - use 'bash compile --clean' instead",
    ),
    # Fingerprint disabling
    (
        r"--no-fingerprint",
        "--no-fingerprint is forbidden - makes builds 10-100x slower, use 'bash test --clean' instead",
    ),
    # Wrong Python invocation
    (
        r"uv\s+run\s+python\s+test\.py",
        "uv run python test.py is forbidden - use 'bash test' or 'uv run test.py' instead",
    ),
]

FORBIDDEN_ENV_VARS = [
    "SCCACHE_DISABLE",
]

OVERRIDE_ENV_VAR = "FL_AGENT_ALLOW_ALL_CMDS"


def extract_command(tool_input: dict) -> str | None:
    """Extract the command string from tool input."""
    return tool_input.get("command")


def is_forbidden_command(command: str) -> tuple[str, str] | None:
    """
    Check if command starts with a forbidden standalone command.

    Returns (forbidden_command, recommendation) if found, None otherwise.
    """
    # Remove leading/trailing whitespace
    command = command.strip()

    # Check if command starts with any forbidden command as a standalone word
    for forbidden in FORBIDDEN_COMMANDS:
        # Match forbidden command at start, followed by whitespace or nothing
        pattern = rf"^{re.escape(forbidden)}(\s|$)"
        if re.match(pattern, command):
            recommendation = COMMAND_RECOMMENDATIONS.get(
                forbidden, f'set environment variable "{OVERRIDE_ENV_VAR}"'
            )
            return (forbidden, recommendation)

    return None


def check_forbidden_pattern(command: str) -> tuple[bool, str] | None:
    """
    Check if command matches any forbidden pattern.

    Returns (True, error_message) if forbidden pattern found, None otherwise.
    """
    for pattern, error_msg in FORBIDDEN_PATTERNS:
        if re.search(pattern, command):
            return (True, error_msg)
    return None


def check_forbidden_env_vars(command: str) -> tuple[bool, str] | None:
    """
    Check if command sets forbidden environment variables.

    Returns (True, error_message) if forbidden env var found, None otherwise.
    """
    for env_var in FORBIDDEN_ENV_VARS:
        # Match VAR=value or VAR=value command
        pattern = rf"\b{re.escape(env_var)}="
        if re.search(pattern, command):
            error_msg = (
                f"Setting {env_var} is forbidden. "
                f'If you really want to set this, use environment variable "{OVERRIDE_ENV_VAR}"'
            )
            return (True, error_msg)
    return None


def main():
    # Read JSON input from stdin
    try:
        input_data = json.load(sys.stdin)
    except json.JSONDecodeError:
        # If we can't parse input, allow the command
        sys.exit(0)

    # Only check Bash commands
    tool_name = input_data.get("tool_name", "")
    if tool_name != "Bash":
        sys.exit(0)

    # Extract command
    tool_input = input_data.get("tool_input", {})
    command = extract_command(tool_input)

    if not command:
        sys.exit(0)

    # Type narrowing: command is guaranteed to be str here
    assert command is not None

    # Check for override environment variable
    override_enabled = bool(os.environ.get(OVERRIDE_ENV_VAR))

    # Check 1: Forbidden commands
    forbidden_result = is_forbidden_command(command)
    if forbidden_result:
        forbidden_cmd, recommendation = forbidden_result

        if override_enabled:
            if DRY_RUN:
                print(
                    f"[DRY-RUN] Would allow '{forbidden_cmd}' due to {OVERRIDE_ENV_VAR}",
                    file=sys.stderr,
                )
            sys.exit(0)

        error_msg = f"{forbidden_cmd} is forbidden - {recommendation}"

        if DRY_RUN:
            print(f"[DRY-RUN] Would block: {error_msg}", file=sys.stderr)
            sys.exit(0)
        else:
            print(error_msg, file=sys.stderr)
            sys.exit(2)

    # Check 2: Forbidden patterns
    pattern_result = check_forbidden_pattern(command)
    if pattern_result:
        if override_enabled:
            if DRY_RUN:
                print(
                    f"[DRY-RUN] Would allow pattern match due to {OVERRIDE_ENV_VAR}",
                    file=sys.stderr,
                )
            sys.exit(0)

        _, error_msg = pattern_result
        if DRY_RUN:
            print(f"[DRY-RUN] Would block: {error_msg}", file=sys.stderr)
            sys.exit(0)
        else:
            print(error_msg, file=sys.stderr)
            sys.exit(2)

    # Check 3: Forbidden environment variables
    env_var_result = check_forbidden_env_vars(command)
    if env_var_result:
        if override_enabled:
            if DRY_RUN:
                print(
                    f"[DRY-RUN] Would allow env var due to {OVERRIDE_ENV_VAR}",
                    file=sys.stderr,
                )
            sys.exit(0)

        _, error_msg = env_var_result
        if DRY_RUN:
            print(f"[DRY-RUN] Would block: {error_msg}", file=sys.stderr)
            sys.exit(0)
        else:
            print(error_msg, file=sys.stderr)
            sys.exit(2)

    # Allow the command
    sys.exit(0)


if __name__ == "__main__":
    main()
