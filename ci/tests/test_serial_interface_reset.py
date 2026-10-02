"""RED tests: SerialInterface protocol and adapters must have reset_device()."""

from __future__ import annotations

import asyncio
import sys
import types

import pytest


def test_factory_prefers_supported_native_async_monitor(monkeypatch) -> None:
    """The complete async API bypasses the thread-pool compatibility path."""
    from ci.util.serial_interface import create_serial_interface

    class _NativeMonitor:
        def __init__(self, **kwargs) -> None:
            self.kwargs = kwargs
            self.writes: list[str] = []
            self.read_count = 0

        async def __aenter__(self):
            return self

        async def __aexit__(self, *_args) -> None:
            return None

        async def read_lines(self, timeout: float = 30.0) -> list[str]:
            # Like the real monitor: one batch, then wait out the timeout.
            self.read_count += 1
            if self.read_count == 1:
                return ["reply", "following"]
            await asyncio.sleep(timeout)
            return []

        async def write(self, data: str) -> int:
            self.writes.append(data)
            return len(data)

        async def in_waiting(self) -> int:
            return 0

        async def reset_input_buffer(self) -> None:
            return None

        async def reset_device(self, **_kwargs) -> bool:
            return True

    fake_api = types.ModuleType("fbuild.api")
    fake_api.AsyncSerialMonitor = _NativeMonitor
    fake_api.SerialMonitor = type(
        "SyncMustNotBeUsed",
        (),
        {"__init__": lambda self, **kwargs: pytest.fail("sync fallback used")},
    )
    monkeypatch.setitem(sys.modules, "fbuild.api", fake_api)

    adapter = create_serial_interface("TEST_PORT")
    assert type(adapter).__name__ == "NativeFbuildSerialAdapter"

    async def exercise() -> None:
        await adapter.connect()
        await adapter.write("hello")
        first_read = adapter.read_lines(timeout=1.0)
        assert await anext(first_read) == "reply"
        await first_read.aclose()
        assert [line async for line in adapter.read_lines(timeout=0.05)] == [
            "following"
        ]
        assert await adapter.reset_device(None)
        await adapter.close()

    asyncio.run(exercise())
    assert adapter._monitor.writes == ["hello"]
    # The second read drains the preserved line, then waits out its timeout.
    assert adapter._monitor.read_count == 2


def test_factory_falls_back_when_native_api_is_incomplete(monkeypatch) -> None:
    """Older fbuild wheels expose an experimental, incompatible async class."""
    from ci.util.serial_interface import FbuildSerialAdapter, create_serial_interface

    class _OldAsyncMonitor:
        async def read_lines(self, timeout_secs: float = 30.0) -> list[str]:
            return []

    fake_api = types.ModuleType("fbuild.api")
    fake_api.AsyncSerialMonitor = _OldAsyncMonitor
    fake_api.SerialMonitor = lambda **kwargs: object()
    monkeypatch.setitem(sys.modules, "fbuild.api", fake_api)

    adapter = create_serial_interface("TEST_PORT")
    try:
        assert isinstance(adapter, FbuildSerialAdapter)
    finally:
        adapter._executor.shutdown(wait=True)


def test_factory_falls_back_when_native_api_is_absent(monkeypatch) -> None:
    """Wheels predating AsyncSerialMonitor keep using the sync adapter."""
    from ci.util.serial_interface import FbuildSerialAdapter, create_serial_interface

    fake_api = types.ModuleType("fbuild.api")
    fake_api.SerialMonitor = lambda **kwargs: object()
    monkeypatch.setitem(sys.modules, "fbuild.api", fake_api)

    adapter = create_serial_interface("TEST_PORT")
    try:
        assert isinstance(adapter, FbuildSerialAdapter)
    finally:
        adapter._executor.shutdown(wait=True)


def test_native_adapter_recovers_lost_daemon_once(monkeypatch) -> None:
    """The native path retains post-deploy daemon restart behavior."""
    from collections import deque

    from ci.util import serial_interface
    from ci.util.serial_interface import NativeFbuildSerialAdapter

    class _Monitor:
        def __init__(self, fails: bool) -> None:
            self.fails = fails
            self.enters = 0

        async def __aenter__(self):
            self.enters += 1
            if self.fails:
                raise ConnectionError("failed to connect to daemon WebSocket: refused")
            return self

    first = _Monitor(fails=True)
    second = _Monitor(fails=False)
    recovered = iter([second])
    restarts: list[bool] = []
    monkeypatch.setattr(
        serial_interface, "ensure_fbuild_daemon", lambda: restarts.append(True)
    )

    adapter = object.__new__(NativeFbuildSerialAdapter)
    adapter._monitor = first
    adapter._new_monitor = lambda: next(recovered)
    adapter._pending_lines = deque()
    asyncio.run(adapter.connect())

    assert restarts == [True]
    assert first.enters == second.enters == 1
    assert adapter._monitor is second


def test_native_adapter_reads_past_first_batch_until_timeout() -> None:
    """A reply in a later native batch than a log line is still delivered."""
    from collections import deque

    from ci.util.serial_interface import NativeFbuildSerialAdapter

    class _Monitor:
        def __init__(self) -> None:
            self.batches = [["E (3215) task_wdt: log"], ['REMOTE: {"id":1}']]

        async def read_lines(self, timeout: float) -> list[str]:
            # Like the real monitor: drain a batch, else wait out the timeout.
            if self.batches:
                return self.batches.pop(0)
            await asyncio.sleep(timeout)
            return []

    adapter = object.__new__(NativeFbuildSerialAdapter)
    adapter._monitor = _Monitor()
    adapter._pending_lines = deque()

    async def _collect() -> list[str]:
        lines: list[str] = []
        async for line in adapter.read_lines(timeout=5.0):
            lines.append(line)
            if line.startswith("REMOTE:"):
                break
        return lines

    assert asyncio.run(_collect()) == [
        "E (3215) task_wdt: log",
        'REMOTE: {"id":1}',
    ]


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
