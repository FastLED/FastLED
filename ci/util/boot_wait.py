"""Spin-poll helpers for device boot/reset signals.

Replaces fixed ``asyncio.sleep(3.0)``-style waits used by autoresearch /
``RpcClient`` with a poll loop that watches serial output for an expected
validation signal (e.g. ``"entry 0x"``, ``"ESP-ROM:"``, ``"Before Setup Start"``).

If any of the ``patterns`` appears in a serial line the helper returns
immediately; otherwise it keeps reading until ``timeout`` elapses. The caller
gets back the matched signal (or ``None`` on timeout) plus any lines that were
drained during the wait, so the existing verbose logging / boot-drain paths
can keep their behavior.

The issue (#2265) mandates that ``timeout`` IS the fallback — on a slow device
that never produces the expected signature the helper simply times out,
matching the behavior of the previous fixed sleep. It never raises.
"""

from __future__ import annotations

import asyncio
import time
from typing import TYPE_CHECKING, Callable, Iterable


if TYPE_CHECKING:
    from ci.util.serial_interface import SerialInterface


# Canonical validation signals per boot phase.
#
# - ``BOOT_SIGNALS_ROM``: ROM bootloader / second-stage loader markers. Hit as
#   soon as the ESP32 starts booting after a reset.
# - ``BOOT_SIGNALS_SETUP``: Arduino ``setup()`` has started executing; RPC
#   dispatcher is about to come up.
# - ``BOOT_SIGNALS_ANY``: Any line that indicates the device has come back to
#   life. Used by generic ``connect()`` which doesn't know which phase it is in.
BOOT_SIGNALS_ROM: tuple[str, ...] = (
    "entry 0x",
    "ESP-ROM:",
    "rst:",
    "boot:",
)

BOOT_SIGNALS_SETUP: tuple[str, ...] = (
    "Before Setup Start",
    "setup() start",
    "RESULT:",
    "REMOTE: ",
)

BOOT_SIGNALS_ANY: tuple[str, ...] = BOOT_SIGNALS_ROM + BOOT_SIGNALS_SETUP


async def wait_for_signal(
    iface: "SerialInterface",
    patterns: Iterable[str],
    timeout: float,
    spin_interval: float = 0.05,
    on_line: "Callable[[str], None] | None" = None,
) -> tuple[str | None, list[str]]:
    """Spin-poll ``iface`` for any of ``patterns`` until ``timeout`` elapses.

    Args:
        iface: SerialInterface to read lines from.
        patterns: Substrings to look for in incoming serial lines. Match is by
            simple ``in`` containment (not regex) so callers can pass the
            literal boot banner text.
        timeout: Maximum wall-clock seconds to wait. Acts as the fallback when
            the expected signal never arrives: the helper returns ``None``
            without raising so slow devices still work.
        spin_interval: Per-iteration read timeout / polling cadence (seconds).
            The helper calls ``iface.read_lines(timeout=spin_interval)`` each
            tick so it can exit promptly once a matching line appears.
        on_line: Optional callback invoked for every non-matching line that
            was drained. Useful for mirroring verbose boot output.

    Returns:
        Tuple of (matched_pattern, drained_lines). ``matched_pattern`` is the
        first ``patterns`` element found, or ``None`` on timeout. ``drained_lines``
        contains every line observed during the wait, including the matching
        one, so callers that previously relied on ``drain_boot_output()`` still
        see the boot banner.
    """
    patterns_list = list(patterns)
    drained: list[str] = []
    # Short-circuit: nothing to match means caller expects us to just drain
    # for the full duration. We still poll so we can yield to the event loop.
    if not patterns_list:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                async for line in iface.read_lines(timeout=spin_interval):
                    drained.append(line)
                    if on_line is not None:
                        on_line(line)
                    if time.monotonic() >= deadline:
                        break
            except KeyboardInterrupt as ki:
                from ci.util.global_interrupt_handler import handle_keyboard_interrupt

                handle_keyboard_interrupt(ki)
                raise
            except Exception:
                # Read timeouts are normal.
                pass
            await asyncio.sleep(0)
        return (None, drained)

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            async for line in iface.read_lines(timeout=spin_interval):
                drained.append(line)
                if on_line is not None:
                    on_line(line)
                for pat in patterns_list:
                    if pat in line:
                        return (pat, drained)
                if time.monotonic() >= deadline:
                    break
        except KeyboardInterrupt as ki:
            from ci.util.global_interrupt_handler import handle_keyboard_interrupt

            handle_keyboard_interrupt(ki)
            raise
        except Exception:
            # Read timeouts are normal — keep polling until deadline.
            pass
        # Yield to the event loop so other tasks (interrupt handler, etc.)
        # can run between read attempts.
        await asyncio.sleep(0)

    return (None, drained)


# ROM reset banners. ESP8266 prints "ets Jan  8 2013,rst cause:2, ..." and
# ESP32-class ROMs print "rst:0xc (SW_CPU_RESET),boot:0x13 ...". Seeing several
# in one boot window means the firmware is resetting before it can answer RPC.
_RESET_BANNERS: tuple[str, ...] = ("rst cause:", "rst:0x")

BOOT_LOOP_THRESHOLD = 3


class BootLoopError(RuntimeError):
    """The device reset repeatedly while we waited for it to boot."""


def count_reset_banners(lines: Iterable[str]) -> int:
    """Count ROM reset banners in ``lines``."""
    return sum(1 for line in lines if any(b in line for b in _RESET_BANNERS))


def raise_if_boot_looping(
    lines: list[str], threshold: int = BOOT_LOOP_THRESHOLD
) -> None:
    """Raise ``BootLoopError`` when ``lines`` hold ``threshold`` or more resets."""
    resets = count_reset_banners(lines)
    if resets < threshold:
        return
    raise BootLoopError(
        f"Device is boot-looping: {resets} ROM reset banners in {len(lines)} "
        "boot line(s), so it never reached setup(). Look at the lines just "
        "before each reset. A crash with no dump can mean the core muted "
        "its postmortem (ESP8266 prints it only after Serial.begin / "
        "setDebugOutput) or the reset happened before the console started."
    )
