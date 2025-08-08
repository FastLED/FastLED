#!/usr/bin/env python3
import os
import platform
import sys
import threading
import time
import traceback
from pathlib import Path
from typing import Protocol, cast

import psutil

from ci.util.test_types import TestArgs


class ReconfigurableIO(Protocol):
    def reconfigure(self, *, encoding: str, errors: str) -> None: ...


def setup_environment(args: TestArgs) -> None:
    """Set up the test environment based on arguments"""
    # Handle --quick flag
    if args.quick:
        os.environ["FASTLED_ALL_SRC"] = "1"
        print("Quick mode enabled. FASTLED_ALL_SRC=1")

    # Handle build flags
    if args.show_compile:
        os.environ["FASTLED_TEST_SHOW_COMPILE"] = "1"
    if args.show_link:
        os.environ["FASTLED_TEST_SHOW_LINK"] = "1"


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


def get_process_tree_info(pid: int) -> str:
    """Get information about a process and its children"""
    try:
        process = psutil.Process(pid)
        info = [f"Process {pid} ({process.name()})"]
        info.append(f"Status: {process.status()}")
        info.append(f"CPU Times: {process.cpu_times()}")
        info.append(f"Memory: {process.memory_info()}")

        # Get child processes
        children = process.children(recursive=True)
        if children:
            info.append("\nChild processes:")
            for child in children:
                info.append(f"  Child {child.pid} ({child.name()})")
                info.append(f"    Status: {child.status()}")
                info.append(f"    CPU Times: {child.cpu_times()}")
                info.append(f"    Memory: {child.memory_info()}")

        return "\n".join(info)
    except:
        return f"Could not get process info for PID {pid}"


def kill_process_tree(pid: int) -> None:
    """Kill a process and all its children"""
    try:
        parent = psutil.Process(pid)
        children = parent.children(recursive=True)

        # First try graceful termination
        for child in children:
            try:
                child.terminate()
            except psutil.NoSuchProcess:
                pass

        # Give them a moment to terminate
        _, alive = psutil.wait_procs(children, timeout=3)

        # Force kill any that are still alive
        for child in alive:
            try:
                child.kill()
            except psutil.NoSuchProcess:
                pass

        # Finally terminate the parent
        try:
            parent.terminate()
            parent.wait(3)  # Give it 3 seconds to terminate
        except (psutil.NoSuchProcess, psutil.TimeoutExpired):
            try:
                parent.kill()  # Force kill if still alive
            except psutil.NoSuchProcess:
                pass
    except psutil.NoSuchProcess:
        pass  # Process already gone


def dump_thread_stacks() -> None:
    """Dump stack trace of the main thread and process tree info"""
    print("\n=== MAIN THREAD STACK TRACE ===")
    for thread in threading.enumerate():
        print(f"\nThread {thread.name}:")
        if thread.ident is not None:
            frame = sys._current_frames().get(thread.ident)
            if frame:
                traceback.print_stack(frame)
    print("=== END STACK TRACE ===\n")

    # Dump process tree information
    print("\n=== PROCESS TREE INFO ===")
    print(get_process_tree_info(os.getpid()))
    print("=== END PROCESS TREE INFO ===\n")


def setup_watchdog(timeout: int = 60) -> threading.Thread:
    """Start a watchdog timer to kill the process if it takes too long

    Args:
        timeout: Number of seconds to wait before killing process (default: 60 seconds)
    """

    def watchdog_timer() -> None:
        time.sleep(timeout)
        print(
            f"\n🚨 WATCHDOG TIMER EXPIRED - Process took too long! ({timeout} seconds)"
        )
        dump_thread_stacks()

        # Kill all child processes and then ourselves
        kill_process_tree(os.getpid())

        # If we're still here, force exit
        time.sleep(1)  # Give a moment for output to flush
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
        kill_process_tree(os.getpid())
        os._exit(1)

    daemon_thread = threading.Thread(
        target=force_exit, daemon=True, name="ForceExitDaemon"
    )
    daemon_thread.start()
    return daemon_thread
