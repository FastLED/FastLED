"""Host-side AutoResearch RPC acceptance validation tests."""

from __future__ import annotations

import asyncio
from pathlib import Path
from types import SimpleNamespace
from typing import Any
from unittest.mock import AsyncMock, MagicMock, patch

from ci.autoresearch.context import QuietContext, RunContext
from ci.autoresearch.phases import _run_tests_or_special_mode


_PATCH_MOD = "ci.autoresearch.phases"


def _make_ctx(commands: list[dict[str, Any]]) -> RunContext:
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
        wave2d_perf_grid=None,
        net_server_mode=False,
        net_client_mode=False,
        net_loopback_mode=False,
        ota_mode=False,
        ble_mode=False,
        decode_mode=False,
        gpio_only_mode=False,
        parallel_mode=False,
        final_environment="esp32s3",
        upload_port="COM5",
        use_fbuild=False,
        build_driver=None,
    )


def _response(data: Any) -> MagicMock:
    response = MagicMock()
    response.data = data
    return response


def _run_rpc(commands: list[dict[str, Any]], responses: list[Any]) -> int:
    ctx = _make_ctx(commands)
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


def test_run_single_response_driver_must_match_request() -> None:
    response = _valid_single_pass()
    response["driver"] = "RMT"

    rc = _run_rpc([_run_single_command()], [response])

    assert rc == 1


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
