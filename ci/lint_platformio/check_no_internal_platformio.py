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

The checker is in ERROR MODE — any violation fails the lint. PlatformIO may
be invoked only from the modules enumerated in
``SANCTIONED_PLATFORMIO_SURFACE`` (the deliberate #3279 comparison-only
backend and the user-facing PIO consumer surface) plus the shrinking
``MIGRATION_DEBT`` list. A new call site anywhere else is a hard failure.

To downgrade to warn-only, pass ``warn_only=True`` to ``run_platformio_lint``
(or set ``FASTLED_LINT_PLATFORMIO_WARN_ONLY=1``).

Matching is invocation-shaped, not mention-shaped: comment tails, backtick
spans, and prose inside string literals and docstrings are masked out before
patterns are applied, while argv lists like ``["pio", "run"]`` ARE caught.
"""

from __future__ import annotations

import re
from dataclasses import dataclass

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
#   * .git internal data
#
# NOTE: This list is about *documentation and user-facing surface* — places
# where merely naming PlatformIO is fine. Code that genuinely invokes
# PlatformIO belongs in SANCTIONED_PLATFORMIO_SURFACE below instead, so the
# two reasons for exemption stay distinguishable.
ALLOWED_PATH_FRAGMENTS: tuple[str, ...] = (
    # Linter itself
    "ci/lint_platformio/",
    # The linter's own test fixtures — they must contain the literal
    # forbidden patterns in order to assert that the checker catches them.
    "ci/tests/test_no_internal_platformio_checker.py",
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


# --- Sanctioned PlatformIO surface ----------------------------------------
#
# These are NOT debt. Each entry is a module whose *purpose* is to drive
# PlatformIO, kept deliberately per #3279 Phase 3, which retained the PIO
# backend as a comparison-only escape hatch rather than deleting it. Gating
# them would mean deleting the #3279 drift harness, and that is a policy
# decision for #3279 — not something a lint sweep gets to make.
#
# The checker's job after this list exists is to be a RATCHET: PlatformIO may
# be invoked from these files and nowhere else. Any new call site elsewhere
# fails CI.
SANCTIONED_PLATFORMIO_SURFACE: tuple[str, ...] = (
    # The PioCompiler backend itself — #3279 comparison-only, reachable only
    # behind _assert_explicit_platformio_backend()/FASTLED_BACKEND_PLATFORMIO_EXPLICIT.
    "ci/compiler/pio.py",
    # Declares the --platformio/--pio/--backend=platformio opt-in flags that
    # gate the above. Hard dependents: ci/check_backend_flag_drift.py,
    # ci/tests/test_check_backend_flag_drift.py, both drift workflows.
    "ci/compiler/argument_parser.py",
    # #3279-mandated nightly fbuild-vs-PIO parity monitor. Its whole function
    # is running both backends and diffing them.
    ".github/workflows/nightly_fbuild_pio_parity.yml",
    ".github/workflows/backend_flag_drift_teensy40.yml",
    "ci/check_backend_flag_drift.py",
    # Regression test for the *user-facing* PIO consumer surface: a fresh
    # clone must still build via `pio run` against the frozen root
    # platformio.ini (#3274/#3279/#3281). Deleting this removes the only
    # coverage that downstream PlatformIO users still work.
    ".github/workflows/build_clone_and_compile.yml",
    # `bash debug` and per-symbol size measurement read the root
    # platformio.ini, which fbuild deliberately does not parse.
    "ci/debug_attached.py",
    "ci/compiled_size.py",
    # Documented manual bloat-audit tools (#2905/#2886). They measure flags
    # injected through root platformio.ini, so they must use the PIO backend.
    "ci/bloat.py",
    "tests/measure_esp32s3_opt_ins.py",
)


# --- Migration debt -------------------------------------------------------
#
# Unlike the list above, these ARE debt and are expected to shrink to empty.
# Each entry needs a tracking issue and a concrete migration path.
MIGRATION_DEBT: tuple[tuple[str, str], ...] = (
    (
        ".github/workflows/build_esp_extra_libs.yml",
        "Builds ci/kitchensink via `pio run` to exercise PlatformIO's LDF "
        "dependency resolution, which fbuild does not yet reproduce. Port "
        "kitchensink to fbuild, then drop this entry. See #2701.",
    ),
)


def _is_path_allowed(file_path: str) -> bool:
    """Return True if file_path is exempt (allowlist / sanctioned / debt)."""
    normalized = file_path.replace("\\", "/")
    if any(fragment in normalized for fragment in ALLOWED_PATH_FRAGMENTS):
        return True
    if any(fragment in normalized for fragment in SANCTIONED_PLATFORMIO_SURFACE):
        return True
    return any(fragment in normalized for fragment, _reason in MIGRATION_DEBT)


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


_HINT_BY_CATEGORY: dict[str, str] = {
    category: hint for _pattern, category, hint in _PATTERNS
}


# Lines that look like in-file comments quoting the forbidden patterns
# (e.g. doc strings inside this checker itself). We skip those rather than
# blanket-allowlisting the file — keeps the checker honest about its own
# source.
_COMMENT_PREFIXES: tuple[str, ...] = ("#", "//", "*", "<!--", "/*", ";")


def _line_is_comment(line: str) -> bool:
    stripped = line.lstrip()
    return any(stripped.startswith(prefix) for prefix in _COMMENT_PREFIXES)


# --- Prose vs. invocation -------------------------------------------------
#
# The rule is "don't SHELL OUT to PlatformIO", not "don't MENTION PlatformIO".
# A docstring that explains why `pio run` is forbidden is not an invocation,
# and flagging it pushes authors to mangle their own prose to appease a
# linter. So before matching we mask out every region that is documentation
# rather than code:
#
#   * comment tails (``#``, ``;``, ``//``, ``::`` — per file type)
#   * backtick spans — the universal markdown/rST convention for quoting a
#     command *as text* (`` `pio run` ``)
#   * string literals, including multi-line triple-quoted docstrings
#
# String literals get a second, narrower look: a literal whose ENTIRE content
# is the forbidden token (``"--platformio"``) is an argparse flag definition
# or a subprocess argument — a real violation. A literal that merely contains
# the token amid other words (``help="... --platformio ..."``) is prose.
#
# Adjacent literals on one line are also joined and re-tested so that
# split-token argv lists like ``["pio", "run"]`` are still caught.

_TRIPLE_DELIMS: tuple[str, ...] = ('"""', "'''")

# Comment introducers per file extension. Order matters only for display.
_COMMENT_CHARS_BY_SUFFIX: tuple[tuple[tuple[str, ...], tuple[str, ...]], ...] = (
    ((".py", ".sh", ".bash", ".ps1", ".toml", ".yml", ".yaml"), ("#",)),
    ((".ini", ".cfg"), ("#", ";")),
    ((".cmd", ".bat"), ("::", "rem ", "REM ")),
    ((".md",), ()),
)

# Patterns that describe a *command* (as opposed to a flag). Only these are
# re-tested against joined string literals, where a false positive from
# accidental adjacency would be most likely.
_COMMAND_CATEGORIES: frozenset[str] = frozenset(
    {"pio_run", "platformio_run", "platformio_version_probe"}
)


def _comment_chars(file_path: str) -> tuple[str, ...]:
    normalized = file_path.replace("\\", "/").lower()
    for suffixes, chars in _COMMENT_CHARS_BY_SUFFIX:
        if normalized.endswith(suffixes):
            return chars
    return ("#",)


@dataclass(slots=True)
class MaskedLine:
    """One line split into the part that executes and the part that documents.

    Attributes:
        residue: The line with every prose region blanked out — what's left
            is bare code, e.g. a shell command in a YAML ``run:`` step.
        literals: Contents of string literals that opened and closed on this
            line. Checked separately, since ``"--platformio"`` is a real flag
            definition while ``"see --platformio for details"`` is prose.
        in_string: The triple-quote delimiter still open at end-of-line, or
            ``None``. Threaded into the next line so multi-line docstrings
            stay masked.
    """

    residue: str
    literals: list[str]
    in_string: str | None


def _is_contraction(line: str, i: int) -> bool:
    """True if ``line[i]`` is an apostrophe inside a word, e.g. ``don't``."""
    return (
        i > 0 and i + 1 < len(line) and line[i - 1].isalnum() and line[i + 1].isalnum()
    )


def _mask_prose(
    line: str, comment_chars: tuple[str, ...], in_string: str | None
) -> MaskedLine:
    """Split a line into executable residue and documentation."""
    residue: list[str] = []
    literals: list[str] = []
    n = len(line)
    i = 0

    # Resume a docstring left open by a previous line.
    if in_string is not None:
        idx = line.find(in_string)
        if idx == -1:
            return MaskedLine("", [], in_string)
        literals.append(line[:idx])
        i = idx + len(in_string)
        in_string = None

    while i < n:
        ch = line[i]

        if any(line.startswith(cc, i) for cc in comment_chars):
            break  # comment tail — everything after is prose

        if ch == "`":
            close = line.find("`", i + 1)
            if close == -1:
                break  # unterminated quote span; treat remainder as prose
            i = close + 1
            continue

        if ch == "'" and _is_contraction(line, i):
            # An apostrophe inside a word ("don't") is not a quote. Treating
            # it as one opens a phantom literal that swallows the rest of the
            # line — which would let `echo don't && pio run` slip the gate.
            residue.append(ch)
            i += 1
            continue

        if ch in "\"'":
            triple = next((d for d in _TRIPLE_DELIMS if line.startswith(d, i)), None)
            if triple is not None:
                close = line.find(triple, i + len(triple))
                if close == -1:
                    literals.append(line[i + len(triple) :])
                    return MaskedLine("".join(residue), literals, triple)
                literals.append(line[i + len(triple) : close])
                i = close + len(triple)
                continue
            j = i + 1
            while j < n and line[j] != ch:
                j += 2 if line[j] == "\\" else 1
            if j >= n:
                break  # unterminated literal; remainder is prose
            literals.append(line[i + 1 : j])
            i = j + 1
            continue

        residue.append(ch)
        i += 1

    return MaskedLine("".join(residue), literals, in_string)


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
        comment_chars = _comment_chars(file_content.path)
        in_string: str | None = None

        for line_number, line in enumerate(file_content.lines, 1):
            masked = _mask_prose(line, comment_chars, in_string)
            in_string = masked.in_string

            if _line_is_comment(line):
                continue

            category = self._first_match(masked.residue, masked.literals)
            if category is None:
                continue

            hint = _HINT_BY_CATEGORY[category]
            self.violations.setdefault(file_content.path, []).append(
                (line_number, f"[{category}] {hint}  >> {line.strip()}")
            )
        return []

    @staticmethod
    def _first_match(residue: str, literals: list[str]) -> str | None:
        """Return the category of the first real violation on a line, if any."""
        # Only argv-shaped tokens take part in the join. A literal containing
        # whitespace is a sentence, not a command word, and joining it would
        # resurrect exactly the prose false positives this pass removes.
        joined = " ".join(
            tok for tok in (lit.strip() for lit in literals) if tok and " " not in tok
        )

        for pattern, category, _hint in _PATTERNS:
            # Bare code — an actual shell command or YAML `run:` step.
            if pattern.search(residue):
                return category
            # A string literal that IS the token, e.g. argparse("--platformio").
            if any(pattern.fullmatch(lit.strip()) for lit in literals):
                return category
            # Split argv lists, e.g. ["pio", "run"].
            if category in _COMMAND_CATEGORIES and pattern.search(joined):
                return category
        return None
