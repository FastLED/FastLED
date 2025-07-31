#!/usr/bin/env python3
import os
import sys
import threading
import time
from pathlib import Path
from typing import Protocol, cast

from ci.ci.test_types import TestArgs


class ReconfigurableIO(Protocol):
    def reconfigure(self, *, encoding: str, errors: str) -> None: ...


def setup_environment(args: TestArgs) -> None:
    """Set up the test environment based on arguments"""
    # Handle --quick flag
    if args.quick:
        os.environ["FASTLED_ALL_SRC"] = "1"
        print("Quick mode enabled. FASTLED_ALL_SRC=1")

    # Handle legacy flag - set environment variable for subprocesses
    if args.legacy:
        os.environ["USE_CMAKE"] = "1"
        print("Legacy mode enabled. Using CMake build system.")
    else:
        # Ensure USE_CMAKE is not set
        os.environ.pop("USE_CMAKE", None)


def setup_windows_console() -> None:
    """Configure Windows console for UTF-8 output"""
    if os.name == "nt":  # Windows
        if hasattr(sys.stdout, "reconfigure"):
            cast(ReconfigurableIO, sys.stdout).reconfigure(
                encoding="utf-8", errors="replace"
            )
        if hasattr(sys.stderr, "reconfigure"):
            cast(ReconfigurableIO, sys.stderr).reconfigure(
                encoding="utf-8", errors="replace"
            )


def setup_watchdog(timeout: int = 600) -> threading.Thread:
    """Start a watchdog timer to kill the process if it takes too long"""

    def watchdog_timer() -> None:
        time.sleep(timeout)  # Default 10 minutes
        print(f"Watchdog timer expired after {timeout} seconds - forcing exit")
        os._exit(2)  # Exit with error code 2 to indicate timeout

    watchdog = threading.Thread(
        target=watchdog_timer, daemon=True, name="WatchdogTimer"
    )
    watchdog.start()
    return watchdog


def setup_force_exit() -> threading.Thread:
    """Set up a force exit daemon thread"""

    def force_exit() -> None:
        time.sleep(1)
        print("Force exit daemon thread invoked")
        os._exit(1)

    daemon_thread = threading.Thread(
        target=force_exit, daemon=True, name="ForceExitDaemon"
    )
    daemon_thread.start()
    return daemon_thread
