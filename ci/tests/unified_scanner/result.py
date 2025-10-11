# pyright: reportUnknownMemberType=false
from dataclasses import dataclass
from typing import Optional


@dataclass
class CheckResult:
    """Result from a file checker."""

    checker_name: str
    file_path: str
    line_number: Optional[int]
    severity: str  # "ERROR", "WARNING"
    message: str
    suggestion: Optional[str] = None  # e.g., "use // ok include"

    def format_for_ci(self) -> str:
        """Format error for CI detection (must include 'ERROR' keyword)."""
        location = (
            f"{self.file_path}:{self.line_number}"
            if self.line_number
            else self.file_path
        )
        msg = f"{self.severity}: [{self.checker_name}] {location}: {self.message}"
        if self.suggestion:
            msg += f"\n  Suggestion: {self.suggestion}"
        return msg
