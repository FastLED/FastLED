"""Windows-specific link retry helpers for transient ld.lld permission-denied failures.

On Windows, the link step for `tests/runner.exe` can intermittently fail with:

    ld.lld: error: failed to write output 'tests/runner.exe': permission denied

The root cause is Windows refusing to overwrite a running .exe — either a stale
runner process from a prior test invocation that hasn't been fully reaped, or
an anti-virus / Windows Defender scanner holding a read handle on the freshly
written .exe for a few milliseconds after close.

This module provides:
  * ``is_link_permission_denied_error`` — detect the specific error signature.
  * ``kill_stale_runner_processes`` — terminate lingering runner.exe /
    example_runner.exe processes whose executable lives under ``build_dir``.

Both helpers are intentionally no-ops on non-Windows platforms so the callers
can use them unconditionally without regressing Linux/macOS behavior.

Related: FastLED issue #2268.
"""

from __future__ import annotations

import os
import re
import sys
from pathlib import Path


# Names of runner executables that participate in the DLL-based test
# architecture. These are the binaries that ``ld.lld`` overwrites at link time
# and that may be left running from a prior invocation.
_RUNNER_NAMES: tuple[str, ...] = (
    "runner.exe",
    "example_runner.exe",
)


# Match the ld.lld error on any of the runner binaries. Kept broad enough to
# tolerate forward or backward slashes in the path as emitted by ninja.
#
# Example line:
#   ld.lld: error: failed to write output 'tests/runner.exe': permission denied
_PERM_DENIED_PATTERN = re.compile(
    r"ld\.lld:\s*error:\s*failed to write output\s+'[^']*"
    r"(?:runner|example_runner)\.exe'\s*:\s*permission denied",
    re.IGNORECASE,
)


def is_link_permission_denied_error(output: str) -> bool:
    """Return True if *output* contains the Windows ld.lld permission-denied error.

    Only matches failures on runner.exe / example_runner.exe — other
    permission-denied messages (e.g. an actual bug in build config) are NOT
    treated as transient and therefore will NOT be retried.
    """
    if sys.platform != "win32":
        return False
    if not output:
        return False
    return bool(_PERM_DENIED_PATTERN.search(output))


def kill_stale_runner_processes(build_dir: Path) -> int:
    """Kill any runner.exe / example_runner.exe processes under *build_dir*.

    Iterates running processes (via ``psutil``) and terminates any whose name
    matches a known runner binary AND whose executable path resolves to
    somewhere under ``build_dir``. This avoids killing unrelated processes
    with the same name that happen to be running on the machine.

    Returns the number of processes killed. No-op and returns 0 on non-Windows.

    Safe to call when psutil is not importable — returns 0 silently.
    """
    if sys.platform != "win32":
        return 0

    try:
        import psutil  # noqa: PLC0415 - lazy import, Windows-only helper
    except ImportError:
        return 0

    try:
        build_root = str(build_dir.resolve())
    except OSError:
        return 0

    killed = 0
    for proc in psutil.process_iter(["pid", "name", "exe"]):
        try:
            name = (proc.info.get("name") or "").lower()
            if name not in _RUNNER_NAMES:
                continue

            exe = proc.info.get("exe")
            if not exe:
                continue

            # Only terminate runners launched from *our* build directory.
            try:
                exe_resolved = str(Path(exe).resolve())
            except OSError:
                continue

            if exe_resolved == build_root or exe_resolved.startswith(
                build_root + os.sep
            ):
                proc.kill()
                killed += 1
        except (psutil.NoSuchProcess, psutil.AccessDenied, OSError):
            continue

    return killed
