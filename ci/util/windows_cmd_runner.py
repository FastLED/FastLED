#!/usr/bin/env python3
"""Windows CMD runner for Git Bash compatibility.

This module provides utilities to run PlatformIO commands via Windows CMD shell
when running from Git Bash/MSys environment. This solves the ESP-IDF toolchain
incompatibility with Git Bash on Windows.

The ESP-IDF v5.5.x idf_tools.py script explicitly checks for Git Bash indicators
(MSYSTEM, TERM, SHELL, etc.) and aborts with:
    "ERROR: MSys/Mingw is not supported. Please follow the getting started guide"

This module strips Git Bash environment variables and runs commands via cmd.exe
to provide a clean Windows environment for ESP-IDF tooling.

Usage:
    from ci.util.windows_cmd_runner import should_use_cmd_runner, get_clean_windows_env

    if should_use_cmd_runner():
        # Run command via cmd.exe with clean environment
        env = get_clean_windows_env()
        subprocess.run(cmd, shell=True, env=env)
    else:
        # Run command directly (Linux/Mac or native Windows shell)
        subprocess.run(cmd)
"""

import os
import platform


def is_git_bash() -> bool:
    """Detect if running in Git Bash/MSys environment on Windows.

    Returns:
        True if running in Git Bash, False otherwise
    """
    return platform.system() == "Windows" and "MSYSTEM" in os.environ


def should_use_cmd_runner() -> bool:
    """Determine if CMD runner should be used for PlatformIO commands.

    Returns:
        True if running in Git Bash on Windows, False otherwise
    """
    return is_git_bash()


def get_clean_windows_env() -> dict[str, str]:
    """Create a clean Windows environment without Git Bash indicators.

    This removes Git Bash environment variables (MSYSTEM, TERM, SHELL, etc.)
    that might cause ESP-IDF tooling to detect Git Bash and abort compilation.

    This function is extracted from ci/util/pio_package_daemon.py where it
    was originally implemented for package installations. Now we reuse it
    for compilation as well.

    Returns:
        Clean environment dict suitable for running pio via cmd.exe
    """
    # Start with minimal system environment
    clean_env: dict[str, str] = {}

    # Git Bash environment variables to EXCLUDE (these cause ESP-IDF to abort)
    git_bash_vars = {
        "MSYSTEM",  # MSYS2/Git Bash indicator
        "MSYSTEM_CARCH",
        "MSYSTEM_CHOST",
        "MSYSTEM_PREFIX",
        "MINGW_CHOST",
        "MINGW_PACKAGE_PREFIX",
        "MINGW_PREFIX",
        "TERM",  # Often set to 'xterm' in Git Bash
        "SHELL",  # Points to bash.exe in Git Bash
        "BASH",
        "BASH_ENV",
        "SHLVL",
        "OLDPWD",
        "LS_COLORS",
        "ACLOCAL_PATH",
        "MANPATH",
        "INFOPATH",
        "PKG_CONFIG_PATH",
        "ORIGINAL_PATH",
        "MSYS",
    }

    # Copy safe environment variables from parent
    for key, value in os.environ.items():
        # Skip Git Bash indicators
        if key in git_bash_vars:
            continue

        # Skip variables starting with MSYS or MINGW prefixes
        if key.startswith(("MSYS", "MINGW", "BASH_")):
            continue

        # Keep important system variables
        clean_env[key] = value

    # Ensure critical variables are set for Windows
    if platform.system() == "Windows":
        # Force UTF-8 encoding
        clean_env["PYTHONIOENCODING"] = "utf-8"
        clean_env["PYTHONUTF8"] = "1"

        # Set COMSPEC to cmd.exe if not already set
        if "COMSPEC" not in clean_env:
            clean_env["COMSPEC"] = "C:\\Windows\\System32\\cmd.exe"

        # Ensure SystemRoot is set (required by many Windows tools)
        if "SystemRoot" not in clean_env:
            clean_env["SystemRoot"] = "C:\\Windows"

        # Ensure TEMP/TMP are set
        if "TEMP" not in clean_env:
            clean_env["TEMP"] = os.environ.get("TEMP", "C:\\Windows\\Temp")
        if "TMP" not in clean_env:
            clean_env["TMP"] = os.environ.get("TMP", "C:\\Windows\\Temp")

    return clean_env


def format_cmd_for_shell(cmd: list[str]) -> str:
    """Format command list for Windows CMD shell execution.

    Properly quotes arguments that contain spaces or special characters.

    Args:
        cmd: Command as list of strings

    Returns:
        Command string suitable for shell=True execution
    """
    quoted_parts: list[str] = []
    for part in cmd:
        # Quote if contains spaces or special characters
        if " " in part or any(c in part for c in "&|<>^"):
            # Escape quotes inside the part
            escaped = part.replace('"', '\\"')
            quoted_parts.append(f'"{escaped}"')
        else:
            quoted_parts.append(part)

    return " ".join(quoted_parts)
