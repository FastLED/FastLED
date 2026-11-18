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


class _AttachMonitor:
    """One monitor; its `__aenter__` raises `error`, or attaches if None."""

    def __init__(self, error: Exception | None) -> None:
        self.error = error
        self.enters = 0
        self.exits = 0

    async def __aenter__(self):
        self.enters += 1
        if self.error is not None:
            raise self.error
        return self

    async def __aexit__(self, *_exc: object) -> None:
        self.exits += 1


def _attach_adapter(monkeypatch, errors: list[Exception | None]):
    """An adapter whose successive monitors fail with `errors` in order."""
    from collections import deque

    from ci.util import serial_interface
    from ci.util.serial_interface import NativeFbuildSerialAdapter

    monkeypatch.setattr(serial_interface, "ATTACH_RETRY_PAUSE_SECONDS", 0.0)
    monitors = [_AttachMonitor(error) for error in errors]
    fresh = iter(monitors[1:])
    adapter = object.__new__(NativeFbuildSerialAdapter)
    adapter._monitor_args = {"port": "FAKE_PORT"}
    adapter._monitor = monitors[0]
    adapter._new_monitor = lambda: next(fresh)
    adapter._pending_lines = deque()
    return adapter, monitors


def test_native_adapter_retries_a_session_closed_attach(monkeypatch) -> None:
    """FastLED#5271: the daemon's 3 s open cap surfaces as 'session closed'."""
    adapter, monitors = _attach_adapter(
        monkeypatch, [ConnectionError("session closed"), None]
    )
    asyncio.run(adapter.connect())
    assert [m.enters for m in monitors] == [1, 1]
    assert monitors[0].exits == 1  # the failed monitor is released
    assert adapter._monitor is monitors[1]  # and a fresh one attached


_DAEMON_ERROR = "failed to connect to daemon WebSocket: connection actively refused"


def test_native_adapter_attach_retry_is_bounded(monkeypatch) -> None:
    adapter, monitors = _attach_adapter(
        monkeypatch, [ConnectionError("session closed") for _ in range(4)]
    )
    with pytest.raises(ConnectionError, match="session closed"):
        asyncio.run(adapter.connect())
    assert [m.enters for m in monitors] == [1, 1, 1, 0]


def test_native_adapter_daemon_recovery_counts_against_attach_limit(
    monkeypatch,
) -> None:
    """Two 'session closed' then a lost daemon: no fourth attach."""
    from ci.util import serial_interface

    restarts: list[None] = []
    monkeypatch.setattr(
        serial_interface, "ensure_fbuild_daemon", lambda: restarts.append(None)
    )
    adapter, monitors = _attach_adapter(
        monkeypatch,
        [
            ConnectionError("session closed"),
            ConnectionError("session closed"),
            RuntimeError(_DAEMON_ERROR),
            None,
        ],
    )
    with pytest.raises(RuntimeError, match="daemon WebSocket"):
        asyncio.run(adapter.connect())
    assert [m.enters for m in monitors] == [1, 1, 1, 0]
    assert restarts == []


def test_native_adapter_handles_interrupt_during_daemon_recovery(
    monkeypatch,
) -> None:
    from ci.util import serial_interface

    def _interrupt() -> None:
        raise KeyboardInterrupt

    handled: list[KeyboardInterrupt] = []
    monkeypatch.setattr(serial_interface, "ensure_fbuild_daemon", _interrupt)
    monkeypatch.setattr(serial_interface, "handle_keyboard_interrupt", handled.append)
    adapter, _monitors = _attach_adapter(
        monkeypatch, [RuntimeError(_DAEMON_ERROR), None]
    )
    with pytest.raises(KeyboardInterrupt):
        asyncio.run(adapter.connect())
    assert len(handled) == 1


def test_native_adapter_does_not_retry_other_attach_errors(monkeypatch) -> None:
    adapter, monitors = _attach_adapter(
        monkeypatch,
        [ConnectionError("daemon rejected serial attach: permission denied"), None],
    )
    with pytest.raises(ConnectionError, match="permission denied"):
        asyncio.run(adapter.connect())
    assert [m.enters for m in monitors] == [1, 0]


def test_native_adapter_final_error_keeps_the_first_attempt(monkeypatch) -> None:
    """FastLED#5281: 'session closed', a lost daemon, then a timeout after
    recovery: the raised error is the timeout, caused by the daemon error,
    with the first 'session closed' attempt kept as a note."""
    from ci.util import serial_interface

    monkeypatch.setattr(serial_interface, "ensure_fbuild_daemon", lambda: None)
    adapter, _monitors = _attach_adapter(
        monkeypatch,
        [
            ConnectionError("session closed"),
            RuntimeError(_DAEMON_ERROR),
            TimeoutError("handshake timed out"),
            None,
        ],
    )
    with pytest.raises(TimeoutError) as raised:
        asyncio.run(adapter.connect())
    assert isinstance(raised.value.__cause__, RuntimeError)
    assert raised.value.__notes__ == [
        "attach attempt 1 of 3 failed: ConnectionError: session closed"
    ]


def test_fbuild_adapter_close_exits_the_monitor_current_when_it_runs() -> None:
    """FastLED#5281: a connect() still queued on the executor may replace
    the monitor before close()'s exit runs; the replacement must close."""
    import threading

    from ci.util import serial_interface
    from ci.util.serial_interface import FbuildSerialAdapter

    class _Monitor:
        def __init__(self) -> None:
            self.exits = 0

        def __exit__(self, *_exc: object) -> None:
            self.exits += 1

    old, new = _Monitor(), _Monitor()
    adapter = object.__new__(FbuildSerialAdapter)
    adapter._monitor = old
    adapter._executor = serial_interface.ThreadPoolExecutor(max_workers=1)
    release = threading.Event()

    def _queued_connect() -> None:
        # Only swap once close() has queued its exit; a timed-out gate must
        # fail the test, not let it pass without the race.
        assert release.wait(5), "close() never queued its exit"
        adapter._monitor = new  # what daemon recovery does

    async def _run() -> None:
        busy = asyncio.get_running_loop().run_in_executor(
            adapter._executor, _queued_connect
        )
        closing = asyncio.ensure_future(adapter.close())
        await asyncio.sleep(0)  # close() runs up to its executor submit
        release.set()
        await busy
        await closing

    try:
        asyncio.run(_run())
    finally:
        release.set()  # never strand the worker, even on failure
        adapter._executor.shutdown(wait=True)
    assert (old.exits, new.exits) == (0, 1)


def test_native_adapter_notes_number_the_real_attempt(monkeypatch) -> None:
    """FastLED#5281 review: a daemon recovery uses up attempt 1, so the
    retried 'session closed' is attempt 2 in the note too."""
    from ci.util import serial_interface

    monkeypatch.setattr(serial_interface, "ensure_fbuild_daemon", lambda: None)
    adapter, _monitors = _attach_adapter(
        monkeypatch,
        [
            RuntimeError(_DAEMON_ERROR),
            ConnectionError("session closed"),
            TimeoutError("handshake timed out"),
            None,
        ],
    )
    with pytest.raises(TimeoutError) as raised:
        asyncio.run(adapter.connect())
    assert raised.value.__notes__ == [
        "attach attempt 2 of 3 failed: ConnectionError: session closed"
    ]
