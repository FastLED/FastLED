from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""
Utility functions for file locking - extracted to avoid circular imports.

Contains the cross-platform process alive check used by both
lock_database.py and file_lock_rw.py.
"""

import logging
import os
import platform


logger = logging.getLogger(__name__)


def is_process_alive(pid: int) -> bool:
    """
    Check if a process with given PID is still running (cross-platform).

    Args:
        pid: Process ID to check

    Returns:
        True if process exists, False otherwise
    """
    if pid <= 0:
        return False

    try:
        # Unix/Linux/macOS: send signal 0 (doesn't actually send signal, just checks)
        if platform.system() != "Windows":
            os.kill(pid, 0)
            return True
        else:
            # Windows: Try to open process handle
            import ctypes

            windll = getattr(ctypes, "windll")
            kernel32 = windll.kernel32
            PROCESS_QUERY_INFORMATION = 0x0400
            handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION, False, pid)
            if handle:
                kernel32.CloseHandle(handle)
                return True
            return False
    except (OSError, ProcessLookupError):
        return False
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        logger.warning(f"Error checking if PID {pid} is alive: {e}")
        return False  # Assume dead if we can't check
