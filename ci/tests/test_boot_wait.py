"""Unit tests for ci.util.boot_wait spin-poll helper (issue #2265).

These tests use a fake ``SerialInterface`` so they run in host unit tests
without any hardware. They exercise three behaviors:

1. ``wait_for_signal`` returns immediately when a matching line appears.
2. It falls back to the ``timeout`` deadline when no signal arrives (the
   "slow device" path — must NOT raise).
3. It drains and mirrors lines so existing verbose-logging callers still
   see the boot banner.
"""

from __future__ import annotations

import asyncio
import time
from collections.abc import AsyncIterator

import pytest

from ci.util.boot_wait import (
    BOOT_SIGNALS_ANY,
    BOOT_SIGNALS_ROM,
    BOOT_SIGNALS_SETUP,
    wait_for_signal,
)


class _FakeSerial:
    """Minimal ``SerialInterface`` stand-in that emits canned lines.

    Each ``read_lines`` call yields the next chunk of queued lines then
    returns (mimicking the fbuild adapter which returns early once its
    current batch is exhausted rather than blocking for the full timeout).
    """

    def __init__(self, batches: list[list[str]], per_batch_delay: float = 0.0) -> None:
        self._batches = list(batches)
        self._per_batch_delay = per_batch_delay

    async def read_lines(self, timeout: float) -> AsyncIterator[str]:  # noqa: ARG002
        if not self._batches:
            # Simulate "nothing available within timeout" by sleeping the
            # requested interval, like a real serial adapter would.
            await asyncio.sleep(min(timeout, 0.01))
            return
        batch = self._batches.pop(0)
        for line in batch:
            if self._per_batch_delay:
                await asyncio.sleep(self._per_batch_delay)
            yield line


def test_boot_signal_constants_are_populated() -> None:
    assert BOOT_SIGNALS_ROM, "ROM signals must not be empty"
    assert BOOT_SIGNALS_SETUP, "setup signals must not be empty"
    assert set(BOOT_SIGNALS_ROM).issubset(set(BOOT_SIGNALS_ANY))
    assert set(BOOT_SIGNALS_SETUP).issubset(set(BOOT_SIGNALS_ANY))


def test_wait_for_signal_returns_early_on_match() -> None:
    """Spin-poll exits as soon as any expected pattern is seen."""
    iface = _FakeSerial(
        batches=[
            ["garbage line"],
            ["ESP-ROM:esp32s3-20210327"],  # should match BOOT_SIGNALS_ROM
            ["Before Setup Start"],  # should NOT be consumed — we exit first
        ]
    )

    start = time.monotonic()
    matched, drained = asyncio.run(
        wait_for_signal(iface, BOOT_SIGNALS_ANY, timeout=2.0, spin_interval=0.02)  # type: ignore[arg-type]
    )
    elapsed = time.monotonic() - start

    assert matched == "ESP-ROM:", f"expected ESP-ROM: match, got {matched!r}"
    assert any("ESP-ROM:" in line for line in drained), (
        f"drained lines missing banner: {drained}"
    )
    # Should return well under the 2s deadline.
    assert elapsed < 1.0, f"took {elapsed:.2f}s for early-exit path"


def test_wait_for_signal_times_out_without_raising() -> None:
    """When no signal appears, return (None, lines) — do NOT raise.

    This is the critical contract from issue #2265: "slow devices still
    work via the max timeout" — the timeout IS the fallback.
    """
    iface = _FakeSerial(batches=[["unrelated"], ["more junk"]])
    matched, drained = asyncio.run(
        wait_for_signal(
            iface,
            ["THIS_NEVER_APPEARS"],
            timeout=0.15,
            spin_interval=0.02,
        )
    )
    assert matched is None
    # Drained lines were still captured for verbose logging callers.
    for expected in ("unrelated", "more junk"):
        assert any(expected in line for line in drained), (
            f"expected {expected!r} in drained lines: {drained}"
        )


def test_wait_for_signal_invokes_on_line_callback() -> None:
    """Verbose callers can mirror lines via the on_line callback."""
    iface = _FakeSerial(
        batches=[
            ["booting..."],
            ["entry 0x40080000"],
        ]
    )
    mirrored: list[str] = []

    matched, drained = asyncio.run(
        wait_for_signal(
            iface,
            BOOT_SIGNALS_ROM,
            timeout=1.0,
            spin_interval=0.02,
            on_line=mirrored.append,
        )
    )
    assert matched == "entry 0x"
    assert "booting..." in mirrored
    assert any("entry 0x" in m for m in mirrored)
    assert mirrored == drained


def test_wait_for_signal_empty_patterns_drains_for_timeout() -> None:
    """Empty patterns list means "just drain for the timeout window."""
    iface = _FakeSerial(batches=[["line-a", "line-b"]])
    start = time.monotonic()
    matched, drained = asyncio.run(
        wait_for_signal(iface, [], timeout=0.1, spin_interval=0.02)
    )
    elapsed = time.monotonic() - start

    assert matched is None
    assert "line-a" in drained and "line-b" in drained
    # Should wait close to timeout (within a small slack), since no patterns
    # means we never early-exit.
    assert elapsed >= 0.08, f"returned too early: {elapsed:.3f}s"


@pytest.mark.parametrize(
    "pattern,line,should_match",
    [
        ("entry 0x", "I (123) entry 0x40080000 boot", True),
        ("ESP-ROM:", "ESP-ROM:esp32s3-20210327", True),
        ("Before Setup Start", "[user] Before Setup Start now", True),
        ("entry 0x", "some unrelated log line", False),
    ],
)
def test_pattern_matching_is_substring_containment(
    pattern: str, line: str, should_match: bool
) -> None:
    """Patterns are matched by ``in`` containment, not regex or equality."""
    iface = _FakeSerial(batches=[[line]])
    matched, _ = asyncio.run(
        wait_for_signal(iface, [pattern], timeout=0.2, spin_interval=0.02)
    )
    if should_match:
        assert matched == pattern
    else:
        assert matched is None
