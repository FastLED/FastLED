#!/usr/bin/env python3
"""Build timing tracker for performance analysis.

Tracks and reports the time spent in each build phase:
- Meson Configuration
- Ninja maintenance
- Compilation (PCH, library, test files)
- Test Execution
"""

import time
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class BuildTimer:
    """Tracks timing for each build phase."""

    # Phase timing (seconds)
    meson_setup_time: float = 0.0
    ninja_maintenance_time: float = 0.0
    compile_time: float = 0.0
    test_execution_time: float = 0.0
    total_time: float = 0.0

    # Checkpoint tracking
    _checkpoints: dict[str, float] = field(default_factory=dict)
    _start_time: Optional[float] = None

    def start(self) -> None:
        """Mark the start of the build process."""
        self._start_time = time.time()
        self._checkpoints["start"] = self._start_time

    def checkpoint(self, name: str) -> None:
        """Record a timing checkpoint."""
        if self._start_time is None:
            raise RuntimeError("Timer not started - call start() first")
        self._checkpoints[name] = time.time()

    def calculate_phases(self) -> None:
        """Calculate phase durations from checkpoints."""
        if not self._checkpoints:
            return

        cp = self._checkpoints

        # Calculate phase times based on checkpoint sequence
        if "start" in cp:
            if "meson_setup_done" in cp:
                self.meson_setup_time = cp["meson_setup_done"] - cp["start"]

            if "ninja_maintenance_done" in cp and "meson_setup_done" in cp:
                self.ninja_maintenance_time = (
                    cp["ninja_maintenance_done"] - cp["meson_setup_done"]
                )

            if "compile_done" in cp and "ninja_maintenance_done" in cp:
                self.compile_time = cp["compile_done"] - cp["ninja_maintenance_done"]
            elif "compile_done" in cp and "meson_setup_done" in cp:
                # If ninja maintenance wasn't tracked separately
                self.compile_time = cp["compile_done"] - cp["meson_setup_done"]

            if "test_execution_done" in cp and "compile_done" in cp:
                self.test_execution_time = (
                    cp["test_execution_done"] - cp["compile_done"]
                )

            if "test_execution_done" in cp:
                self.total_time = cp["test_execution_done"] - cp["start"]
            elif "compile_done" in cp:
                self.total_time = cp["compile_done"] - cp["start"]
            else:
                self.total_time = time.time() - cp["start"]

    def format_table(self) -> str:
        """Format timing information as a table."""
        self.calculate_phases()

        # Only show phases that have meaningful timing
        phases = []

        if self.meson_setup_time > 0:
            phases.append(("Meson Setup", self.meson_setup_time))

        if self.ninja_maintenance_time > 0:
            phases.append(("Ninja Maintenance", self.ninja_maintenance_time))

        if self.compile_time > 0:
            phases.append(("Compilation", self.compile_time))

        if self.test_execution_time > 0:
            phases.append(("Test Execution", self.test_execution_time))

        if not phases:
            return ""

        # Build table
        lines = []
        lines.append("")
        lines.append("┌─ Build Timing Breakdown ─────────────────┐")

        # Find max label width for alignment
        max_label_width = max(len(label) for label, _ in phases)

        for label, duration in phases:
            percentage = (
                (duration / self.total_time * 100) if self.total_time > 0 else 0
            )
            bar_width = int(percentage / 5)  # 20 chars = 100%
            bar = "█" * bar_width + "░" * (20 - bar_width)

            lines.append(
                f"│ {label:<{max_label_width}} {duration:>6.2f}s [{bar}] {percentage:>5.1f}%"
            )

        lines.append("├───────────────────────────────────────────┤")
        lines.append(f"│ {'Total':<{max_label_width}} {self.total_time:>6.2f}s")
        lines.append("└───────────────────────────────────────────┘")

        return "\n".join(lines)

    def format_simple(self) -> str:
        """Format timing as simple summary line."""
        self.calculate_phases()

        parts = []
        if self.meson_setup_time > 0:
            parts.append(f"meson={self.meson_setup_time:.2f}s")
        if self.compile_time > 0:
            parts.append(f"compile={self.compile_time:.2f}s")
        if self.test_execution_time > 0:
            parts.append(f"tests={self.test_execution_time:.2f}s")

        if parts:
            return f"Timing: {', '.join(parts)}, total={self.total_time:.2f}s"
        return f"Total: {self.total_time:.2f}s"
