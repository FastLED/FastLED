"""Regression tests for JSON-RPC error propagation."""

from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator

import pytest
from typeguard import typechecked

from ci.rpc_client import RpcClient, RpcCrashError, RpcError, RpcTimeoutError


class _ErrorSerial:
    async def connect(self: "_ErrorSerial") -> None:
        pass

    async def close(self: "_ErrorSerial") -> None:
        pass

    async def write(self: "_ErrorSerial", data: str) -> None:
        assert '"id":1' in data

    async def read_lines(self: "_ErrorSerial", timeout: float) -> AsyncIterator[str]:  # noqa: ARG002
        yield (
            'REMOTE: {"jsonrpc":"2.0","id":1,'
            '"error":{"code":-32601,"message":"Method not found: missing"}}'
        )

    async def reset_device(
        self: "_ErrorSerial", board: str | None
    ) -> bool:  # pragma: no cover
        return True


def test_json_rpc_error_is_not_masked_as_timeout() -> None:
    async def _run() -> None:
        client = RpcClient("FAKE", serial_interface=_ErrorSerial())
        await client.connect(boot_wait=0, drain_boot=False)
        with pytest.raises(RpcError, match=r"-32601.*Method not found") as caught:
            await client.send("missing", {}, timeout=0.1)
        assert caught.value.code == -32601
        assert caught.value.message == "Method not found: missing"
        assert caught.value.data is None
        await client.close()

    asyncio.run(_run())


class _WriteFailsSerial:
    """A transport whose write fails the way `PyserialMonitor.write` does."""

    async def connect(self: "_WriteFailsSerial") -> None:
        pass

    async def close(self: "_WriteFailsSerial") -> None:
        pass

    async def write(self: "_WriteFailsSerial", data: str) -> None:  # noqa: ARG002
        raise RuntimeError("Serial write error: [Errno 5] Input/output error")

    async def read_lines(
        self: "_WriteFailsSerial", timeout: float
    ) -> AsyncIterator[str]:  # noqa: ARG002
        return
        yield ""  # pragma: no cover - makes this an async generator

    async def reset_device(
        self: "_WriteFailsSerial", board: str | None
    ) -> bool:  # pragma: no cover
        return True


class _CrashSerial:
    """A transport whose response is a device crash, not a write failure."""

    async def connect(self: "_CrashSerial") -> None:
        pass

    async def close(self: "_CrashSerial") -> None:
        pass

    async def write(self: "_CrashSerial", data: str) -> None:
        pass

    async def read_lines(self: "_CrashSerial", timeout: float) -> AsyncIterator[str]:  # noqa: ARG002
        raise RpcCrashError("device crashed", ["#0 boom"])
        yield ""  # pragma: no cover - makes this an async generator

    async def reset_device(
        self: "_CrashSerial", board: str | None
    ) -> bool:  # pragma: no cover
        return True


def test_rpc_error_is_a_runtime_error() -> None:
    """Callers reporting transport failures catch `RuntimeError` (#4191).

    `ci/autoresearch/decode.py` and `ci/autoresearch/gpio.py` both end their
    RPC paths with a bare `except RuntimeError` that prints the failure and
    returns. Normalizing serial write failures into `RpcError` would have
    turned those clean exits into tracebacks unless `RpcError` widened to
    include them, so this is the property that makes the normalization safe.
    """

    assert issubclass(RpcError, RuntimeError)
    assert issubclass(RpcCrashError, RpcError)
    # And not the other way round: a timeout is not a transport failure, and
    # the callers that retry on one must not catch the other by accident.
    assert not issubclass(RpcTimeoutError, RpcError)


def test_a_failed_write_arrives_as_rpc_error() -> None:
    """`send` promises `RpcTimeoutError` or `RpcError` and now keeps that.

    `PyserialMonitor.write` reports a failed write as a bare
    `RuntimeError`, which used to propagate out of `send` unchanged -- so
    every caller handling `(RpcError, RpcTimeoutError)` missed it. Found in
    the peer-OTA path (#4178), where it meant no retry and an error naming
    neither the board nor the call.
    """

    async def _run() -> None:
        client = RpcClient("FAKE", serial_interface=_WriteFailsSerial())
        await client.connect(boot_wait=0, drain_boot=False)
        with pytest.raises(RpcError, match=r"ping: Serial write error") as caught:
            await client.send("ping", {}, timeout=0.1)
        # The call is named, which the bare RuntimeError never was.
        assert "ping" in str(caught.value)
        assert isinstance(caught.value.__cause__, RuntimeError)
        await client.close()

    asyncio.run(_run())


def test_a_crash_keeps_its_own_type_and_decoded_lines() -> None:
    """The normalization must not flatten richer errors on its way past.

    `RpcCrashError` is an `RpcError`, so a naive `except RuntimeError`
    rewrap would swallow it and lose `decoded_lines` -- the stack trace is
    the whole point of that type.
    """

    async def _run() -> None:
        client = RpcClient("FAKE", serial_interface=_CrashSerial())
        await client.connect(boot_wait=0, drain_boot=False)
        with pytest.raises(RpcCrashError) as caught:
            await client.send("boom", {}, timeout=0.1)
        assert caught.value.decoded_lines == ["#0 boom"]
        await client.close()

    asyncio.run(_run())


class _CountingSerial:
    """Records how many times it was opened and closed."""

    def __init__(self: "_CountingSerial") -> None:
        self.connects = 0
        self.closes = 0

    async def connect(self: "_CountingSerial") -> None:
        self.connects += 1

    async def close(self: "_CountingSerial") -> None:
        self.closes += 1

    async def write(self: "_CountingSerial", data: str) -> None:
        pass

    async def read_lines(self: "_CountingSerial", timeout: float) -> AsyncIterator[str]:  # noqa: ARG002
        return
        yield ""  # pragma: no cover - makes this an async generator

    async def reset_device(
        self: "_CountingSerial", board: str | None
    ) -> bool:  # pragma: no cover
        return True


@typechecked
def test_close_keeps_an_injected_serial_interface() -> None:
    """A borrowed transport must survive close/reconnect (#4233).

    `connect()` fabricates a default fbuild backend whenever `_serial` is
    None, so a `close()` that drops an injected interface makes the next
    `connect()` silently talk over a different transport. Every retry loop
    closes and reconnects, so this is the common path, not a corner.
    """

    async def _run() -> None:
        iface = _CountingSerial()
        client = RpcClient("FAKE", serial_interface=iface)
        await client.connect(boot_wait=0, drain_boot=False)
        await client.close()
        # Still the caller's interface, not a replacement.
        assert client._serial is iface
        await client.connect(boot_wait=0, drain_boot=False)
        assert client._serial is iface
        assert iface.connects == 2
        assert iface.closes == 1
        await client.close()

    asyncio.run(_run())


@typechecked
def test_close_still_releases_an_owned_serial_interface() -> None:
    """The other half: an interface the client made is still dropped.

    Keeping it would leak a backend the client owns across a close, which is
    the behaviour the ownership flag exists to distinguish.
    """

    async def _run() -> None:
        client = RpcClient("FAKE", serial_interface=_CountingSerial())
        client._owns_serial = True  # pretend connect() built it
        await client.connect(boot_wait=0, drain_boot=False)
        await client.close()
        assert client._serial is None

    asyncio.run(_run())
