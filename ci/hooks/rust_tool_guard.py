#!/usr/bin/env python3
"""
Pre-command hook to force Rust/fbuild builds through `soldr <tool>`.

Background
----------
fbuild ships its own `tool_guard.py` PreToolUse hook that rewrites every
`cargo` / `rustc` / `rustfmt` / `clippy-driver` invocation to run under
`soldr` (https://github.com/FastLED/fbuild#429). soldr seeds zccache + a
shared target-tree cache, turning multi-minute cold cargo builds into
second-long warm hits.

Claude Code loads hooks from the **session project root**, not from the
directory where commands execute. When an agent runs in this FastLED
session and then `cd`s into a fbuild dev checkout (e.g. under clud-pr
against a fbuild issue, or driving fbuild tooling from FastLED), the
fbuild-side hook is never consulted — the agent runs bare `cargo build`
and burns minutes that should have been seconds.

This hook closes the gap by enforcing `soldr <tool>` at the FastLED
session root, where it does fire regardless of cwd.

Trigger
-------
The hook blocks a Bash command when:
  1. The first token (after any `VAR=value` env-var prefix) is one of
     `cargo`, `rustc`, `rustfmt`, `clippy-driver`, OR the command
     contains ` <tool> ` mid-chain (catches `cd /some/path && cargo
     build` and `(cd x; cargo …)`).
  2. AND the matched token is not already prefixed with `soldr`.

A Cargo.toml-proximity heuristic was considered (per the issue text)
but rejected: Claude Code spawns hooks with cwd pinned to the session
project root (`cd "$d"` in `.claude/settings.json`), so any
`find_cargo_toml(os.getcwd())` walk runs from FastLED root regardless
of where the bash command will actually execute (`cd elsewhere &&
cargo build`). That makes the Cargo.toml gate either silently miss
cross-project cases or hard-block any `cargo` anywhere. The tool-name
check by itself is precise enough — false positives for non-Rust
shell scripts literally named `cargo`/`rustc`/`rustfmt`/`clippy-
driver` are vanishingly rare, and the override env var handles them.

Exit codes
----------
- 0: allow command
- 2: block command (error message on stderr -> Claude treats as deny)

Override
--------
Set `FL_AGENT_ALLOW_BARE_CARGO=1` in the command string or process
environment to bypass this check. Use sparingly — e.g. for `cargo
--version` capability probes that genuinely don't need soldr's cache,
or for the rare non-Rust shell script that shadows the name.

Related
-------
- FastLED/fbuild#429: upstream Claude Code feature request for
  per-directory hook discovery (root-cause investigation).
- FastLED/fbuild#424 / #427: the PR stack where the soldr-bypass
  observation first surfaced (~15 min of wall time burned on cold
  cargo builds before someone caught it).
- FastLED #2871: this hook's tracking issue.
"""

from __future__ import annotations

import json
import os
import re
import sys


RUST_TOOLS = ("cargo", "rustc", "rustfmt", "clippy-driver")

OVERRIDE_ENV_VAR = "FL_AGENT_ALLOW_BARE_CARGO"

# Match VAR=value (with optional quotes) at the start of the command.
# Mirrors check_forbidden_commands.parse_env_vars_from_command — kept
# local so this hook has no intra-repo import dependency.
_ENV_PREFIX_RE = re.compile(
    r'^([A-Z_][A-Z0-9_]*)=((?:[^\s"\']|"[^"]*"|\'[^\']*\')+)\s+'
)


def strip_env_prefix(command: str) -> tuple[str, dict[str, str]]:
    """Strip leading `VAR=value` env-var assignments. Return (rest, vars)."""
    env_vars: dict[str, str] = {}
    rest = command.lstrip()
    while True:
        m = _ENV_PREFIX_RE.match(rest)
        if not m:
            break
        name = m.group(1)
        value = m.group(2)
        if value.startswith('"') and value.endswith('"'):
            value = value[1:-1]
        elif value.startswith("'") and value.endswith("'"):
            value = value[1:-1]
        env_vars[name] = value
        rest = rest[m.end() :]
    return rest, env_vars


def _matches_rust_tool(command: str) -> str | None:
    """Return the matched Rust tool name, or None.

    Matches two locations:
      1. First token (after env-var prefix) is one of RUST_TOOLS.
      2. Mid-chain occurrence: ` <tool> ` somewhere in the command
         (catches `cd /x && cargo build`, `(cd y; rustc …)`, etc.).
    """
    rest, _ = strip_env_prefix(command.lstrip())
    # If already running under soldr, treat as compliant.
    if re.match(r"soldr(\s|$)", rest):
        return None
    for tool in RUST_TOOLS:
        # Position 1: first token.
        if re.match(rf"{re.escape(tool)}(\s|$)", rest):
            return tool
        # Position 2: mid-chain. Match the tool when preceded by a
        # shell separator (`&&`, `||`, `;`, `(`, `|`, whitespace) so we
        # don't match substrings like `mycargo` or `--rustflags=…`.
        # Python's `re` doesn't support variable-width lookbehind, so
        # do the `soldr` exclusion as a second pass: walk every
        # occurrence and check the immediately-preceding non-space
        # token.
        for m in re.finditer(rf"(?:^|[;&|()\s]){re.escape(tool)}(?:\s|$)", rest):
            before = rest[: m.start()].rstrip()
            last_token = before.rsplit(None, 1)[-1] if before else ""
            if last_token == "soldr":
                continue
            return tool
    return None


def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except json.JSONDecodeError:
        return 0

    if payload.get("tool_name") != "Bash":
        return 0

    tool_input = payload.get("tool_input", {})
    command = tool_input.get("command")
    if not isinstance(command, str) or not command.strip():
        return 0

    # Override check: command-prefixed or process env.
    _, command_env_vars = strip_env_prefix(command)
    override_in_command = command_env_vars.get(OVERRIDE_ENV_VAR, "") in (
        "1",
        "true",
        "True",
        "TRUE",
    )
    override_in_env = os.environ.get(OVERRIDE_ENV_VAR, "") in (
        "1",
        "true",
        "True",
        "TRUE",
    )
    if override_in_command or override_in_env:
        return 0

    matched = _matches_rust_tool(command)
    if not matched:
        return 0

    # Build the suggested rewrite: prefix the offending token with soldr.
    # We can't safely rewrite mid-chain commands (quoting, subshells),
    # so emit guidance instead of an exact substitution.
    rest_no_env, _ = strip_env_prefix(command.lstrip())
    if re.match(rf"{re.escape(matched)}(\s|$)", rest_no_env):
        suggested = f"soldr {rest_no_env}"
    else:
        suggested = (
            f"replace each `{matched} …` invocation in this chain with "
            f"`soldr {matched} …`"
        )

    msg = (
        f"Bare `{matched}` invocation is blocked: must run through "
        f"`soldr {matched}` so zccache + soldr's target-tree cache fire "
        f"(turns multi-minute cold builds into seconds-long warm hits).\n"
        f"  Try: {suggested}\n"
        f"  Background: FastLED #2871, FastLED/fbuild#429.\n"
        f"  Override (sparingly — `--version`, capability probes, non-Rust "
        f"shell scripts that shadow the name): {OVERRIDE_ENV_VAR}=1 {command}"
    )
    print(msg, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
