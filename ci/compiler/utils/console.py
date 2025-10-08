"""Console output utilities for colored text."""


def red_text(text: str) -> str:
    """Return text in red color."""
    return f"\033[31m{text}\033[0m"


def green_text(text: str) -> str:
    """Return text in green color."""
    return f"\033[32m{text}\033[0m"


def orange_text(text: str) -> str:
    """Return text in orange color."""
    return f"\033[33m{text}\033[0m"
