"""Unit tests for device-to-device network AutoResearch flows."""

from __future__ import annotations

import asyncio
import contextlib
import importlib
import io
import os
import sys
import time
from collections.abc import Callable
from pathlib import Path
from types import ModuleType
from typing import Any
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


# pty and TIOCEXCL are POSIX-only. These two tests exercise the real serial
# contention mechanism rather than emulating it, so they are resolved through
# getattr and skipped where the platform has no such thing. ty treats
# possibly-missing-attribute as a hard error, so the lookups cannot be direct.
def _optional_module(module_name: str) -> "ModuleType | None":
    """Import `module_name`, or None when the platform does not have it.

    `sys.platform != "win32"` is not a POSIX capability check: a non-Windows
    platform without `pty`/`fcntl`/`termios` would raise during collection,
    before the skip marker below could apply.
    """
    try:
        return importlib.import_module(module_name)
    except KeyboardInterrupt as ki:
        # Re-raising alone leaves the interrupt un-propagated to the main
        # thread, which is what KBI002 is about; this is the repo's handler,
        # used the same way in ci/tests/test_elf.py.
        handle_keyboard_interrupt(ki)
        raise
    except ImportError:
        return None


def _optional_callable(module_name: str, attribute: str) -> "Callable[..., Any] | None":
    module = _optional_module(module_name)
    if module is None:
        return None
    found = getattr(module, attribute, None)
    return found if callable(found) else None


def _optional_int(module_name: str, attribute: str) -> "int | None":
    module = _optional_module(module_name)
    if module is None:
        return None
    found = getattr(module, attribute, None)
    return found if isinstance(found, int) else None


_openpty = _optional_callable("pty", "openpty")
_ttyname: "Callable[..., Any] | None" = getattr(os, "ttyname", None)
_ioctl = _optional_callable("fcntl", "ioctl")
_TIOCEXCL = _optional_int("termios", "TIOCEXCL")

requires_posix_tty = pytest.mark.skipif(
    not all((_openpty, _ttyname, _ioctl, _TIOCEXCL)),
    reason="pty/TIOCEXCL are POSIX-only",
)

from ci.autoresearch.net import (
    _connect_peer_with_retry,
    _describe_failed_client_tests,
    _describe_port_holder,
    _port_is_locked,
    _reclaim_stale_port_locks,
    _summarize_client_tests,
    run_net_peer_autoresearch,
)
from ci.autoresearch.ota import _settle_link, run_ota_peer_autoresearch
from ci.rpc_client import RpcError, RpcTimeoutError


def _response(data: dict[str, Any]) -> MagicMock:
    response = MagicMock()
    response.data = data
    return response


def test_ota_peer_reports_missing_firmware(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A missing fbuild artifact fails cleanly before opening either board."""
    result = asyncio.run(
        run_ota_peer_autoresearch(
            upload_port="COM18",
            peer_upload_port="COM9",
            serial_iface=None,
            firmware_path=None,
        )
    )

    assert result == 1
    assert "RP2350W firmware is missing: None" in capsys.readouterr().out


def test_net_peer_runs_ten_device_only_reconnect_cycles(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """The peer path uses RPC/fbuild serial only, never a host WiFi manager."""
    primary = MagicMock()
    peer = MagicMock()
    primary.connect = AsyncMock()
    primary.close = AsyncMock()
    peer.connect = AsyncMock()
    peer.close = AsyncMock()
    primary_methods: list[str] = []
    peer_methods: list[str] = []
    client_test_timeouts: list[float] = []

    async def primary_send(method: str, *_args: Any, **_kwargs: Any) -> MagicMock:
        primary_methods.append(method)
        if method == "runNetClientTest":
            client_test_timeouts.append(_kwargs["timeout"])
        responses = {
            "status": {"platform": "Raspberry Pi Pico 2 W (RP2350)"},
            "wifiConnect": {"success": True},
            "wifiStatus": {"connected": True, "ip": "192.168.4.2"},
            "startNetServer": {"success": True, "port": 80},
            "runNetClientTest": {
                "success": True,
                "tests_passed": 12,
                "tests_failed": 0,
                "results": [
                    {
                        "test": "POST /echo 4096-byte FNV-1a",
                        "bytes": 4096,
                        "passed": True,
                    }
                ],
            },
            "stopNet": {"success": True},
            "ping": {"success": True},
        }
        return _response(responses[method])

    async def peer_send(method: str, *_args: Any, **_kwargs: Any) -> MagicMock:
        peer_methods.append(method)
        if method == "runNetClientTest":
            client_test_timeouts.append(_kwargs["timeout"])
        responses = {
            "status": {"platform": "ESP32-C6 (RISC-V)"},
            "startNetServer": {
                "success": True,
                "ssid": "FastLED-AutoResearch",
                "password": "fastled123",
                "ip": "192.168.4.1",
                "port": 80,
            },
            "runNetClientTest": {
                "success": True,
                "tests_passed": 12,
                "tests_failed": 0,
                "results": [
                    {
                        "test": "POST /echo 4096-byte FNV-1a",
                        "bytes": 4096,
                        "passed": True,
                    }
                ],
            },
            "stopNet": {"success": True},
            "ping": {"success": True},
        }
        return _response(responses[method])

    primary.send = AsyncMock(side_effect=primary_send)
    peer.send = AsyncMock(side_effect=peer_send)

    with (
        patch("ci.autoresearch.net.RpcClient", side_effect=[primary, peer]),
        patch("ci.util.serial_interface.create_serial_interface"),
    ):
        result = asyncio.run(
            run_net_peer_autoresearch(
                upload_port="COM17",
                peer_upload_port="COM9",
                serial_iface=MagicMock(),
                timeout=60.0,
            )
        )

    assert result == 0
    assert primary_methods.count("wifiConnect") == 10
    assert primary_methods.count("runNetClientTest") == 10
    assert primary_methods.count("stopNet") >= 11
    assert peer_methods.count("runNetClientTest") == 10
    assert len(client_test_timeouts) == 20
    assert all(call_timeout > 20.0 for call_timeout in client_test_timeouts)
    assert "ping" in peer_methods
    primary.close.assert_awaited_once()
    peer.close.assert_awaited_once()

    # The point of the summariser is auditable output. Asserting only that the
    # run returns 0 would pass even if it printed nothing, which is the gap
    # this change exists to close -- so assert what a reader would actually
    # have to read off the log.
    out = capsys.readouterr().out
    for label in ("RP2350W -> ESP32-C6", "ESP32-C6 -> RP2350W"):
        assert f"{label}: tests_passed=12 tests_failed=0" in out, label
        assert (
            f"{label} payload: test='POST /echo 4096-byte FNV-1a' "
            "bytes=4096 passed=True"
        ) in out, label
    # Ten cycles x two directions: twenty payload rows, not one.
    assert out.count("payload: test='POST /echo 4096-byte FNV-1a'") == 20


def test_ota_peer_stages_artifact_without_a_host_wifi_manager(tmp_path) -> None:
    """OTA peer validation uses only RPC/fbuild channels before device WiFi."""
    artifact = tmp_path / "firmware.bin"
    artifact.write_bytes(b"firmware")
    primary_before = MagicMock()
    primary_after = MagicMock()
    peer = MagicMock()
    for client in (primary_before, primary_after, peer):
        client.connect = AsyncMock()
        client.close = AsyncMock()

    async def primary_before_send(
        method: str, *_args: Any, **_kwargs: Any
    ) -> MagicMock:
        responses = {
            "status": {"platform": "Raspberry Pi Pico 2 W (RP2350)"},
            "wifiConnect": {"success": True},
            "wifiStatus": {"connected": True},
            "applyOtaArtifact": {"success": True},
            "stopNet": {"success": True},
            # `_settle_link` pings each board before the first real call.
            "ping": {"success": True},
        }
        return _response(responses[method])

    async def primary_after_send(method: str, *_args: Any, **_kwargs: Any) -> MagicMock:
        responses = {
            # The run's final criterion asks the board whether it applied the
            # image, because the C6 having served it only proves a download
            # (#3956). A blanket success for every method answered that with
            # `{"success": True}`, which `ota_applied_ok` reads as "not proven"
            # -- correctly, since neither flag is there. Answer the question the
            # success path is meant to be asserting.
            "rpOtaUpdateStatus": {
                "success": True,
                "attempted": True,
                "succeeded": True,
            },
        }
        return _response(responses.get(method, {"success": True}))

    async def peer_send(method: str, *_args: Any, **_kwargs: Any) -> MagicMock:
        responses = {
            "status": {"platform": "ESP32-C6 (RISC-V)"},
            "ping": {"success": True},
            "beginOtaArtifact": {"success": True},
            "writeOtaArtifact": {"success": True},
            "finishOtaArtifact": {
                "success": True,
                "sha256": "c3bf47ea1f4a4a605470313cacb3a44f4a461f68c6faeab07e737610cb5ac835",
            },
            "startNetServer": {
                "success": True,
                "ssid": "FastLED-AutoResearch",
                "password": "fastled123",
            },
            "startOtaArtifactServer": {
                "success": True,
                "ip": "192.168.4.1",
                "port": 8081,
            },
            "otaArtifactStatus": {"success": True, "servedRequests": 1},
            "stopNet": {"success": True},
        }
        return _response(responses[method])

    primary_before.send = AsyncMock(side_effect=primary_before_send)
    primary_after.send = AsyncMock(side_effect=primary_after_send)
    peer.send = AsyncMock(side_effect=peer_send)
    encoded_chunk = MagicMock()
    encoded_chunk.decode.return_value = ""
    with (
        patch(
            "ci.autoresearch.ota.RpcClient",
            side_effect=[primary_before, peer, primary_after],
        ),
        patch(
            "ci.autoresearch.ota.base64.b64encode", return_value=encoded_chunk
        ) as encode,
        patch("ci.util.serial_interface.create_serial_interface"),
        patch("ci.autoresearch.ota.asyncio.sleep", new_callable=AsyncMock),
    ):
        result = asyncio.run(
            run_ota_peer_autoresearch(
                upload_port="COM17",
                peer_upload_port="COM9",
                serial_iface=MagicMock(),
                firmware_path=artifact,
                timeout=60.0,
            )
        )

    assert result == 0
    encode.assert_called_once_with(b"firmware")
    # Order, not index: `_settle_link` pings before the first real call, and a
    # fixed position breaks whenever a step is added ahead of this one.
    peer_methods = [call.args[0] for call in peer.send.await_args_list]
    assert "beginOtaArtifact" in peer_methods
    assert peer_methods.index("status") < peer_methods.index("beginOtaArtifact")
    write_calls = []
    for call in peer.send.await_args_list:
        if call.args[0] == "writeOtaArtifact":
            write_calls.append(call)
    assert len(write_calls) == 1
    assert write_calls[0].args[1] == ""


def test_settle_link_retries_a_failed_serial_write() -> None:
    """A failed write must be retried, and must name the board when it isn't.

    `PyserialMonitor.write` converts `serial.SerialException` into
    `RuntimeError("Serial write error: ...")`. `RpcClient.send` normalizes
    that into `RpcError` (#4191), so it is what a caller sees and what this
    mocks. Before the retry caught it, a write failure escaped entirely and
    surfaced as a transport error naming neither the board nor the call,
    which is the failure #3956 exists to fix.
    """
    client = MagicMock()
    client.send = AsyncMock(side_effect=RpcError("Serial write error: boom"))

    with pytest.raises(RpcTimeoutError) as caught:
        asyncio.run(_settle_link(client, "primary (COM18)", lambda: 30.0))

    message = str(caught.value)
    # Names the board, the number of attempts, and keeps the original cause.
    assert "primary (COM18)" in message
    assert "3 attempts" in message
    assert "Serial write error: boom" in message
    # All three attempts were made rather than one throw escaping.
    assert client.send.await_count == 3


def test_settle_link_returns_once_the_board_answers() -> None:
    """The retry stops at the first success, and does not raise."""
    client = MagicMock()
    client.send = AsyncMock(
        side_effect=[
            # What `send` raises for a failed write since #4191.
            RpcError("Serial write error: boom"),
            _response({"success": True}),
        ]
    )
    asyncio.run(_settle_link(client, "peer (COM9)", lambda: 30.0))
    assert client.send.await_count == 2


def test_summarize_client_tests_rejects_incomplete_reports() -> None:
    """A report that cannot substantiate the payload leg is not a pass.

    #3899 wants decisive evidence of the >=4 KiB exchange. Printing
    `tests_passed=None` for a truncated response would read as weak evidence
    rather than a broken report, so each missing piece raises instead.
    """
    complete = {
        "tests_passed": 12,
        "tests_failed": 0,
        "results": [
            {"test": "POST /echo 4096-byte FNV-1a", "bytes": 4096, "passed": True}
        ],
    }
    _summarize_client_tests("ok", complete)  # does not raise

    for missing in ("tests_passed", "tests_failed", "results"):
        broken = {key: value for key, value in complete.items() if key != missing}
        with pytest.raises(RpcError, match="payload leg"):
            _summarize_client_tests("broken", broken)

    # Present but empty: the battery ran without the echo row.
    with pytest.raises(RpcError, match="did not run"):
        _summarize_client_tests("no-echo", {**complete, "results": []})

    # Booleans are not acceptable integers for a tally.
    with pytest.raises(RpcError, match="payload leg"):
        _summarize_client_tests("boolish", {**complete, "tests_passed": True})


def test_describe_failed_client_tests_names_the_no_response_row() -> None:
    """The failing row must be named, not buried in eleven passing ones.

    This is the payload captured on the bench: RP2350W -> ESP32-C6, cycle 10,
    where only `GET /leds` came back with nothing at all. Raising the raw
    dict put that row in the middle of one unwrapped line.
    """
    data: dict[str, Any] = {
        "success": False,
        "tests_passed": 11,
        "tests_failed": 1,
        "results": [
            {"test": "GET /ping", "passed": True, "body_read": 4},
            {
                "test": "GET /leds",
                "passed": False,
                "error": "No HTTP response: request sent, nothing read",
                "status_line": "",
                "body_read": 0,
                "content_length": -1,
            },
            {"test": "POST /echo 4096-byte FNV-1a", "passed": True, "bytes": 4096},
        ],
    }

    described = _describe_failed_client_tests(data)

    assert "1 of 3 sub-tests failed" in described
    assert "GET /leds" in described
    assert "No HTTP response" in described
    # The field that separates "answered wrongly" from "did not answer".
    assert "status_line=''" in described
    # Passing rows must not be listed.
    assert "GET /ping" not in described


def test_describe_failed_client_tests_handles_a_malformed_report() -> None:
    """A report with no results array must say so, not raise."""
    assert "no 'results' array" in _describe_failed_client_tests({"success": False})
    assert "no sub-test reported a failure" == _describe_failed_client_tests(
        {"results": [{"test": "GET /ping", "passed": True}]}
    )


def test_describe_failed_client_tests_reports_a_malformed_row() -> None:
    """A non-dict row must be reported, not skipped into a false all-clear."""
    described = _describe_failed_client_tests(
        {"results": [{"test": "GET /ping", "passed": True}, "not-a-dict"]}
    )
    assert "1 of 2 sub-tests failed" in described
    assert "result[1] is not an object" in described
    assert "not-a-dict" in described


def test_connect_peer_retries_a_mute_endpoint() -> None:
    """A first connect that opens but never answers must be retried.

    This is the captured bench failure: `No response with ID 1 within 15.0s`
    while attaching to the ESP32-C6, with connect() itself succeeding. A
    stale CDC endpoint opens fine and stays mute, so only the follow-up ping
    detects it.
    """
    peer = AsyncMock()
    peer.connect = AsyncMock()
    peer.close = AsyncMock()
    peer.send = AsyncMock(
        side_effect=[RpcTimeoutError("No response with ID 1 within 15.0s"), MagicMock()]
    )

    with patch("ci.autoresearch.net.asyncio.sleep", new=AsyncMock()):
        asyncio.run(
            _connect_peer_with_retry(peer, "ESP32-C6", "/dev/ttyACM1", lambda: 30.0, 3)
        )

    assert peer.connect.await_count == 2
    assert peer.send.await_count == 2


def test_connect_peer_gives_up_and_names_the_port() -> None:
    """Exhausting the retries must fail loudly, naming port and cause."""
    peer = AsyncMock()
    peer.connect = AsyncMock()
    peer.close = AsyncMock()
    peer.send = AsyncMock(side_effect=RpcTimeoutError("still mute"))

    with patch("ci.autoresearch.net.asyncio.sleep", new=AsyncMock()):
        with pytest.raises(RpcTimeoutError) as excinfo:
            asyncio.run(
                _connect_peer_with_retry(
                    peer, "ESP32-C6", "/dev/ttyACM1", lambda: 30.0, attempts=2
                )
            )

    message = str(excinfo.value)
    assert "/dev/ttyACM1" in message
    assert "2 connect attempts" in message
    assert "still mute" in message
    assert peer.connect.await_count == 2


def test_connect_peer_does_not_retry_a_healthy_board() -> None:
    """A board that answers first time must not be reconnected."""
    peer = AsyncMock()
    peer.connect = AsyncMock()
    peer.close = AsyncMock()
    peer.send = AsyncMock(return_value=MagicMock())

    asyncio.run(
        _connect_peer_with_retry(peer, "ESP32-C6", "/dev/ttyACM1", lambda: 30.0, 3)
    )

    assert peer.connect.await_count == 1
    assert peer.close.await_count == 0


def test_connect_peer_clamps_waits_to_the_run_budget() -> None:
    """Retry waits must not outlive the run's own deadline.

    Unclamped, three mute attempts spend ~43s of boot waits, pings and
    back-offs before any later call notices the deadline passed -- a recovery
    mechanism consuming the run it exists to save.
    """
    peer = AsyncMock()
    peer.connect = AsyncMock()
    peer.close = AsyncMock()
    peer.send = AsyncMock(side_effect=[RpcTimeoutError("mute"), MagicMock()])
    sleeps: list[float] = []

    async def _record_sleep(seconds: float) -> None:
        sleeps.append(seconds)

    # Only 1.5s of budget left: every wait must be clamped below its default.
    with patch("ci.autoresearch.net.asyncio.sleep", new=_record_sleep):
        asyncio.run(
            _connect_peer_with_retry(peer, "ESP32-C6", "/dev/ttyACM1", lambda: 1.5, 3)
        )

    assert peer.connect.await_args_list[0].kwargs["boot_wait"] == 1.5
    assert peer.send.await_args_list[0].kwargs["timeout"] == 1.5
    assert sleeps == [1.5]


def test_connect_peer_propagates_an_expired_budget() -> None:
    """A budget helper that raises must abort the retry loop, not be swallowed."""
    peer = AsyncMock()
    peer.connect = AsyncMock()
    peer.close = AsyncMock()
    peer.send = AsyncMock()

    def _expired() -> float:
        raise RpcTimeoutError("--net-peer deadline expired")

    captured = io.StringIO()
    with contextlib.redirect_stdout(captured):
        with pytest.raises(RpcTimeoutError, match="deadline expired"):
            asyncio.run(
                _connect_peer_with_retry(peer, "ESP32-C6", "/dev/ttyACM1", _expired, 3)
            )

    # The point of the test: an expired budget must not be *reported* as an
    # unresponsive board. Checking the budget inside the try block still
    # aborts (the back-off re-raises), but first prints
    # "ESP32-C6 did not answer ... reconnecting" -- blaming the peer for the
    # caller running out of time.
    assert peer.connect.await_count == 0
    assert peer.send.await_count == 0
    assert "did not answer" not in captured.getvalue()
    assert "reconnecting" not in captured.getvalue()


def test_connect_peer_reads_the_ping_budget_after_connect() -> None:
    """The ping budget must be read after connect(), not before it.

    connect() can consume most of the remaining budget itself, so a ping
    timeout computed beforehand is stale and can outlive the caller's
    deadline -- the exact thing the clamping exists to prevent. The budget
    here shrinks *because* connect ran, so a reading taken before it differs
    from one taken after.
    """
    budget = {"left": 10.0}

    def _remaining() -> float:
        return budget["left"]

    peer = AsyncMock()
    peer.close = AsyncMock()
    peer.send = AsyncMock(return_value=MagicMock())

    async def _connect_that_spends_the_budget(**_kwargs: object) -> None:
        budget["left"] = 0.5

    peer.connect = AsyncMock(side_effect=_connect_that_spends_the_budget)

    asyncio.run(
        _connect_peer_with_retry(peer, "ESP32-C6", "/dev/ttyACM1", _remaining, 3)
    )

    # boot_wait is bounded by the pre-connect budget...
    connect_call = peer.connect.await_args
    assert connect_call is not None
    assert connect_call.kwargs["boot_wait"] == 3.0
    # ...and the ping by what is actually left afterwards, not by 10.0.
    send_call = peer.send.await_args
    assert send_call is not None
    assert send_call.kwargs["timeout"] == 0.5


def test_connect_peer_retries_a_port_that_will_not_open() -> None:
    """A port that cannot be opened is the same fault class as a mute one.

    Observed on the bench: attempts 1-2 failed mute with RpcTimeoutError, and
    the third reconnect raised "attach failed: open_port(...) exceeded 3s"
    from the serial layer -- a different exception type, which escaped a
    narrow catch list and propagated raw, so this helper's own diagnostic
    never printed and the caller saw a bare serial error instead.
    """
    peer = AsyncMock()
    peer.close = AsyncMock()
    peer.send = AsyncMock(return_value=MagicMock())
    peer.connect = AsyncMock(
        side_effect=[
            RuntimeError("attach failed: open_port(/dev/ttyACM2) exceeded 3s"),
            None,
        ]
    )

    with patch("ci.autoresearch.net.asyncio.sleep", new=AsyncMock()):
        asyncio.run(
            _connect_peer_with_retry(peer, "ESP32-C6", "/dev/ttyACM2", lambda: 30.0, 3)
        )

    assert peer.connect.await_count == 2


def test_connect_peer_reports_a_serial_error_in_its_own_message() -> None:
    """Exhausting the attempts must surface this helper's diagnostic.

    Previously a non-RpcError type escaped entirely, so the run reported the
    raw serial error with no port, attempt count, or context.
    """
    peer = AsyncMock()
    peer.close = AsyncMock()
    peer.send = AsyncMock()
    peer.connect = AsyncMock(
        side_effect=RuntimeError("attach failed: open_port exceeded 3s")
    )

    with patch("ci.autoresearch.net.asyncio.sleep", new=AsyncMock()):
        with pytest.raises(RpcTimeoutError) as excinfo:
            asyncio.run(
                _connect_peer_with_retry(
                    peer, "ESP32-C6", "/dev/ttyACM2", lambda: 30.0, attempts=2
                )
            )

    message = str(excinfo.value)
    assert "/dev/ttyACM2" in message
    assert "2 connect attempts" in message
    assert "attach failed" in message


def test_connect_peer_bounds_a_transport_that_never_opens() -> None:
    """A hung port open must not outlive the caller's deadline.

    `boot_wait` was bounded by the remaining budget, but the transport open
    itself was not: the serial layer self-bounds at about 3 s and knows
    nothing of this caller's deadline, so with under 3 s left the attach
    alone could overrun before the ping and back-off checks ran.
    """
    peer = MagicMock()

    async def _never_opens(**_kwargs: Any) -> None:
        await asyncio.sleep(30.0)

    peer.connect = AsyncMock(side_effect=_never_opens)
    peer.close = AsyncMock()
    peer.send = AsyncMock()

    started = time.monotonic()
    with pytest.raises(RpcTimeoutError):
        asyncio.run(
            _connect_peer_with_retry(peer, "ESP32-C6", "/dev/ttyACM1", lambda: 0.25, 2)
        )
    elapsed = time.monotonic() - started

    # Two attempts of a 30 s hang would be a minute; the budget caps each.
    assert elapsed < 5.0, f"took {elapsed:.1f}s"
    # And the half-opened transport is closed every time, final attempt too.
    assert peer.close.await_count == 2


@requires_posix_tty
def test_describe_port_holder_is_silent_when_the_port_opens() -> None:
    """A port that opens is not contended, whoever else holds an fd.

    Measured on the bench: a tty accepts multiple openers, and another
    process merely holding one is harmless. Naming that holder would accuse
    the wrong thing. See FastLED/fbuild#1429.
    """
    assert _openpty is not None and _ttyname is not None
    master, slave = _openpty()
    try:
        node = _ttyname(slave)
        # A second handle is open on this pty right now, and it still opens.
        assert _describe_port_holder(node, "/proc") == ""
    finally:
        os.close(master)
        os.close(slave)


@requires_posix_tty
def test_describe_port_holder_names_the_locker_under_real_contention() -> None:
    """When TIOCEXCL is set, say who set it and that the device is fine.

    Setting TIOCEXCL by hand reproduces the exact `Device or resource busy`
    the serial layer reports as "serial driver may be wedged".
    """
    assert _openpty is not None and _ttyname is not None
    master, slave = _openpty()
    try:
        node = _ttyname(slave)
        assert _ioctl is not None and _TIOCEXCL is not None
        _ioctl(slave, _TIOCEXCL)
        described = _describe_port_holder(node, "/proc")
        assert "locked against other openers" in described
        assert "the device itself is fine" in described
        assert str(os.getpid()) in described
    finally:
        os.close(master)
        os.close(slave)


@requires_posix_tty
def test_describe_port_holder_follows_a_by_id_symlink(tmp_path: Path) -> None:
    """The stable name a bench config uses must still name the holder.

    `/dev/serial/by-id/usb-...` is what a config pins, because `/dev/ttyACM*`
    renumbers between plugs. But `/proc/<pid>/fd/<n>` links to the canonical
    node, so matching on the configured basename finds nothing: the probe
    still detects EBUSY and then reports it with no holder to name, which is
    the whole diagnostic this helper exists to give.

    A symlink standing in for the by-id path, since a pty cannot be given one
    under /dev here.
    """
    assert _openpty is not None and _ttyname is not None
    master, slave = _openpty()
    try:
        node = _ttyname(slave)
        alias = tmp_path / "usb-Raspberry_Pi_Pico_by-id-alias"
        os.symlink(node, alias)
        assert _ioctl is not None and _TIOCEXCL is not None
        _ioctl(slave, _TIOCEXCL)

        # Through the alias, not the node the fd table records.
        described = _describe_port_holder(str(alias), "/proc")
        assert "locked against other openers" in described
        assert str(os.getpid()) in described
        # And it reports the name the caller passed, not the resolved one --
        # that is the name in their config and the one they can act on.
        assert str(alias) in described
    finally:
        os.close(master)
        os.close(slave)


def test_describe_port_holder_is_silent_for_a_missing_port() -> None:
    """An absent port is not contention; say nothing rather than guess."""
    assert _describe_port_holder("/dev/ttyACM-nonexistent-xyz", "/proc") == ""


@requires_posix_tty
def test_reclaim_stale_port_locks_skips_healthy_ports(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A port that opens needs no restart, and must not trigger one.

    Restarting the daemon is disruptive; doing it on every run because a
    probe was sloppy would be worse than the failure it recovers.
    """
    assert _openpty is not None and _ttyname is not None
    master, slave = _openpty()
    try:
        node = _ttyname(slave)
        assert _reclaim_stale_port_locks([node]) is False
        assert "restarting" not in capsys.readouterr().out
    finally:
        os.close(master)
        os.close(slave)


def test_reclaim_stale_port_locks_ignores_empty_ports() -> None:
    """Missing ports are not locks; never restart on their account."""
    assert _reclaim_stale_port_locks([]) is False
    assert _reclaim_stale_port_locks([""]) is False


@requires_posix_tty
def test_port_is_locked_tracks_real_exclusivity() -> None:
    """_port_is_locked must key on EBUSY, not on someone holding a handle."""
    assert (
        _openpty is not None
        and _ttyname is not None
        and _ioctl is not None
        and _TIOCEXCL is not None
    )
    master, slave = _openpty()
    try:
        node = _ttyname(slave)
        # A second handle is open right now, and the port is still not locked.
        assert _port_is_locked(node) is False
        _ioctl(slave, _TIOCEXCL)
        assert _port_is_locked(node) is True
    finally:
        os.close(master)
        os.close(slave)
