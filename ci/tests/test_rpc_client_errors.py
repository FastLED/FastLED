"""Regression tests for JSON-RPC error propagation."""

from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator

import pytest

from ci.rpc_client import RpcClient, RpcCrashError, RpcError, RpcTimeoutError


class _ErrorSerial:
    async def connect(self) -> None:
        pass

    async def close(self) -> None:
        pass

    async def write(self, data: str) -> None:
        assert '"id":1' in data

    async def read_lines(self, timeout: float) -> AsyncIterator[str]:  # noqa: ARG002
        yield (
            'REMOTE: {"jsonrpc":"2.0","id":1,'
            '"error":{"code":-32601,"message":"Method not found: missing"}}'
        )

    async def reset_device(self, board: str | None) -> bool:  # pragma: no cover
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

    async def connect(self) -> None:
        pass

    async def close(self) -> None:
        pass

    async def write(self, data: str) -> None:  # noqa: ARG002
        raise RuntimeError("Serial write error: [Errno 5] Input/output error")

    async def read_lines(self, timeout: float) -> AsyncIterator[str]:  # noqa: ARG002
        return
        yield ""  # pragma: no cover - makes this an async generator

    async def reset_device(self, board: str | None) -> bool:  # pragma: no cover
        return True


class _CrashSerial:
    """A transport whose response is a device crash, not a write failure."""

    async def connect(self) -> None:
        pass

    async def close(self) -> None:
        pass

    async def write(self, data: str) -> None:
        pass

    async def read_lines(self, timeout: float) -> AsyncIterator[str]:  # noqa: ARG002
        raise RpcCrashError("device crashed", ["#0 boom"])
        yield ""  # pragma: no cover - makes this an async generator

    async def reset_device(self, board: str | None) -> bool:  # pragma: no cover
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
