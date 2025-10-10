"""
Text formatting utilities for compilation output.

This module provides color formatting functions for console output
to make compilation results more readable.
"""


def green_text(text: str) -> str:
    """
    Return text in green color for terminal output.

    Args:
        text: The text to format

    Returns:
        Text wrapped with ANSI green color codes
    """
    return f"\033[32m{text}\033[0m"


def red_text(text: str) -> str:
    """
    Return text in red color for terminal output.

    Args:
        text: The text to format

    Returns:
        Text wrapped with ANSI red color codes
    """
    return f"\033[31m{text}\033[0m"


def yellow_text(text: str) -> str:
    """
    Return text in yellow color for terminal output.

    Args:
        text: The text to format

    Returns:
        Text wrapped with ANSI yellow color codes
    """
    return f"\033[33m{text}\033[0m"


def blue_text(text: str) -> str:
    """
    Return text in blue color for terminal output.

    Args:
        text: The text to format

    Returns:
        Text wrapped with ANSI blue color codes
    """
    return f"\033[34m{text}\033[0m"


def bold_text(text: str) -> str:
    """
    Return text in bold for terminal output.

    Args:
        text: The text to format

    Returns:
        Text wrapped with ANSI bold codes
    """
    return f"\033[1m{text}\033[0m"
