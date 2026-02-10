#!/usr/bin/env python3
"""Deadlock detection and stack dumping for hung tests."""

import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Optional

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.timestamp_print import ts_print


def find_debugger() -> tuple[Optional[str], str]:
    """
    Find available debugger on the system.

    Returns:
        Tuple of (debugger_path, debugger_name) or (None, "none")
    """
    # Try clang-tool-chain-lldb first (preferred, supports --batch/--attach-pid)
    lldb = shutil.which("clang-tool-chain-lldb")
    if lldb:
        return (lldb, "lldb")

    # Try gdb as fallback
    gdb = shutil.which("gdb")
    if gdb:
        return (gdb, "gdb")

    return (None, "none")


def dump_stack_trace_lldb(pid: int, test_name: str) -> Optional[str]:
    """
    Attach lldb to a process and dump stack traces for all threads.

    NOTE: Crash handlers use signal chaining - they dump internally first,
    then uninstall themselves and re-raise signals. This allows both internal
    crash dumps (for actual crashes) and external debugger attachment (for hangs).

    The process can optionally set FASTLED_DISABLE_CRASH_HANDLER=1 to completely
    disable internal handlers if only external dumps are desired.

    Args:
        pid: Process ID to attach to
        test_name: Name of the test (for logging)

    Returns:
        Stack trace output or None if failed
    """
    debugger, debugger_type = find_debugger()

    if not debugger or debugger_type == "none":
        ts_print(
            "⚠️  No debugger found (clang-tool-chain-lldb or gdb required for stack traces)"
        )
        ts_print(
            "   LLDB is bundled with clang-tool-chain (already installed via pyproject.toml)"
        )
        return None

    ts_print(f"📍 Attaching {debugger_type} to hung process (PID {pid})...")
    ts_print(
        "   Note: Crash handlers use signal chaining (internal dump → external debugger)"
    )

    try:
        if debugger_type == "lldb":
            return _dump_with_lldb(debugger, pid, test_name)
        else:  # gdb
            return _dump_with_gdb(debugger, pid, test_name)
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
    except Exception as e:
        ts_print(f"❌ Failed to attach debugger: {e}")
        return None


def _dump_with_lldb(lldb_path: str, pid: int, test_name: str) -> Optional[str]:
    """Dump stack trace using lldb."""
    # Create lldb command script
    with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".lldb") as f:
        # Write lldb commands
        f.write("# Attach to process and dump all thread stacks\n")
        f.write("settings set target.process.stop-on-exec false\n")
        f.write("settings set target.process.stop-on-sharedlibrary-events false\n")
        f.write("settings set target.x86-disassembly-flavor intel\n")
        f.write("\n")
        f.write("# Get backtrace for all threads\n")
        f.write("thread backtrace all\n")
        f.write("\n")
        f.write("# Show thread info\n")
        f.write("thread list\n")
        f.write("\n")
        f.write("# Quit\n")
        f.write("quit\n")
        script_path = f.name

    try:
        # Run lldb with the script
        cmd = [
            lldb_path,
            "--batch",
            "--source",
            script_path,
            "--attach-pid",
            str(pid),
        ]

        ts_print(f"Running: {' '.join(cmd)}")

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=30,  # 30 second timeout for debugger
        )

        output = result.stdout + result.stderr

        # Clean up script
        os.unlink(script_path)

        if output and output.strip():
            return output
        else:
            return "Debugger attached but produced no output"

    except subprocess.TimeoutExpired:
        ts_print("⏱️  Debugger timed out after 30 seconds")
        try:
            os.unlink(script_path)
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
        except:
            pass
        return "Debugger timed out"
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
    except Exception as e:
        ts_print(f"❌ Debugger failed: {e}")
        try:
            os.unlink(script_path)
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
        except:
            pass
        return None


def _dump_with_gdb(gdb_path: str, pid: int, test_name: str) -> Optional[str]:
    """Dump stack trace using gdb."""
    # Create gdb command script
    with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".gdb") as f:
        f.write("set pagination off\n")
        f.write("set confirm off\n")
        f.write("set print pretty on\n")
        f.write("\n")
        f.write("# Get backtrace for all threads\n")
        f.write("thread apply all backtrace full\n")
        f.write("\n")
        f.write("# Show thread info\n")
        f.write("info threads\n")
        f.write("\n")
        f.write("quit\n")
        script_path = f.name

    try:
        # Run gdb with the script
        cmd = [
            gdb_path,
            "--batch",
            "--command",
            script_path,
            "--pid",
            str(pid),
        ]

        ts_print(f"Running: {' '.join(cmd)}")

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=30,  # 30 second timeout for debugger
        )

        output = result.stdout + result.stderr

        # Clean up script
        os.unlink(script_path)

        if output and output.strip():
            return output
        else:
            return "Debugger attached but produced no output"

    except subprocess.TimeoutExpired:
        ts_print("⏱️  Debugger timed out after 30 seconds")
        try:
            os.unlink(script_path)
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
        except:
            pass
        return "Debugger timed out"
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
    except Exception as e:
        ts_print(f"❌ Debugger failed: {e}")
        try:
            os.unlink(script_path)
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
        except:
            pass
        return None


def handle_hung_test(pid: int, test_name: str, timeout_seconds: float) -> str:
    """
    Handle a hung test by dumping stack trace and killing it.

    Args:
        pid: Process ID of hung test
        test_name: Name of the test
        timeout_seconds: How long the test has been running

    Returns:
        Formatted error message with stack trace
    """
    ts_print("=" * 80)
    ts_print(f"🚨 HUNG TEST DETECTED: {test_name}")
    ts_print(f"⏱️  Test exceeded timeout of {timeout_seconds:.1f} seconds")
    ts_print(f"🔍 Process ID: {pid}")
    ts_print("=" * 80)

    # Dump stack trace
    stack_trace = dump_stack_trace_lldb(pid, test_name)

    # Format output
    error_msg = f"\n{'=' * 80}\n"
    error_msg += f"HUNG TEST: {test_name}\n"
    error_msg += f"Timeout: {timeout_seconds:.1f}s\n"
    error_msg += f"PID: {pid}\n"
    error_msg += "=" * 80 + "\n"

    if stack_trace:
        error_msg += "\nTHREAD STACK TRACES:\n"
        error_msg += "-" * 80 + "\n"
        error_msg += stack_trace
        error_msg += "\n" + "-" * 80 + "\n"
    else:
        error_msg += "\n⚠️  Could not obtain stack trace (debugger not available)\n"

    error_msg += "=" * 80 + "\n"

    # Print to console
    ts_print(error_msg)

    # Kill the hung process
    try:
        if platform.system() == "Windows":
            subprocess.run(
                ["taskkill", "/F", "/PID", str(pid)], capture_output=True, timeout=5
            )
        else:
            subprocess.run(["kill", "-9", str(pid)], capture_output=True, timeout=5)
        ts_print(f"🔪 Killed hung process (PID {pid})")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
    except Exception as e:
        ts_print(f"⚠️  Failed to kill process: {e}")

    return error_msg


if __name__ == "__main__":
    # Test the debugger detection
    debugger, debugger_type = find_debugger()
    if debugger:
        print(f"Found debugger: {debugger_type} at {debugger}")
    else:
        print("No debugger found (lldb or gdb required)")
        sys.exit(1)
