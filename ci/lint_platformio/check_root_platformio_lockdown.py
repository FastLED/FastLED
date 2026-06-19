#!/usr/bin/env python3
"""Enforce justification + PR/SHA comments on every change to root `./platformio.ini`.

Root `./platformio.ini` is a frozen surface owned only by `bash autoresearch`
and `bash debug`. CI compile, size-check, bloat, and regression tests
consume `ci/boards.py` instead. To prevent silent drift, every change here
must carry an adjacent `; justification:` comment and an `; added-in:`
back-reference (PR number or SHA). See #3274 and the matching CodeRabbit
rule in `.coderabbit.yaml` (path: `platformio.ini`).

This check is diff-based: it compares the current `platformio.ini` against
the merge base with `origin/master` and flags any added non-comment line
that lacks an adjacent justification comment in the same diff hunk. It is
no-op when:

  - the file is unchanged,
  - the current branch is `master`/`main`,
  - we cannot determine a base (detached HEAD, no `origin/master`, etc.).

Default mode is WARN (exit 0). Pass `--error` or set
``FASTLED_LINT_ROOT_PLATFORMIO_ERROR=1`` to gate.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Iterable, NamedTuple


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
ROOT_PIO_INI = PROJECT_ROOT / "platformio.ini"

# An added non-comment, non-blank, non-section-header line that requires
# justification. Section headers like "[env:foo]" are scaffolding, not
# semantic config, so they're exempt. PlatformIO uses both ";" and "#"
# as comment leaders; only ";" is canonical for this file.
_SECTION_HEADER_RE = re.compile(r"^\s*\[[^\]]+\]\s*$")
_JUSTIFICATION_RE = re.compile(r";\s*justification\s*:", re.IGNORECASE)
_ADDED_IN_RE = re.compile(r";\s*added-in\s*:\s*(?:PR-\d+|[0-9a-f]{40})", re.IGNORECASE)


class Violation(NamedTuple):
    line_no: int  # 1-indexed line number in the NEW file
    line_text: str
    reason: str


def _run(cmd: list[str]) -> tuple[int, str]:
    """Run a git command, return (returncode, stdout). Stderr is discarded."""
    try:
        result = subprocess.run(
            cmd,
            cwd=str(PROJECT_ROOT),
            capture_output=True,
            text=True,
            check=False,
        )
        return result.returncode, result.stdout
    except (OSError, FileNotFoundError):
        return 1, ""


def _current_branch() -> str | None:
    code, out = _run(["git", "rev-parse", "--abbrev-ref", "HEAD"])
    if code != 0:
        return None
    branch = out.strip()
    if branch == "HEAD":
        return None
    return branch or None


def _resolve_base() -> str | None:
    """Pick a sensible base to diff against. Prefer origin/master."""
    for ref in ("origin/master", "origin/main", "master", "main"):
        code, _ = _run(["git", "rev-parse", "--verify", "--quiet", ref])
        if code == 0:
            return ref
    return None


def _diff_added_lines(base: str) -> list[tuple[int, str]]:
    """Return [(new_line_no, raw_line), ...] for additions in platformio.ini.

    Uses unified diff with zero context lines to make hunk header parsing
    simple. Each hunk header is `@@ -a,b +c,d @@`; we walk additions and
    advance `c` line-by-line.
    """
    code, diff = _run(["git", "diff", "--unified=3", base, "--", "platformio.ini"])
    if code != 0 or not diff:
        return []

    added: list[tuple[int, str]] = []
    new_line_no = 0
    in_hunk = False
    hunk_re = re.compile(r"^@@\s+-\d+(?:,\d+)?\s+\+(\d+)(?:,\d+)?\s+@@")
    for raw in diff.splitlines():
        if raw.startswith("+++") or raw.startswith("---"):
            continue
        m = hunk_re.match(raw)
        if m:
            in_hunk = True
            new_line_no = int(m.group(1))
            continue
        if not in_hunk:
            continue
        if raw.startswith("+"):
            added.append((new_line_no, raw[1:]))
            new_line_no += 1
        elif raw.startswith("-"):
            continue
        else:
            # context line — advance the new-file pointer
            new_line_no += 1
    return added


def _is_semantic_addition(line: str) -> bool:
    """Return True if this line is the kind that requires justification."""
    stripped = line.strip()
    if not stripped:
        return False
    if stripped.startswith(";") or stripped.startswith("#"):
        return False
    if _SECTION_HEADER_RE.match(stripped):
        return False
    return True


def _has_adjacent_justification(
    target_line_no: int, all_lines: Iterable[str]
) -> tuple[bool, bool]:
    """Look at the full new file for a `; justification:` and `; added-in:`
    comment within 3 lines before or after `target_line_no` (1-indexed).

    Returns (has_justification, has_added_in).

    Window is ±3 — tight enough that an unrelated nearby comment cannot
    satisfy the rule by accident, loose enough that a 2-line
    `; justification: / ; added-in:` block immediately above (or below)
    a small added env block still covers every line of the block.
    """
    lines = list(all_lines)
    lo = max(0, target_line_no - 1 - 3)
    hi = min(len(lines), target_line_no - 1 + 4)
    window = lines[lo:hi]
    has_just = any(_JUSTIFICATION_RE.search(line) for line in window)
    has_added = any(_ADDED_IN_RE.search(line) for line in window)
    return has_just, has_added


def _warn_only_from_env() -> bool:
    """Resolve warn-only mode from FASTLED_LINT_ROOT_PLATFORMIO_ERROR.

    Returns True (warn-only) unless the env var is explicitly set to "1".
    Callers that have an explicit preference (e.g. `--error` CLI flag)
    should bypass this helper and pass their own bool to `check`.
    """
    return os.environ.get("FASTLED_LINT_ROOT_PLATFORMIO_ERROR", "") != "1"


def check(warn_only: bool) -> bool:
    """Run the lockdown check. Return True if clean (or warn-only mode)."""
    if not ROOT_PIO_INI.exists():
        return True

    branch = _current_branch()
    if branch in (None, "master", "main"):
        # No-op on master/main or detached HEAD — nothing to gate.
        return True

    base = _resolve_base()
    if base is None:
        return True

    added = _diff_added_lines(base)
    if not added:
        return True

    # Load full new file for adjacency lookups.
    new_lines = ROOT_PIO_INI.read_text(encoding="utf-8", errors="replace").splitlines()

    violations: list[Violation] = []
    for line_no, raw in added:
        if not _is_semantic_addition(raw):
            continue
        has_just, has_added_in = _has_adjacent_justification(line_no, new_lines)
        if not has_just:
            violations.append(
                Violation(
                    line_no=line_no,
                    line_text=raw.rstrip(),
                    reason="missing adjacent `; justification: <reason>` comment",
                )
            )
        elif not has_added_in:
            violations.append(
                Violation(
                    line_no=line_no,
                    line_text=raw.rstrip(),
                    reason=(
                        "missing adjacent `; added-in: <PR-NNN | 40-char SHA>` "
                        "back-reference"
                    ),
                )
            )

    if not violations:
        print(
            "✅ Root platformio.ini lockdown: "
            f"{len(added)} added line(s) — all justified."
        )
        return True

    mode_label = "WARN" if warn_only else "ERROR"
    print()
    print("=" * 80)
    print(
        f"[root-platformio.ini-lockdown] {mode_label} mode — "
        f"{len(violations)} unjustified change(s) in platformio.ini:"
    )
    print("=" * 80)
    print(
        "Root ./platformio.ini is a frozen surface (#3274). Every added non-comment\n"
        "line must carry an adjacent `; justification: <reason>` AND\n"
        "`; added-in: PR-<NNN>` (or 40-char SHA) comment. See `.coderabbit.yaml`.\n"
    )
    for v in violations:
        print(f"  platformio.ini:{v.line_no}: {v.reason}")
        print(f"    | {v.line_text}")
    print()
    print(
        "Add a comment block like:\n"
        "  -D NEW_THING=1\n"
        "  ; justification: <which interactive flow needs this and why>\n"
        "  ; added-in: PR-<NNN>"
    )
    print("=" * 80)

    if warn_only:
        print(
            f"⚠️  root-platformio.ini-lockdown: {len(violations)} violation(s) "
            "(warn-only — not failing CI). Set FASTLED_LINT_ROOT_PLATFORMIO_ERROR=1 "
            "to gate, or pass --error."
        )
        return True

    return False


def main(argv: list[str] | None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Enforce justification comments on every change to root "
            "./platformio.ini (see #3274 and .coderabbit.yaml)."
        ),
    )
    parser.add_argument(
        "--error",
        action="store_true",
        help="Fail (exit 1) on any unjustified change. Default is warn-only.",
    )
    args = parser.parse_args(argv)
    # CLI --error always wins; otherwise honour the env-var fallback.
    warn_only = False if args.error else _warn_only_from_env()
    ok = check(warn_only=warn_only)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(None))
