"""RED tests: SerialInterface protocol and adapters must have reset_device()."""

from __future__ import annotations

import asyncio
import sys
from types import ModuleType

import pytest


def test_factory_uses_supported_async_monitor(monkeypatch) -> None:
    """The supported fbuild API runs without the sync thread adapter."""
    from ci.util import serial_interface

    api = ModuleType("fbuild.api")

    class AsyncMonitor:
        def __init__(self, **kwargs):
            self.kwargs = kwargs
            self.writes = []
            self.read_count = 0

        async def __aenter__(self):
            return self

        async def __aexit__(self, *args):
            return False

        async def write(self, data):
            self.writes.append(data)
            return len(data)

        async def read_lines(self, timeout=30.0):
            self.read_count += 1
            if self.read_count == 1:
                return ["reply"]
            await asyncio.sleep(timeout)
            return []

        async def reset_device(self, board=None, wait_for_output=False, timeout=5.0):
            assert wait_for_output and timeout == 5.0
            return True

    api.AsyncSerialMonitor = AsyncMonitor
    monkeypatch.setitem(sys.modules, "fbuild.api", api)
    monkeypatch.setattr(
        serial_interface, "_supported_async_fbuild", lambda: AsyncMonitor
    )

    adapter = serial_interface.create_serial_interface("COM9")

    async def exercise():
        await adapter.connect()
        await adapter.write("ping")
        lines = [line async for line in adapter.read_lines(0.01)]
        reset = await adapter.reset_device(None)
        await adapter.close()
        return lines, reset

    lines, reset = asyncio.run(exercise())
    assert lines == ["reply"]
    assert reset
    assert adapter._monitor.writes == ["ping"]
    assert not hasattr(adapter, "_executor")


def test_async_monitor_requires_supported_fbuild_version(monkeypatch) -> None:
    """Older experimental async APIs keep using the sync fallback."""
    from ci.util import serial_interface

    api = ModuleType("fbuild.api")
    marker = object()
    api.AsyncSerialMonitor = marker
    monkeypatch.setitem(sys.modules, "fbuild.api", api)

    monkeypatch.setattr(serial_interface, "version", lambda name: "2.5.28")
    assert serial_interface._supported_async_fbuild() is None
    monkeypatch.setattr(serial_interface, "version", lambda name: "2.5.29")
    assert serial_interface._supported_async_fbuild() is marker


def test_async_adapter_recovers_lost_daemon(monkeypatch) -> None:
    from ci.util import serial_interface

    attempts = []
    restarted = []

    class AsyncMonitor:
        def __init__(self, **kwargs):
            self.fail = not attempts
            attempts.append(self)

        async def __aenter__(self):
            if self.fail:
                raise RuntimeError(
                    "failed to connect to daemon WebSocket: connection refused"
                )
            return self

    monkeypatch.setattr(
        serial_interface, "ensure_fbuild_daemon", lambda: restarted.append(True)
    )
    adapter = serial_interface.AsyncFbuildSerialAdapter(AsyncMonitor, "COM9")
    asyncio.run(adapter.connect())
    assert len(attempts) == 2
    assert restarted == [True]
    assert adapter._monitor is attempts[-1]


def test_serial_interface_protocol_has_reset_device() -> None:
    """SerialInterface protocol must declare reset_device."""
    from ci.util.serial_interface import SerialInterface

    assert "reset_device" in dir(SerialInterface), (
        "SerialInterface protocol missing reset_device method"
    )


def test_fbuild_adapter_has_reset_device() -> None:
    """FbuildSerialAdapter must implement reset_device."""
    from ci.util.serial_interface import FbuildSerialAdapter

    assert hasattr(FbuildSerialAdapter, "reset_device"), (
        "FbuildSerialAdapter missing reset_device method"
    )


def test_pyserial_adapter_has_reset_device() -> None:
    """PySerialAdapter must implement reset_device."""
    from ci.util.serial_interface import PySerialAdapter

    assert hasattr(PySerialAdapter, "reset_device"), (
        "PySerialAdapter missing reset_device method"
    )


def test_fbuild_adapter_recovers_lost_post_deploy_daemon(monkeypatch) -> None:
    """A dead fbuild WebSocket is restarted and connected exactly once."""
    from ci.util import serial_interface
    from ci.util.serial_interface import FbuildSerialAdapter

    class _Monitor:
        def __init__(self, should_fail: bool) -> None:
            self.should_fail = should_fail
            self.enter_count = 0

        def __enter__(self):
            self.enter_count += 1
            if self.should_fail:
                raise RuntimeError(
                    "failed to connect to daemon WebSocket: connection actively refused"
                )
            return self

    first = _Monitor(should_fail=True)
    recovered = _Monitor(should_fail=False)
    monitors = iter([first, recovered])
    restart_count = 0

    def _restart() -> None:
        nonlocal restart_count
        restart_count += 1

    adapter = object.__new__(FbuildSerialAdapter)
    adapter._monitor = next(monitors)
    adapter._new_monitor = lambda: next(monitors)
    adapter._executor = serial_interface.ThreadPoolExecutor(max_workers=1)
    monkeypatch.setattr(serial_interface, "ensure_fbuild_daemon", _restart)

    try:
        asyncio.run(adapter.connect())
    finally:
        adapter._executor.shutdown(wait=True)

    assert restart_count == 1
    assert first.enter_count == 1
    assert recovered.enter_count == 1
    assert adapter._monitor is recovered


def test_fbuild_adapter_does_not_mask_non_daemon_connection_error() -> None:
    """Serial-port failures propagate without attempting daemon recovery."""
    from ci.util import serial_interface
    from ci.util.serial_interface import FbuildSerialAdapter

    class _Monitor:
        def __enter__(self):
            raise OSError("COM9 is busy")

    adapter = object.__new__(FbuildSerialAdapter)
    adapter._monitor = _Monitor()
    adapter._executor = serial_interface.ThreadPoolExecutor(max_workers=1)

    try:
        with pytest.raises(OSError, match="COM9 is busy"):
            asyncio.run(adapter.connect())
    finally:
        adapter._executor.shutdown(wait=True)


def test_fbuild_adapter_propagates_interrupt_during_daemon_recovery(
    monkeypatch,
) -> None:
    """Daemon recovery preserves worker-thread interrupt propagation."""
    from ci.util import serial_interface
    from ci.util.serial_interface import FbuildSerialAdapter

    class _Monitor:
        def __enter__(self):
            raise RuntimeError(
                "failed to connect to daemon WebSocket: connection actively refused"
            )

    handled: list[KeyboardInterrupt] = []

    def _interrupt() -> None:
        raise KeyboardInterrupt

    adapter = object.__new__(FbuildSerialAdapter)
    adapter._monitor = _Monitor()
    adapter._executor = serial_interface.ThreadPoolExecutor(max_workers=1)
    monkeypatch.setattr(serial_interface, "ensure_fbuild_daemon", _interrupt)
    monkeypatch.setattr(serial_interface, "handle_keyboard_interrupt", handled.append)

    try:
        with pytest.raises(KeyboardInterrupt):
            asyncio.run(adapter.connect())
    finally:
        adapter._executor.shutdown(wait=True)

    assert len(handled) == 1
