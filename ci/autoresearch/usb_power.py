"""Preflight warning for Windows USB selective suspend on HIL boards.

Windows may power down a USB port to save energy ("selective suspend" in the
power plan, "allow the computer to turn off this device" per device). A dev
board that does not handle suspend/resume cleanly can fail to come back and
land as `Unknown USB Device (Device Descriptor Request Failed)` — problem code
43. In `fbuild port scan` that board's old COM record then shows as
`health=phantom`, which reads like a wedged board and sends people chasing
BOOTSEL and PnP recovery when the real cause is host power management.

This module only *warns*. It never fails a run, never mutates host settings,
and is a no-op off Windows. Changing the setting needs an elevated shell and
is the operator's call:

    powercfg -setacvalueindex SCHEME_CURRENT 2a737441-1930-4402-8d77-b2bebba308a3 \
        48e6b7a6-50f5-4782-a5d4-53bb8f07e226 0

See agents/docs/hardware-autoresearch.md and FastLED #3864.
"""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from typing import Callable, Optional

from typeguard import typechecked

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


# Windows power-setting GUIDs for the USB subgroup / selective suspend.
_USB_SUBGROUP_GUID = "2a737441-1930-4402-8d77-b2bebba308a3"
_SELECTIVE_SUSPEND_GUID = "48e6b7a6-50f5-4782-a5d4-53bb8f07e226"

# Only a plain COMn name is ever interpolated into a PowerShell script. See
# `port_presence` for why this is an allowlist and not an escape.
_COM_PORT_RE = re.compile(r"^COM\d+$", re.IGNORECASE)

# How many parents to walk from the COM port up toward the root hub. Four
# covers port -> composite device -> hub -> root hub with room to spare.
_MAX_ANCESTORS = 4

CommandRunner = Callable[[list[str]], str]


def _run(cmd: list[str]) -> str:
    """Run a command, returning stdout. Empty string on any failure."""
    from running_process import RunningProcess

    try:
        # capture_output=True is load-bearing: it defaults to False, which
        # streams to the console and leaves result.stdout empty, silently
        # disabling every check in this module.
        result = RunningProcess.run(
            cmd, cwd=None, check=False, timeout=30, capture_output=True
        )
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        return ""
    return result.stdout or ""


def _powershell(script: str, run: CommandRunner) -> str:
    return run(["powershell", "-NoProfile", "-NonInteractive", "-Command", script])


@typechecked
@dataclass(frozen=True)
class PortPresence:
    """Whether a port is in the live device tree, and when it was last seen.

    `present` is deliberately tri-state. `None` means the host could not say,
    and callers must treat that as "no opinion" rather than "absent" — see
    `absent_port_error`, where conflating the two would abort a run against a
    perfectly healthy board.

    `last_seen_secs` is seconds since `DEVPKEY_Device_LastArrivalDate`, or
    `None` when no arrival date is recorded. It is independent of `present`:
    an attached board still has an arrival date, and an absent one usually
    does too — that is exactly what makes it useful.
    """

    present: Optional[bool]
    last_seen_secs: Optional[int]


def parse_presence(output: str) -> PortPresence:
    """Parse a `present|last_arrival_seconds_ago` line."""
    for line in output.splitlines():
        line = line.strip()
        if "|" not in line:
            continue
        present_raw, _, arrived_raw = line.partition("|")
        present: Optional[bool]
        if present_raw.strip().lower() == "true":
            present = True
        elif present_raw.strip().lower() == "false":
            present = False
        else:
            present = None
        arrived: Optional[int] = None
        try:
            arrived = int(arrived_raw.strip())
        except ValueError:
            arrived = None
        return PortPresence(present=present, last_seen_secs=arrived)
    return PortPresence(present=None, last_seen_secs=None)


def port_presence(
    port: str, platform: str = sys.platform, run: CommandRunner = _run
) -> PortPresence:
    """Query the host for `port`'s presence and last-arrival time.

    `platform` is injectable so the Windows behaviour is exercised on
    non-Windows CI, matching `selective_suspend_warnings`.

    Anything that is not a plain `COMn` name returns "unknown" without
    shelling out. That is an allowlist rather than an escape, for two
    reasons. It closes the injection path — `port` reaches a single-quoted
    PowerShell literal, so a value containing `'` could otherwise close the
    string and run arbitrary code as the invoking user. And it is also a
    correctness fix: `--upload-port` legitimately accepts forms like
    `UF2=E:\\`, whose trailing backslash would corrupt the `-match` regex.
    Neither form names a COM device, so there is nothing to look up.
    """
    if platform != "win32" or not _COM_PORT_RE.match(port):
        return PortPresence(present=None, last_seen_secs=None)
    script = f"""
$ErrorActionPreference='SilentlyContinue'
$d = Get-PnpDevice -Class Ports | Where-Object {{ $_.FriendlyName -match '\\({port}\\)' }} | Select-Object -First 1
if (-not $d) {{ exit 0 }}
$a = (Get-PnpDeviceProperty -InstanceId $d.InstanceId -KeyName 'DEVPKEY_Device_LastArrivalDate').Data
$secs = ''
if ($a) {{ $secs = [int]((Get-Date) - $a).TotalSeconds }}
Write-Output ("{{0}}|{{1}}" -f $d.Present, $secs)
"""
    return parse_presence(_powershell(script, run))


def humanise_age(secs: int) -> str:
    """Coarse age: `45s`, `12m`, `3h`, `6d` — enough to tell live from stale."""
    if secs < 60:
        return f"{secs}s"
    if secs < 3600:
        return f"{secs // 60}m"
    if secs < 86_400:
        return f"{secs // 3600}h"
    return f"{secs // 86_400}d"


def absent_port_error(
    port: str, platform: str = sys.platform, run: CommandRunner = _run
) -> Optional[str]:
    """Error text when an explicitly-requested port is not attached.

    `None` means "carry on" — including when presence is unknowable. Only an
    unambiguous `Present = False` stops a run.

    Why this exists: `bash autoresearch --upload-port COM17` used to install
    packages, lint and build for ~7 minutes before fbuild refused the deploy
    on a board that was never plugged in. Worse, the refusal reads as a wedged
    devnode, which is how FastLED #3864 lost hours to a board that had simply
    been unplugged for six days.

    Deliberately scoped to an *explicitly named* port. With no `--upload-port`
    fbuild can still reach an RP-series board through the BOOTSEL volume even
    with no working CDC record, so absence there is not fatal.
    """
    presence = port_presence(port, platform=platform, run=run)
    if presence.present is not False:
        return None
    age = presence.last_seen_secs
    seen = f", last seen {humanise_age(age)} ago" if age is not None else ""
    return (
        f"{port} is not attached{seen}. Windows keeps the record after a board "
        f"is unplugged, so this is a stale devnode, not a fault — plug the board "
        f"in and re-run. (Deploy would fail after the build otherwise. For an "
        f"RP-series board already in BOOTSEL, omit --upload-port or pass "
        f"--upload-port UF2=<volume>.)"
    )


def plan_selective_suspend_enabled(run: CommandRunner = _run) -> Optional[bool]:
    """Is USB selective suspend enabled in the active power plan?

    None when it cannot be determined (not Windows, powercfg missing, output
    shape changed) — callers must treat None as "no opinion", not "disabled".
    """
    out = _run_powercfg(run)
    if not out:
        return None
    # `powercfg /q` prints "Current AC Power Setting Index: 0x00000001"
    match = re.search(r"Current AC Power Setting Index:\s*0x([0-9a-fA-F]+)", out)
    if not match:
        return None
    return int(match.group(1), 16) != 0


def _run_powercfg(run: CommandRunner) -> str:
    return run(
        [
            "powercfg",
            "/q",
            "SCHEME_CURRENT",
            _USB_SUBGROUP_GUID,
            _SELECTIVE_SUSPEND_GUID,
        ]
    )


def hub_chain_power_off(port: str, run: CommandRunner = _run) -> list[tuple[str, bool]]:
    """USB ancestors of `port` and whether each may be powered off.

    Returns (instance_id, power_off_allowed) pairs, nearest ancestor first.
    Empty when the port is not present or nothing can be resolved — an absent
    board has no live hub chain to inspect.

    Same `COMn` allowlist as `port_presence`: `port` lands in a single-quoted
    PowerShell literal, so anything else is refused rather than escaped.
    """
    if not _COM_PORT_RE.match(port):
        return []
    script = f"""
$ErrorActionPreference='SilentlyContinue'
$dev = Get-PnpDevice -Class Ports -PresentOnly |
    Where-Object {{ $_.FriendlyName -match '\\({port}\\)' }} | Select-Object -First 1
if (-not $dev) {{ exit 0 }}
$id = $dev.InstanceId
for ($i = 0; $i -lt {_MAX_ANCESTORS}; $i++) {{
    $parent = (Get-PnpDeviceProperty -InstanceId $id -KeyName 'DEVPKEY_Device_Parent').Data
    if (-not $parent) {{ break }}
    if ($parent -notmatch '^USB') {{ break }}
    $pw = Get-CimInstance -Namespace root\\wmi -ClassName MSPower_DeviceEnable |
        Where-Object {{ $_.InstanceName -like ($parent + '*') }} | Select-Object -First 1
    if ($pw) {{ Write-Output ("{{0}}|{{1}}" -f $parent, $pw.Enable) }}
    $id = $parent
}}
"""
    out = _powershell(script, run)
    chain: list[tuple[str, bool]] = []
    for line in out.splitlines():
        line = line.strip()
        if "|" not in line:
            continue
        instance, _, enable = line.partition("|")
        chain.append((instance.strip(), enable.strip().lower() == "true"))
    return chain


def selective_suspend_warnings(
    port: str | None,
    *,
    platform: str | None = None,
    run: CommandRunner = _run,
) -> list[str]:
    """Warning lines for a HIL run. Empty list means nothing to report."""
    if (platform or sys.platform) != "win32":
        return []

    warnings: list[str] = []

    chain = hub_chain_power_off(port, run=run) if port else []
    powered_off = [instance for instance, enabled in chain if enabled]
    if powered_off:
        warnings.append(
            f"USB selective suspend is allowed on the hub chain for {port}: "
            + ", ".join(powered_off)
        )
    elif plan_selective_suspend_enabled(run=run):
        # Fall back to the plan-wide setting when the specific chain cannot be
        # resolved (typical when the board is not currently attached).
        warnings.append(
            "USB selective suspend is enabled in the active Windows power plan"
        )

    if warnings:
        warnings.append(
            "Windows may power down the port mid-session; a board that does not "
            "resume cleanly then appears as 'Device Descriptor Request Failed' "
            "(code 43) and its COM record shows health=phantom. "
            "Disable with an elevated: powercfg -setacvalueindex SCHEME_CURRENT "
            f"{_USB_SUBGROUP_GUID} {_SELECTIVE_SUSPEND_GUID} 0"
        )
    return warnings


def warn_selective_suspend(port: str | None, *, run: CommandRunner = _run) -> None:
    """Print selective-suspend warnings, if any. Never raises."""
    try:
        lines = selective_suspend_warnings(port, run=run)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        return  # a preflight hint must never break a run
    for line in lines:
        print(f"⚠️  {line}")
    if lines:
        print()
