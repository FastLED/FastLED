"""
Watchdog state management for tracking active processes.

This module provides simple state management for tracking which processes
are currently active, which can be useful for monitoring and cleanup operations.
"""

from typing import List


# Global state to track active processes
_active_processes: List[str] = []


def set_active_processes(processes: List[str]) -> None:
    """
    Set the list of currently active processes.

    Args:
        processes: List of command strings representing active processes
    """
    global _active_processes
    _active_processes = processes.copy()


def clear_active_processes() -> None:
    """
    Clear all tracked active processes.
    """
    global _active_processes
    _active_processes = []


def get_active_processes() -> List[str]:
    """
    Get the current list of active processes.

    Returns:
        List of command strings representing active processes
    """
    return _active_processes.copy()
