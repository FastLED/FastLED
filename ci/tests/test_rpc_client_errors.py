"""Regression tests for JSON-RPC error propagation."""

from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator

import pytest

from ci.rpc_client import RpcClient, RpcError


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
        with pytest.raises(RpcError, match=r"-32601.*Method not found"):
            await client.send("missing", timeout=0.1)
        await client.close()

    asyncio.run(_run())
