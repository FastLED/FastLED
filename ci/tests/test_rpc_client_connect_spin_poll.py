"""Integration-ish tests for ``RpcClient.connect`` spin-poll (issue #2265).

Uses a fake SerialInterface so these run in host unit tests with no hardware.
Verifies that connect() returns quickly when a boot signal appears, rather
than sleeping the full ``boot_wait`` duration.
"""

from __future__ import annotations

import asyncio
import time
from collections.abc import AsyncIterator

import pytest

from ci.rpc_client import RpcClient


class _FakeSerial:
    """Fake SerialInterface that emits a boot signal after a tiny delay."""

    def __init__(
        self,
        banner: str = "ESP-ROM:esp32s3-20210327\n",
        emit_banner: bool = True,
    ) -> None:
        self._banner_line = banner.rstrip("\n")
        self._emit_banner = emit_banner
        self._connected = False
        self._banner_sent = False
        self._closed = False

    async def connect(self) -> None:
        self._connected = True

    async def close(self) -> None:
        self._closed = True

    async def write(self, data: str) -> None:  # pragma: no cover - unused here
        pass

    async def read_lines(self, timeout: float) -> AsyncIterator[str]:  # noqa: ARG002
        if self._emit_banner and not self._banner_sent:
            self._banner_sent = True
            yield self._banner_line
        else:
            # Simulate "no data within timeout" by sleeping a tick.
            await asyncio.sleep(min(timeout, 0.01))

    async def reset_device(self, board: str | None) -> bool:  # pragma: no cover
        return True


def test_connect_returns_early_on_boot_signal() -> None:
    """If a boot signal appears, connect() returns well under boot_wait."""
    fake = _FakeSerial(banner="ESP-ROM:esp32s3-20210327", emit_banner=True)
    client = RpcClient("FAKE_PORT", serial_interface=fake)

    async def _run() -> float:
        start = time.monotonic()
        # Use a generous 3.0s boot_wait — historical default. We expect to
        # return in a fraction of that once the banner line is consumed.
        await client.connect(boot_wait=3.0, drain_boot=False)
        elapsed = time.monotonic() - start
        await client.close()
        return elapsed

    elapsed = asyncio.run(_run())
    assert elapsed < 1.5, (
        f"connect() took {elapsed:.2f}s despite banner being available; "
        "spin-poll should have exited early"
    )


def test_connect_falls_back_to_timeout_without_signal() -> None:
    """Slow device (no banner) must still complete — timeout IS the fallback."""
    fake = _FakeSerial(emit_banner=False)
    client = RpcClient("FAKE_PORT", serial_interface=fake)

    async def _run() -> float:
        start = time.monotonic()
        # Short boot_wait so the test stays fast.
        await client.connect(boot_wait=0.25, drain_boot=False)
        elapsed = time.monotonic() - start
        await client.close()
        return elapsed

    elapsed = asyncio.run(_run())
    # Should wait roughly the full boot_wait (with slack for scheduling).
    assert 0.15 <= elapsed <= 2.0, f"expected to wait ~0.25s, got {elapsed:.2f}s"


def test_connect_respects_empty_boot_signals_opts_out_of_spin_poll() -> None:
    """Passing boot_signals=[] disables early-exit (legacy behavior)."""
    fake = _FakeSerial(banner="ESP-ROM:", emit_banner=True)
    client = RpcClient("FAKE_PORT", serial_interface=fake)

    async def _run() -> float:
        start = time.monotonic()
        await client.connect(boot_wait=0.3, drain_boot=False, boot_signals=[])
        elapsed = time.monotonic() - start
        await client.close()
        return elapsed

    elapsed = asyncio.run(_run())
    # With empty patterns we must NOT early-exit — we wait the full boot_wait.
    assert elapsed >= 0.2, (
        f"connect() returned too early ({elapsed:.2f}s) with boot_signals=[]"
    )
