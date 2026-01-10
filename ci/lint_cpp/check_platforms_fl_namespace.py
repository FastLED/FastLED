#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false, reportMissingParameterType=false
"""Linter to check that files in src/platforms/ either have 'namespace fl {' or an exception.

Files in src/platforms/**/*.{h,cpp} must either:
- Contain 'namespace fl {', OR
- Have a comment with '// ok no namespace fl' to opt out

This prevents namespace pollution from platform-specific implementations.
"""

import os
import unittest

from ci.util.check_files import (
    FileContent,
    FileContentChecker,
    MultiCheckerFileProcessor,
)
from ci.util.paths import PROJECT_ROOT


NUM_WORKERS = 1 if os.environ.get("NO_PARALLEL") else (os.cpu_count() or 1) * 4


SRC_ROOT = PROJECT_ROOT / "src"
PLATFORMS_DIR = SRC_ROOT / "platforms"


class PlatformsFlNamespaceChecker(FileContentChecker):
    """FileContentChecker implementation for ensuring platforms files have 'namespace fl' or exception."""

    def __init__(self):
        self.violations: dict[str, str] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed (only PROJECT_ROOT/src/platforms/ subdirectory files)."""
        # Fast normalized path check
        normalized_path = file_path.replace("\\", "/")

        # Must be in /src/platforms/ subdirectory
        if "/src/platforms/" not in normalized_path:
            return False

        # Must NOT be in examples or tests
        if "/examples/" in normalized_path or "/tests/" in normalized_path:
            return False

        # Only check .h, .cpp, and .hpp files
        return any(file_path.endswith(ext) for ext in [".h", ".cpp", ".hpp"])

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for 'namespace fl {' or exception comment."""
        content = file_content.content

        # Check for namespace fl
        if "namespace fl" in content:
            return []

        # Check for exception comment
        if "// ok no namespace fl" in content:
            return []

        # Neither found - violation
        self.violations[file_content.path] = (
            "Missing 'namespace fl {' or '// ok no namespace fl' comment"
        )
        return []  # We collect violations internally


def check_file_has_namespace_or_exception(file_path: str) -> tuple[bool, str]:
    """
    Check if a file has 'namespace fl {' or an exception comment.

    Args:
        file_path (str): Path to the file to check

    Returns:
        Tuple[bool, str]: (is_valid, error_message)
    """
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Check for namespace fl
        if "namespace fl" in content:
            return True, ""

        # Check for exception comment
        if "// ok no namespace fl" in content:
            return True, ""

        # Neither found - violation
        return False, "Missing 'namespace fl {' or '// ok no namespace fl' comment"

    except (UnicodeDecodeError, IOError):
        # Skip files that can't be read
        return True, ""


class PlatformsFlNamespaceTester(unittest.TestCase):
    def test_platforms_have_namespace_or_exception(self) -> None:
        """Check that files in src/platforms/ have 'namespace fl' or exception comment."""
        # Find all .h, .cpp, and .hpp files in src/platforms/
        files_to_check: list[str] = []
        try:
            if PLATFORMS_DIR.exists():
                for file_path in PLATFORMS_DIR.rglob("*"):
                    if file_path.is_file() and file_path.suffix in [
                        ".h",
                        ".cpp",
                        ".hpp",
                    ]:
                        files_to_check.append(str(file_path))
        except (OSError, IOError):
            pass

        if not files_to_check:
            print("✅ No files found in src/platforms/")
            return

        # Create checker and processor
        checker = PlatformsFlNamespaceChecker()
        processor = MultiCheckerFileProcessor()

        # Process all files in a single pass
        processor.process_files_with_checkers(files_to_check, [checker])

        # Get violations from checker
        violations_dict = checker.violations

        if violations_dict:
            violations: list[str] = []
            for file_path, error_msg in violations_dict.items():
                rel_path = os.path.relpath(file_path, PROJECT_ROOT)
                violations.append(f"{rel_path}: {error_msg}")

            msg = f"Found {len(violations)} file(s) in src/platforms/ without 'namespace fl' or exception:\n"
            for violation in violations:
                print(violation)
            self.fail(
                msg
                + "\n".join(violations)
                + "\n\n💡 Files in src/platforms/**/*.{h,cpp} must either:\n"
                + "  1. Contain 'namespace fl {', OR\n"
                + "  2. Have '// ok no namespace fl' comment to opt out"
            )
        else:
            print(
                f"✅ All {len(files_to_check)} files in src/platforms/ have 'namespace fl' or exception"
            )


if __name__ == "__main__":
    unittest.main()
