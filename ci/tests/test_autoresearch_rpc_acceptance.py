"""Host-side AutoResearch RPC acceptance validation tests."""

from __future__ import annotations

import asyncio
from pathlib import Path
from types import SimpleNamespace
from typing import Any
from unittest.mock import AsyncMock, MagicMock, patch

from ci.autoresearch.context import QuietContext, RunContext
from ci.autoresearch.phases import _classify_test_failure, _run_tests_or_special_mode


_PATCH_MOD = "ci.autoresearch.phases"


def _make_ctx(
    commands: list[dict[str, Any]],
    effective_tx_pin: int = 1,
    effective_rx_pin: int = 0,
) -> RunContext:
    return RunContext(
        args=SimpleNamespace(
            pin_toggle_rx=False,
            ws2812_loopback=False,
            tx_pin=None,
            rx_pin=None,
        ),
        drivers=["PARLIO"],
        json_rpc_commands=commands,
        expect_keywords=[],
        fail_keywords=["ERROR"],
        timeout_seconds=60.0,
        build_dir=Path("/fake/project"),
        simd_test_mode=False,
        coroutine_test_mode=False,
        ieee754_test_mode=False,
        perf_wave2d_grid=None,
        net_server_mode=False,
        net_client_mode=False,
        net_loopback_mode=False,
        net_peer_mode=False,
        ota_mode=False,
        ble_mode=False,
        decode_mode=False,
        gpio_only_mode=False,
        parallel_mode=False,
        final_environment="esp32s3",
        upload_port="COM5",
        use_fbuild=False,
        build_driver=None,
        effective_tx_pin=effective_tx_pin,
        effective_rx_pin=effective_rx_pin,
    )


def _response(data: Any) -> MagicMock:
    response = MagicMock()
    response.data = data
    return response


def _run_rpc(
    commands: list[dict[str, Any]],
    responses: list[Any],
    effective_tx_pin: int = 1,
    effective_rx_pin: int = 0,
) -> int:
    ctx = _make_ctx(
        commands,
        effective_tx_pin=effective_tx_pin,
        effective_rx_pin=effective_rx_pin,
    )
    qctx = QuietContext(quiet=False)

    mock_client = AsyncMock()
    mock_client.send = AsyncMock(side_effect=[_response(data) for data in responses])
    mock_client.connect = AsyncMock()
    mock_client.close = AsyncMock()

    with (
        patch(f"{_PATCH_MOD}.RpcClient", return_value=mock_client),
        patch(f"{_PATCH_MOD}.kill_port_users"),
        patch("time.sleep"),
    ):
        return asyncio.run(_run_tests_or_special_mode(ctx, qctx))


def _run_single_command() -> dict[str, Any]:
    return {
        "method": "runSingleTest",
        "params": {
            "driver": "PARLIO",
            "laneSizes": [100],
            "pattern": "MSB_LSB_A",
            "iterations": 1,
            "timing": "WS2812B-V5",
        },
    }


def _set_pins_command() -> dict[str, Any]:
    return {"method": "setPins", "params": [{"txPin": 22, "rxPin": 8}]}


def _valid_single_pass() -> dict[str, Any]:
    return {
        "success": True,
        "passed": True,
        "driver": "PARLIO",
        "laneCount": 1,
        "laneSizes": [100],
        "duration_ms": 42,
        "passedTests": 1,
        "totalTests": 1,
        "requestedTxPin": 1,
        "requestedRxPin": 0,
        "actualTxPin": 1,
        "actualRxPin": 0,
        "captureBackend": "FLEXPWM",
        "captureEvidenceExpected": True,
        "captureEvidenceBytes": 300,
        "captureEvidenceRawEdges": 2400,
    }


def test_run_single_valid_pass_is_accepted() -> None:
    rc = _run_rpc([_run_single_command()], [_valid_single_pass()])
    assert rc == 0


def test_run_single_missing_passed_field_fails() -> None:
    response = _valid_single_pass()
    del response["passed"]

    rc = _run_rpc([_run_single_command()], [response])

    assert rc == 1


def test_run_single_pass_requires_matching_test_counts() -> None:
    response = _valid_single_pass()
    response["passedTests"] = 0

    rc = _run_rpc([_run_single_command()], [response])

    assert rc == 1


def test_setup_only_response_does_not_satisfy_final_pass() -> None:
    rc = _run_rpc(
        [_set_pins_command()],
        [{"success": True, "txPin": 22, "rxPin": 8}],
    )

    assert rc == 1


def test_non_dict_test_response_fails_after_setup_ok() -> None:
    rc = _run_rpc(
        [_set_pins_command(), _run_single_command()],
        [{"success": True, "txPin": 22, "rxPin": 8}, "OK"],
    )

    assert rc == 1


def test_set_pins_response_must_match_request() -> None:
    response = _valid_single_pass()
    response["requestedTxPin"] = 22
    response["requestedRxPin"] = 8
    response["actualTxPin"] = 22
    response["actualRxPin"] = 8

    rc = _run_rpc(
        [_set_pins_command(), _run_single_command()],
        [{"success": True, "txPin": 22, "rxPin": 9}, response],
        effective_tx_pin=22,
        effective_rx_pin=8,
    )

    assert rc == 1


def test_set_pins_and_test_response_agree_with_effective_pins() -> None:
    response = _valid_single_pass()
    response["requestedTxPin"] = 22
    response["requestedRxPin"] = 8
    response["actualTxPin"] = 22
    response["actualRxPin"] = 8

    rc = _run_rpc(
        [_set_pins_command(), _run_single_command()],
        [{"success": True, "txPin": 22, "rxPin": 8}, response],
        effective_tx_pin=22,
        effective_rx_pin=8,
    )

    assert rc == 0


def test_run_single_response_driver_must_match_request() -> None:
    response = _valid_single_pass()
    response["driver"] = "RMT"

    rc = _run_rpc([_run_single_command()], [response])

    assert rc == 1


def test_run_single_missing_actual_pin_fails() -> None:
    response = _valid_single_pass()
    del response["actualRxPin"]

    rc = _run_rpc([_run_single_command()], [response])

    assert rc == 1


def test_run_single_missing_capture_backend_fails() -> None:
    response = _valid_single_pass()
    del response["captureBackend"]

    rc = _run_rpc([_run_single_command()], [response])

    assert rc == 1


def test_run_single_pass_requires_capture_evidence() -> None:
    response = _valid_single_pass()
    response["captureEvidenceBytes"] = 0
    response["captureEvidenceRawEdges"] = 0

    rc = _run_rpc([_run_single_command()], [response])

    assert rc == 1


def test_run_single_pass_requires_capture_evidence_fields() -> None:
    response = _valid_single_pass()
    del response["captureEvidenceBytes"]
    del response["captureEvidenceRawEdges"]

    rc = _run_rpc([_run_single_command()], [response])

    assert rc == 1


def test_run_single_pin_override_is_accepted_when_response_matches() -> None:
    command = _run_single_command()
    command["params"]["pinTx"] = 6
    response = _valid_single_pass()
    response["requestedTxPin"] = 6
    response["actualTxPin"] = 6

    rc = _run_rpc([command], [response])

    assert rc == 0


def _run_parallel_command() -> dict[str, Any]:
    return {
        "method": "runParallelTest",
        "params": {
            "drivers": [
                {"driver": "PARLIO", "laneSizes": [100]},
                {"driver": "RMT", "laneSizes": [100]},
            ],
            "pattern": "MSB_LSB_A",
            "iterations": 1,
            "timing": "WS2812B-V5",
        },
    }


def _valid_parallel_pass() -> dict[str, Any]:
    return {
        "success": True,
        "passed": True,
        "duration_ms": 50,
        "totalTests": 1,
        "passedTests": 1,
        "requestedTxPin": 1,
        "requestedRxPin": 0,
        "actualTxPin": 1,
        "actualRxPin": 0,
        "captureBackend": "FLEXPWM",
        "captureEvidenceExpected": True,
        "captureEvidenceBytes": 300,
        "captureEvidenceRawEdges": 2400,
        "drivers": [
            {"driver": "PARLIO", "pinTx": 1, "laneSizes": [100], "laneCount": 1},
            {"driver": "RMT", "pinTx": 2, "laneSizes": [100], "laneCount": 1},
        ],
    }


def test_run_parallel_valid_pass_is_accepted() -> None:
    rc = _run_rpc([_run_parallel_command()], [_valid_parallel_pass()])
    assert rc == 0


def test_run_parallel_missing_test_counts_fails() -> None:
    response = _valid_parallel_pass()
    del response["totalTests"]

    rc = _run_rpc([_run_parallel_command()], [response])

    assert rc == 1


def test_run_parallel_pass_requires_capture_evidence() -> None:
    response = _valid_parallel_pass()
    response["captureEvidenceBytes"] = 0
    response["captureEvidenceRawEdges"] = 0

    rc = _run_rpc([_run_parallel_command()], [response])

    assert rc == 1


def test_failed_response_with_no_edges_is_zero_capture() -> None:
    failure_class, detail = _classify_test_failure(
        {
            "passed": False,
            "captureEvidenceBytes": 0,
            "captureEvidenceRawEdges": 0,
            "patterns": [
                {
                    "capturedBytes": 0,
                    "rawEdgesAfterWait": 0,
                    "mismatchedBytes": 300,
                    "captureFailed": True,
                }
            ],
        }
    )

    assert failure_class == "zero_capture"
    assert "no raw edges" in detail


def test_failed_response_with_edges_is_decode_mismatch() -> None:
    failure_class, detail = _classify_test_failure(
        {
            "passed": False,
            "captureEvidenceBytes": 32,
            "captureEvidenceRawEdges": 128,
            "patterns": [
                {
                    "capturedBytes": 32,
                    "rawEdgesAfterWait": 128,
                    "mismatchedBytes": 300,
                    "captureFailed": False,
                }
            ],
        }
    )

    assert failure_class == "decode_mismatch"
    assert "did not match" in detail


def test_driver_that_declined_is_not_a_capture_failure() -> None:
    """A backend that refused the chipset must not read as a wiring fault.

    Verbatim shape of a `bash autoresearch rp2350 --uart` response on the
    Pico 2 W: GPIO0 -> GPIO1 *is* UART0 TX/RX, so the route is wired, but
    the RP UART backend declines WS2812B-V5 because the bit period needs a
    baud above its maximum. It never transmitted, so of course RX saw
    nothing -- reporting `zero_capture` describes the consequence and hides
    the driver's own explanation. See FastLED#3899.
    """
    failure_class, detail = _classify_test_failure(
        {
            "passed": False,
            "rpUartStartAttempted": False,
            "rpUartStartSucceeded": False,
            "rpUartEncodedSize": 0,
            "rpUartActualBaud": 0,
            "rpUartLastError": (
                "RP UART: chipset timing needs a baud above the UART backend maximum"
            ),
            "captureEvidenceBytes": 0,
            "captureEvidenceRawEdges": 0,
            "patterns": [
                {
                    "capturedBytes": 0,
                    "rawEdgesAfterWait": 0,
                    "mismatchedBytes": 300,
                    "captureFailed": True,
                }
            ],
        }
    )

    assert failure_class == "driver_declined"
    # The driver's own words reach the operator, not a paraphrase.
    assert "baud above the UART backend maximum" in detail


def test_driver_that_did_transmit_still_classifies_on_capture() -> None:
    """`rpUartStartAttempted=True` must not divert a real capture failure.

    The new branch sits ahead of the capture evidence, so this pins that it
    only catches a genuine decline rather than swallowing every RP UART
    failure.
    """
    failure_class, _ = _classify_test_failure(
        {
            "passed": False,
            "rpUartStartAttempted": True,
            "rpUartStartSucceeded": True,
            "captureEvidenceBytes": 0,
            "captureEvidenceRawEdges": 0,
            "patterns": [
                {
                    "capturedBytes": 0,
                    "rawEdgesAfterWait": 0,
                    "mismatchedBytes": 300,
                    "captureFailed": True,
                }
            ],
        }
    )

    assert failure_class == "zero_capture"


def test_driver_decline_without_a_reason_still_says_it_declined() -> None:
    """Absent `rpUartLastError`, the class must still be the decline.

    Losing the reason is worse reporting; silently falling back to
    `zero_capture` would be a wrong finding.
    """
    failure_class, detail = _classify_test_failure(
        {
            "passed": False,
            "rpUartStartAttempted": False,
            "captureEvidenceBytes": 0,
            "captureEvidenceRawEdges": 0,
        }
    )

    assert failure_class == "driver_declined"
    assert "gave no reason" in detail
