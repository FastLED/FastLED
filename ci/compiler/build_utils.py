"""Build utility functions for FastLED PlatformIO builds."""

import os
from pathlib import Path


def get_utf8_env() -> dict[str, str]:
    """Get environment with UTF-8 encoding to prevent Windows CP1252 encoding errors.

    PlatformIO outputs Unicode characters (checkmarks, etc.) that fail on Windows
    when using the default CP1252 console encoding. This ensures UTF-8 is used.
    """
    env = os.environ.copy()
    env["PYTHONIOENCODING"] = "utf-8"
    return env


def create_building_banner(example: str) -> str:
    """Create a building banner for the given example."""
    banner_text = f"BUILDING {example}"
    border_char = "="
    padding = 2
    text_width = len(banner_text)
    total_width = text_width + (padding * 2)

    top_border = border_char * (total_width + 4)
    middle_line = (
        f"{border_char} {' ' * padding}{banner_text}{' ' * padding} {border_char}"
    )
    bottom_border = border_char * (total_width + 4)

    banner = f"{top_border}\n{middle_line}\n{bottom_border}"

    # Apply blue color using ANSI escape codes
    blue_color = "\033[34m"
    reset_color = "\033[0m"
    return f"{blue_color}{banner}{reset_color}"


def get_example_error_message(project_root: Path, example: str) -> str:
    """Generate appropriate error message for missing example.

    Args:
        project_root: FastLED project root directory
        example: Example name or path that was not found

    Returns:
        Error message describing where the example was expected
    """
    example_path = Path(example)

    if example_path.is_absolute():
        return f"Example directory not found: {example}"
    else:
        return f"Example not found: {project_root / 'examples' / example}"
