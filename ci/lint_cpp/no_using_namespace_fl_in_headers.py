#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false, reportMissingParameterType=false
import os
import unittest

from ci.util.check_files import (
    FileContent,
    FileContentChecker,
    MultiCheckerFileProcessor,
    collect_files_to_check,
)
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"
PLATFORMS_DIR = os.path.join(SRC_ROOT, "platforms")

NUM_WORKERS = 1 if os.environ.get("NO_PARALLEL") else (os.cpu_count() or 1) * 4


class UsingNamespaceFlChecker(FileContentChecker):
    """FileContentChecker implementation for detecting 'using namespace fl;' in headers."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed (only PROJECT_ROOT/src/ directory headers)."""
        # Fast normalized path check
        normalized_path = file_path.replace("\\", "/")

        # Must be in /src/ subdirectory
        if "/src/" not in normalized_path:
            return False

        # Must NOT be in examples or tests (examples/*/src/ should not match)
        if "/examples/" in normalized_path or "/tests/" in normalized_path:
            return False

        # Skip FastLED.h specifically
        if "FastLED.h" in file_path:
            return False

        # Only check header files
        return any(file_path.endswith(ext) for ext in [".h", ".hpp"])

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for 'using namespace fl;' declarations."""
        violations: list[tuple[int, str]] = []

        for line_num, line in enumerate(file_content.lines, 1):
            # Skip comment lines
            if line.startswith("//"):
                continue
            # Check for 'using namespace fl;'
            if "using namespace fl;" in line:
                violations.append((line_num, line.strip()))

        if violations:
            self.violations[file_content.path] = violations

        return []  # We collect violations internally


class NoUsingNamespaceFlInHeaderTester(unittest.TestCase):
    def check_file(self, file_path: str) -> list[str]:
        if "FastLED.h" in file_path:
            return []
        failings: list[str] = []
        with open(file_path, "r", encoding="utf-8") as f:
            for line_number, line in enumerate(f, 1):
                if line.startswith("//"):
                    continue
                if "using namespace fl;" in line:
                    failings.append(f"{file_path}:{line_number}: {line.strip()}")
        return failings

    def test_no_using_namespace(self) -> None:
        """Searches through the program files to check for 'using namespace fl;' in headers."""
        # Use the new FileContentChecker-based approach
        src_dir = str(SRC_ROOT)

        # Collect files using collect_files_to_check
        files_to_check = collect_files_to_check([src_dir], extensions=[".h", ".hpp"])

        # Create checker and processor
        checker = UsingNamespaceFlChecker()
        processor = MultiCheckerFileProcessor()

        # Process all files in a single pass
        processor.process_files_with_checkers(files_to_check, [checker])

        # Get violations from checker
        violations = checker.violations

        if violations:
            all_failings: list[str] = []
            for file_path, line_info in violations.items():
                for line_num, line_content in line_info:
                    all_failings.append(f"{file_path}:{line_num}: {line_content}")

            msg = (
                f'Found {len(all_failings)} header file(s) "using namespace fl": \n'
                + "\n".join(all_failings)
            )
            for failing in all_failings:
                print(failing)
            self.fail(msg)
        else:
            print("No using namespace fl; found in headers.")


if __name__ == "__main__":
    unittest.main()
