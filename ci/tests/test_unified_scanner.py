# pyright: reportUnknownMemberType=false
import os
import unittest

import pytest
from unified_scanner import BaseChecker, CheckResult, UnifiedFileScanner
from unified_scanner.checkers import (
    BannedDefineChecker,
    BannedHeadersChecker,
    NamespaceIncludeChecker,
    PragmaOnceChecker,
    StdNamespaceChecker,
)

from ci.util.paths import PROJECT_ROOT


NUM_WORKERS = 1 if os.environ.get("NO_PARALLEL") else (os.cpu_count() or 1) * 4


class TestUnifiedScanner(unittest.TestCase):
    """Single test that runs all file checkers."""

    @pytest.mark.slow
    def test_all_file_checkers(self) -> None:
        """Run all file checkers in a single filesystem scan."""

        # Register all checkers
        checkers: list[BaseChecker] = [
            PragmaOnceChecker(),
            NamespaceIncludeChecker(),
            BannedDefineChecker(),
            StdNamespaceChecker(),  # Now enabled with multiline comment handling
            BannedHeadersChecker(),  # Now enabled with full banned headers list
        ]

        # Create scanner
        scanner = UnifiedFileScanner(checkers, max_workers=NUM_WORKERS)

        # Scan directories (SINGLE filesystem scan)
        # Full coverage to match all old tests:
        # - src/ (all subdirectories including fl, fx, sensors, platforms)
        # - examples/ (for example sketches)
        # - tests/ (for test code)
        directories = [
            PROJECT_ROOT / "src",
            PROJECT_ROOT / "examples",
            PROJECT_ROOT / "tests",
        ]

        results = scanner.scan_directories(directories)

        # Pretty-print errors for CI detection
        if results:
            print("\n" + "=" * 80)
            print("CODE QUALITY VIOLATIONS DETECTED")
            print("=" * 80)

            # Group by checker
            by_checker: dict[str, list[CheckResult]] = {}
            for result in results:
                if result.checker_name not in by_checker:
                    by_checker[result.checker_name] = []
                by_checker[result.checker_name].append(result)

            # Print each checker's violations
            for checker_name, violations in sorted(by_checker.items()):
                print(f"\n[{checker_name}] {len(violations)} violation(s):")
                for violation in violations[:10]:  # Show first 10
                    print(violation.format_for_ci())

                if len(violations) > 10:
                    print(f"  ... and {len(violations) - 10} more violations")

            print("=" * 80)

            # Fail test if any ERROR-level violations
            error_count = sum(1 for r in results if r.severity == "ERROR")
            self.assertEqual(
                error_count,
                0,
                f"Found {error_count} ERROR-level violations (see above)",
            )


if __name__ == "__main__":
    unittest.main()
