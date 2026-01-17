from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""Diagnostic script to identify processes holding a serial port open.

This script helps diagnose port locking issues by:
1. Listing all processes that may be holding a serial port
2. Showing process tree (parent/child relationships)
3. Checking which processes match debug_attached.py kill patterns
4. Reporting PIDs, process names, command lines, and open files

Usage:
    uv run python ci/debug/port_diagnostic.py COM4
    uv run python ci/debug/port_diagnostic.py /dev/ttyUSB0
"""

import argparse
import sys
from typing import Any, cast

import psutil


def format_cmdline(cmdline: list[str] | None, max_length: int = 80) -> str:
    """Format command line for display, truncating if needed."""
    if not cmdline:
        return "<no cmdline>"
    full = " ".join(cmdline)
    if len(full) <= max_length:
        return full
    return full[: max_length - 3] + "..."


def get_process_tree(proc: psutil.Process) -> list[psutil.Process]:
    """Get the process tree (parents) for a process."""
    tree: list[psutil.Process] = []
    current = proc
    try:
        while current:
            tree.append(current)
            current = current.parent()
    except (psutil.NoSuchProcess, psutil.AccessDenied):
        pass
    return tree


def check_matches_kill_patterns(
    proc_name: str, cmdline: list[str] | None, port_name: str
) -> tuple[bool, list[str]]:
    """Check if process matches debug_attached.py kill patterns.

    Returns:
        Tuple of (matches, list of matching patterns)
    """
    matches: list[str] = []
    proc_name_lower = proc_name.lower()

    # Check Phase 1: pio.exe processes
    if proc_name_lower in ["pio.exe", "pio"]:
        matches.append("Phase 1: pio.exe process")

    # Check Phase 2: esptool processes
    if cmdline:
        cmdline_str = " ".join(cmdline).lower()
        if "esptool" in cmdline_str or "esptool.exe" in proc_name_lower:
            matches.append("Phase 2: esptool process")

    # Check Phase 3: Python PlatformIO processes
    if "python" in proc_name_lower and cmdline:
        cmdline_str = " ".join(cmdline)
        cmdline_lower = cmdline_str.lower()

        # Would skip due to clud protection?
        if "clud" in cmdline_lower:
            matches.append("Phase 3: PROTECTED (clud)")
        elif "pio" in cmdline_lower:
            is_pio_command = any(
                keyword in cmdline_lower
                for keyword in ["pio.exe", "tool-scons", "platformio"]
            )
            if is_pio_command:
                is_fastled8 = (
                    "fastled8" in cmdline_lower or ".platformio" in cmdline_lower
                )
                if is_fastled8:
                    matches.append("Phase 3: Python PlatformIO process")
                else:
                    matches.append("Phase 3: SKIPPED (not fastled8)")

    # Check kill_process_using_port patterns
    serial_exes = [
        "python.exe",
        "python3.exe",
        "python",
        "pio.exe",
        "pio",
        "esptool.exe",
        "esptool.py",
        "esptool",
        "platformio.exe",
        "platformio",
        "miniterm.exe",
        "miniterm.py",
        "miniterm",
        "putty.exe",
        "putty",
        "teraterm.exe",
        "teraterm",
    ]

    cmdline_patterns = [
        "pio monitor",
        "pio device monitor",
        "device monitor",
        "miniterm",
        "esptool",
        port_name.lower(),
    ]

    if any(exe in proc_name_lower for exe in serial_exes):
        if cmdline:
            cmdline_str = " ".join(cmdline).lower()
            if any(pattern in cmdline_str for pattern in cmdline_patterns):
                matches.append("kill_process_using_port: Serial port user")

    return len(matches) > 0, matches


def find_processes_with_open_files(
    port_name: str,
) -> list[tuple[psutil.Process, list[str]]]:
    """Find processes with the specific port open as a file handle.

    Note: On Windows, this function skips the open_files() check because it's
    extremely slow and can hang. COM ports don't typically show up as file
    handles anyway. This function returns empty list on Windows.

    Returns:
        List of tuples (process, list of open file paths matching port)
    """
    results: list[tuple[psutil.Process, list[str]]] = []

    # Skip on Windows - open_files() is too slow and COM ports don't show as files
    import platform

    if platform.system() == "Windows":
        return results

    port_lower = port_name.lower()

    for proc in psutil.process_iter(["pid", "name"]):
        try:
            open_files = proc.open_files()
            matching_files = [
                f.path for f in open_files if port_lower in f.path.lower()
            ]
            if matching_files:
                results.append((proc, matching_files))
        except (psutil.NoSuchProcess, psutil.AccessDenied, AttributeError):
            # Some processes don't have open_files() or access is denied
            pass

    return results


def diagnose_port(port_name: str) -> None:
    """Diagnose which processes might be holding a port."""
    print("=" * 80)
    print(f"Port Lock Diagnostic: {port_name}")
    print("=" * 80)
    print()

    # Step 1: Try to identify processes with the port open as a file handle
    print("[1] Processes with open file handles matching port name:")
    print("-" * 80)
    processes_with_files = find_processes_with_open_files(port_name)

    if processes_with_files:
        for proc, files in processes_with_files:
            try:
                proc_info = cast(
                    dict[str, Any],
                    proc.as_dict(attrs=["pid", "name", "cmdline"]),  # type: ignore[reportUnknownMemberType]
                )
                print(f"  PID {proc_info['pid']}: {proc_info['name']}")
                print(f"    Command: {format_cmdline(proc_info['cmdline'])}")
                print(f"    Open files: {', '.join(files)}")

                # Check if it matches kill patterns
                matches, patterns = check_matches_kill_patterns(
                    proc_info["name"], proc_info["cmdline"], port_name
                )
                if matches:
                    print(f"    ✅ Would be killed by: {', '.join(patterns)}")
                else:
                    print("    ❌ NOT matched by any kill pattern")
                print()
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass
    else:
        print("  No processes found with open file handles matching port name")
        print("  (This is expected on Windows - COM ports may not show as open files)")
        print()

    # Step 2: List all processes that match kill patterns
    print("[2] All processes matching debug_attached.py kill patterns:")
    print("-" * 80)

    found_any = False
    for proc in psutil.process_iter(["pid", "name", "cmdline", "ppid"]):
        try:
            proc_info = cast(
                dict[str, Any],
                proc.as_dict(attrs=["pid", "name", "cmdline", "ppid"]),  # type: ignore[reportUnknownMemberType]
            )
            matches, patterns = check_matches_kill_patterns(
                proc_info["name"], proc_info["cmdline"], port_name
            )

            if matches:
                found_any = True
                print(
                    f"  PID {proc_info['pid']}: {proc_info['name']} (Parent: {proc_info['ppid']})"
                )
                print(f"    Command: {format_cmdline(proc_info['cmdline'])}")
                print(f"    Patterns: {', '.join(patterns)}")

                # Show process tree
                tree = get_process_tree(proc)
                if len(tree) > 1:
                    print("    Process tree:")
                    for i, ancestor in enumerate(reversed(tree)):
                        try:
                            indent = "      " + "  " * i
                            ancestor_info = cast(
                                dict[str, Any],
                                ancestor.as_dict(attrs=["pid", "name"]),  # type: ignore[reportUnknownMemberType]
                            )
                            print(
                                f"{indent}└─ PID {ancestor_info['pid']}: {ancestor_info['name']}"
                            )
                        except (psutil.NoSuchProcess, psutil.AccessDenied):
                            pass
                print()
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass

    if not found_any:
        print("  No processes found matching kill patterns")
        print()

    # Step 3: List ALL serial-related processes (broad search)
    print("[3] All processes with serial-related names (broader search):")
    print("-" * 80)

    serial_keywords = [
        "python",
        "pio",
        "esptool",
        "platformio",
        "miniterm",
        "putty",
        "teraterm",
        "serial",
        "monitor",
        "com",
        "tty",
    ]

    found_any = False
    for proc in psutil.process_iter(["pid", "name", "cmdline"]):
        try:
            proc_info = proc.as_dict(attrs=["pid", "name", "cmdline"])  # type: ignore[reportUnknownMemberType]
            proc_name_lower = proc_info["name"].lower()

            if any(keyword in proc_name_lower for keyword in serial_keywords):
                found_any = True
                print(f"  PID {proc_info['pid']}: {proc_info['name']}")
                print(f"    Command: {format_cmdline(proc_info['cmdline'])}")

                # Check if it matches kill patterns
                matches, patterns = check_matches_kill_patterns(
                    proc_info["name"], proc_info["cmdline"], port_name
                )
                if matches:
                    print(f"    ✅ Would be killed by: {', '.join(patterns)}")
                else:
                    print("    ❌ NOT matched by any kill pattern")
                print()
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass

    if not found_any:
        print("  No serial-related processes found")
        print()

    # Step 4: Summary and recommendations
    print("=" * 80)
    print("SUMMARY AND RECOMMENDATIONS")
    print("=" * 80)

    if processes_with_files:
        print("✅ Found processes with port open as file handle")
        all_matched = all(
            check_matches_kill_patterns(proc.name(), proc.cmdline(), port_name)[0]
            for proc, _ in processes_with_files
        )
        if all_matched:
            print("✅ All processes would be killed by existing logic")
        else:
            print("❌ Some processes would NOT be killed by existing logic")
            print("   → Need to update kill patterns or use psutil.open_files()")
    else:
        print("⚠️  No processes found with port as open file handle")
        print("   This is normal on Windows - COM ports don't show as file handles")
        print("   The port may be locked at the driver level even after process kill")
        print()
        print("   Recommendations:")
        print("   1. Increase retry delay in debug_attached.py (currently 3s)")
        print("   2. Add explicit port availability check before upload retry")
        print("   3. Consider using pyserial to explicitly close port before kill")
        print("   4. Try killing all Python processes (not just fastled8)")


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Diagnose serial port locking issues",
        epilog="Example: uv run python ci/debug/port_diagnostic.py COM4",
    )
    parser.add_argument(
        "port", help="Serial port to diagnose (e.g., COM4, /dev/ttyUSB0)"
    )

    args = parser.parse_args()

    try:
        diagnose_port(args.port)
        return 0
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"❌ Error during diagnosis: {e}", file=sys.stderr)
        import traceback

        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
