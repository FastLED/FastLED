#!/usr/bin/env python3
"""Checker for forbidden internal PlatformIO build invocations (issue #2701).

FastLED board builds use fbuild exclusively. Internal/CI build code
must not shell out to PlatformIO. This checker flags:

  * ``pio run`` invocations as a build step
  * ``platformio run`` / ``platformio --version`` invocations
  * Argparse acceptance or passing of ``--platformio`` / ``--pio``
  * Argparse acceptance or passing of ``--backend platformio``
  * Generation/staging of ``platformio.ini`` files as a build artifact

User-facing references stay allowed via the ALLOWED_PATH_FRAGMENTS
allowlist (README/install docs, ``examples/`` comments, ``library.json``
/ ``library.properties``, issue templates, etc.).

The checker is in WARN MODE — it reports violations but ``run_platformio_lint``
returns ``True`` by default so the linter does not gate CI red while the
follow-up sweep PR removes existing call sites. To promote the checker
to error-on-violation, pass ``warn_only=False`` (or set the
``FASTLED_LINT_PLATFORMIO_ERROR`` env var to ``1``).
"""

from __future__ import annotations

import re

from ci.util.check_files import FileContent, FileContentChecker


# --- Allowlist -------------------------------------------------------------
#
# Paths (substring match, normalized to forward slashes) where PlatformIO
# references are intentional and out of scope for the fbuild-only rule.
#
# Categories:
#   * User-facing docs (README, install instructions, examples/*)
#   * The PlatformIO library manifest (library.json / library.properties)
#   * Issue templates (.github/ISSUE_TEMPLATE/)
#   * The linter itself (this file documents the patterns it forbids)
#   * Backend comparison harness (ci/check_backend_flag_drift.py — when
#     it lands; intentionally calls both backends side-by-side)
#   * .git internal data
#
# NOTE: This is intentionally permissive. The point of the warn-mode first
# pass is to surface the current state of internal PlatformIO usage so the
# follow-up sweep PR has a punchlist.
ALLOWED_PATH_FRAGMENTS: tuple[str, ...] = (
    # Linter itself
    "ci/lint_platformio/",
    # The backend-comparison harness (planned, may not exist yet on master)
    "ci/check_backend_flag_drift.py",
    # User-facing docs
    "/README.md",
    "/docs/",
    # User examples — comments are allowed to mention PlatformIO usage
    "/examples/",
    # PlatformIO library manifest surface (this IS the PIO consumer API)
    "/library.json",
    "/library.properties",
    # GitHub issue templates
    "/.github/ISSUE_TEMPLATE/",
    # Pull request templates
    "/.github/PULL_REQUEST_TEMPLATE",
    # Git internals (worktree metadata, packed refs)
    "/.git/",
    # Claude harness configuration (not part of the build/CI surface). Match
    # only direct subdirs of the project root, NOT the worktree path itself
    # (worktrees live under `<repo>/.claude/worktrees/<branch>/...` and need
    # to be scanned just like the canonical checkout).
    "ci/hooks/",
    ".claude/agents/",
    ".claude/hooks/",
    ".claude/skills/",
    ".claude/settings.json",
    ".claude/settings.local.json",
    ".claude/commands/",
    ".claude/teensy",  # worktree-resident log files (.claude/teensy*_ci.log)
    "/.claude.log",
    # Agents docs intentionally document the (forbidden) historical patterns
    "/agents/docs/",
    "/agents/tasks/",
    "/agents/tests.md",
    "/agents/ci.md",
    "/agents/examples.md",
    # Hardware-autoresearch doc references PlatformIO env names for users
    "agents/docs/hardware-autoresearch.md",
    # Lessons / notes
    "/CLAUDE.md",
    # Wiki submodule (user-facing)
    "/wiki/",
    # Build artifacts and caches
    "/.build/",
    "/.pio/",
    "/.cache/",
    "/build_info",  # build_info.json files
    # Docker README mentions PIO for users
    "ci/docker_utils/README.md",
    "ci/docker_utils/task.md",
)


def _is_path_allowed(file_path: str) -> bool:
    """Return True if file_path is in the allowlist (substring match)."""
    normalized = file_path.replace("\\", "/")
    return any(fragment in normalized for fragment in ALLOWED_PATH_FRAGMENTS)


# --- Forbidden patterns ----------------------------------------------------
#
# Each entry is (compiled regex, human-readable category, fix-hint).
#
# Patterns are line-scoped (no multiline) and intentionally narrow so that:
#   * Comments that quote the forbidden patterns *as documentation* are NOT
#     flagged (we check for shell-style command position, not mere mention).
#   * Allowlisted contexts still catch the literal pattern in source — the
#     allowlist filters by path, not by line content.
#
# A line is skipped if it looks like an in-source comment (#, //) — pure
# documentation/explanation of the rule itself should not self-flag.
#
# NOTE: The patterns intentionally do NOT use word-boundary `\b` after
# `--pio` because `--platformio` also matches and we want to flag both.
# Each pattern is tested independently with `re.search`.

_PATTERNS: tuple[tuple[re.Pattern[str], str, str], ...] = (
    # `pio run` as a builder invocation
    (
        re.compile(r"\bpio\s+run\b"),
        "pio_run",
        "Forbidden: `pio run` as build invocation. Use fbuild instead.",
    ),
    # `platformio run` as a builder invocation
    (
        re.compile(r"\bplatformio\s+run\b"),
        "platformio_run",
        "Forbidden: `platformio run` as build invocation. Use fbuild instead.",
    ),
    # `platformio --version` (PIO availability probe in CI)
    (
        re.compile(r"\bplatformio\s+--version\b"),
        "platformio_version_probe",
        "Forbidden: `platformio --version` probe. Internal build no longer requires PlatformIO.",
    ),
    # `--backend platformio` flag passing/acceptance
    (
        re.compile(r"--backend[\s=]+platformio\b"),
        "backend_platformio_flag",
        "Forbidden: `--backend platformio`. fbuild is the only supported backend.",
    ),
    # `--platformio` flag (legacy bash compile/ci-compile path selector)
    (
        re.compile(r"(?<![\w-])--platformio(?![\w-])"),
        "platformio_flag",
        "Forbidden: `--platformio` flag. Drop the PlatformIO backend path.",
    ),
    # `--pio` flag (legacy short form)
    (
        re.compile(r"(?<![\w-])--pio(?![\w-])"),
        "pio_flag",
        "Forbidden: `--pio` flag. Drop the PlatformIO backend path.",
    ),
)


# Lines that look like in-file comments quoting the forbidden patterns
# (e.g. doc strings inside this checker itself). We skip those rather than
# blanket-allowlisting the file — keeps the checker honest about its own
# source.
_COMMENT_PREFIXES: tuple[str, ...] = ("#", "//", "*", "<!--", "/*")


def _line_is_comment(line: str) -> bool:
    stripped = line.lstrip()
    return any(stripped.startswith(prefix) for prefix in _COMMENT_PREFIXES)


# File extensions considered "build/CI code" where internal PIO usage is
# forbidden. Other files (e.g. .cpp, .h) are skipped — board source code is
# never expected to drive PlatformIO directly, and matching there produces
# false positives on enum names like `BackendPlatformio`.
_BUILD_CODE_SUFFIXES: tuple[str, ...] = (
    ".py",
    ".yml",
    ".yaml",
    ".sh",
    ".bash",
    ".ps1",
    ".cmd",
    ".bat",
    ".toml",
    ".cfg",
    ".ini",  # to catch generated platformio.ini emitters
    ".md",  # internal docs (agents/) excluded via allowlist; CI docs are not
)


class NoInternalPlatformIOChecker(FileContentChecker):
    """Flags internal PlatformIO build invocations outside the allowlist."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        normalized = file_path.replace("\\", "/")
        if not normalized.endswith(_BUILD_CODE_SUFFIXES):
            return False
        if _is_path_allowed(normalized):
            return False
        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        for line_number, line in enumerate(file_content.lines, 1):
            if _line_is_comment(line):
                continue
            for pattern, category, hint in _PATTERNS:
                if pattern.search(line):
                    self.violations.setdefault(file_content.path, []).append(
                        (line_number, f"[{category}] {hint}  >> {line.strip()}")
                    )
                    # one violation per line is enough
                    break
        return []
