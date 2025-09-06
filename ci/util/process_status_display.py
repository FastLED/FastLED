#!/usr/bin/env python3
"""Process status display classes for real-time monitoring."""

import logging
import threading
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import TYPE_CHECKING, List, Optional


if TYPE_CHECKING:
    from ci.util.running_process_group import GroupStatus, RunningProcessGroup

logger = logging.getLogger(__name__)


@dataclass
class DisplayConfig:
    """Configuration for display output format."""

    format_type: str = "ascii"  # "ascii", "rich", "csv"
    use_colors: bool = True
    update_interval: float = 0.1
    max_name_width: int = 20
    max_output_width: int = 40


class ProcessStatusDisplay(ABC):
    """Abstract base class for process status displays."""

    def __init__(
        self, group: "RunningProcessGroup", config: Optional[DisplayConfig] = None
    ):
        self.group = group
        self.config = config or DisplayConfig()
        self._display_thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()

    @abstractmethod
    def format_status_line(
        self, group_status: "GroupStatus", spinner_index: int
    ) -> str:
        """Format the complete status display."""
        pass

    def start_display(self) -> threading.Thread:
        """Start the real-time display in a background thread."""
        if self._display_thread and self._display_thread.is_alive():
            return self._display_thread

        self._stop_event.clear()
        self._display_thread = threading.Thread(target=self._display_loop, daemon=True)
        self._display_thread.start()
        return self._display_thread

    def stop_display(self) -> None:
        """Stop the display thread."""
        if self._display_thread:
            self._stop_event.set()
            self._display_thread.join(timeout=1.0)

    def _display_loop(self) -> None:
        """Main display loop running in background thread."""
        spinner_index = 0
        last_status_time = 0
        status_interval = 2  # Show status every 2 seconds

        while not self._stop_event.is_set():
            try:
                # Wait for monitoring to start, or exit if stopped
                if not self.group._status_monitoring_active:
                    time.sleep(0.1)
                    continue

                current_time = time.time()
                group_status = self.group.get_status()

                # Show periodic status updates instead of continuous display
                if current_time - last_status_time >= status_interval:
                    running_count = sum(1 for p in group_status.processes if p.is_alive)
                    completed_count = group_status.completed_processes

                    if running_count > 0:
                        # Show a brief status update
                        spinner_char = ["|", "/", "-", "\\\\"][spinner_index % 4]
                        print(
                            f"{spinner_char} Progress: {completed_count}/{group_status.total_processes} completed, {running_count} running..."
                        )
                        last_status_time = current_time

                spinner_index = (spinner_index + 1) % 4
                time.sleep(1.0)  # Check every second but only print every 5 seconds

            except Exception as e:
                logger.warning(f"Display update error: {e}")
                time.sleep(1.0)


class ASCIIStatusDisplay(ProcessStatusDisplay):
    """ASCII-compatible status display."""

    def __init__(
        self, group: "RunningProcessGroup", config: Optional[DisplayConfig] = None
    ):
        if config is None:
            config = DisplayConfig(format_type="ascii")
        super().__init__(group, config)

        self._spinner_chars = ["|", "/", "-", "\\\\"]
        self._status_chars = {
            "running": "|>",
            "done": "OK",
            "failed": "XX",
            "pending": "--",
        }

    def format_status_line(
        self, group_status: "GroupStatus", spinner_index: int
    ) -> str:
        """Format status display using ASCII characters."""
        lines: List[str] = []

        # Header
        lines.append(
            f"Process Group: {group_status.group_name} - Progress: {group_status.completed_processes}/{group_status.total_processes} ({group_status.completion_percentage:.1f}%)"
        )
        lines.append("-" * 80)

        # Process status lines
        for proc_status in group_status.processes:
            # Get status character
            if proc_status.is_alive:
                status_char = self._spinner_chars[
                    spinner_index % len(self._spinner_chars)
                ]
            elif proc_status.is_completed:
                if proc_status.return_value == 0:
                    status_char = self._status_chars["done"]
                else:
                    status_char = self._status_chars["failed"]
            else:
                status_char = self._status_chars["pending"]

            # Format fields
            name = proc_status.name[: self.config.max_name_width].ljust(
                self.config.max_name_width
            )

            if proc_status.is_completed:
                status_text = f"DONE({proc_status.return_value or 0})"
            elif proc_status.is_alive:
                status_text = "RUNNING"
            else:
                status_text = "PENDING"

            duration = f"{proc_status.running_time_seconds:.1f}s"
            output = (proc_status.last_output_line or "")[
                : self.config.max_output_width
            ]

            lines.append(
                f"{status_char} {name} | {status_text:>8} | {duration:>8} | {output}"
            )

        return "\\n".join(lines)


class RichStatusDisplay(ProcessStatusDisplay):
    """Rich library-based status display with enhanced formatting."""

    def __init__(
        self, group: "RunningProcessGroup", config: Optional[DisplayConfig] = None
    ):
        if config is None:
            config = DisplayConfig(format_type="rich")
        super().__init__(group, config)

        try:
            from rich.console import Console
            from rich.live import Live
            from rich.progress import (
                Progress,
                SpinnerColumn,
                TextColumn,
                TimeElapsedColumn,
            )
            from rich.table import Table

            self.Progress = Progress
            self.Live = Live
            self.Table = Table
            self.Console = Console
            self._rich_available = True

            self._spinner_chars = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧"]
            self._status_chars = {
                "running": "⠋",
                "done": "✓",
                "failed": "✗",
                "pending": " ",
            }
        except ImportError:
            logger.warning("Rich not available, falling back to ASCII display")
            self._rich_available = False
            # Fallback to ASCII chars
            self._spinner_chars = ["|", "/", "-", "\\\\"]
            self._status_chars = {
                "running": "|>",
                "done": "OK",
                "failed": "XX",
                "pending": "--",
            }

    def format_status_line(
        self, group_status: "GroupStatus", spinner_index: int
    ) -> str:
        """Format status display using Rich library features."""
        if not self._rich_available:
            # Fallback to ASCII formatting
            return self._format_ascii_fallback(group_status, spinner_index)

        try:
            return self._format_rich_display(group_status, spinner_index)
        except Exception as e:
            logger.warning(f"Rich formatting failed, using ASCII fallback: {e}")
            return self._format_ascii_fallback(group_status, spinner_index)

    def _format_rich_display(
        self, group_status: "GroupStatus", spinner_index: int
    ) -> str:
        """Format using Rich library."""
        table = self.Table(title=f"Process Group: {group_status.group_name}")

        table.add_column("Status", width=8)
        table.add_column("Process", width=self.config.max_name_width)
        table.add_column("State", width=10)
        table.add_column("Duration", width=10)
        table.add_column("Last Output", width=self.config.max_output_width)

        for proc_status in group_status.processes:
            # Get status character
            if proc_status.is_alive:
                status_char = self._spinner_chars[
                    spinner_index % len(self._spinner_chars)
                ]
            elif proc_status.is_completed:
                if proc_status.return_value == 0:
                    status_char = self._status_chars["done"]
                else:
                    status_char = self._status_chars["failed"]
            else:
                status_char = self._status_chars["pending"]

            # Format fields
            name = proc_status.name[: self.config.max_name_width]

            if proc_status.is_completed:
                status_text = f"DONE({proc_status.return_value or 0})"
                if proc_status.return_value == 0:
                    status_text = f"[green]{status_text}[/green]"
                else:
                    status_text = f"[red]{status_text}[/red]"
            elif proc_status.is_alive:
                status_text = "[yellow]RUNNING[/yellow]"
            else:
                status_text = "[dim]PENDING[/dim]"

            duration = f"{proc_status.running_time_seconds:.1f}s"
            output = (proc_status.last_output_line or "")[
                : self.config.max_output_width
            ]

            table.add_row(status_char, name, status_text, duration, output)

        # Render table to string
        console = self.Console(width=120, force_terminal=False)
        with console.capture() as capture:
            console.print(table)

        return capture.get()

    def _format_ascii_fallback(
        self, group_status: "GroupStatus", spinner_index: int
    ) -> str:
        """Fallback ASCII formatting when Rich fails."""
        lines: List[str] = []

        # Header
        lines.append(
            f"Process Group: {group_status.group_name} - Progress: {group_status.completed_processes}/{group_status.total_processes} ({group_status.completion_percentage:.1f}%)"
        )
        lines.append("-" * 80)

        # Process status lines
        for proc_status in group_status.processes:
            # Get status character
            if proc_status.is_alive:
                status_char = self._spinner_chars[
                    spinner_index % len(self._spinner_chars)
                ]
            elif proc_status.is_completed:
                if proc_status.return_value == 0:
                    status_char = self._status_chars["done"]
                else:
                    status_char = self._status_chars["failed"]
            else:
                status_char = self._status_chars["pending"]

            # Format fields
            name = proc_status.name[: self.config.max_name_width].ljust(
                self.config.max_name_width
            )

            if proc_status.is_completed:
                status_text = f"DONE({proc_status.return_value or 0})"
            elif proc_status.is_alive:
                status_text = "RUNNING"
            else:
                status_text = "PENDING"

            duration = f"{proc_status.running_time_seconds:.1f}s"
            output = (proc_status.last_output_line or "")[
                : self.config.max_output_width
            ]

            lines.append(
                f"{status_char} {name} | {status_text:>8} | {duration:>8} | {output}"
            )

        return "\\n".join(lines)


def get_display_format() -> DisplayConfig:
    """Auto-detect best display format for current environment."""
    try:
        # Test Rich availability
        import sys

        import rich

        if sys.stdout.encoding and sys.stdout.encoding.lower() in ["utf-8", "utf8"]:
            return DisplayConfig(format_type="rich")
    except ImportError:
        pass

    # Fallback to ASCII
    return DisplayConfig(format_type="ascii")


def create_status_display(
    group: "RunningProcessGroup", display_type: str = "auto"
) -> ProcessStatusDisplay:
    """Factory function to create best available display."""

    if display_type == "auto":
        config = get_display_format()
        display_type = config.format_type

    if display_type == "rich":
        try:
            return RichStatusDisplay(group)
        except Exception as e:
            logger.warning(f"Rich display creation failed, falling back to ASCII: {e}")
            return ASCIIStatusDisplay(group)

    # Default to ASCII
    return ASCIIStatusDisplay(group)


def display_process_status(
    group: "RunningProcessGroup",
    display_type: str = "auto",
    update_interval: float = 0.1,
) -> threading.Thread:
    """Convenience function to start real-time process status display."""

    display = create_status_display(group, display_type)
    display.config.update_interval = update_interval

    return display.start_display()
