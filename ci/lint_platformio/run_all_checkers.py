#!/usr/bin/env python3
"""PlatformIO-internal-usage linting (issue #2701).

Walks the repo, applies ``NoInternalPlatformIOChecker``, and prints any
violations.

ERROR MODE (default since the #2701 sweep): any violation fails the lint.
PlatformIO may be invoked only from the explicitly enumerated surface in
``check_no_internal_platformio.SANCTIONED_PLATFORMIO_SURFACE``; a new call
site anywhere else is a hard failure.

To downgrade to warn-only (escape hatch for bisecting a bad sweep):
  - Pass ``--warn-only`` on the CLI, OR
  - Set ``FASTLED_LINT_PLATFORMIO_WARN_ONLY=1`` in the env

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
from ci.lint_platformio.check_root_platformio_lockdown import (
    check as run_root_platformio_lockdown_lint,
)
from ci.util.check_files import FileContentChecker, MultiCheckerFileProcessor


__all__ = [
    "run_platformio_lint",
    "run_root_platformio_lockdown_lint",
]


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


def _git_ignored(root: Path, paths: list[str]) -> set[str]:
    """Subset of `paths` that git ignores.

    Returns an empty set when git is unavailable or errors, so the checker
    still works outside a checkout -- failing open here only means scanning a
    few extra files, never missing a real one.
    """
    if not paths:
        return set()

    # Feed repo-relative POSIX paths: git echoes back exactly the spelling it
    # was given, so anything else makes the returned names impossible to map
    # onto the absolute paths the walk produced.
    rel_to_abs: dict[str, str] = {}
    for absolute in paths:
        try:
            rel = os.path.relpath(absolute, root).replace("\\", "/")
        except ValueError:
            continue  # different drive on Windows; cannot be repo-relative
        rel_to_abs[rel] = absolute
    if not rel_to_abs:
        return set()

    try:
        from running_process import RunningProcess  # noqa: PLC0415 - lazy

        # `git check-ignore --stdin` echoes back only the ignored paths and
        # exits 1 when none match, so check=False.
        result = RunningProcess.run(
            ["git", "check-ignore", "--stdin"],
            input="\n".join(rel_to_abs) + "\n",
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            cwd=str(root),
            check=False,
            timeout=60,
        )
    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import (  # noqa: PLC0415 - lazy
            handle_keyboard_interrupt,
        )

        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        # Fail open: git missing, not a checkout, or check-ignore erroring just
        # means we scan a few extra files. Never a missed violation.
        return set()

    out = result.stdout or ""
    ignored: set[str] = set()
    for line in out.splitlines():
        rel = line.strip().replace("\\", "/")
        if rel in rel_to_abs:
            ignored.add(rel_to_abs[rel])
    return ignored


def _collect_files(root: Path) -> list[str]:
    """Walk the repo and collect files of interest, pruning skip dirs.

    Git-ignored files are excluded. They cannot reach CI -- a developer's
    local scratch script is not repository content -- so gating on them just
    fails `bash lint` for something the reviewer will never see. This bit
    people with a root `tmp.sh` containing `pio run -v`: ignored by
    .gitignore, invisible to CI, and yet a hard lint failure locally.
    """
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

    ignored = _git_ignored(root, files)
    if ignored:
        files = [f for f in files if f not in ignored]
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
            consult ``FASTLED_LINT_PLATFORMIO_WARN_ONLY`` (defaults to error).

    Returns:
        True if clean OR warn-only mode is active.
    """
    if warn_only is None:
        warn_only = os.environ.get("FASTLED_LINT_PLATFORMIO_WARN_ONLY", "") == "1"

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


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Lint for forbidden internal PlatformIO build usage (issue #2701).",
    )
    parser.add_argument(
        "--error",
        action="store_true",
        help="Deprecated no-op: error mode is now the default.",
    )
    parser.add_argument(
        "--warn-only",
        action="store_true",
        help="Report violations but exit 0. Escape hatch; default is to fail.",
    )
    parser.add_argument(
        "--list-violations",
        action="store_true",
        help="Print the current violation punchlist without failing.",
    )
    return parser


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    """Parse ``argv`` (defaults to ``sys.argv[1:]``). Split out for testing."""
    return build_parser().parse_args(argv)


def resolve_warn_only(args: argparse.Namespace) -> bool:
    """Map parsed flags to warn-only mode. ``--error`` is a deprecated no-op."""
    return bool(args.warn_only or args.list_violations)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    ok = run_platformio_lint(warn_only=resolve_warn_only(args))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
