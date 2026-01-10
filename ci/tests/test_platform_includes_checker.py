#!/usr/bin/env python3
"""
Unit tests for the platform includes checker.

Tests that the PlatformIncludesChecker correctly identifies deep platform header
includes in platform-agnostic code (src/fl/**).
"""

import unittest
from pathlib import Path

from ci.lint_cpp.platform_includes_checker import PlatformIncludesChecker
from ci.util.check_files import FileContent


class TestPlatformIncludesChecker(unittest.TestCase):
    """Test suite for PlatformIncludesChecker."""

    def setUp(self):
        """Set up test fixtures."""
        self.checker = PlatformIncludesChecker()

    def test_should_process_file_fl_directory(self):
        """Test that header files in src/fl/ are processed."""
        self.assertTrue(
            self.checker.should_process_file("/path/to/project/src/fl/test.h")
        )
        self.assertTrue(
            self.checker.should_process_file("/path/to/project/src/fl/subdir/test.hpp")
        )
        # .cpp files should NOT be processed (implementation files are exempt)
        self.assertFalse(
            self.checker.should_process_file("/path/to/project/src/fl/subdir/test.cpp")
        )

    def test_should_not_process_file_platforms_directory(self):
        """Test that files in src/platforms/ are NOT processed."""
        self.assertFalse(
            self.checker.should_process_file(
                "/path/to/project/src/platforms/esp/32/driver.h"
            )
        )
        self.assertFalse(
            self.checker.should_process_file(
                "/path/to/project/src/platforms/shared/int_windows.h"
            )
        )

    def test_should_not_process_file_tests_platforms_directory(self):
        """Test that files in tests/platforms/ are NOT processed."""
        self.assertFalse(
            self.checker.should_process_file(
                "/path/to/project/tests/platforms/esp/test.cpp"
            )
        )

    def test_should_process_file_examples_directory(self):
        """Test that header files in examples/ are processed."""
        # .ino files should NOT be processed (implementation files are exempt)
        self.assertFalse(
            self.checker.should_process_file(
                "/path/to/project/examples/Blink/Blink.ino"
            )
        )
        # Header files should be processed
        self.assertTrue(
            self.checker.should_process_file("/path/to/project/examples/Test/helper.h")
        )

    def test_deep_platform_header_violation(self):
        """Test that deep platform headers are detected as violations."""
        lines = [
            '#include "fl/int.h"',
            '#include "platforms/shared/int_windows.h"',  # VIOLATION
            '#include "platforms/esp/32/driver.h"',  # VIOLATION
        ]
        content = FileContent(
            path="/path/to/project/src/fl/test.h",
            content="\n".join(lines),
            lines=lines,
        )

        self.checker.check_file_content(content)

        self.assertIn("/path/to/project/src/fl/test.h", self.checker.violations)
        violations = self.checker.violations["/path/to/project/src/fl/test.h"]
        self.assertEqual(len(violations), 2)
        self.assertEqual(violations[0][0], 2)  # Line 2
        self.assertIn("platforms/shared/int_windows.h", violations[0][1])
        self.assertEqual(violations[1][0], 3)  # Line 3
        self.assertIn("platforms/esp/32/driver.h", violations[1][1])

    def test_top_level_platform_header_allowed(self):
        """Test that top-level platform headers (platforms/*.h) are allowed."""
        lines = [
            '#include "platforms/int.h"',  # ALLOWED (top-level)
            '#include "platforms/io.h"',  # ALLOWED (top-level)
        ]
        content = FileContent(
            path="/path/to/project/src/fl/test.h",
            content="\n".join(lines),
            lines=lines,
        )

        self.checker.check_file_content(content)

        self.assertNotIn("/path/to/project/src/fl/test.h", self.checker.violations)

    def test_exception_comment_ok_platform_headers(self):
        """Test that '// ok platform headers' comment allows deep includes."""
        lines = [
            '#include "platforms/shared/int_windows.h"  // ok platform headers',
            '#include "platforms/esp/32/driver.h"  // OK PLATFORM HEADERS',
        ]
        content = FileContent(
            path="/path/to/project/src/fl/test.h",
            content="\n".join(lines),
            lines=lines,
        )

        self.checker.check_file_content(content)

        self.assertNotIn("/path/to/project/src/fl/test.h", self.checker.violations)

    def test_exception_comment_ok_dash_dash_platform_headers(self):
        """Test that '// ok -- platform headers' comment allows deep includes."""
        lines = [
            '#include "platforms/shared/int_windows.h"  // ok -- platform headers',
        ]
        content = FileContent(
            path="/path/to/project/src/fl/test.h",
            content="\n".join(lines),
            lines=lines,
        )

        self.checker.check_file_content(content)

        self.assertNotIn("/path/to/project/src/fl/test.h", self.checker.violations)

    def test_fl_header_includes_allowed(self):
        """Test that fl/ headers are always allowed."""
        lines = [
            '#include "fl/int.h"',
            '#include "fl/stl/vector.h"',
            '#include "fl/math.h"',
        ]
        content = FileContent(
            path="/path/to/project/src/fl/test.h",
            content="\n".join(lines),
            lines=lines,
        )

        self.checker.check_file_content(content)

        self.assertNotIn("/path/to/project/src/fl/test.h", self.checker.violations)

    def test_multiline_comment_skipping(self):
        """Test that includes inside multi-line comments are skipped."""
        lines = [
            "/* Comment block",
            '#include "platforms/shared/int_windows.h"',  # Inside comment
            "*/",
            '#include "platforms/esp/32/driver.h"',  # VIOLATION (not in comment)
        ]
        content = FileContent(
            path="/path/to/project/src/fl/test.h",
            content="\n".join(lines),
            lines=lines,
        )

        self.checker.check_file_content(content)

        self.assertIn("/path/to/project/src/fl/test.h", self.checker.violations)
        violations = self.checker.violations["/path/to/project/src/fl/test.h"]
        self.assertEqual(len(violations), 1)
        self.assertEqual(violations[0][0], 4)  # Line 4 only

    def test_single_line_comment_skipping(self):
        """Test that includes inside single-line comments are skipped."""
        lines = [
            '// #include "platforms/shared/int_windows.h"',  # Comment
            '#include "platforms/esp/32/driver.h"',  # VIOLATION
        ]
        content = FileContent(
            path="/path/to/project/src/fl/test.h",
            content="\n".join(lines),
            lines=lines,
        )

        self.checker.check_file_content(content)

        self.assertIn("/path/to/project/src/fl/test.h", self.checker.violations)
        violations = self.checker.violations["/path/to/project/src/fl/test.h"]
        self.assertEqual(len(violations), 1)
        self.assertEqual(violations[0][0], 2)  # Line 2 only

    def test_no_violations_empty_file(self):
        """Test that empty files produce no violations."""
        lines: list[str] = []
        content = FileContent(
            path="/path/to/project/src/fl/test.h",
            content="",
            lines=lines,
        )

        self.checker.check_file_content(content)

        self.assertNotIn("/path/to/project/src/fl/test.h", self.checker.violations)

    def test_windows_path_separators(self):
        """Test that Windows path separators are handled correctly."""
        # Use Windows-style path separator
        self.assertTrue(
            self.checker.should_process_file("C:\\path\\to\\project\\src\\fl\\test.h")
        )
        self.assertFalse(
            self.checker.should_process_file(
                "C:\\path\\to\\project\\src\\platforms\\esp\\driver.h"
            )
        )

    def test_case_insensitive_exception_comment(self):
        """Test that exception comments are case-insensitive."""
        lines = [
            '#include "platforms/shared/int_windows.h"  // OK PLATFORM HEADERS',
            '#include "platforms/esp/32/driver.h"  // Ok Platform Headers',
        ]
        content = FileContent(
            path="/path/to/project/src/fl/test.h",
            content="\n".join(lines),
            lines=lines,
        )

        self.checker.check_file_content(content)

        self.assertNotIn("/path/to/project/src/fl/test.h", self.checker.violations)


if __name__ == "__main__":
    unittest.main()
