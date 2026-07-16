#!/usr/bin/env python3
"""Meson build file linting — discovers and runs all meson.build checkers.

Architecture mirrors ci/lint_cpp/run_all_checkers.py:
  - FileContentChecker interface from ci/util/check_files.py
  - MultiCheckerFileProcessor for single-load multi-pass
  - Scope-based checker organization
  - Violation collection and reporting
"""

from __future__ import annotations

import sys
from pathlib import Path

from ci.lint_meson.backslash_path_checker import BackslashPathChecker
from ci.lint_meson.path_norm_join_checker import PathNormJoinChecker
from ci.util.check_files import FileContentChecker, MultiCheckerFileProcessor


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent


def _collect_meson_files(root: Path) -> list[str]:
    """Find repository Meson files, excluding generated and nested worktrees."""
    excluded_directories = {".build", ".claude", ".git"}
    return sorted(
        str(path)
        for path in root.rglob("meson.build")
        if not excluded_directories.intersection(path.relative_to(root).parts)
    )


def _collect_violations(
    checkers: list[FileContentChecker],
) -> dict[str, list[tuple[int, str]]]:
    """Collect violations from all checkers."""
    all_violations: dict[str, list[tuple[int, str]]] = {}
    for checker in checkers:
        violations: dict[str, list[tuple[int, str]]] | None = getattr(
            checker, "violations", None
        )
        if violations:
            for path, issues in violations.items():
                all_violations.setdefault(path, []).extend(issues)
    return all_violations


def _print_violations(
    violations: dict[str, list[tuple[int, str]]],
) -> int:
    """Print violations and return total count."""
    total = 0
    for path, issues in sorted(violations.items()):
        # Show relative path
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


def run_meson_lint() -> bool:
    """Run all meson.build linters. Returns True if clean."""
    meson_files = _collect_meson_files(PROJECT_ROOT)
    if not meson_files:
        return True

    # All checkers — add new checkers here
    checkers: list[FileContentChecker] = [
        BackslashPathChecker(),
        PathNormJoinChecker(),
    ]

    processor = MultiCheckerFileProcessor()
    processor.process_files_with_checkers(meson_files, checkers)

    violations = _collect_violations(checkers)
    if not violations:
        return True

    total = sum(len(v) for v in violations.values())
    for checker in checkers:
        checker_violations: dict[str, list[tuple[int, str]]] | None = getattr(
            checker, "violations", None
        )
        if not checker_violations:
            continue
        name = type(checker).__name__
        count = sum(len(v) for v in checker_violations.values())
        print(f"\n{'=' * 80}")
        print(
            f"[{name}] Found {count} violation(s) in {len(checker_violations)} file(s):"
        )
        print("=" * 80)
        _print_violations(checker_violations)

    print(f"\n{'=' * 80}")
    print(f"❌ Total meson.build violations: {total}")
    print("=" * 80)
    return False


def main() -> None:
    """CLI entry point."""
    success = run_meson_lint()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
