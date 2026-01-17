#!/usr/bin/env python3
"""
Deploy test DLL and runner.exe to tests/bin directory.

This script is called by Meson to deploy test artifacts after building.
It creates the directory structure: tests/bin/<test_name>/
And copies: runner.exe (renamed to <test_name>.exe) and <test_name>.dll
"""

import shutil
import sys
import time
from pathlib import Path


# Import kill_file_lock utility
sys.path.insert(0, str(Path(__file__).parent))
from util.kill_file_lock import kill_process_holding_file


def safe_copy(src: Path, dest: Path, max_retries: int = 3) -> None:
    """
    Safely copy a file, handling Windows file locking issues.

    On Windows, executable files may be locked by the OS or antivirus.
    This function attempts to remove the destination file before copying,
    and retries on failure.
    """
    for attempt in range(max_retries):
        try:
            # Try to remove destination file first (releases locks)
            if dest.exists():
                try:
                    dest.unlink()
                except PermissionError:
                    # File is locked - try to kill the process holding it
                    print(
                        f"Warning: {dest} is locked, attempting to kill process...",
                        file=sys.stderr,
                    )
                    if kill_process_holding_file(dest):
                        # Wait a moment for the process to fully release the file
                        time.sleep(0.2)
                        # Try to delete again
                        try:
                            dest.unlink()
                        except PermissionError:
                            if attempt < max_retries - 1:
                                time.sleep(0.5 * (attempt + 1))
                                continue
                            else:
                                raise
                    else:
                        if attempt < max_retries - 1:
                            # Wait and retry
                            time.sleep(0.5 * (attempt + 1))
                            continue
                        else:
                            raise

            # Copy the file
            shutil.copy2(src, dest)
            return

        except PermissionError as e:
            if attempt < max_retries - 1:
                # Wait and retry
                print(
                    f"Warning: {dest} is locked, retrying in {0.5 * (attempt + 1)}s...",
                    file=sys.stderr,
                )
                time.sleep(0.5 * (attempt + 1))
            else:
                raise PermissionError(
                    f"Failed to copy {src} to {dest} after {max_retries} attempts. "
                    f"File may be locked by another process. Try closing any running tests."
                ) from e


def main():
    if len(sys.argv) < 5:
        print(
            "Usage: deploy_test.py <runner_exe> <test_dll> <test_name> <bin_dir>",
            file=sys.stderr,
        )
        sys.exit(1)

    runner_exe = Path(sys.argv[1])
    test_dll = Path(sys.argv[2])
    test_name = sys.argv[3]
    bin_dir = Path(sys.argv[4])

    # Create test directory
    test_dir = bin_dir / test_name
    test_dir.mkdir(parents=True, exist_ok=True)

    # Copy and rename runner.exe to <test_name>.exe
    dest_exe = test_dir / f"{test_name}.exe"
    safe_copy(runner_exe, dest_exe)
    print(f"Deployed runner: {dest_exe}")

    # Copy test DLL
    dest_dll = test_dir / f"{test_name}.dll"
    safe_copy(test_dll, dest_dll)
    print(f"Deployed DLL: {dest_dll}")

    # Note: clang-build-tools should auto-deploy MSYS DLLs side-by-side
    # when the DLL is built. If dependencies are missing, they may need
    # to be copied manually or the PATH adjusted.

    print(f"Deployed {test_name} to {test_dir}")


if __name__ == "__main__":
    main()
