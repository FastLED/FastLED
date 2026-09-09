"""Unit tests for device-to-device network AutoResearch flows."""

from __future__ import annotations

import asyncio
import contextlib
import io
from typing import Any
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from ci.autoresearch.net import (
    _connect_peer_with_retry,
    _describe_failed_client_tests,
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
        return _response({"success": True})

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
    assert peer.connect.await_args.kwargs["boot_wait"] == 3.0
    # ...and the ping by what is actually left afterwards, not by 10.0.
    assert peer.send.await_args.kwargs["timeout"] == 0.5


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
