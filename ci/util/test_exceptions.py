#!/usr/bin/env python3
"""Custom exceptions for test failures that need to bubble up to callers."""

import subprocess
from dataclasses import dataclass
from typing import List, Optional


@dataclass
class TestFailureInfo:
    """Information about a single test failure"""

    test_name: str
    command: str
    return_code: int
    output: str
    error_type: str = "test_failure"


class FastLEDTestException(Exception):
    """Base exception for FastLED test failures"""

    def __init__(self, message: str, failures: Optional[List[TestFailureInfo]] = None):
        super().__init__(message)
        self.failures = failures or []
        self.message = message

    def add_failure(self, failure: TestFailureInfo) -> None:
        """Add a test failure to this exception"""
        self.failures.append(failure)

    def has_failures(self) -> bool:
        """Check if this exception contains any failures"""
        return len(self.failures) > 0

    def get_failure_summary(self) -> str:
        """Get a summary of all failures"""
        if not self.failures:
            return self.message

        summary = [self.message]
        summary.append(f"\nFailed tests ({len(self.failures)}):")
        for failure in self.failures:
            summary.append(
                f"  - {failure.test_name}: {failure.error_type} (exit code {failure.return_code})"
            )

        return "\n".join(summary)

    def get_detailed_failure_info(self) -> str:
        """Get detailed information about all failures"""
        if not self.failures:
            return self.message

        details = [self.message]
        details.append(f"\n{'=' * 50}")
        details.append("DETAILED FAILURE INFORMATION")
        details.append(f"{'=' * 50}")

        for i, failure in enumerate(self.failures, 1):
            cmd_str: str = (
                subprocess.list2cmdline(failure.command)
                if isinstance(failure.command, list)
                else failure.command
            )
            assert isinstance(cmd_str, str)
            details.append(f"\n{i}. {failure.test_name}")
            details.append(f"   Command: {cmd_str}")
            details.append(f"   Error Type: {failure.error_type}")
            details.append(f"   Exit Code: {failure.return_code}")
            details.append(f"   Output:")
            # Indent the output
            for line in failure.output.split("\n"):
                details.append(f"     {line}")

        return "\n".join(details)


class CompilationFailedException(FastLEDTestException):
    """Exception for compilation failures"""

    def __init__(
        self,
        message: str = "Compilation failed",
        failures: Optional[List[TestFailureInfo]] = None,
    ):
        super().__init__(message, failures)


class TestExecutionFailedException(FastLEDTestException):
    """Exception for test execution failures"""

    def __init__(
        self,
        message: str = "Test execution failed",
        failures: Optional[List[TestFailureInfo]] = None,
    ):
        super().__init__(message, failures)


class TestTimeoutException(FastLEDTestException):
    """Exception for test timeouts"""

    def __init__(
        self,
        message: str = "Test execution timed out",
        failures: Optional[List[TestFailureInfo]] = None,
    ):
        super().__init__(message, failures)
