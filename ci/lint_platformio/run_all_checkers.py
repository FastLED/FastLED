#!/usr/bin/env python3
"""PlatformIO-internal-usage linting (issue #2701).

Walks the repo, applies ``NoInternalPlatformIOChecker``, and prints any
violations.

WARN MODE (default): exits 0 even if violations are found. The companion
sweep PR will remove the violating call sites; until then we don't want
the linter to gate CI red.

To run as an error gate (CI green = no violations):
  - Pass ``--error`` on the CLI, OR
  - Set ``FASTLED_LINT_PLATFORMIO_ERROR=1`` in the env

To dump the current punchlist:
  uv run python ci/lint_platformio/run_all_checkers.py --list-violations
"""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

from ci.lint_platformio.check_no_internal_platformio import (
    NoInternalPlatformIOChecker,
)
from ci.util.check_files import FileContentChecker, MultiCheckerFileProcessor


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent


# Directories never walked. Cuts a huge amount of I/O on caches/build artifacts.
_SKIP_DIRS: frozenset[str] = frozenset(
    {
        ".build",
        ".pio",
        ".cache",
        ".venv",
        "node_modules",
        "__pycache__",
        ".mypy_cache",
        ".ruff_cache",
        "build",
        "dist",
        # Don't recurse into sibling worktrees from the canonical checkout —
        # they get linted on their own when active.
        "worktrees",
    }
)


# Files of interest. We only care about build/CI code paths.
_INTEREST_SUFFIXES: tuple[str, ...] = (
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
    ".ini",
    ".md",
)


def _collect_files(root: Path) -> list[str]:
    """Walk the repo and collect files of interest, pruning skip dirs."""
    files: list[str] = []
    for dirpath, dirnames, filenames in os.walk(root):
        # In-place prune
        dirnames[:] = [
            d for d in dirnames if d not in _SKIP_DIRS and not d.startswith(".build")
        ]
        for name in filenames:
            if name.endswith(_INTEREST_SUFFIXES):
                files.append(os.path.join(dirpath, name))
    files.sort()
    return files


def _collect_violations(
    checkers: list[FileContentChecker],
) -> dict[str, list[tuple[int, str]]]:
    all_violations: dict[str, list[tuple[int, str]]] = {}
    for checker in checkers:
        violations: dict[str, list[tuple[int, str]]] | None = getattr(
            checker, "violations", None
        )
        if violations:
            for path, issues in violations.items():
                all_violations.setdefault(path, []).extend(issues)
    return all_violations


def _print_violations(violations: dict[str, list[tuple[int, str]]]) -> int:
    total = 0
    for path, issues in sorted(violations.items()):
        try:
            rel = str(Path(path).relative_to(PROJECT_ROOT))
        except ValueError:
            rel = path
        rel = rel.replace("\\", "/")
        print(f"\n{rel}:")
        for line_no, msg in sorted(issues):
            print(f"  Line {line_no}: {msg}")
            total += 1
    return total


def run_platformio_lint(warn_only: bool | None = None) -> bool:
    """Run the PlatformIO-internal-usage checker.

    Args:
        warn_only: If True, always return True (warn mode). If False,
            return False when violations exist (error mode). If None,
            consult ``FASTLED_LINT_PLATFORMIO_ERROR`` (defaults to warn).

    Returns:
        True if clean OR warn-only mode is active.
    """
    if warn_only is None:
        warn_only = os.environ.get("FASTLED_LINT_PLATFORMIO_ERROR", "") != "1"

    files = _collect_files(PROJECT_ROOT)
    if not files:
        return True

    checkers: list[FileContentChecker] = [NoInternalPlatformIOChecker()]
    processor = MultiCheckerFileProcessor()
    processor.process_files_with_checkers(files, checkers)

    violations = _collect_violations(checkers)
    if not violations:
        print("✅ PlatformIO-internal-usage check: no violations")
        return True

    total = sum(len(v) for v in violations.values())
    mode_label = "WARN" if warn_only else "ERROR"
    print(f"\n{'=' * 80}")
    print(
        f"[PlatformIO-internal-usage] {mode_label} mode — "
        f"found {total} violation(s) in {len(violations)} file(s):"
    )
    print("=" * 80)
    _print_violations(violations)
    print(f"\n{'=' * 80}")

    if warn_only:
        print(
            f"⚠️  PlatformIO-internal-usage: {total} violation(s) (warn-only — "
            f"not failing CI). See issue #2701."
        )
        print("=" * 80)
        return True

    print(f"❌ PlatformIO-internal-usage: {total} violation(s) (gating).")
    print("=" * 80)
    return False


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Lint for forbidden internal PlatformIO build usage (issue #2701).",
    )
    parser.add_argument(
        "--error",
        action="store_true",
        help="Fail (exit 1) if any violation is found. Default is warn-only.",
    )
    parser.add_argument(
        "--list-violations",
        action="store_true",
        help="Alias for the default behavior: print the current violation punchlist.",
    )
    args = parser.parse_args()

    # --list-violations is the default warn-mode behavior; alias for clarity.
    warn_only = not args.error
    ok = run_platformio_lint(warn_only=warn_only)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
