"""Output formatting and colored printing utilities for Meson build system."""

from ci.util.color_output import print_blue, print_green, print_red, print_yellow
from ci.util.timestamp_print import get_elapsed_str


def print_success(msg: str) -> None:
    """Print success message in green."""
    print_green(msg)


def print_error(msg: str) -> None:
    """Print error message in red."""
    print_red(msg)


def print_warning(msg: str) -> None:
    """Print warning message in yellow."""
    print_yellow(msg)


def print_info(msg: str) -> None:
    """Print info message in blue."""
    print_blue(msg)


def print_banner(
    title: str, emoji: str = "", width: int = 50, verbose: bool | None = None
) -> None:
    """Print a lightweight section separator.

    Args:
        title: The banner title text
        emoji: Optional emoji to prefix the title (e.g., "🔧", "📦")
        width: Total banner width (default 50)
        verbose: If False, suppress banner. If None, always print (backward compat)
    """
    # Skip banner in non-verbose mode when verbose is explicitly False
    if verbose is False:
        return

    # Format title with emoji if provided
    display_title = f"{emoji} {title}" if emoji else title

    # Simple single-line separator
    padding = width - len(display_title) - 2  # -2 for leading "── "
    if padding < 0:
        padding = 0
    separator = f"── {display_title} " + "─" * padding

    # Print simple separator with timestamp (no blank line before for compactness)
    print(f"{get_elapsed_str()} {separator}")
