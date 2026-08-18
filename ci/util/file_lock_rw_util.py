from ci.util.global_interrupt_handler import handle_keyboard_interrupt


"""
Utility functions for file locking - extracted to avoid circular imports.

Contains the cross-platform process alive check used by both
lock_database.py and file_lock_rw.py.
"""

import errno
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
            try:
                os.kill(pid, 0)
                return True
            except ProcessLookupError:
                return False
            except PermissionError:
                # The process exists, but this user cannot signal it.
                return True
            except OSError as e:
                if e.errno == errno.ESRCH:
                    return False
                logger.warning(f"Unable to determine whether PID {pid} is alive: {e}")
                return True
        else:
            # Opening a Windows process handle only proves that the process
            # object still exists. A terminated child remains openable while
            # its parent retains a handle, so using OpenProcess alone makes a
            # dead lock owner look alive indefinitely. Query whether the
            # process object is signaled instead.
            import ctypes
            from ctypes import wintypes

            kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel32.OpenProcess.argtypes = [
                wintypes.DWORD,
                wintypes.BOOL,
                wintypes.DWORD,
            ]
            kernel32.OpenProcess.restype = wintypes.HANDLE
            kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
            kernel32.WaitForSingleObject.restype = wintypes.DWORD
            kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
            kernel32.CloseHandle.restype = wintypes.BOOL

            synchronize = 0x00100000
            error_invalid_parameter = 87
            wait_object_0 = 0
            wait_timeout = 258
            handle = kernel32.OpenProcess(synchronize, False, pid)
            if not handle:
                error = ctypes.get_last_error()
                if error == error_invalid_parameter:
                    return False
                # Access denied and transient query failures are not proof of
                # death. Treat them as alive rather than stealing their lock.
                logger.warning(
                    f"Unable to open PID {pid} for liveness check "
                    f"(Windows error {error}); treating it as alive"
                )
                return True

            try:
                wait_result = kernel32.WaitForSingleObject(handle, 0)
                if wait_result == wait_object_0:
                    return False
                if wait_result == wait_timeout:
                    return True
                logger.warning(
                    f"Unable to query wait state for PID {pid} "
                    f"(result {wait_result}); treating it as alive"
                )
                return True
            finally:
                kernel32.CloseHandle(handle)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        logger.warning(f"Error checking if PID {pid} is alive: {e}")
        return True  # Indeterminate is not safe evidence for stealing a lock
