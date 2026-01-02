#!/usr/bin/env python3
"""
Banner and tree-style printing utilities for critical workflow sections.

This module provides formatted output functions for phase banners, status trees,
and progress sections to improve visibility of long-running operations.
"""

from enum import Enum


class BannerStyle(Enum):
    """Banner styling options."""

    PHASE = "phase"  # Double lines (═══) for major phases
    SUCCESS = "success"  # Single lines (───) + green
    ERROR = "error"  # Single lines (───) + red
    INFO = "info"  # Single lines (───) + blue
    WARNING = "warning"  # Single lines (───) + yellow


class BannerPrinter:
    """Print styled banners and trees for critical sections."""

    # Box drawing characters
    DOUBLE_LINE = "═"
    SINGLE_LINE = "─"
    TREE_BRANCH = "├─"
    TREE_LAST = "└─"
    TREE_PIPE = "│"

    # Standard width for banners
    BANNER_WIDTH = 60

    @staticmethod
    def print_phase_banner(title: str, details: dict[str, str] | None = None) -> None:
        """Print a phase banner with optional details.

        Example output:
            ═══════════════════════════════════════════════════════════
            PHASE 0: PACKAGE INSTALLATION
            ═══════════════════════════════════════════════════════════
            📋 Project: C:\\Users\\niteris\\dev\\fastled
            📋 Environment: esp32c6
            📋 Caller PID: 12345

        Args:
            title: Banner title text
            details: Optional dictionary of key-value pairs to display below banner
        """
        separator = BannerPrinter.DOUBLE_LINE * BannerPrinter.BANNER_WIDTH

        print(separator)
        print(title)
        print(separator)

        if details:
            for key, value in details.items():
                print(f"📋 {key}: {value}")
            print()  # Blank line after details

    @staticmethod
    def print_progress_section(title: str) -> None:
        """Print a progress section header.

        Example output:
            ──────────────────────────────────────────────────────────
            Package Installation Progress:
            ──────────────────────────────────────────────────────────

        Args:
            title: Section title
        """
        separator = BannerPrinter.SINGLE_LINE * BannerPrinter.BANNER_WIDTH

        print()
        print(title)
        print(separator)

    @staticmethod
    def print_tree_status(root: str, items: list[tuple[str, str, str | None]]) -> None:
        """Print tree-style status messages.

        Example output:
            🔍 Checking packages...
               ├─ Validating integrity...
               ├─ ❌ Missing: toolchain-riscv32-esp
               └─ 📦 Installation required

        Args:
            root: Root message (first line)
            items: List of (icon, message, status_type) tuples
                  status_type can be: "success", "error", "warning", "info", or None
        """
        print(root)

        for i, (icon, message, status_type) in enumerate(items):
            is_last = i == len(items) - 1
            branch = BannerPrinter.TREE_LAST if is_last else BannerPrinter.TREE_BRANCH

            # Add color/emphasis based on status type if provided
            if status_type == "success":
                formatted_msg = f"{icon} {message}"
            elif status_type == "error":
                formatted_msg = f"{icon} {message}"
            elif status_type == "warning":
                formatted_msg = f"{icon} {message}"
            elif status_type == "info":
                formatted_msg = f"{icon} {message}"
            else:
                formatted_msg = f"{icon} {message}" if icon else message

            print(f"   {branch} {formatted_msg}")

    @staticmethod
    def print_success_banner(message: str, elapsed_time: str | None = None) -> None:
        """Print success completion banner.

        Example output:
            ──────────────────────────────────────────────────────────
            ✅ Package installation completed (total: 14m 52s)

        Args:
            message: Success message
            elapsed_time: Optional formatted elapsed time string
        """
        separator = BannerPrinter.SINGLE_LINE * BannerPrinter.BANNER_WIDTH

        print(separator)
        if elapsed_time:
            print(f"✅ {message} (total: {elapsed_time})")
        else:
            print(f"✅ {message}")
        print()

    @staticmethod
    def print_error_banner(
        title: str, error_message: str, recommendations: list[str] | None = None
    ) -> None:
        """Print error banner with optional recommendations.

        Example output:
            ❌ Package installation FAILED
            ──────────────────────────────────────────────────────────
            Error: Installation already in progress
            Active installation:
              └─ PID: 49428
              └─ CWD: C:\\Users\\...\\examples\\Blink

            Recommended actions:
              1. Wait for active installation to complete, then retry
              2. Check daemon status: uv run python ci/util/pio_package_client.py --status
            ──────────────────────────────────────────────────────────

        Args:
            title: Error title
            error_message: Detailed error message
            recommendations: Optional list of recommended actions
        """
        separator = BannerPrinter.SINGLE_LINE * BannerPrinter.BANNER_WIDTH

        print(f"\n❌ {title}")
        print(separator)
        print(f"Error: {error_message}")

        if recommendations:
            print("\nRecommended actions:")
            for i, rec in enumerate(recommendations, 1):
                print(f"  {i}. {rec}")

        print(separator)
        print()

    @staticmethod
    def format_elapsed_time(seconds: float) -> str:
        """Format elapsed time in human-readable form.

        Args:
            seconds: Elapsed time in seconds

        Returns:
            Formatted string like "2m 30s" or "45s"
        """
        if seconds < 60:
            return f"{int(seconds)}s"
        elif seconds < 3600:
            minutes = int(seconds // 60)
            secs = int(seconds % 60)
            return f"{minutes}m {secs}s"
        else:
            hours = int(seconds // 3600)
            minutes = int((seconds % 3600) // 60)
            return f"{hours}h {minutes}m"


# Convenience functions for common use cases
def print_phase_banner(title: str, details: dict[str, str] | None = None) -> None:
    """Convenience function for print_phase_banner."""
    BannerPrinter.print_phase_banner(title, details)


def print_progress_section(title: str) -> None:
    """Convenience function for print_progress_section."""
    BannerPrinter.print_progress_section(title)


def print_tree_status(root: str, items: list[tuple[str, str, str | None]]) -> None:
    """Convenience function for print_tree_status."""
    BannerPrinter.print_tree_status(root, items)


def print_success_banner(message: str, elapsed_time: str | None = None) -> None:
    """Convenience function for print_success_banner."""
    BannerPrinter.print_success_banner(message, elapsed_time)


def print_error_banner(
    title: str, error_message: str, recommendations: list[str] | None = None
) -> None:
    """Convenience function for print_error_banner."""
    BannerPrinter.print_error_banner(title, error_message, recommendations)


def format_elapsed_time(seconds: float) -> str:
    """Convenience function for format_elapsed_time."""
    return BannerPrinter.format_elapsed_time(seconds)
