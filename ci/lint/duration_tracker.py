"""Duration tracking and summary table generation."""

import time
from dataclasses import dataclass


@dataclass
class StageResult:
    """Result of a single lint stage."""

    name: str
    duration: float
    skipped: bool = False


class DurationTracker:
    """
    Tracks duration of lint stages and generates summary table.

    Matches the exact output format of the bash script.
    """

    def __init__(self) -> None:
        self.results: list[StageResult] = []
        self.start_times: dict[str, float] = {}

    def start_stage(self, name: str) -> None:
        """Record the start time of a stage."""
        self.start_times[name] = time.time()

    def record_stage(self, name: str, duration: float, skipped: bool = False) -> None:
        """Record a completed stage."""
        self.results.append(StageResult(name, duration, skipped))

    def end_stage(self, name: str, skipped: bool = False) -> None:
        """Calculate and record the duration for a stage."""
        if name in self.start_times:
            duration = time.time() - self.start_times[name]
            self.record_stage(name, duration, skipped)
        else:
            # If start time not found, record with 0 duration
            self.record_stage(name, 0.0, skipped)

    def generate_summary(self) -> str:
        """
        Generate the summary table matching bash script format.

        Returns:
            Formatted summary table as a string
        """
        if not self.results:
            return ""

        # Calculate column widths
        max_name_width = max(len(r.name) for r in self.results)
        header_width = len("Linter Name")
        max_name_width = max(max_name_width, header_width)

        # Build separator line
        separator = "-" * max_name_width + "-+-" + "-" * 24

        # Build table
        lines = [
            "",
            "Linting Execution Summary:",
            separator,
            f"{'Linter Name':<{max_name_width}} | {'Duration':>24}",
            separator,
        ]

        # Add each result
        for result in self.results:
            if result.skipped:
                lines.append(
                    f"{result.name:<{max_name_width}} | {'Skipped (no changes)':>24}"
                )
            else:
                lines.append(
                    f"{result.name:<{max_name_width}} | {int(result.duration):>23}s"
                )

        lines.append(separator)

        return "\n".join(lines)
