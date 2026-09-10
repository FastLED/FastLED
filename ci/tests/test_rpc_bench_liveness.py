"""A connected-but-inert RpcBench must refuse to exist (#4207).

Attaching a second client to a port another client already holds succeeds --
`connect()` returns without raising -- and the second client is then
completely inert, every call timing out. `RpcBench.call()` maps a timeout to
`None`, so that client is indistinguishable from a device answering nothing,
and the `None` reaches callers as though it described the firmware. It did
exactly that once, reporting "deployed firmware schema does not contain
rpSpiLoopback" against a board that exposes the method.

These tests pin the contract that replaces it: a client that reaches a caller
has round-tripped at least once, or construction raised.
"""

from __future__ import annotations

import asyncio
import json
from collections.abc import AsyncIterator

import pytest

from ci.autoresearch import rpc_bench
from ci.autoresearch.rpc_bench import RpcBench
from ci.rpc_client import RpcError


class _ScriptedSerial:
    """A serial interface that answers requests from a scripted rule.

    `responder` receives the decoded request and returns the line to send
    back, or None to stay silent -- which is what an inert second attach
    looks like from the client's side.
    """

    def __init__(self, responder: object) -> None:
        self._responder = responder
        # A boot banner so `connect()`'s spin-poll returns immediately
        # instead of burning its full 3 s wait in every one of these tests.
        self._pending: list[str] = ["setup() start"]
        self.closed = False
        self.writes: list[str] = []

    async def connect(self) -> None:
        return None

    async def close(self) -> None:
        self.closed = True

    async def write(self, data: str) -> None:
        self.writes.append(data)
        request = json.loads(data)
        reply = self._responder(request)  # type: ignore[operator]
        if reply is not None:
            self._pending.append(reply)

    async def read_lines(self, timeout: float) -> AsyncIterator[str]:
        # Behaves like a stream for the whole budget rather than ending after
        # one line: `_wait_for_response` iterates this generator once, so a
        # generator that returns early reads as "no more data ever".
        deadline = asyncio.get_event_loop().time() + timeout
        while asyncio.get_event_loop().time() < deadline:
            if self._pending:
                yield self._pending.pop(0)
            else:
                await asyncio.sleep(0.005)

    async def reset_device(self, board: str | None) -> bool:
        return True


def _install(
    monkeypatch: pytest.MonkeyPatch, responder: object
) -> list[_ScriptedSerial]:
    created: list[_ScriptedSerial] = []

    def _factory(port: str, *args: object, **kwargs: object) -> _ScriptedSerial:
        iface = _ScriptedSerial(responder)
        created.append(iface)
        return iface

    monkeypatch.setattr(rpc_bench, "create_serial_interface", _factory)
    return created


# `RpcClient` only treats a line as a response when it carries this prefix.
REMOTE = "REMOTE: "


def _ok(request: dict) -> str:
    return REMOTE + json.dumps({"id": request["id"], "result": {"message": "pong"}})


def _method_not_found(request: dict) -> str:
    return REMOTE + json.dumps(
        {"id": request["id"], "error": {"code": -32601, "message": "Method not found"}}
    )


def _silent(request: dict) -> None:
    return None


def test_a_live_device_constructs_normally(monkeypatch: pytest.MonkeyPatch) -> None:
    _install(monkeypatch, _ok)
    bench = RpcBench("FAKE_PORT", timeout=1.0)
    try:
        assert bench.call("ping") == {"message": "pong"}
    finally:
        bench.close()


def test_an_inert_client_raises_instead_of_being_returned(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The whole point: silence at construction must not become a live object."""
    _install(monkeypatch, _silent)
    with pytest.raises(RpcError) as caught:
        RpcBench("FAKE_PORT", timeout=1.0)
    message = str(caught.value)
    # The diagnostic has to name the cause, or the next person debugs the
    # firmware again instead of the second attach.
    assert "#4207" in message
    assert "another client already attached" in message
    assert "FAKE_PORT" in message


def test_a_firmware_without_the_probe_method_is_still_live(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """An error reply is a reply.

    Probing for liveness must not double as a firmware feature check, or it
    starts rejecting exactly the boards it exists to protect.
    """
    _install(monkeypatch, _method_not_found)
    bench = RpcBench("FAKE_PORT", timeout=1.0)
    try:
        assert bench.call("anything") is rpc_bench.METHOD_NOT_FOUND
    finally:
        bench.close()


def test_the_probe_is_one_round_trip(monkeypatch: pytest.MonkeyPatch) -> None:
    # A probe that cost several round trips would show up as latency on every
    # runner, so pin the count rather than trusting the implementation.
    created = _install(monkeypatch, _ok)
    bench = RpcBench("FAKE_PORT", timeout=1.0)
    try:
        assert len(created) == 1
        assert len(created[0].writes) == 1
        assert json.loads(created[0].writes[0])["method"] == "ping"
    finally:
        bench.close()


def test_a_failed_probe_does_not_leak_the_event_loop(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Construction raises, so the caller never gets an object to close."""
    created = _install(monkeypatch, _silent)
    with pytest.raises(RpcError):
        RpcBench("FAKE_PORT", timeout=1.0)
    assert len(created) == 1
    assert created[0].closed is True
