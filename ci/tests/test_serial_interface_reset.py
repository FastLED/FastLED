"""RED tests: SerialInterface protocol and adapters must have reset_device()."""

from __future__ import annotations

import asyncio

import pytest


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
