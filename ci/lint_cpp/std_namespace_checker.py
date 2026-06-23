#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker for std:: namespace usage - should use fl:: instead."""

from ci.util.check_files import FileContent, FileContentChecker, is_excluded_file


# Bridge files that MUST use std:: because they implement the fl:: → std:: mapping.
# These files are entirely skipped by the checker.
BRIDGE_FILE_WHITELIST: set[str] = {
    # Platform mutex implementations (bridge std::mutex → fl::mutex)
    "platforms/stub/mutex_stub_stl.h",
    "platforms/stub/mutex_stub_noop.h",
    "platforms/esp/32/mutex_esp32.h",
    "platforms/arm/rp/mutex_rp.h",
    "platforms/arm/stm32/mutex_stm32.h",
    "platforms/arm/stm32/mutex_stm32_rtos.h",
    "platforms/arm/d21/mutex_samd.h",
    "platforms/arm/nrf52/mutex_nrf52.h",
    # Platform condition variable implementations
    "platforms/stub/condition_variable_stub.h",
    "platforms/esp/32/condition_variable_esp32.h",
    "platforms/esp/32/condition_variable_esp32.cpp.hpp",
    # Platform thread implementations
    "platforms/stub/thread_stub_stl.h",
    "platforms/stub/thread_stub_noop.h",
    # Platform semaphore implementations
    "platforms/stub/semaphore_stub_stl.h",
    "platforms/stub/semaphore_stub_noop.h",
    "platforms/esp/32/semaphore_esp32.h",
    # Platform semaphore implementations (ARM)
    "platforms/arm/d21/semaphore_samd.h",
    "platforms/arm/d21/semaphore_samd.cpp.hpp",
    "platforms/arm/rp/semaphore_rp.h",
    "platforms/arm/rp/semaphore_rp.cpp.hpp",
    "platforms/arm/stm32/semaphore_stm32.h",
    "platforms/arm/stm32/semaphore_stm32.cpp.hpp",
    # Platform time (implements fl::millis/micros using std::chrono — circular if using fl::chrono)
    "platforms/stub/platform_time.cpp.hpp",
    # Native host runners (test/example infrastructure, not library code)
    "platforms/apple/run_example.hpp",
    "platforms/apple/run_unit_test.hpp",
    "platforms/posix/run_example.hpp",
    "platforms/posix/run_unit_test.hpp",
    "platforms/win/run_example.hpp",
    "platforms/win/run_unit_test.hpp",
}

# Specific std:: symbols that are allowed everywhere because fl:: has no equivalent.
ALLOWED_STD_SYMBOLS: set[str] = {
    "std::atomic_thread_fence",
    "std::memory_order_acquire",
    "std::memory_order_release",
    "std::memory_order_seq_cst",
    "std::memory_order_relaxed",
    "std::memory_order_acq_rel",
    "std::memory_order_consume",
}


def _is_bridge_file(file_path: str) -> bool:
    """Check if a file is in the bridge file whitelist."""
    # Normalize path separators for cross-platform matching
    normalized = file_path.replace("\\", "/")
    for bridge_path in BRIDGE_FILE_WHITELIST:
        if normalized.endswith(bridge_path):
            return True
    return False


def _has_allowed_std_symbol(line: str) -> bool:
    """Check if a line only contains allowed std:: symbols."""
    # Extract just the code part (before comments)
    code_part = line.split("//")[0]
    if "std::" not in code_part:
        return True

    # Check each std:: occurrence — if ALL are allowed, the line passes
    import re

    std_usages = re.findall(r"std::\w+", code_part)
    return all(usage in ALLOWED_STD_SYMBOLS for usage in std_usages)


class StdNamespaceChecker(FileContentChecker):
    """Checker class for std:: namespace usage."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for std:: namespace usage."""
        # Check file extension
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False

        # Check if file is in excluded list (handles both Windows and Unix paths)
        if is_excluded_file(file_path):
            return False

        # Exclude third-party libraries
        if "third_party" in file_path or "thirdparty" in file_path:
            return False

        # Skip bridge files entirely
        if _is_bridge_file(file_path):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for std:: namespace usage."""
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        # Check each line for std:: usage
        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue  # Skip the line with closing */

            # Skip if we're inside a multi-line comment
            if in_multiline_comment:
                continue

            # Skip single-line comment lines
            if stripped.startswith("//"):
                continue

            # Remove single-line comment portion before checking
            code_part = line.split("//")[0]

            # Check for std:: usage in code portion only
            if "std::" in code_part:
                # Allow lines with "// okay std namespace" bypass annotation
                if "// okay std namespace" in line:
                    continue
                # Allow globally-permitted std:: symbols
                if _has_allowed_std_symbol(line):
                    continue
                violations.append((line_number, stripped))

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run std:: namespace checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = StdNamespaceChecker()
    run_checker_standalone(
        checker, [str(PROJECT_ROOT / "src")], "Found std:: namespace usage"
    )


if __name__ == "__main__":
    main()
