
import os
from pathlib import Path
from typing import Callable
import subprocess
import time

from compile_lock import COMPILE_LOCK

VOLUME_MAPPED_SRC = Path("/host/fastled/src")
RSYNC_DEST = Path("/js/fastled/src")
TIME_START = time.time()

def sync_src_to_target(
    src: Path, dst: Path, callback: Callable[[], None] | None = None
) -> bool:
    """Sync the volume mapped source directory to the FastLED source directory."""
    suppress_print = (
        TIME_START + 30 > time.time()
    )  # Don't print during initial volume map.
    if not src.exists():
        # Volume is not mapped in so we don't rsync it.
        print(f"Skipping rsync, as fastled src at {src} doesn't exist")
        return False
    try:
        exclude_hidden = "--exclude=.*/"  # suppresses folders like .mypy_cache/
        print("\nSyncing source directories...")
        with COMPILE_LOCK:
            # Use rsync to copy files, preserving timestamps and deleting removed files
            cp: subprocess.CompletedProcess = subprocess.run(
                ["rsync", "-av", "--info=NAME", "--delete", f"{src}/", f"{dst}/", exclude_hidden],
                check=True,
                text=True,
                capture_output=True,
            )
            if cp.returncode == 0:
                changed = False
                changed_lines: list[str] = []
                lines = cp.stdout.split("\n")
                for line in lines:
                    suffix = line.strip().split(".")[-1]
                    if suffix in ["cpp", "h", "hpp", "ino", "py", "js", "html", "css"]:
                        if not suppress_print:
                            print(f"Changed file: {line}")
                        changed = True
                        changed_lines.append(line)
                if changed:
                    if not suppress_print:
                        print(f"FastLED code had updates: {changed_lines}")
                    if callback:
                        callback()
                    return True
                print("Source directory synced successfully with no changes")
                return False
            else:
                print(f"Error syncing directories: {cp.stdout}\n\n{cp.stderr}")
                return False

    except subprocess.CalledProcessError as e:
        print(f"Error syncing directories: {e.stdout}\n\n{e.stderr}")
    except Exception as e:
        print(f"Error syncing directories: {e}")
    return False



def sync_source_directory_if_volume_is_mapped(
    callback: Callable[[], None] | None = None
) -> bool:
    """Sync the volume mapped source directory to the FastLED source directory."""
    if not VOLUME_MAPPED_SRC.exists():
        # Volume is not mapped in so we don't rsync it.
        print("Skipping rsync, as fastled src volume not mapped")
        return False
    print("Syncing source directories because host is mapped in")
    return sync_src_to_target(VOLUME_MAPPED_SRC, RSYNC_DEST, callback=callback)