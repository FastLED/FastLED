#!/usr/bin/env python3
"""
Cross-platform colored terminal output utilities.

This module provides platform-neutral colored output for the CI system,
using the Rich library for consistent formatting across Windows, macOS, and Linux.
"""

from typing import TYPE_CHECKING, Optional


if TYPE_CHECKING:
    from rich.console import Console
    from rich.text import Text

try:
    from rich.console import Console
    from rich.text import Text

    _HAS_RICH = True
except ImportError:
    _HAS_RICH = False
    Console = None  # type: ignore
    Text = None  # type: ignore


class ColorOutput:
    """
    Platform-neutral colored terminal output using Rich library.

    Provides methods for printing colored text with consistent formatting
    across different operating systems and terminals.
    """

    def __init__(self, force_terminal: Optional[bool] = None):
        """
        Initialize ColorOutput.

        Args:
            force_terminal: Force terminal mode (True/False) or auto-detect (None)
        """
        if _HAS_RICH and Console is not None:
            self.console = Console(force_terminal=force_terminal)
        else:
            self.console = None

    def _fallback_print(self, message: str, prefix: str = "") -> None:
        """Fallback print when Rich is not available."""
        print(f"{prefix}{message}")

    def print_green(self, message: str) -> None:
        """Print message in green color."""
        if self.console:
            self.console.print(message, style="green")
        else:
            self._fallback_print(message, "✅ ")

    def print_yellow(self, message: str) -> None:
        """Print message in yellow color."""
        if self.console:
            self.console.print(message, style="yellow")
        else:
            self._fallback_print(message, "⚠️  ")

    def print_red(self, message: str) -> None:
        """Print message in red color."""
        if self.console:
            self.console.print(message, style="red")
        else:
            self._fallback_print(message, "❌ ")

    def print_blue(self, message: str) -> None:
        """Print message in blue color."""
        if self.console:
            self.console.print(message, style="blue")
        else:
            self._fallback_print(message, "ℹ️  ")

    def print_cache_hit(self, message: str) -> None:
        """Print cache hit message in green with appropriate formatting."""
        if self.console and Text is not None:
            # Use bright green with a checkmark for cache hits
            text = Text()
            text.append("✓ ", style="bright_green bold")
            text.append(message, style="green")
            self.console.print(text)
        else:
            self._fallback_print(message, "✓ ")

    def print_cache_miss(self, message: str) -> None:
        """Print cache miss/invalidation message in yellow with warning formatting."""
        if self.console and Text is not None:
            # Use yellow with a warning symbol for cache misses/invalidations
            text = Text()
            text.append("⚠️  ", style="yellow bold")
            text.append(message, style="yellow")
            self.console.print(text)
        else:
            self._fallback_print(message, "⚠️  ")


# Global instance for easy access
_color_output = ColorOutput()


# Convenience functions for common use cases
def print_green(message: str) -> None:
    """Print message in green color (global function)."""
    _color_output.print_green(message)


def print_yellow(message: str) -> None:
    """Print message in yellow color (global function)."""
    _color_output.print_yellow(message)


def print_red(message: str) -> None:
    """Print message in red color (global function)."""
    _color_output.print_red(message)


def print_blue(message: str) -> None:
    """Print message in blue color (global function)."""
    _color_output.print_blue(message)


def print_cache_hit(message: str) -> None:
    """Print cache hit message in green with checkmark (global function)."""
    _color_output.print_cache_hit(message)


def print_cache_miss(message: str) -> None:
    """Print cache miss/invalidation message in yellow with warning (global function)."""
    _color_output.print_cache_miss(message)


if __name__ == "__main__":
    # Demo the color output functionality
    print("Color Output Demo")
    print("=" * 40)

    print_green("✓ This is a success message in green")
    print_yellow("⚠️ This is a warning message in yellow")
    print_red("❌ This is an error message in red")
    print_blue("ℹ️ This is an info message in blue")

    print("\nCache Status Examples:")
    print_cache_hit("Fingerprint cache valid - skipping C++ unit tests")
    print_cache_hit("Fingerprint cache valid - skipping Python tests")
    print_cache_miss("Fingerprint cache invalid - C++ files changed, rebuilding")
    print_cache_miss("Fingerprint cache invalid - Python files changed, retesting")
