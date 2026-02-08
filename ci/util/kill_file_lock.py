from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Utility to find and kill processes holding file locks.

This is needed on Windows where test executables may remain running
and prevent redeployment of new test binaries.
"""

import sys
from pathlib import Path


def kill_process_holding_file(file_path: Path) -> bool:
    """
    Find and kill process(es) holding a lock on the specified file.

    On Windows, executable files being replaced are often locked by running
    instances of themselves. This function uses a two-pronged approach:
    1. Look for processes with matching executable name
    2. Try to find processes with open handles (may fail without admin rights)

    Returns:
        True if a process was killed, False otherwise
    """
    try:
        import psutil
    except ImportError:
        print(
            "Warning: psutil not available, cannot kill locked processes",
            file=sys.stderr,
        )
        return False

    if not file_path.exists():
        return False

    file_path = file_path.resolve()
    exe_name = file_path.name
    killed = False

    # Strategy 1: Kill processes by matching executable name
    # This is the most reliable approach on Windows
    for proc in psutil.process_iter(["pid", "name", "exe"]):
        try:
            proc_name = proc.info["name"]
            if proc_name and proc_name.lower() == exe_name.lower():
                # Found a process with matching name
                proc_exe = proc.info.get("exe")
                if proc_exe:
                    proc_exe_path = Path(proc_exe).resolve()
                    if proc_exe_path == file_path:
                        print(
                            f"Found process holding lock: {proc_name} (PID {proc.info['pid']})",
                            file=sys.stderr,
                        )
                        proc.kill()
                        proc.wait(timeout=5)
                        print(f"Killed process {proc.info['pid']}", file=sys.stderr)
                        killed = True
        except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.TimeoutExpired):
            # Process may have exited or we don't have permission
            continue
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception:
            # Ignore other errors and continue
            continue

    # Strategy 2: Try to find by open file handles (may not work on Windows without admin)
    if not killed:
        for proc in psutil.process_iter(["pid", "name"]):
            try:
                # Get open files for this process
                open_files = proc.open_files()
                for open_file in open_files:
                    if Path(open_file.path).resolve() == file_path:
                        print(
                            f"Found process holding lock: {proc.info['name']} (PID {proc.info['pid']})",
                            file=sys.stderr,
                        )
                        proc.kill()
                        proc.wait(timeout=5)
                        print(f"Killed process {proc.info['pid']}", file=sys.stderr)
                        killed = True
            except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.TimeoutExpired):
                # Process may have exited or we don't have permission
                continue
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception:
                # Ignore other errors and continue
                continue

    return killed


def main():
    if len(sys.argv) < 2:
        print("Usage: kill_file_lock.py <file_path>", file=sys.stderr)
        sys.exit(1)

    file_path = Path(sys.argv[1])
    if kill_process_holding_file(file_path):
        print(f"Successfully killed process(es) holding {file_path}")
    else:
        print(f"No processes found holding {file_path}")


if __name__ == "__main__":
    main()
