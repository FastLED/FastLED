#!/usr/bin/env python3
"""
Notification hook: show OS desktop notification when Claude sends a notification.

This is useful when the user has switched to another window during long-running
operations (builds, test suites). The desktop notification brings their attention
back to Claude Code.

Supports:
- Windows: PowerShell toast notification (BurntToast) or system beep fallback
- macOS: osascript display notification
- Linux: notify-send (libnotify)

Exit codes:
- 0: Always (advisory — notification delivery failure is non-fatal)
"""

import json
import os
import platform
import sys

from running_process import RunningProcess, TimeoutExpired


def notify_windows(title: str, message: str) -> None:
    """Send notification on Windows via PowerShell."""
    # Try BurntToast first (rich toast notification)
    try:
        RunningProcess.run(
            [
                "powershell",
                "-NoProfile",
                "-Command",
                f"if (Get-Module -ListAvailable -Name BurntToast) {{ "
                f"New-BurntToastNotification -Text '{title}', '{message}' "
                f"}} else {{ "
                f"[console]::beep(800,300); [console]::beep(1000,200) "
                f"}}",
            ],
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            timeout=5,
        )
        return
    except (TimeoutExpired, FileNotFoundError, OSError, RuntimeError):
        pass

    # Fallback: simple beep
    try:
        RunningProcess.run(
            ["powershell", "-NoProfile", "-Command", "[console]::beep(800,300)"],
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            timeout=3,
        )
    except (TimeoutExpired, FileNotFoundError, OSError, RuntimeError):
        pass


def notify_macos(title: str, message: str) -> None:
    """Send notification on macOS via osascript."""
    try:
        RunningProcess.run(
            [
                "osascript",
                "-e",
                f'display notification "{message}" with title "{title}"',
            ],
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            timeout=5,
        )
    except (TimeoutExpired, FileNotFoundError, OSError, RuntimeError):
        pass


def notify_linux(title: str, message: str) -> None:
    """Send notification on Linux via notify-send."""
    try:
        RunningProcess.run(
            ["notify-send", title, message, "--expire-time=5000"],
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            timeout=5,
        )
    except (TimeoutExpired, FileNotFoundError, OSError, RuntimeError):
        pass


def main() -> int:
    try:
        input_data = json.load(sys.stdin)
    except json.JSONDecodeError:
        return 0

    # Extract notification details
    title = input_data.get("title", "Claude Code")
    message = input_data.get("message", "")

    if not message:
        return 0

    # Truncate long messages for desktop notification
    if len(message) > 200:
        message = message[:197] + "..."

    # Sanitize for shell injection (remove quotes)
    title = title.replace("'", "").replace('"', "").replace("`", "")
    message = message.replace("'", "").replace('"', "").replace("`", "")

    # Send OS-appropriate notification
    system = platform.system()
    if system == "Windows" or os.name == "nt":
        notify_windows(title, message)
    elif system == "Darwin":
        notify_macos(title, message)
    elif system == "Linux":
        notify_linux(title, message)

    return 0


if __name__ == "__main__":
    sys.exit(main())
