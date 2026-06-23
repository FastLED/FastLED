"""GPIO retry paths keep proxy-backed serial sessions open."""

from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator
from typing import Any
from unittest.mock import patch

from ci.autoresearch.gpio import run_gpio_pretest, run_pin_discovery
from ci.rpc_client import RpcResponse


class _FakeSerialInterface:
    def __init__(self) -> None:
        self.reset_count = 0

    async def connect(self) -> None:  # pragma: no cover - driven through fake client
        pass

    async def close(self) -> None:  # pragma: no cover - fake client tracks closes
        pass

    async def write(self, data: str) -> None:  # pragma: no cover
        pass

    async def read_lines(
        self, timeout: float
    ) -> AsyncIterator[str]:  # pragma: no cover
        if False:
            yield ""

    async def reset_device(self, board: str | None) -> bool:
        self.reset_count += 1
        return True


class _FakeRpcClient:
    instances: list["_FakeRpcClient"] = []

    def __init__(
        self,
        port: str,
        timeout: float,
        serial_interface: _FakeSerialInterface | None = None,
        verbose: bool = False,
        **_: Any,
    ) -> None:
        self.port = port
        self.timeout = timeout
        self.serial_interface = serial_interface
        self.verbose = verbose
        self.connect_count = 0
        self.close_count = 0
        self.drain_count = 0
        self.ping_attempts = 0
        _FakeRpcClient.instances.append(self)

    async def connect(self, **_: Any) -> None:
        self.connect_count += 1

    async def close(self) -> None:
        self.close_count += 1

    async def drain_boot_output(self, **_: Any) -> int:
        self.drain_count += 1
        return 0

    async def send(
        self,
        function: str,
        *_: Any,
        **__: Any,
    ) -> RpcResponse:
        if function == "ping":
            self.ping_attempts += 1
            if self.ping_attempts == 1:
                raise RuntimeError("first ping fails")
            return RpcResponse(success=True, data={"message": "pong"}, raw_line="")
        raise AssertionError(f"unexpected RPC call: {function}")

    async def send_and_match(self, function: str, **_: Any) -> RpcResponse:
        if function == "testGpioConnection":
            return RpcResponse(
                success=True,
                data={"connected": True},
                raw_line="",
            )
        if function == "findConnectedPins":
            return RpcResponse(
                success=True,
                data={"found": True, "txPin": 1, "rxPin": 2, "autoApplied": True},
                raw_line="",
            )
        raise AssertionError(f"unexpected RPC call: {function}")


def _reset_fake_clients() -> None:
    _FakeRpcClient.instances = []


def test_gpio_pretest_reuses_serial_proxy_after_reset() -> None:
    serial_interface = _FakeSerialInterface()
    _reset_fake_clients()

    with patch("ci.autoresearch.gpio.RpcClient", _FakeRpcClient):
        ok = asyncio.run(run_gpio_pretest("COM5", serial_interface=serial_interface))

    assert ok is True
    assert len(_FakeRpcClient.instances) == 1
    client = _FakeRpcClient.instances[0]
    assert serial_interface.reset_count == 1
    assert client.drain_count == 1
    assert client.close_count == 1  # final cleanup only


def test_pin_discovery_reuses_serial_proxy_after_reset() -> None:
    serial_interface = _FakeSerialInterface()
    _reset_fake_clients()

    with patch("ci.autoresearch.gpio.RpcClient", _FakeRpcClient):
        result = asyncio.run(
            run_pin_discovery("COM5", serial_interface=serial_interface)
        )

    assert result.success is True
    assert len(_FakeRpcClient.instances) == 1
    client = _FakeRpcClient.instances[0]
    assert result.client is client
    assert serial_interface.reset_count == 1
    assert client.drain_count == 1
    assert client.close_count == 0
