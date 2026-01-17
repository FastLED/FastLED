from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Lock handler utility for detecting and breaking file locks on Windows.

Uses psutil to find processes holding locks on files/directories and forcefully
terminates them to enable cleanup of corrupted package directories.
"""

import platform
import subprocess
import time
from pathlib import Path
from typing import Any


try:
    import psutil
except ImportError:
    psutil = None  # type: ignore


def is_psutil_available() -> bool:
    """Check if psutil is available."""
    return psutil is not None


def find_processes_locking_path(
    path: Path, max_processes: int = 500, timeout: float = 60.0
) -> set[int]:
    """Find all process IDs that have open handles to files under the given path.

    Args:
        path: Path to check for locks (file or directory)
        max_processes: Maximum number of processes to check (default: 500)
        timeout: Maximum time in seconds to spend checking (default: 60.0)

    Returns:
        Set of process IDs holding locks on the path
    """
    if not is_psutil_available():
        print("Warning: psutil not available, cannot detect lock holders")
        return set()

    if not path.exists():
        return set()

    # Normalize path for comparison
    try:
        normalized_path = path.resolve()
    except (OSError, RuntimeError):
        # Path might be in a bad state
        normalized_path = path.absolute()

    locking_pids: set[int] = set()
    path_str = str(normalized_path).lower()
    start_time = time.time()
    processes_checked = 0

    # Priority process names (check these first)
    priority_names = {"python.exe", "python3.exe", "python", "uv.exe", "uv"}

    # Get all processes and sort by priority
    all_procs = list(psutil.process_iter(["pid", "name"]))  # type: ignore[misc]

    # Sort processes: priority names first, then others
    def sort_key(proc: Any) -> int:
        try:
            name = proc.info["name"].lower() if proc.info.get("name") else ""
            return 0 if name in priority_names else 1
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception:
            return 2

    all_procs.sort(key=sort_key)

    # Iterate through processes with limits
    for proc in all_procs:
        # Check timeout
        if time.time() - start_time > timeout:
            print(
                f"  Warning: Lock detection timed out after {timeout}s (checked {processes_checked} processes)"
            )
            break

        # Check process limit
        if processes_checked >= max_processes:
            print(
                f"  Warning: Lock detection stopped after checking {max_processes} processes"
            )
            break

        processes_checked += 1

        try:
            # Get open files for this process
            open_files = proc.open_files()

            for file_info in open_files:
                file_path = Path(file_info.path).resolve()
                file_path_str = str(file_path).lower()

                # Check if this file is under our target path
                if normalized_path.is_dir():
                    # Check if file is inside the directory
                    if file_path_str.startswith(path_str):
                        locking_pids.add(proc.info["pid"])
                        print(
                            f"  Process {proc.info['pid']} ({proc.info['name']}) has open file: {file_info.path}"
                        )
                        break
                else:
                    # Check if file matches exactly
                    if file_path_str == path_str:
                        locking_pids.add(proc.info["pid"])
                        print(
                            f"  Process {proc.info['pid']} ({proc.info['name']}) has open file: {file_info.path}"
                        )
                        # Verify process still exists
                        if not psutil.pid_exists(proc.info["pid"]):  # type: ignore[misc]
                            print(
                                f"  Warning: Process {proc.info['pid']} no longer exists (stale)"
                            )
                            locking_pids.discard(proc.info["pid"])
                        break

        except (
            psutil.NoSuchProcess,
            psutil.AccessDenied,
            psutil.ZombieProcess,
            PermissionError,
        ):
            # Skip processes we can't access
            continue
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception:
            # Skip any other errors (e.g., invalid paths)
            continue

    return locking_pids


def kill_processes(pids: set[int], force: bool = True) -> bool:
    """Kill processes by PID.

    Args:
        pids: Set of process IDs to kill
        force: If True, use SIGKILL (Windows: TerminateProcess), otherwise SIGTERM

    Returns:
        True if all processes were killed successfully
    """
    if not pids:
        return True

    if not is_psutil_available():
        print("Warning: psutil not available, cannot kill processes")
        return False

    all_killed = True

    for pid in pids:
        try:
            proc = psutil.Process(pid)
            proc_name = proc.name()

            print(f"  Killing process {pid} ({proc_name})...")

            if force:
                proc.kill()  # SIGKILL on Unix, TerminateProcess on Windows
            else:
                proc.terminate()  # SIGTERM on Unix, gentle termination on Windows

            # Wait for process to die (with timeout)
            try:
                proc.wait(timeout=3)
                print(f"  ✓ Process {pid} terminated successfully")
            except psutil.TimeoutExpired:
                print(f"  ⚠ Process {pid} did not terminate within timeout, forcing...")
                proc.kill()
                proc.wait(timeout=1)
                print(f"  ✓ Process {pid} force-killed")

        except psutil.NoSuchProcess:
            print(f"  ✓ Process {pid} already terminated")
        except psutil.AccessDenied:
            print(
                f"  ✗ Access denied killing process {pid} (may require admin privileges)"
            )
            all_killed = False
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"  ✗ Failed to kill process {pid}: {e}")
            all_killed = False

    # Give system time to release file handles
    if pids:
        time.sleep(0.5)

    return all_killed


def force_remove_path(path: Path, max_retries: int = 3) -> bool:
    """Force remove a path by killing processes that have it locked.

    Args:
        path: Path to remove (file or directory)
        max_retries: Maximum number of retry attempts

    Returns:
        True if path was successfully removed
    """
    import shutil

    if not path.exists():
        return True

    print(f"Attempting to remove locked path: {path}")

    for attempt in range(max_retries):
        try:
            # Try to remove directly first
            if path.is_dir():
                shutil.rmtree(path)
            else:
                path.unlink()

            print(f"✓ Successfully removed: {path}")
            return True

        except (OSError, PermissionError) as e:
            print(f"Attempt {attempt + 1}/{max_retries}: Failed to remove {path}: {e}")

            if attempt < max_retries - 1:
                # Find and kill locking processes
                print("  Searching for processes locking the path...")
                locking_pids = find_processes_locking_path(path)

                if locking_pids:
                    print(f"  Found {len(locking_pids)} process(es) locking the path")
                    kill_processes(locking_pids, force=True)
                else:
                    print("  No locking processes found")

                    # On Windows, try using system commands as fallback
                    if platform.system() == "Windows":
                        print("  Trying Windows-specific removal...")
                        try:
                            if path.is_dir():
                                result = subprocess.run(
                                    ["cmd", "/c", "rmdir", "/S", "/Q", str(path)],
                                    capture_output=True,
                                    text=True,
                                    timeout=30,
                                )
                            else:
                                result = subprocess.run(
                                    ["cmd", "/c", "del", "/F", "/Q", str(path)],
                                    capture_output=True,
                                    text=True,
                                    timeout=30,
                                )

                            if result.returncode == 0:
                                print("  ✓ Windows command succeeded")
                                return True
                            else:
                                print(f"  ✗ Windows command failed: {result.stderr}")
                        except KeyboardInterrupt:
                            handle_keyboard_interrupt_properly()
                            raise
                        except Exception as cmd_error:
                            print(f"  ✗ Windows command error: {cmd_error}")

                # Wait a bit before retrying
                time.sleep(1)
            else:
                print(f"✗ Failed to remove {path} after {max_retries} attempts")
                return False

    return False


def get_process_info(pids: set[int]) -> list[dict[str, Any]]:
    """Get detailed information about processes.

    Args:
        pids: Set of process IDs

    Returns:
        List of dictionaries with process information
    """
    if not is_psutil_available():
        return []

    process_info: list[dict[str, Any]] = []

    for pid in pids:
        try:
            proc = psutil.Process(pid)
            info = {
                "pid": pid,
                "name": proc.name(),
                "exe": proc.exe(),
                "cmdline": " ".join(proc.cmdline()),
                "status": proc.status(),
            }
            process_info.append(info)
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            process_info.append(
                {
                    "pid": pid,
                    "name": "Unknown",
                    "exe": "Unknown",
                    "cmdline": "Unknown",
                    "status": "Unknown",
                }
            )

    return process_info
