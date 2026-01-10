# pyright: reportUnknownMemberType=false
from abc import ABC, abstractmethod
from pathlib import Path

from .result import CheckResult


class BaseChecker(ABC):
    """Base class for all file checkers."""

    @abstractmethod
    def name(self) -> str:
        """Return checker name for error messages."""
        pass

    @abstractmethod
    def should_check(self, file_path: Path) -> bool:
        """Return True if this checker should process this file."""
        pass

    @abstractmethod
    def check_file(self, file_path: Path, content: str) -> list[CheckResult]:
        """
        Check file content and return any violations.

        Args:
            file_path: Path to the file being checked
            content: Full file content as string

        Returns:
            List of CheckResult objects (empty list if no violations)
        """
        pass
