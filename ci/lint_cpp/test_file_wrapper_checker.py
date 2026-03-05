#!/usr/bin/env python3
"""Checker to ensure test files use FL_TEST_FILE(FL_FILEPATH) wrapper."""

import re
from pathlib import Path

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


TESTS_ROOT = PROJECT_ROOT / "tests"

# Directories exempt from FL_TEST_FILE requirement
EXEMPT_DIRS = {
    TESTS_ROOT / "shared",
    TESTS_ROOT / "profile",
    TESTS_ROOT / "data",
    TESTS_ROOT / "manual",
    TESTS_ROOT / "testing",
}

# Individual files exempt from FL_TEST_FILE requirement
EXEMPT_FILES = {
    "doctest_main.cpp",
    "test.h",
    "test_pch.h",
    "doctest.h",
    "test_config.py",
    "test_helpers.hpp",  # Helper file, not a test file
}

# Files matching these patterns are exempt (helper utilities, not test files)
EXEMPT_PATTERNS = {
    "synth.hpp",
    "audio_context.hpp",
    "auto_gain.hpp",
    "gain.hpp",
    "frequency_bin_mapper.hpp",
    "noise_floor_tracker.hpp",
    "signal_conditioner.hpp",
    "spectral_equalizer.hpp",
    "mic_response_data.hpp",
    "test_utils",  # Directories with test utilities
    "asio",  # ASIO server standalone tests with main()
    "animartrix_detail",  # Animartrix helper implementations
}

# Pattern to match FL_TEST_FILE usage
_FL_TEST_FILE_RE = re.compile(r"^\s*FL_TEST_FILE\s*\(\s*FL_FILEPATH\s*\)")


class TestFileWrapperChecker(FileContentChecker):
    """Checker that ensures test .cpp and .hpp files use FL_TEST_FILE(FL_FILEPATH) { ... }."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        if not file_path.startswith(str(TESTS_ROOT)):
            return False

        if not file_path.endswith((".cpp", ".hpp")):
            return False

        # Skip .cpp.hpp utility files (e.g., server_thread.cpp.hpp)
        if file_path.endswith(".cpp.hpp"):
            return False

        path = Path(file_path)

        # Skip exempt filenames
        if path.name in EXEMPT_FILES:
            return False

        # Skip files matching exempt patterns
        file_name = path.name
        for pattern in EXEMPT_PATTERNS:
            if pattern in file_name:
                return False

        # Skip directories containing exempt patterns
        path_str = str(path).replace("\\", "/")
        for pattern in EXEMPT_PATTERNS:
            if f"/{pattern}/" in path_str or pattern in path.parts:
                return False

        # Skip exempt directories
        for exempt_dir in EXEMPT_DIRS:
            try:
                path.relative_to(exempt_dir)
                return False
            except ValueError:
                continue

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        violations: list[tuple[int, str]] = []

        has_fl_test_file = False
        has_fl_test_case = False
        fl_test_file_line = -1
        closing_brace_line = -1
        last_include_line = -1
        first_ifdef_line = -1

        for i, line in enumerate(file_content.lines):
            # Track last include
            if line.strip().startswith("#include"):
                last_include_line = i

            # Track first #if or #ifdef
            if first_ifdef_line == -1 and (
                line.strip().startswith("#if ")
                or line.strip().startswith("#ifdef ")
                or line.strip().startswith("#ifndef ")
            ):
                first_ifdef_line = i

            # Track FL_TEST_FILE location
            if _FL_TEST_FILE_RE.search(line):
                has_fl_test_file = True
                fl_test_file_line = i

            # Track closing brace (only care about the last one in the file)
            if has_fl_test_file and line.strip() == "} // FL_TEST_FILE":
                closing_brace_line = i

            # Track if file has actual test cases
            if "FL_TEST_CASE(" in line:
                has_fl_test_case = True

        # Only require FL_TEST_FILE if the file contains test cases
        if has_fl_test_case:
            if not has_fl_test_file:
                violations.append(
                    (
                        1,
                        "Missing FL_TEST_FILE(FL_FILEPATH) { ... } wrapper. "
                        "All files with FL_TEST_CASE must be wrapped with FL_TEST_FILE(FL_FILEPATH) { ... } "
                        "for unity build filtering.",
                    )
                )
            elif has_fl_test_file:
                # Check that closing brace exists and has correct comment
                # Find the last closing brace in the file
                last_brace_line = -1
                for i in range(len(file_content.lines) - 1, -1, -1):
                    if file_content.lines[i].strip() == "}":
                        last_brace_line = i
                        break

                if last_brace_line >= 0:
                    # Check if it has the FL_TEST_FILE comment
                    last_line = file_content.lines[last_brace_line].strip()
                    if last_line != "} // FL_TEST_FILE":
                        violations.append(
                            (
                                last_brace_line + 1,
                                "Closing brace for FL_TEST_FILE block must have '// FL_TEST_FILE' comment",
                            )
                        )

                # Check placement: FL_TEST_FILE should come after includes but before #if/#ifdef
                if fl_test_file_line >= 0:
                    if first_ifdef_line >= 0 and fl_test_file_line > first_ifdef_line:
                        violations.append(
                            (
                                fl_test_file_line + 1,
                                "FL_TEST_FILE(FL_FILEPATH) { must come AFTER all #include statements but BEFORE any #if/#ifdef directives. "
                                "Reorder: #includes → FL_TEST_FILE → #if/#ifdef blocks → test code",
                            )
                        )
                    elif (
                        last_include_line >= 0 and fl_test_file_line < last_include_line
                    ):
                        violations.append(
                            (
                                fl_test_file_line + 1,
                                "FL_TEST_FILE(FL_FILEPATH) { must come AFTER all #include statements. "
                                "Reorder: #includes → FL_TEST_FILE → rest of code",
                            )
                        )

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    from ci.util.check_files import run_checker_standalone

    checker = TestFileWrapperChecker()
    run_checker_standalone(
        checker,
        [str(TESTS_ROOT)],
        "Found test files missing FL_TEST_FILE wrapper",
    )


if __name__ == "__main__":
    main()
