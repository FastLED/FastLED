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
from ci.rpc_client import RpcError, RpcTransportError


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


class _UnwritableSerial(_ScriptedSerial):
    """Opens fine, reads fine, and cannot write.

    The shape that slipped through the first version of this gate: `send()`
    normalises a failed write into `RpcError` (#4191), which the probe was
    accepting as "an error reply is a reply".
    """

    async def write(self, data: str) -> None:
        raise RuntimeError("Serial write error: device disappeared")


def test_a_failed_write_is_not_mistaken_for_an_error_reply(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    created: list[_ScriptedSerial] = []

    def _factory(port: str, *args: object, **kwargs: object) -> _ScriptedSerial:
        iface = _UnwritableSerial(_ok)
        created.append(iface)
        return iface

    monkeypatch.setattr(rpc_bench, "create_serial_interface", _factory)

    with pytest.raises(RpcError) as caught:
        RpcBench("FAKE_PORT", timeout=1.0)
    # And it is refused as a transport failure, not swallowed as a reply.
    assert "#4207" in str(caught.value)
    assert created[0].closed is True


def test_a_failed_write_raises_the_transport_subtype() -> None:
    """The distinction the probe reads, pinned at its source.

    Without a distinct type a failed write and an error *reply* are the same
    class, and the probe cannot tell "the device answered with an error" --
    which proves the link round-trips -- from "the request never left".
    """
    assert issubclass(RpcTransportError, RpcError)

    from ci.rpc_client import RpcClient

    async def _run() -> None:
        client = RpcClient(
            "FAKE_PORT", timeout=0.5, serial_interface=_UnwritableSerial(_ok)
        )
        await client.connect(boot_wait=0.0, drain_boot=False)
        await client.send("ping", args=None, timeout=0.5)

    loop = asyncio.new_event_loop()
    try:
        with pytest.raises(RpcTransportError):
            loop.run_until_complete(_run())
    finally:
        loop.close()


class _UnattachableSerial(_ScriptedSerial):
    """A lost daemon, then a handshake timeout after its restart: the error
    NativeFbuildSerialAdapter raises carries the restart's cause."""

    async def connect(self) -> None:
        try:
            raise RuntimeError("failed to connect to daemon WebSocket: refused")
        except RuntimeError as first:
            raise ConnectionError(
                "daemon WebSocket attach/handshake timed out"
            ) from first


def test_a_failed_attach_names_the_whole_error_chain(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Runners print only str(e), so the chained first error must be in it."""
    created: list[_ScriptedSerial] = []

    def _factory(port: str, *args: object, **kwargs: object) -> _ScriptedSerial:
        iface = _UnattachableSerial(_ok)
        created.append(iface)
        return iface

    monkeypatch.setattr(rpc_bench, "create_serial_interface", _factory)
    with pytest.raises(RpcTransportError) as raised:
        RpcBench("FAKE_PORT", timeout=1.0)
    message = str(raised.value)
    assert message.startswith("attach failed: ")
    assert "ConnectionError: daemon WebSocket attach/handshake timed out" in message
    assert "RuntimeError: failed to connect to daemon WebSocket: refused" in message
    assert len(created) == 1


class _InterruptedAttachSerial(_ScriptedSerial):
    """Ctrl-C raised inside the attach itself (e.g. during daemon recovery)."""

    async def connect(self) -> None:
        raise KeyboardInterrupt


def test_an_interrupt_inside_the_attach_still_closes_the_bench(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """FastLED#5281: the interrupt is stored on the finished connect task,
    so draining that task again would re-raise it past close()."""
    created: list[_ScriptedSerial] = []
    handled: list[KeyboardInterrupt] = []

    def _factory(port: str, *args: object, **kwargs: object) -> _ScriptedSerial:
        iface = _InterruptedAttachSerial(_ok)
        created.append(iface)
        return iface

    monkeypatch.setattr(rpc_bench, "create_serial_interface", _factory)
    monkeypatch.setattr(rpc_bench, "handle_keyboard_interrupt", handled.append)
    with pytest.raises(KeyboardInterrupt):
        RpcBench("FAKE_PORT", timeout=1.0)
    assert len(created) == 1
    assert created[0].closed is True
    assert len(handled) == 1


def test_the_error_chain_includes_earlier_attach_attempts() -> None:
    """FastLED#5281: the retried first failure is the symptom; keep it,
    attached to the final error rather than read as its cause."""
    from ci.util.serial_interface import _describe_attempt, _note_earlier_attempts

    final = ConnectionError("daemon rejected serial attach: port busy")
    final.__cause__ = RuntimeError("failed to connect to daemon WebSocket")
    timeout = ConnectionError("daemon WebSocket attach/handshake timed out")
    _note_earlier_attempts(final, [_describe_attempt(1, timeout)])
    message = rpc_bench.describe_error_chain(final)
    assert message == (
        "ConnectionError: daemon rejected serial attach: port busy "
        "[attach attempt 1 failed: ConnectionError: "
        "daemon WebSocket attach/handshake timed out]"
        " <- RuntimeError: failed to connect to daemon WebSocket"
    )


def test_notes_set_to_none_do_not_mask_the_chain() -> None:
    """FastLED#5284 review: `__notes__ = None` reads as no notes."""
    error = ConnectionError("session closed")
    error.__notes__ = None  # type: ignore[attr-defined]
    assert rpc_bench.describe_error_chain(error) == "ConnectionError: session closed"


class _InterruptedTwiceSerial(_InterruptedAttachSerial):
    """Ctrl-C inside the attach, and a second Ctrl-C (a signal, landing as a
    loop callback) while the detach is still in progress."""

    def __init__(self, responder: object) -> None:
        super().__init__(responder)
        self.interrupted_detach = False
        self.close_calls = 0

    async def close(self) -> None:
        self.close_calls += 1
        if not self.interrupted_detach:
            self.interrupted_detach = True

            def _second_ctrl_c() -> None:
                raise KeyboardInterrupt

            asyncio.get_running_loop().call_soon(_second_ctrl_c)
            await asyncio.sleep(0.01)  # still detaching when it lands
        self.closed = True


def test_a_second_interrupt_in_the_detach_still_closes_the_loop(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """FastLED#5284 review: the same close task is resumed, so the monitor
    detaches before the loop closes; the first interrupt is the one raised."""
    created: list[_InterruptedTwiceSerial] = []
    benches: list[RpcBench] = []
    handled: list[KeyboardInterrupt] = []
    original_init = RpcBench.__init__

    def _factory(port: str, *args: object, **kwargs: object) -> _InterruptedTwiceSerial:
        iface = _InterruptedTwiceSerial(_ok)
        created.append(iface)
        return iface

    def _tracking_init(self: RpcBench, *args: object, **kwargs: object) -> None:
        benches.append(self)
        original_init(self, *args, **kwargs)  # type: ignore[arg-type]

    monkeypatch.setattr(rpc_bench, "create_serial_interface", _factory)
    monkeypatch.setattr(rpc_bench, "handle_keyboard_interrupt", handled.append)
    monkeypatch.setattr(RpcBench, "__init__", _tracking_init)
    with pytest.raises(KeyboardInterrupt):
        RpcBench("FAKE_PORT", timeout=1.0)
    assert created[0].interrupted_detach is True
    assert created[0].closed is True
    assert created[0].close_calls == 1  # the same detach resumed, not a new one
    assert benches[0]._loop.is_closed()
    assert len(handled) == 1
