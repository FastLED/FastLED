"""Unit tests for autoresearch phase functions.

Tests the refactored phase decomposition:
- _parse_args_and_build_commands (pure computation)
- _resolve_port_and_environment (mocked I/O)
- _run_build_deploy (mocked BuildDriver)
- _run_schema_and_pin_setup (mocked RPC)
- _run_tests_or_special_mode (mocked RPC)
"""

import asyncio
import os
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch

import pytest
from serial.tools.list_ports_common import ListPortInfo

from ci.autoresearch.args import Args
from ci.autoresearch.build_driver import BuildDriver
from ci.autoresearch.context import QuietContext, RunContext
from ci.autoresearch.phases import (
    _build_environment_for_mode,
    _is_valid_rp_concurrency_result,
    _parse_args_and_build_commands,
    _resolve_port_and_environment,
    _run_build_deploy,
    _run_rp_spi_loopback_tests,
    _run_rp_spi_public_api_tests,
    _run_schema_and_pin_setup,
    _run_tests_or_special_mode,
    _run_watchdog_soak,
    _validate_test_rpc_response,
    stop_autoresearch_watchdog,
)
from ci.rpc_client import RpcError, RpcTimeoutError
from ci.util.port_utils import ChipDetectionResult, auto_detect_upload_port


# Module path for patching symbols imported into phases.py
_PATCH_MOD = "ci.autoresearch.phases"


@pytest.mark.parametrize(
    "counters",
    [
        {},
        {"actual": None, "expected": None},
        {"actual": True, "expected": True},
        {"actual": 0, "expected": 0},
        {"actual": -1, "expected": -1},
        {"actual": 199, "expected": 200},
    ],
)
def test_rp_concurrency_gate_requires_matching_integer_counters(
    counters: dict[str, object],
) -> None:
    result: dict[str, object] = {
        "success": True,
        "supported": True,
        "core1Ready": True,
        "core1Done": True,
        "recursiveMutexReady": True,
        **counters,
    }
    assert _is_valid_rp_concurrency_result(result) is False


def test_rp_concurrency_gate_accepts_matching_integer_counters() -> None:
    assert _is_valid_rp_concurrency_result(
        {
            "success": True,
            "supported": True,
            "core1Ready": True,
            "core1Done": True,
            "recursiveMutexReady": True,
            "actual": 200,
            "expected": 200,
        }
    )


# ============================================================
# Test Factories
# ============================================================


def _make_args(**overrides) -> Args:
    """Create Args with sensible defaults for testing."""
    defaults = dict(
        environment_positional=None,
        parlio=True,
        rmt=False,
        spi=False,
        uart=False,
        lcd=False,
        lcd_spi=False,
        lcd_rgb=False,
        object_fled=False,
        flex_io=False,
        lpuart=False,
        all=False,
        simd=False,
        coroutine=False,
        ieee754=False,
        rpc_smoke=False,
        perf_wave2d=None,
        environment=None,
        verbose=False,
        skip_lint=True,
        upload_port=None,
        timeout="60",
        project_dir=Path("."),
        no_expect=False,
        no_fail_on=False,
        expect_keywords=None,
        fail_keywords=None,
        tx_pin=None,
        rx_pin=None,
        auto_discover_pins=False,
        contaminate_tx_mux=False,
        use_fbuild=False,
        no_fbuild=True,
        clean=False,
        skip_schema=True,
        quiet=False,
        strip_sizes=None,
        lanes=None,
        lane_counts=None,
        color_pattern=None,
        legacy=False,
        legacy_mixed_timings=False,
        legacy_rgbw_small_counts=False,
        chipset="ws2812",
        net_server=False,
        net_client=False,
        net=False,
        net_peer=False,
        peer_environment="esp32c6",
        peer_upload_port=None,
        ota=False,
        ble=False,
        parallel=False,
        decode=None,
        frames=None,
        tight_timing=False,
        tight_timing_iterations=8,
        tight_timing_max_overhead_us=2000,
        pin_toggle_rx=False,
        ws2812_loopback=False,
        pwm_dma_cl=False,
        dma_spi=False,
        dma_uart=False,
        rp_spi_loopback=False,
        rp_spi_index=0,
        rp_spi_public_api=False,
        rp_spi_chipset="apa102",
        rp_uart_index=0,
        rp_pio_index=1,
        rp_pio_both=False,
        test_fault_emit=False,
        # Default existing-test behavior: use the legacy root-platformio.ini
        # path so the ``fake_project_dir`` fixture's hand-written ini is the
        # one read. Tests that exercise the new synthesised-ini path (#3281)
        # override this explicitly.
        use_root_platformio_ini=True,
    )
    defaults.update(overrides)
    return Args(**defaults)


def _make_mock_driver(
    name: str = "fbuild",
    install_ok: bool = True,
    deploy_ok: bool = True,
) -> MagicMock:
    """Create a mock BuildDriver."""
    driver = MagicMock(spec=BuildDriver)
    driver.name = name
    driver.install_packages.return_value = install_ok
    driver.deploy.return_value = deploy_ok
    driver.firmware_path.return_value = Path("/fake/firmware.bin")
    return driver


def _make_ctx(**overrides) -> RunContext:
    """Create RunContext with sensible defaults for testing."""
    defaults = dict(
        args=_make_args(),
        drivers=["PARLIO"],
        json_rpc_commands=[
            {
                "method": "runSingleTest",
                "params": {
                    "driver": "PARLIO",
                    "laneSizes": [100],
                    "pattern": "MSB_LSB_A",
                    "iterations": 1,
                    "timing": "WS2812B-V5",
                },
            }
        ],
        expect_keywords=[],
        fail_keywords=["ERROR"],
        timeout_seconds=60.0,
        build_dir=Path("/fake/project"),
        simd_test_mode=False,
        coroutine_test_mode=False,
        ieee754_test_mode=False,
        rpc_smoke_mode=False,
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
    )
    defaults.update(overrides)
    # Auto-create a mock driver if not explicitly provided
    if defaults["build_driver"] is None:
        defaults["build_driver"] = _make_mock_driver()
    return RunContext(**defaults)


# ============================================================
# Temporary project dir fixture
# ============================================================


@pytest.fixture
def fake_project_dir(tmp_path: Path) -> Path:
    """Create a minimal PlatformIO project structure."""
    (tmp_path / "platformio.ini").write_text("[env:esp32s3]\n")
    (tmp_path / "examples" / "AutoResearch").mkdir(parents=True)
    return tmp_path


@pytest.fixture
def staged_project_dir(tmp_path: Path) -> Path:
    """Create a minimal staged fbuild project structure."""
    (tmp_path / "platformio.ini").write_text("[env:esp32s3]\n")
    (tmp_path / "src" / "sketch").mkdir(parents=True)
    return tmp_path


# ============================================================
# Tests: _parse_args_and_build_commands
# ============================================================


class TestParseArgsAndBuildCommands:
    """Test _parse_args_and_build_commands (pure computation, no mocks needed)."""

    def test_parlio_single_driver(self, fake_project_dir: Path) -> None:
        args = _make_args(parlio=True, project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["PARLIO"]
        assert len(result.json_rpc_commands) == 1
        assert result.json_rpc_commands[0]["method"] == "runSingleTest"

    def test_all_drivers(self, fake_project_dir: Path) -> None:
        args = _make_args(all=True, parlio=False, project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert set(result.drivers) == {
            "PARLIO",
            "RMT",
            "SPI",
            "UART",
            "LCD_CLOCKLESS",
            "LCD_SPI",
            "LCD_RGB",
            "OBJECT_FLED",
            "FLEX_IO",
        }

    def test_perf_wave2d_is_not_classified_as_gpio_only(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            parlio=False,
            perf_wave2d="32x32",
            project_dir=fake_project_dir,
        )
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.perf_wave2d_grid == (32, 32)
        assert result.gpio_only_mode is False

    def test_all_drivers_teensy4_only_real_teensy_drivers(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            all=True,
            parlio=False,
            environment_positional="teensy40",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["OBJECT_FLED", "FLEX_IO"]
        assert {cmd["method"] for cmd in result.json_rpc_commands} == {"runSingleTest"}
        assert not any("__skip_with_pass" in cmd for cmd in result.json_rpc_commands)
        assert all("pinTx" not in cmd["params"] for cmd in result.json_rpc_commands)

    def test_contaminate_tx_mux_sets_run_single_test_field(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            object_fled=True,
            parlio=False,
            contaminate_tx_mux=True,
            environment_positional="teensy41",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.json_rpc_commands[0]["method"] == "runSingleTest"
        assert result.json_rpc_commands[0]["params"]["contaminateTxMux"] is True

    def test_object_fled_legacy_one_strip_command(self, fake_project_dir: Path) -> None:
        args = _make_args(
            object_fled=True,
            parlio=False,
            legacy=True,
            strip_sizes="1",
            tx_pin=22,
            rx_pin=8,
            environment_positional="teensy41",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["OBJECT_FLED"]

        run_commands = [
            cmd for cmd in result.json_rpc_commands if cmd["method"] == "runSingleTest"
        ]
        assert len(run_commands) == 1
        params = run_commands[0]["params"]
        assert params["driver"] == "OBJECT_FLED"
        assert params["laneSizes"] == [1]
        assert params["useLegacyApi"] is True

    def test_object_fled_legacy_same_timing_multistrip_command(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            object_fled=True,
            parlio=False,
            legacy=True,
            lanes="2",
            strip_sizes="3",
            tx_pin=0,
            rx_pin=8,
            environment_positional="teensy41",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["OBJECT_FLED"]

        run_commands = [
            cmd for cmd in result.json_rpc_commands if cmd["method"] == "runSingleTest"
        ]
        assert len(run_commands) == 1
        params = run_commands[0]["params"]
        assert params["driver"] == "OBJECT_FLED"
        assert params["laneSizes"] == [3, 3]
        assert params["useLegacyApi"] is True
        assert "legacyChipsets" not in params

    def test_object_fled_legacy_mixed_timing_multistrip_command(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            object_fled=True,
            parlio=False,
            legacy=True,
            legacy_mixed_timings=True,
            lanes="2",
            strip_sizes="3",
            tx_pin=0,
            rx_pin=8,
            environment_positional="teensy41",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["OBJECT_FLED"]

        run_commands = [
            cmd for cmd in result.json_rpc_commands if cmd["method"] == "runSingleTest"
        ]
        assert len(run_commands) == 1
        params = run_commands[0]["params"]
        assert params["driver"] == "OBJECT_FLED"
        assert params["laneSizes"] == [3, 3]
        assert params["useLegacyApi"] is True
        assert params["legacyChipsets"] == ["WS2812B", "SK6812"]

    def test_object_fled_legacy_rgbw_small_counts_commands(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            object_fled=True,
            parlio=False,
            legacy=True,
            legacy_rgbw_small_counts=True,
            tx_pin=22,
            rx_pin=8,
            environment_positional="teensy41",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["OBJECT_FLED"]

        run_commands = [
            cmd for cmd in result.json_rpc_commands if cmd["method"] == "runSingleTest"
        ]
        assert len(run_commands) == 4
        assert [cmd["params"]["laneSizes"] for cmd in run_commands] == [
            [1],
            [2],
            [3],
            [4],
        ]
        for cmd in run_commands:
            params = cmd["params"]
            assert params["driver"] == "OBJECT_FLED"
            assert params["useLegacyApi"] is True
            assert params["legacyRgbw"] is True
            assert "legacyChipsets" not in params

    def test_ws2814_chipset_requires_legacy(self, fake_project_dir: Path) -> None:
        args = _make_args(
            parlio=False,
            rmt=True,
            chipset="ws2814",
            project_dir=fake_project_dir,
        )
        assert _parse_args_and_build_commands(args) == 1

    def test_ws2814_rejects_legacy_parallel(self, fake_project_dir: Path) -> None:
        args = _make_args(
            parlio=True,
            rmt=True,
            legacy=True,
            chipset="ws2814",
            parallel=True,
            project_dir=fake_project_dir,
        )
        assert _parse_args_and_build_commands(args) == 1

    def test_ws2814_rejects_legacy_rgbw_override(self, fake_project_dir: Path) -> None:
        args = _make_args(
            parlio=False,
            rmt=True,
            legacy=True,
            legacy_rgbw_small_counts=True,
            chipset="ws2814",
            project_dir=fake_project_dir,
        )
        assert _parse_args_and_build_commands(args) == 1

    def test_ws2814_rejects_root_platformio_ini(self, fake_project_dir: Path) -> None:
        args = _make_args(
            parlio=False,
            rmt=True,
            legacy=True,
            chipset="ws2814",
            environment_positional="esp32c6",
            project_dir=fake_project_dir,
            use_root_platformio_ini=True,
        )
        assert _parse_args_and_build_commands(args) == 1

    def test_ws2814_esp32c6_rmt_legacy_canonical_command(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            parlio=False,
            rmt=True,
            legacy=True,
            chipset="ws2814",
            strip_sizes="1,2,3,4",
            environment_positional="esp32c6",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ) as mock_synth:
            result = _parse_args_and_build_commands(args)

        assert isinstance(result, RunContext)
        mock_synth.assert_called_once_with(
            "esp32c6",
            project_root=fake_project_dir.resolve(),
            verbose=False,
            extra_defines=["FL_ESP32_LEGACY_CLOCKLESS_USE_RMT=1"],
        )
        assert result.drivers == ["RMT"]
        assert len(result.json_rpc_commands) == 4
        for strip_size, command in zip(
            [1, 2, 3, 4], result.json_rpc_commands, strict=True
        ):
            assert command["method"] == "runSingleTest"
            assert command["params"] == {
                "driver": "RMT",
                "laneSizes": [strip_size],
                "pattern": "MSB_LSB_A",
                "iterations": 1,
                "timing": "WS2814",
                "useLegacyApi": True,
                "legacyChipsets": ["WS2814"],
            }
            assert "legacyRgbw" not in command["params"]

    def test_ws2814_legacy_chipset_repeats_for_each_lane(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            parlio=False,
            rmt=True,
            legacy=True,
            chipset="ws2814",
            lanes="3",
            tx_pin=0,
            environment_positional="esp32c6",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)

        assert isinstance(result, RunContext)
        params = result.json_rpc_commands[0]["params"]
        assert params["laneSizes"] == [100, 100, 100]
        assert params["legacyChipsets"] == ["WS2814", "WS2814", "WS2814"]
        assert "legacyRgbw" not in params

    def test_ws2818_esp32s3_rmt_legacy_canonical_command(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            parlio=False,
            rmt=True,
            legacy=True,
            chipset="ws2818",
            strip_sizes="1,4",
            environment_positional="esp32s3",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ) as mock_synth:
            result = _parse_args_and_build_commands(args)

        assert isinstance(result, RunContext)
        mock_synth.assert_called_once_with(
            "esp32s3",
            project_root=fake_project_dir.resolve(),
            verbose=False,
            extra_defines=["FL_ESP32_LEGACY_CLOCKLESS_USE_RMT=1"],
        )
        assert result.drivers == ["RMT"]
        assert [command["params"] for command in result.json_rpc_commands] == [
            {
                "driver": "RMT",
                "laneSizes": [strip_size],
                "pattern": "MSB_LSB_A",
                "iterations": 1,
                "timing": "WS2818",
                "useLegacyApi": True,
                "legacyChipsets": ["WS2818"],
            }
            for strip_size in (1, 4)
        ]

    def test_ws2814_failure_response_requires_four_byte_total(self) -> None:
        command = {
            "method": "runSingleTest",
            "params": {
                "driver": "RMT",
                "laneSizes": [2],
                "useLegacyApi": True,
                "legacyChipsets": ["WS2814"],
            },
        }
        response = {
            "success": True,
            "passed": False,
            "totalTests": 4,
            "passedTests": 0,
            "driver": "RMT",
            "patterns": [
                {
                    "totalLeds": 2,
                    "totalBytes": 8,
                    "capturedBytes": 0,
                }
            ],
        }
        assert (
            _validate_test_rpc_response("runSingleTest", command, response, None, None)
            == []
        )

        del response["patterns"][0]["totalBytes"]
        assert _validate_test_rpc_response(
            "runSingleTest", command, response, None, None
        ) == ["patterns[0] missing integer totalBytes for WS2814"]

        response["patterns"] = []
        assert _validate_test_rpc_response(
            "runSingleTest", command, response, None, None
        ) == ["WS2814 response requires a non-empty patterns list"]

        response["patterns"] = ["malformed"]
        assert _validate_test_rpc_response(
            "runSingleTest", command, response, None, None
        ) == ["patterns[0] must be an object for WS2814"]

        response["patterns"] = [{"totalBytes": 8}]
        assert _validate_test_rpc_response(
            "runSingleTest", command, response, None, None
        ) == ["patterns[0] missing integer totalLeds for WS2814"]

    @pytest.mark.parametrize(
        ("field", "value", "expected"),
        (
            ("rpUartStartAttempted", "yes", "rpUartStartAttempted must be boolean"),
            ("rpUartStartSucceeded", 1, "rpUartStartSucceeded must be boolean"),
            (
                "rpUartEncodedSize",
                -1,
                "rpUartEncodedSize must be a nonnegative integer",
            ),
            (
                "rpUartActualBaud",
                True,
                "rpUartActualBaud must be a nonnegative integer",
            ),
            ("rpUartLastError", None, "rpUartLastError must be a string"),
        ),
    )
    def test_rp_uart_diagnostics_require_typed_contract(
        self, field: str, value: object, expected: str
    ) -> None:
        command = {
            "method": "runSingleTest",
            "params": {"driver": "UART", "laneSizes": [1]},
        }
        response = {
            "success": True,
            "passed": False,
            "totalTests": 1,
            "passedTests": 0,
            "driver": "UART",
            "rpUartStartAttempted": True,
            "rpUartStartSucceeded": False,
            "rpUartEncodedSize": 0,
            "rpUartActualBaud": 0,
            "rpUartLastError": "RP UART: peripheral configure failed",
        }
        response[field] = value

        errors = _validate_test_rpc_response(
            "runSingleTest", command, response, None, None
        )

        assert expected in errors

    def test_rp_uart_response_requires_complete_diagnostic_bundle(self) -> None:
        command = {
            "method": "runSingleTest",
            "params": {"driver": "UART0", "laneSizes": [1]},
        }
        response = {
            "success": True,
            "passed": False,
            "totalTests": 1,
            "passedTests": 0,
            "driver": "UART0",
        }

        errors = _validate_test_rpc_response(
            "runSingleTest", command, response, None, None
        )

        assert errors == [
            "rpUartStartAttempted must be boolean",
            "rpUartStartSucceeded must be boolean",
            "rpUartEncodedSize must be a nonnegative integer",
            "rpUartActualBaud must be a nonnegative integer",
            "rpUartLastError must be a string",
        ]

    def test_legacy_mixed_timings_requires_legacy(self, fake_project_dir: Path) -> None:
        args = _make_args(
            object_fled=True,
            parlio=False,
            legacy=False,
            legacy_mixed_timings=True,
            lanes="2",
            strip_sizes="3",
            tx_pin=0,
            rx_pin=8,
            environment_positional="teensy41",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        result = _parse_args_and_build_commands(args)
        assert result == 1

    def test_legacy_mixed_timings_requires_multiple_lanes(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            object_fled=True,
            parlio=False,
            legacy=True,
            legacy_mixed_timings=True,
            lanes="1",
            strip_sizes="3",
            tx_pin=0,
            rx_pin=8,
            environment_positional="teensy41",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        result = _parse_args_and_build_commands(args)
        assert result == 1

    def test_legacy_rgbw_small_counts_requires_legacy(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            object_fled=True,
            parlio=False,
            legacy=False,
            legacy_rgbw_small_counts=True,
            tx_pin=22,
            rx_pin=8,
            environment_positional="teensy41",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        result = _parse_args_and_build_commands(args)
        assert result == 1

    def test_legacy_rgbw_small_counts_requires_single_lane(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            object_fled=True,
            parlio=False,
            legacy=True,
            legacy_rgbw_small_counts=True,
            lanes="2",
            tx_pin=0,
            rx_pin=8,
            environment_positional="teensy41",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        result = _parse_args_and_build_commands(args)
        assert result == 1

    def test_legacy_rgbw_small_counts_rejects_strip_size_override(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            object_fled=True,
            parlio=False,
            legacy=True,
            legacy_rgbw_small_counts=True,
            strip_sizes="3",
            tx_pin=22,
            rx_pin=8,
            environment_positional="teensy41",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        result = _parse_args_and_build_commands(args)
        assert result == 1

    def test_object_fled_legacy_current_pin_rejects_multistrip(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            object_fled=True,
            parlio=False,
            legacy=True,
            lanes="2",
            strip_sizes="3",
            tx_pin=22,
            rx_pin=8,
            environment_positional="teensy41",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        result = _parse_args_and_build_commands(args)
        assert result == 1

    def test_spi_driver_name_remains_spi_on_esp32(self, fake_project_dir: Path) -> None:
        args = _make_args(parlio=False, spi=True, project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["SPI"]
        assert result.json_rpc_commands[0]["params"]["driver"] == "SPI"

    def test_spi_driver_name_maps_to_spi_unified_on_teensy4(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            parlio=False,
            spi=True,
            environment_positional="teensy41",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["SPI_UNIFIED"]
        assert result.json_rpc_commands[0]["params"]["driver"] == "SPI_UNIFIED"

    def test_flex_io_does_not_hide_tx_pin_override(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            parlio=False,
            flex_io=True,
            environment_positional="teensy41",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["FLEX_IO"]
        command = result.json_rpc_commands[0]
        assert command["method"] == "runSingleTest"
        assert command["params"]["driver"] == "FLEX_IO"
        assert "pinTx" not in command["params"]

    @pytest.mark.parametrize(
        "environment",
        (
            "rp2040",
            "rpipico",
            "rpipicow",
            "rp2350",
            "rpipico2",
            "rp2350w",
            "rpipico2w",
        ),
    )
    def test_flex_io_driver_name_maps_to_pio1_on_rp2xxx_aliases(
        self, fake_project_dir: Path, environment: str
    ) -> None:
        args = _make_args(
            parlio=False,
            flex_io=True,
            environment_positional=environment,
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["PIO1"]
        assert result.json_rpc_commands[0]["params"]["driver"] == "PIO1"

    def test_flex_io_selects_pio0_on_rp2040(self, fake_project_dir: Path) -> None:
        args = _make_args(
            parlio=False,
            flex_io=True,
            rp_pio_index=0,
            environment_positional="rp2040",
            project_dir=fake_project_dir,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["PIO0"]
        assert result.json_rpc_commands[0]["params"]["driver"] == "PIO0"

    def test_flex_io_selects_pio2_on_rp2350(self, fake_project_dir: Path) -> None:
        args = _make_args(
            parlio=False,
            flex_io=True,
            rp_pio_index=2,
            environment_positional="rp2350w",
            project_dir=fake_project_dir,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["PIO2"]
        assert result.json_rpc_commands[0]["params"]["driver"] == "PIO2"

    @pytest.mark.parametrize(
        ("environment", "uart_index", "driver"),
        (
            ("rp2040", 0, "UART0"),
            ("rpipico", 1, "UART1"),
            ("rp2350", 0, "UART0"),
            ("rp2350w", 1, "UART1"),
            ("rpipico2w", 0, "UART0"),
        ),
    )
    def test_uart_selects_concrete_rp_uart_driver(
        self,
        fake_project_dir: Path,
        environment: str,
        uart_index: int,
        driver: str,
    ) -> None:
        args = _make_args(
            parlio=False,
            uart=True,
            rp_uart_index=uart_index,
            environment_positional=environment,
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == [driver]
        assert result.json_rpc_commands[0]["params"]["driver"] == driver

    def test_flex_io_rejects_pio2_on_rp2040(self, fake_project_dir: Path) -> None:
        args = _make_args(
            parlio=False,
            flex_io=True,
            rp_pio_index=2,
            environment_positional="rp2040",
            project_dir=fake_project_dir,
        )
        assert _parse_args_and_build_commands(args) == 1

    @pytest.mark.parametrize(
        "environment",
        (
            "rp2040",
            "rpipico",
            "rpipicow",
            "rp2350",
            "rpipico2",
            "rp2350w",
            "rpipico2w",
        ),
    )
    def test_rp_pio_both_generates_one_parallel_command_for_rp2xxx_aliases(
        self, fake_project_dir: Path, environment: str
    ) -> None:
        args = _make_args(
            parlio=False,
            flex_io=True,
            rp_pio_both=True,
            parallel=True,
            environment_positional=environment,
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["PIO0", "PIO1"]
        assert result.json_rpc_commands[0]["method"] == "runParallelTest"

    def test_rp_pio_parallel_response_uses_resource_evidence(self) -> None:
        command = {
            "method": "runParallelTest",
            "params": {
                "drivers": [
                    {"driver": "PIO0", "laneSizes": [100]},
                    {"driver": "PIO1", "laneSizes": [100]},
                ]
            },
        }
        response = {
            "success": True,
            "passed": True,
            "totalTests": 1,
            "passedTests": 1,
            "drivers": [
                {"driver": "PIO0", "pinTx": 11},
                {"driver": "PIO1", "pinTx": 12},
            ],
            "captureBackend": "PIO_RX",
            "captureEvidenceBytes": 0,
            "captureEvidenceRawEdges": 0,
            "requestedTxPin": 11,
            "requestedRxPin": 8,
            "actualTxPin": 11,
            "actualRxPin": 8,
            "concurrent_resource_test": True,
            "show_success": True,
        }
        assert (
            _validate_test_rpc_response("runParallelTest", command, response, 11, 8)
            == []
        )

    def test_lpuart_is_deprecated_alias_for_uart(
        self, fake_project_dir: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        args = _make_args(
            parlio=False,
            lpuart=True,
            project_dir=fake_project_dir,
        )
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["UART"]
        assert result.json_rpc_commands[0]["params"]["driver"] == "UART"
        assert args.uart is True
        assert args.lpuart is False
        assert "--lpuart is deprecated; use --uart" in capsys.readouterr().err

    def test_flexio_alias_defaults_to_zero_when_present(
        self, capsys: pytest.CaptureFixture[str]
    ) -> None:
        args = Args.parse_args(["--flexio"])
        assert args.flex_io is False
        assert "omit it or use --flex-io" in capsys.readouterr().err

    def test_flexio_alias_one_enables_flex_io(
        self, capsys: pytest.CaptureFixture[str]
    ) -> None:
        args = Args.parse_args(["--flexio", "1"])
        assert args.flex_io is True
        assert "--flexio is deprecated; use --flex-io" in capsys.readouterr().err

    def test_flexio_alias_zero_is_noop(
        self, capsys: pytest.CaptureFixture[str]
    ) -> None:
        args = Args.parse_args(["--flexio", "0"])
        assert args.flex_io is False
        assert "omit it or use --flex-io" in capsys.readouterr().err

    def test_multiple_drivers(self, fake_project_dir: Path) -> None:
        args = _make_args(parlio=True, rmt=True, spi=True, project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.drivers == ["PARLIO", "RMT", "SPI"]
        # 3 drivers x 1 lane x 1 strip size = 3 commands
        assert len(result.json_rpc_commands) == 3

    def test_decode_mode_sets_flag(self, fake_project_dir: Path) -> None:
        args = _make_args(parlio=False, decode="test.bin", project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.decode_mode is True

    def test_decode_with_driver_returns_error(self, fake_project_dir: Path) -> None:
        args = _make_args(parlio=True, decode="test.bin", project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, int)
        assert result == 1

    def test_parallel_needs_two_drivers(self, fake_project_dir: Path) -> None:
        args = _make_args(
            parlio=True, rmt=False, parallel=True, project_dir=fake_project_dir
        )
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, int)
        assert result == 1

    def test_parallel_with_two_drivers(self, fake_project_dir: Path) -> None:
        args = _make_args(
            parlio=True, rmt=True, parallel=True, project_dir=fake_project_dir
        )
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.parallel_mode is True
        assert result.json_rpc_commands[0]["method"] == "runParallelTest"

    def test_net_and_driver_mutual_exclusivity(self, fake_project_dir: Path) -> None:
        args = _make_args(parlio=True, net_server=True, project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, int)
        assert result == 1

    def test_net_peer_requires_explicit_rp2350w_and_c6_ports(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            parlio=False,
            net_peer=True,
            environment_positional="rp2350w",
            upload_port="COM17",
            peer_upload_port="COM9",
            project_dir=fake_project_dir,
            use_root_platformio_ini=True,
        )
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.net_peer_mode is True
        assert result.gpio_only_mode is False

    def test_net_peer_rejects_missing_companion_port(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            parlio=False,
            net_peer=True,
            environment_positional="rp2350w",
            upload_port="COM17",
            project_dir=fake_project_dir,
        )
        assert _parse_args_and_build_commands(args) == 1

    def test_net_peer_rejects_wrong_primary_environment(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            parlio=False,
            net_peer=True,
            environment_positional="esp32c6",
            upload_port="COM17",
            peer_upload_port="COM9",
            project_dir=fake_project_dir,
        )
        assert _parse_args_and_build_commands(args) == 1

    def test_lane_range_parsing(self, fake_project_dir: Path) -> None:
        args = _make_args(lanes="1-4", project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        # 1 driver x 4 lanes x 1 strip size = 4 commands
        assert len(result.json_rpc_commands) == 4

    def test_invalid_lane_format(self, fake_project_dir: Path) -> None:
        args = _make_args(lanes="abc", project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, int)
        assert result == 1

    def test_gpio_only_mode(self, fake_project_dir: Path) -> None:
        args = _make_args(parlio=False, project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.gpio_only_mode is True
        assert result.drivers == []

    def test_rp_spi_loopback_is_not_gpio_only(self, fake_project_dir: Path) -> None:
        args = _make_args(
            parlio=False,
            rp_spi_loopback=True,
            environment_positional="rp2040",
            project_dir=fake_project_dir,
            use_root_platformio_ini=False,
        )
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_project_dir,
        ):
            result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.gpio_only_mode is False
        assert result.drivers == []

    def test_rp_spi_loopback_flag_parses(self) -> None:
        args = Args.parse_args(["--rp-spi-loopback", "--rp-spi-index", "1"])
        assert args.rp_spi_loopback is True
        assert args.rp_spi_index == 1

    def test_rp_uart_index_flag_parses(self) -> None:
        args = Args.parse_args(["--uart", "--rp-uart-index", "1"])
        assert args.uart is True
        assert args.rp_uart_index == 1

    def test_simd_mode(self, fake_project_dir: Path) -> None:
        args = _make_args(parlio=False, simd=True, project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.simd_test_mode is True

    def test_strip_sizes_preset(self, fake_project_dir: Path) -> None:
        args = _make_args(strip_sizes="tiny", project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        # tiny = [10, 100], so 2 strip sizes x 1 driver x 1 lane = 2 commands
        assert len(result.json_rpc_commands) == 2

    def test_strip_sizes_custom(self, fake_project_dir: Path) -> None:
        args = _make_args(strip_sizes="50,200,500", project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert len(result.json_rpc_commands) == 3

    def test_lane_counts(self, fake_project_dir: Path) -> None:
        args = _make_args(lane_counts="100,200,300", project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        # setLaneSizes + runSingleTest = 2 commands
        assert len(result.json_rpc_commands) == 2
        assert result.json_rpc_commands[0]["method"] == "setLaneSizes"

    def test_lane_counts_accepts_16_lanes(self, fake_project_dir: Path) -> None:
        args = _make_args(
            lane_counts=",".join(["100"] * 16),
            project_dir=fake_project_dir,
        )
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.json_rpc_commands[0]["method"] == "setLaneSizes"
        assert result.json_rpc_commands[0]["params"] == [[100] * 16]

    def test_lane_counts_rejects_17_lanes(self, fake_project_dir: Path) -> None:
        args = _make_args(
            lane_counts=",".join(["100"] * 17),
            project_dir=fake_project_dir,
        )
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, int)
        assert result == 1

    def test_lanes_rejects_above_16(self, fake_project_dir: Path) -> None:
        args = _make_args(lanes="17", project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, int)
        assert result == 1

    def test_color_pattern(self, fake_project_dir: Path) -> None:
        args = _make_args(color_pattern="ff00aa", project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        # setSolidColor + runSingleTest = 2 commands
        assert len(result.json_rpc_commands) == 2
        assert result.json_rpc_commands[0]["method"] == "setSolidColor"

    def test_invalid_color_pattern(self, fake_project_dir: Path) -> None:
        args = _make_args(color_pattern="xyz", project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, int)
        assert result == 1

    def test_missing_platformio_ini(self, tmp_path: Path) -> None:
        # No platformio.ini in tmp_path
        args = _make_args(project_dir=tmp_path)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, int)
        assert result == 1

    def test_staged_project_dir_uses_src_sketch(
        self, staged_project_dir: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        monkeypatch.delenv("PLATFORMIO_SRC_DIR", raising=False)
        args = _make_args(project_dir=staged_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.build_dir == staged_project_dir
        assert os.environ["PLATFORMIO_SRC_DIR"] == str(
            staged_project_dir / "src" / "sketch"
        )

    def test_lcd_forces_esp32s3(self, fake_project_dir: Path) -> None:
        args = _make_args(parlio=False, lcd=True, project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.final_environment == "esp32s3"
        assert "LCD_CLOCKLESS" in result.drivers

    # ============================================================
    # #3281: synthesised .build/pio/<board>/platformio.ini path
    # ============================================================

    def test_synthesised_path_calls_staging_when_board_known(
        self, tmp_path: Path
    ) -> None:
        """When the board is known up-front and the legacy flag is OFF,
        ``_parse_args_and_build_commands`` should synthesise the staged
        ``.build/pio/<board>/`` project and use it as ``build_dir`` — NO
        root ``./platformio.ini`` is required."""
        # Note: NO platformio.ini in tmp_path on purpose. The synthesis path
        # must NOT require one to exist.
        (tmp_path / "examples" / "AutoResearch").mkdir(parents=True)

        fake_build_dir = tmp_path / ".build" / "pio" / "esp32s3"
        fake_build_dir.mkdir(parents=True)

        args = _make_args(
            environment_positional="esp32s3",
            project_dir=tmp_path,
            use_root_platformio_ini=False,
        )

        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_build_dir,
        ) as mock_synth:
            result = _parse_args_and_build_commands(args)

        assert isinstance(result, RunContext), result
        mock_synth.assert_called_once_with(
            "esp32s3",
            project_root=tmp_path.resolve(),
            verbose=False,
            extra_defines=[],
        )
        assert result.build_dir == fake_build_dir

    @pytest.mark.parametrize(
        ("requested_environment", "canonical_environment"),
        (
            ("rpipico", "rp2040"),
            ("rpipico2", "rp2350"),
            ("rpipico2w", "rp2350w"),
        ),
    )
    def test_rp_board_alias_stages_the_canonical_environment(
        self,
        tmp_path: Path,
        requested_environment: str,
        canonical_environment: str,
    ) -> None:
        (tmp_path / "examples" / "AutoResearch").mkdir(parents=True)
        fake_build_dir = tmp_path / ".build" / "pio" / canonical_environment
        fake_build_dir.mkdir(parents=True)
        args = _make_args(
            environment_positional=requested_environment,
            project_dir=tmp_path,
            use_root_platformio_ini=False,
        )

        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=fake_build_dir,
        ) as mock_synth:
            result = _parse_args_and_build_commands(args)

        assert isinstance(result, RunContext)
        assert result.final_environment == canonical_environment
        mock_synth.assert_called_once_with(
            canonical_environment,
            project_root=tmp_path.resolve(),
            verbose=False,
            extra_defines=[],
        )

    def test_synthesised_path_defers_when_board_unknown(self, tmp_path: Path) -> None:
        """When the board is NOT known up-front (no positional, no --env,
        no --lcd*) and the legacy flag is OFF, parse-time synthesis must NOT
        happen — synthesis is deferred to ``_resolve_port_and_environment``
        after chip auto-detect."""
        # No platformio.ini in tmp_path — synthesised path must tolerate that.
        (tmp_path / "examples" / "AutoResearch").mkdir(parents=True)

        args = _make_args(
            environment_positional=None,
            project_dir=tmp_path,
            use_root_platformio_ini=False,
        )

        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project"
        ) as mock_synth:
            result = _parse_args_and_build_commands(args)

        assert isinstance(result, RunContext), result
        mock_synth.assert_not_called()
        # build_dir falls back to project_root so the sketch resolver still
        # finds examples/AutoResearch/.
        assert result.build_dir == tmp_path.resolve()
        assert result.final_environment is None

    def test_legacy_flag_still_requires_root_platformio_ini(
        self, tmp_path: Path
    ) -> None:
        """With ``--use-root-platformio-ini`` set, the legacy
        ``platformio.ini`` existence check still fires and a missing file is
        an error — proving the escape hatch keeps the old behavior."""
        # No platformio.ini in tmp_path.
        args = _make_args(
            project_dir=tmp_path,
            use_root_platformio_ini=True,
        )
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, int)
        assert result == 1

    def test_teensy_root_platformio_ini_rejected_up_front(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            environment_positional="teensy41",
            object_fled=True,
            parlio=False,
            project_dir=fake_project_dir,
            use_root_platformio_ini=True,
        )
        result = _parse_args_and_build_commands(args)
        assert result == 1

    def test_teensy_specific_driver_rejects_root_platformio_without_env(
        self, fake_project_dir: Path
    ) -> None:
        args = _make_args(
            object_fled=True,
            parlio=False,
            project_dir=fake_project_dir,
            use_root_platformio_ini=True,
        )
        result = _parse_args_and_build_commands(args)
        assert result == 1

    def test_timeout_parsing(self, fake_project_dir: Path) -> None:
        args = _make_args(timeout="2m", project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.timeout_seconds == 120.0

    def test_ble_and_ota_mutual_exclusivity(self, fake_project_dir: Path) -> None:
        args = _make_args(
            parlio=False, ble=True, ota=True, project_dir=fake_project_dir
        )
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, int)
        assert result == 1


# ============================================================
# Tests: _resolve_port_and_environment
# ============================================================


class TestResolvePortAndEnvironment:
    """Test _resolve_port_and_environment (mocks port detection)."""

    def test_auto_detected_rp_normalizes_uart_driver_name(self, tmp_path: Path) -> None:
        (tmp_path / "examples" / "AutoResearch").mkdir(parents=True)
        staged_dir = tmp_path / ".build" / "pio" / "rp2350w"
        staged_dir.mkdir(parents=True)
        args = _make_args(
            upload_port=None,
            environment_positional=None,
            parlio=False,
            uart=True,
            rp_uart_index=1,
            project_dir=tmp_path,
            use_root_platformio_ini=False,
        )
        ctx = _parse_args_and_build_commands(args)
        assert isinstance(ctx, RunContext)
        assert ctx.drivers == ["UART"]
        assert ctx.json_rpc_commands[0]["params"]["driver"] == "UART"

        port_result = MagicMock(ok=True, selected_port="COM17")
        chip_result = MagicMock(
            ok=True,
            chip_type="RP2350",
            environment="rp2350w",
        )
        with (
            patch(
                f"{_PATCH_MOD}.auto_detect_upload_port",
                return_value=port_result,
            ),
            patch(
                f"{_PATCH_MOD}.detect_attached_chip",
                return_value=chip_result,
            ),
            patch(
                "ci.autoresearch.staging.synthesise_autoresearch_project",
                return_value=staged_dir,
            ),
            patch(
                f"{_PATCH_MOD}.select_build_driver",
                return_value=_make_mock_driver(),
            ),
        ):
            rc = asyncio.run(_resolve_port_and_environment(ctx))

        assert rc is None
        assert ctx.final_environment == "rp2350w"
        assert ctx.drivers == ["UART1"]
        assert ctx.json_rpc_commands[0]["params"]["driver"] == "UART1"

    def test_auto_detect_port_success(self) -> None:
        ctx = _make_ctx(upload_port=None)
        ctx.args = _make_args(upload_port=None)
        mock_result = MagicMock(ok=True, selected_port="COM5")
        with (
            patch(
                f"{_PATCH_MOD}.auto_detect_upload_port", return_value=mock_result
            ) as auto_detect,
            patch(
                f"{_PATCH_MOD}.select_build_driver", return_value=_make_mock_driver()
            ),
            patch(
                "ci.util.pio_package_daemon.get_default_environment",
                return_value=None,
            ),
        ):
            rc = asyncio.run(_resolve_port_and_environment(ctx))
        assert rc is None
        assert ctx.upload_port == "COM5"
        assert ctx.final_environment == "esp32s3"
        auto_detect.assert_called_once_with(expected_environment="esp32s3")

    def test_auto_detect_port_uses_requested_environment(self) -> None:
        ctx = _make_ctx(upload_port=None, final_environment="esp32c6")
        ctx.args = _make_args(upload_port=None, environment="esp32c6")
        mock_result = MagicMock(ok=True, selected_port="COM9")
        with (
            patch(
                f"{_PATCH_MOD}.auto_detect_upload_port", return_value=mock_result
            ) as auto_detect,
            patch(
                f"{_PATCH_MOD}.select_build_driver", return_value=_make_mock_driver()
            ),
            patch(
                "ci.util.pio_package_daemon.get_default_environment",
                return_value=None,
            ),
        ):
            rc = asyncio.run(_resolve_port_and_environment(ctx))
        assert rc is None
        assert ctx.upload_port == "COM9"
        auto_detect.assert_called_once_with(expected_environment="esp32c6")

    # Every environment in RP2XXX_ENVIRONMENTS, both families. The gate is
    # `bool(_active_rp2xxx_environment(...))`, which spans
    # RP2040_ENVIRONMENTS | RP2350_ENVIRONMENTS, so covering only the RP2350
    # half would leave the rp2040 family asserting nothing.
    @pytest.mark.parametrize(
        "environment",
        (
            "rp2040",
            "rpipico",
            "rpipicow",
            "rp2350",
            "rpipico2",
            "rp2350w",
            "rpipico2w",
        ),
    )
    def test_rp2xxx_driver_mode_delegates_port_selection_to_fbuild(
        self: "TestResolvePortAndEnvironment", environment: str
    ) -> None:
        args = _make_args(
            environment=environment,
            upload_port=None,
            parlio=True,
            rpc_smoke=False,
        )
        ctx = _make_ctx(
            args=args,
            final_environment=environment,
            upload_port=None,
            rpc_smoke_mode=False,
        )

        with (
            patch(
                f"{_PATCH_MOD}.auto_detect_upload_port",
                side_effect=AssertionError("RP port selection belongs to fbuild"),
            ) as auto_detect,
            patch(
                f"{_PATCH_MOD}.select_build_driver",
                return_value=_make_mock_driver(),
            ),
        ):
            rc = asyncio.run(_resolve_port_and_environment(ctx))

        assert rc is None
        assert ctx.upload_port is None
        auto_detect.assert_not_called()

    def test_ws2814_deferred_synthesis_injects_rmt_binding(
        self, tmp_path: Path
    ) -> None:
        (tmp_path / "examples" / "AutoResearch").mkdir(parents=True)
        staged_dir = tmp_path / ".build" / "pio" / "esp32c6"
        staged_dir.mkdir(parents=True)
        args = _make_args(
            upload_port=None,
            environment_positional=None,
            parlio=False,
            rmt=True,
            legacy=True,
            chipset="ws2814",
            project_dir=tmp_path,
            use_root_platformio_ini=False,
        )
        ctx = _make_ctx(
            args=args,
            upload_port=None,
            final_environment=None,
            build_dir=tmp_path.resolve(),
        )
        port_result = MagicMock(ok=True, selected_port="COM9")
        chip_result = MagicMock(
            ok=True,
            chip_type="ESP32-C6",
            environment="esp32c6",
        )
        with (
            patch(
                f"{_PATCH_MOD}.auto_detect_upload_port",
                return_value=port_result,
            ),
            patch(
                f"{_PATCH_MOD}.detect_attached_chip",
                return_value=chip_result,
            ),
            patch(
                "ci.autoresearch.staging.synthesise_autoresearch_project",
                return_value=staged_dir,
            ) as synth,
            patch(
                f"{_PATCH_MOD}.select_build_driver",
                return_value=_make_mock_driver(),
            ),
        ):
            rc = asyncio.run(_resolve_port_and_environment(ctx))

        assert rc is None
        assert ctx.final_environment == "esp32c6"
        assert ctx.build_dir == staged_dir
        synth.assert_called_once_with(
            "esp32c6",
            project_root=tmp_path.resolve(),
            verbose=False,
            extra_defines=["FL_ESP32_LEGACY_CLOCKLESS_USE_RMT=1"],
        )

    def test_teensy_auto_detect_rejects_root_platformio_ini(self) -> None:
        ctx = _make_ctx(upload_port=None, final_environment=None)
        ctx.args = _make_args(
            upload_port=None,
            parlio=False,
            all=True,
            use_root_platformio_ini=True,
        )
        mock_port_result = MagicMock(ok=True, selected_port="COM8")
        mock_chip_result = MagicMock(
            ok=True, chip_type="Teensy 4.1", environment="teensy41"
        )
        with (
            patch(
                f"{_PATCH_MOD}.auto_detect_upload_port",
                return_value=mock_port_result,
            ),
            patch(
                f"{_PATCH_MOD}.detect_attached_chip",
                return_value=mock_chip_result,
            ),
        ):
            rc = asyncio.run(_resolve_port_and_environment(ctx))
        assert rc == 1
        assert ctx.final_environment == "teensy41"

    def test_cli_upload_port(self) -> None:
        ctx = _make_ctx(upload_port=None)
        ctx.args = _make_args(upload_port="/dev/ttyUSB0")
        with (
            patch(
                f"{_PATCH_MOD}.select_build_driver", return_value=_make_mock_driver()
            ),
            patch(
                "ci.util.pio_package_daemon.get_default_environment",
                return_value=None,
            ),
        ):
            rc = asyncio.run(_resolve_port_and_environment(ctx))
        assert rc is None
        assert ctx.upload_port == "/dev/ttyUSB0"

    def test_port_detection_failure(self) -> None:
        ctx = _make_ctx(upload_port=None, final_environment=None)
        ctx.args = _make_args(upload_port=None)
        mock_result = MagicMock(ok=False, error_message="No USB", all_ports=[])
        call_count = [0]
        original_monotonic = __import__("time").monotonic

        def fake_monotonic():
            call_count[0] += 1
            if call_count[0] <= 2:
                return original_monotonic()
            return original_monotonic() + 999

        with (
            patch(f"{_PATCH_MOD}.auto_detect_upload_port", return_value=mock_result),
            patch(f"{_PATCH_MOD}.time") as mock_time,
        ):
            mock_time.monotonic = fake_monotonic
            mock_time.sleep = MagicMock()
            rc = asyncio.run(_resolve_port_and_environment(ctx))
        assert rc == 1

    def test_teensy_port_detection_never_uploads_stale_pio_hex(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        stale_hex = tmp_path / ".pio" / "build" / "teensy41" / "firmware.hex"
        stale_hex.parent.mkdir(parents=True)
        stale_hex.write_text("", encoding="utf-8")

        ctx = _make_ctx(
            upload_port=None,
            final_environment="teensy41",
            build_dir=tmp_path,
        )
        ctx.args = _make_args(
            upload_port=None,
            environment="teensy41",
            object_fled=True,
            parlio=False,
            use_root_platformio_ini=False,
            project_dir=tmp_path,
        )
        mock_result = MagicMock(
            ok=False,
            selected_port=None,
            error_message="No USB",
            all_ports=[],
        )
        call_count = [0]
        original_monotonic = __import__("time").monotonic

        def fake_monotonic() -> float:
            call_count[0] += 1
            if call_count[0] <= 2:
                return original_monotonic()
            return original_monotonic() + 999

        with (
            patch(f"{_PATCH_MOD}.auto_detect_upload_port", return_value=mock_result),
            patch(f"{_PATCH_MOD}.time") as mock_time,
            patch(f"{_PATCH_MOD}.subprocess.run") as mock_subprocess_run,
        ):
            mock_time.monotonic = fake_monotonic
            mock_time.sleep = MagicMock()
            rc = asyncio.run(_resolve_port_and_environment(ctx))

        output = capsys.readouterr().out
        assert rc == 1
        mock_subprocess_run.assert_not_called()
        assert "AutoResearch will not pre-upload stale .pio firmware" in output
        assert "Firmware uploaded via Teensy bootloader" not in output


class TestAutoDetectUploadPort:
    """Test USB port detection edge cases used by autoresearch."""

    def test_expected_environment_probe_failure_falls_back_to_descriptor(self) -> None:
        port = ListPortInfo("COM9")
        port.description = "USB JTAG/serial debug unit"
        port.hwid = "USB VID:PID=303A:1001"

        with (
            patch(
                "ci.util.port_utils.serial.tools.list_ports.comports",
                return_value=[port],
            ),
            patch(
                "ci.util.port_utils.detect_attached_chip",
                return_value=ChipDetectionResult(
                    ok=False,
                    chip_type=None,
                    environment=None,
                    error_message="probe timed out",
                ),
            ) as detect_chip,
        ):
            result = auto_detect_upload_port("esp32c6")

        assert result.ok is True
        assert result.selected_port == "COM9"
        # FastLED #3446: the 3.0s explicit timeout override was removed so
        # the call picks up `detect_attached_chip`'s richer default. Test
        # now just asserts the call happened with the port name.
        detect_chip.assert_called_once_with("COM9")

    def test_expected_environment_positive_mismatch_fails(self) -> None:
        port = ListPortInfo("COM9")
        port.description = "USB JTAG/serial debug unit"
        port.hwid = "USB VID:PID=303A:1001"

        with (
            patch(
                "ci.util.port_utils.serial.tools.list_ports.comports",
                return_value=[port],
            ),
            patch(
                "ci.util.port_utils.detect_attached_chip",
                return_value=ChipDetectionResult(
                    ok=True,
                    chip_type="ESP32-S3",
                    environment="esp32s3",
                    error_message=None,
                ),
            ),
        ):
            result = auto_detect_upload_port("esp32c6")

        assert result.ok is False
        assert result.selected_port is None
        assert "No USB serial port matched expected environment" in (
            result.error_message or ""
        )

    def test_lpc845brk_lpcxpresso_vcom_fingerprint_matches(self) -> None:
        """LPC845-BRK with LPCXpresso VCOM firmware (16C0:0483) is accepted."""
        port = ListPortInfo("COM12")
        port.description = "USB Serial Device"
        port.hwid = "USB VID:PID=16C0:0483"
        port.vid = 0x16C0
        port.pid = 0x0483

        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[port],
        ):
            result = auto_detect_upload_port("lpc845brk")

        assert result.ok is True
        assert result.selected_port == "COM12"

    def test_rp2040_requires_application_cdc_fingerprint(self) -> None:
        """A nearby CP210x must never be selected for an RP2040 run."""
        port = ListPortInfo("COM11")
        port.description = "Silicon Labs CP210x USB to UART Bridge"
        port.hwid = "USB VID:PID=10C4:EA60"
        port.vid = 0x10C4
        port.pid = 0xEA60
        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[port],
        ):
            result = auto_detect_upload_port("rp2040")
        assert result.ok is False
        assert result.selected_port is None
        assert "2E8A:000A" in (result.error_message or "")

    def test_rp2040_application_cdc_fingerprint_matches(self) -> None:
        port = ListPortInfo("COM11")
        port.description = "USB Serial Device"
        port.hwid = "USB VID:PID=2E8A:000A"
        port.vid = 0x2E8A
        port.pid = 0x000A
        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[port],
        ):
            result = auto_detect_upload_port("rp2040")
        assert result.ok is True
        assert result.selected_port == "COM11"

    def test_rp2040_rejects_rp2350_application_cdc_fingerprint(self) -> None:
        rp2350 = ListPortInfo("COM12")
        rp2350.description = "USB Serial Device"
        rp2350.hwid = "USB VID:PID=2E8A:000F"
        rp2350.vid = 0x2E8A
        rp2350.pid = 0x000F

        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[rp2350],
        ):
            result = auto_detect_upload_port("rp2040")

        assert result.ok is False
        assert result.selected_port is None

    def test_rpipicow_uses_its_board_exact_application_identity(
        self: "TestAutoDetectUploadPort",
    ) -> None:
        pico = ListPortInfo("COM11")
        pico.description = "USB Serial Device"
        pico.hwid = "USB VID:PID=2E8A:000A"
        pico.vid = 0x2E8A
        pico.pid = 0x000A

        pico_w = ListPortInfo("COM12")
        pico_w.description = "USB Serial Device"
        pico_w.hwid = "USB VID:PID=2E8A:F00A"
        pico_w.vid = 0x2E8A
        pico_w.pid = 0xF00A

        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[pico, pico_w],
        ):
            result = auto_detect_upload_port("rpipicow")

        assert result.ok is True
        assert result.selected_port == "COM12"

    def test_rpipico2_application_cdc_fingerprint_matches(self) -> None:
        port = ListPortInfo("COM12")
        port.description = "USB Serial Device"
        port.hwid = "USB VID:PID=2E8A:000F"
        port.vid = 0x2E8A
        port.pid = 0x000F
        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[port],
        ):
            result = auto_detect_upload_port("rpipico2")
        assert result.ok is True
        assert result.selected_port == "COM12"

    @pytest.mark.parametrize(
        ("environment", "pid"),
        (
            ("rp2350", 0x000F),
            ("rpipico2", 0x000F),
            ("rp2350w", 0xF00F),
            ("rpipico2w", 0xF00F),
        ),
    )
    def test_rp2350_aliases_accept_board_exact_application_cdc_fingerprint(
        self, environment: str, pid: int
    ) -> None:
        port = ListPortInfo("COM17")
        port.description = "USB Serial Device"
        port.hwid = f"USB VID:PID=2E8A:{pid:04X}"
        port.vid = 0x2E8A
        port.pid = pid

        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[port],
        ):
            result = auto_detect_upload_port(environment)

        assert result.ok is True
        assert result.selected_port == "COM17"

    @pytest.mark.parametrize(
        ("environment", "wrong_pid"),
        (("rp2350", 0xF00F), ("rp2350w", 0x000F)),
    )
    def test_rp2350_variants_reject_each_others_application_identity(
        self, environment: str, wrong_pid: int
    ) -> None:
        other_variant = ListPortInfo("COM12")
        other_variant.description = "USB Serial Device"
        other_variant.hwid = f"USB VID:PID=2E8A:{wrong_pid:04X}"
        other_variant.vid = 0x2E8A
        other_variant.pid = wrong_pid

        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[other_variant],
        ):
            result = auto_detect_upload_port(environment)

        assert result.ok is False
        assert result.selected_port is None

    def test_rp2350w_strict_fingerprint_skips_unrelated_usb_serial(self) -> None:
        unrelated = ListPortInfo("COM11")
        unrelated.description = "Silicon Labs CP210x USB to UART Bridge"
        unrelated.hwid = "USB VID:PID=10C4:EA60"
        unrelated.vid = 0x10C4
        unrelated.pid = 0xEA60

        rp2350 = ListPortInfo("COM17")
        rp2350.description = "USB Serial Device"
        rp2350.hwid = "USB VID:PID=2E8A:F00F"
        rp2350.vid = 0x2E8A
        rp2350.pid = 0xF00F

        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[unrelated, rp2350],
        ):
            result = auto_detect_upload_port("rp2350w")

        assert result.ok is True
        assert result.selected_port == "COM17"

    def test_rp2350_strict_fingerprint_skips_attached_esp32c6(self) -> None:
        esp32c6 = ListPortInfo("COM9")
        esp32c6.description = "USB JTAG/serial debug unit"
        esp32c6.hwid = "USB VID:PID=303A:1001"
        esp32c6.vid = 0x303A
        esp32c6.pid = 0x1001

        rp2350 = ListPortInfo("COM17")
        rp2350.description = "USB Serial Device"
        rp2350.hwid = "USB VID:PID=2E8A:F00F"
        rp2350.vid = 0x2E8A
        rp2350.pid = 0xF00F

        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[esp32c6, rp2350],
        ):
            result = auto_detect_upload_port("rp2350w")

        assert result.ok is True
        assert result.selected_port == "COM17"

    def test_rp2350_strict_fingerprint_rejects_unrelated_usb_serial(self) -> None:
        unrelated = ListPortInfo("COM11")
        unrelated.description = "Silicon Labs CP210x USB to UART Bridge"
        unrelated.hwid = "USB VID:PID=10C4:EA60"
        unrelated.vid = 0x10C4
        unrelated.pid = 0xEA60

        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[unrelated],
        ):
            result = auto_detect_upload_port("rp2350")

        assert result.ok is False
        assert result.selected_port is None
        assert "2E8A:000F" in (result.error_message or "")

    def test_rp2350w_serial_identity_selects_original_board(self) -> None:
        replacement = ListPortInfo("COM18")
        replacement.description = "USB Serial Device"
        replacement.hwid = "USB VID:PID=2E8A:F00F"
        replacement.vid = 0x2E8A
        replacement.pid = 0xF00F
        replacement.serial_number = "OTHER-RP2350W"

        original = ListPortInfo("COM19")
        original.description = "USB Serial Device"
        original.hwid = "USB VID:PID=2E8A:F00F"
        original.vid = 0x2E8A
        original.pid = 0xF00F
        original.serial_number = "2DCB876B587EA334"

        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[replacement, original],
        ):
            result = auto_detect_upload_port(
                "rp2350w", expected_serial_number="2DCB876B587EA334"
            )

        assert result.ok is True
        assert result.selected_port == "COM19"

    def test_lpc845brk_lpc_link2_cmsis_dap_fingerprint_matches(self) -> None:
        """LPC845-BRK with LPC-Link2 CMSIS-DAP firmware (1FC9:0132) is accepted.

        FastLED #3468 follow-up: newer LPC845-BRK boards ship with NXP's own
        LPC-Link2 CMSIS-DAP firmware pre-flashed on the debug probe. The
        probe still exposes the LPC845's USART0 as a VCOM alongside the
        CMSIS-DAP HID interface — VID:PID 1FC9:0132 belongs to NXP rather
        than the community "V-USB" pool. Both firmwares are valid for
        AutoResearch.
        """
        port = ListPortInfo("COM10")
        port.description = "USB Serial Device"
        port.hwid = "USB VID:PID=1FC9:0132"
        port.vid = 0x1FC9
        port.pid = 0x0132

        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[port],
        ):
            result = auto_detect_upload_port("lpc845brk")

        assert result.ok is True
        assert result.selected_port == "COM10"

    def test_lpc845brk_neither_fingerprint_matches_reports_both(self) -> None:
        """Error message lists BOTH accepted VID:PIDs when neither is found."""
        port = ListPortInfo("COM7")
        port.description = "USB Serial Device"
        port.hwid = "USB VID:PID=303A:1001"
        port.vid = 0x303A
        port.pid = 0x1001

        with patch(
            "ci.util.port_utils.serial.tools.list_ports.comports",
            return_value=[port],
        ):
            result = auto_detect_upload_port("lpc845brk")

        assert result.ok is False
        assert result.selected_port is None
        assert "16C0:0483" in (result.error_message or "")
        assert "1FC9:0132" in (result.error_message or "")


# ============================================================
# Tests: _run_build_deploy
# ============================================================


class TestRunBuildDeploy:
    """Test _run_build_deploy (uses mock BuildDriver)."""

    @pytest.fixture(autouse=True)
    def disable_host_watchdog_for_unit_deploys(self):
        """Direct phase tests do not own host watchdog or USB inventory state."""
        with (
            patch(f"{_PATCH_MOD}.start_autoresearch_watchdog"),
            patch(f"{_PATCH_MOD}.absent_port_error", return_value=None),
        ):
            yield

    def test_build_deploy_success(self) -> None:
        mock_driver = _make_mock_driver()
        ctx = _make_ctx(build_driver=mock_driver)
        qctx = QuietContext(quiet=False)
        rc = asyncio.run(_run_build_deploy(ctx, qctx))
        assert rc is None
        mock_driver.install_packages.assert_called_once()
        mock_driver.deploy.assert_called_once()

    def test_canonical_rp2350w_environment_is_used_for_fbuild_deploy(self) -> None:
        mock_driver = _make_mock_driver()
        ctx = _make_ctx(
            args=_make_args(skip_lint=True),
            build_driver=mock_driver,
            final_environment="rp2350w",
        )
        qctx = QuietContext(quiet=False)

        rc = asyncio.run(_run_build_deploy(ctx, qctx))

        assert rc is None
        assert mock_driver.deploy.call_args.kwargs["environment"] == "rp2350w"

    def test_net_peer_deploys_c6_through_fbuild_on_explicit_port(self) -> None:
        mock_driver = _make_mock_driver()
        ctx = _make_ctx(
            args=_make_args(
                skip_lint=True,
                net_peer=True,
                environment_positional="rp2350w",
                upload_port="COM17",
                peer_upload_port="COM9",
            ),
            build_driver=mock_driver,
            final_environment="rp2350w",
            upload_port="COM17",
            net_peer_mode=True,
        )
        qctx = QuietContext(quiet=False)
        with patch(
            "ci.autoresearch.staging.synthesise_autoresearch_project",
            return_value=Path("/fake/peer-project"),
        ) as stage_peer:
            rc = asyncio.run(_run_build_deploy(ctx, qctx))

        assert rc is None
        stage_peer.assert_called_once()
        assert mock_driver.deploy.call_count == 2
        assert mock_driver.deploy.call_args.kwargs["environment"] == "esp32c6"
        assert mock_driver.deploy.call_args.kwargs["upload_port"] == "COM9"

    def test_build_deploy_install_failure(self) -> None:
        mock_driver = _make_mock_driver(install_ok=False)
        ctx = _make_ctx(build_driver=mock_driver)
        qctx = QuietContext(quiet=False)
        rc = asyncio.run(_run_build_deploy(ctx, qctx))
        assert rc == 1
        mock_driver.deploy.assert_not_called()

    def test_build_deploy_deploy_failure(self) -> None:
        mock_driver = _make_mock_driver(deploy_ok=False)
        ctx = _make_ctx(build_driver=mock_driver)
        qctx = QuietContext(quiet=False)
        rc = asyncio.run(_run_build_deploy(ctx, qctx))
        assert rc == 1

    def test_skip_lint(self) -> None:
        mock_driver = _make_mock_driver()
        ctx = _make_ctx(build_driver=mock_driver)
        ctx.args = _make_args(skip_lint=True)
        qctx = QuietContext(quiet=False)
        with patch(f"{_PATCH_MOD}.run_cpp_lint") as mock_lint:
            rc = asyncio.run(_run_build_deploy(ctx, qctx))
            mock_lint.assert_not_called()
        assert rc is None

    def test_lint_failure(self) -> None:
        mock_driver = _make_mock_driver()
        ctx = _make_ctx(build_driver=mock_driver)
        ctx.args = _make_args(skip_lint=False)
        qctx = QuietContext(quiet=False)
        with patch(f"{_PATCH_MOD}.run_cpp_lint", return_value=False):
            rc = asyncio.run(_run_build_deploy(ctx, qctx))
        assert rc == 1

    def test_lpc_ieee754_uses_dedicated_build_environment(self) -> None:
        mock_driver = _make_mock_driver()
        ctx = _make_ctx(
            args=_make_args(skip_lint=True, ieee754=True),
            build_driver=mock_driver,
            final_environment="lpc845brk",
            ieee754_test_mode=True,
        )
        qctx = QuietContext(quiet=False)
        with patch(
            f"{_PATCH_MOD}._build_and_deploy_nxplpc", return_value=True
        ) as flash:
            rc = asyncio.run(_run_build_deploy(ctx, qctx))
        assert rc is None
        assert _build_environment_for_mode(ctx) == "lpc845brk_ieee754"
        mock_driver.install_packages.assert_called_once_with(
            Path("/fake/project"), "lpc845brk_ieee754"
        )
        flash.assert_called_once()
        assert flash.call_args.kwargs["environment"] == "lpc845brk_ieee754"
        mock_driver.deploy.assert_not_called()

    def test_stopping_host_watchdog_signals_and_clears_the_runner_handle(self) -> None:
        ctx = _make_ctx()
        cancel_event = MagicMock()
        ctx._watchdog_task = cancel_event

        stop_autoresearch_watchdog(ctx)

        cancel_event.set.assert_called_once_with()
        assert ctx._watchdog_task is None


# ============================================================
# Tests: _run_schema_and_pin_setup
# ============================================================


class TestRunSchemaAndPinSetup:
    """Test _run_schema_and_pin_setup (mocks RPC client)."""

    def test_rp2350_postdeploy_rescan_polls_until_cdc_appears(self) -> None:
        ctx = _make_ctx(
            final_environment="rp2350",
            upload_port=None,
            use_fbuild=True,
            rpc_smoke_mode=True,
        )
        missed = MagicMock(selected_port=None)
        discovered = MagicMock(selected_port="COM17")

        with (
            patch(
                f"{_PATCH_MOD}.auto_detect_upload_port",
                side_effect=[missed, missed, discovered],
            ) as auto_detect,
            patch(
                "ci.util.serial_interface.create_serial_interface",
                return_value=MagicMock(),
            ),
            patch(f"{_PATCH_MOD}.CrashTraceDecoder", return_value=MagicMock()),
        ):
            rc = asyncio.run(_run_schema_and_pin_setup(ctx))

        assert rc is None
        assert ctx.upload_port == "COM17"
        assert auto_detect.call_count == 3
        auto_detect.assert_called_with(expected_environment="rp2350")

    def test_cli_pin_override(self) -> None:
        args = _make_args(tx_pin=3, rx_pin=4, skip_schema=True)
        ctx = _make_ctx(args=args)
        with patch(
            f"{_PATCH_MOD}.run_gpio_pretest",
            new_callable=AsyncMock,
            return_value=True,
        ):
            rc = asyncio.run(_run_schema_and_pin_setup(ctx))
        assert rc is None
        assert ctx.effective_tx_pin == 3
        assert ctx.effective_rx_pin == 4
        assert ctx.json_rpc_commands[0] == {
            "method": "setPins",
            "params": [{"txPin": 3, "rxPin": 4}],
        }

    def test_default_pins(self) -> None:
        args = _make_args(skip_schema=True)
        ctx = _make_ctx(args=args)
        with patch(
            f"{_PATCH_MOD}.run_gpio_pretest",
            new_callable=AsyncMock,
            return_value=True,
        ):
            rc = asyncio.run(_run_schema_and_pin_setup(ctx))
        assert rc is None
        assert ctx.effective_tx_pin == 1
        assert ctx.effective_rx_pin == 2
        assert ctx.json_rpc_commands[0] == {
            "method": "setPins",
            "params": [{"txPin": 1, "rxPin": 2}],
        }

    def test_teensy_default_pins_match_firmware(self) -> None:
        args = _make_args(skip_schema=True)
        ctx = _make_ctx(args=args, final_environment="teensy41")
        with patch(
            f"{_PATCH_MOD}.run_gpio_pretest",
            new_callable=AsyncMock,
            return_value=True,
        ):
            rc = asyncio.run(_run_schema_and_pin_setup(ctx))
        assert rc is None
        assert ctx.effective_tx_pin == 1
        assert ctx.effective_rx_pin == 2
        assert ctx.json_rpc_commands[0] == {
            "method": "setPins",
            "params": [{"txPin": 1, "rxPin": 2}],
        }

    def test_teensy_flex_io_default_tx_pin_is_visible(self) -> None:
        args = _make_args(parlio=False, flex_io=True, skip_schema=True)
        ctx = _make_ctx(
            args=args,
            drivers=["FLEX_IO"],
            final_environment="teensy41",
        )
        with patch(
            f"{_PATCH_MOD}.run_gpio_pretest",
            new_callable=AsyncMock,
            return_value=True,
        ) as pretest:
            rc = asyncio.run(_run_schema_and_pin_setup(ctx))
        assert rc is None
        assert ctx.effective_tx_pin == 6
        assert ctx.effective_rx_pin == 2
        assert ctx.json_rpc_commands[0] == {
            "method": "setPins",
            "params": [{"txPin": 6, "rxPin": 2}],
        }
        pretest.assert_awaited_once_with(
            "COM5", 6, 2, serial_interface=ctx.serial_iface
        )

    def test_teensy_flex_io_explicit_pin_override_wins(self) -> None:
        args = _make_args(
            parlio=False,
            flex_io=True,
            tx_pin=22,
            rx_pin=8,
            skip_schema=True,
        )
        ctx = _make_ctx(
            args=args,
            drivers=["FLEX_IO"],
            final_environment="teensy41",
        )
        with patch(
            f"{_PATCH_MOD}.run_gpio_pretest",
            new_callable=AsyncMock,
            return_value=True,
        ):
            rc = asyncio.run(_run_schema_and_pin_setup(ctx))
        assert rc is None
        assert ctx.effective_tx_pin == 22
        assert ctx.effective_rx_pin == 8
        assert ctx.json_rpc_commands[0] == {
            "method": "setPins",
            "params": [{"txPin": 22, "rxPin": 8}],
        }

    def test_esp32p4_default_pins_match_firmware(self) -> None:
        args = _make_args(skip_schema=True)
        ctx = _make_ctx(args=args, final_environment="esp32p4")
        with patch(
            f"{_PATCH_MOD}.run_gpio_pretest",
            new_callable=AsyncMock,
            return_value=True,
        ):
            rc = asyncio.run(_run_schema_and_pin_setup(ctx))
        assert rc is None
        assert ctx.effective_tx_pin == 5
        assert ctx.effective_rx_pin == 6
        assert ctx.json_rpc_commands[0] == {
            "method": "setPins",
            "params": [{"txPin": 5, "rxPin": 6}],
        }

    def test_teensy_cli_half_override_uses_firmware_default_complement(self) -> None:
        args = _make_args(tx_pin=22, skip_schema=True)
        ctx = _make_ctx(args=args, final_environment="teensy41")
        with patch(
            f"{_PATCH_MOD}.run_gpio_pretest",
            new_callable=AsyncMock,
            return_value=True,
        ):
            rc = asyncio.run(_run_schema_and_pin_setup(ctx))
        assert rc is None
        assert ctx.effective_tx_pin == 22
        assert ctx.effective_rx_pin == 2
        assert ctx.json_rpc_commands[0] == {
            "method": "setPins",
            "params": [{"txPin": 22, "rxPin": 2}],
        }

    def test_gpio_pretest_failure(self) -> None:
        args = _make_args(skip_schema=True)
        ctx = _make_ctx(args=args)
        with patch(
            f"{_PATCH_MOD}.run_gpio_pretest",
            new_callable=AsyncMock,
            return_value=False,
        ):
            rc = asyncio.run(_run_schema_and_pin_setup(ctx))
        assert rc == 1

    def test_simd_skips_pin_discovery(self) -> None:
        args = _make_args(skip_schema=True, parlio=False, simd=True)
        ctx = _make_ctx(args=args, simd_test_mode=True)
        rc = asyncio.run(_run_schema_and_pin_setup(ctx))
        assert rc is None
        assert ctx.effective_tx_pin is None
        assert ctx.effective_rx_pin is None

    def test_perf_wave2d_skips_all_pin_setup(self) -> None:
        args = _make_args(skip_schema=True, parlio=False, perf_wave2d="32x32")
        ctx = _make_ctx(
            args=args,
            drivers=[],
            json_rpc_commands=[],
            perf_wave2d_grid=(32, 32),
        )
        with patch(
            f"{_PATCH_MOD}.run_gpio_pretest",
            new_callable=AsyncMock,
        ) as pretest:
            rc = asyncio.run(_run_schema_and_pin_setup(ctx))
        assert rc is None
        assert ctx.effective_tx_pin is None
        assert ctx.effective_rx_pin is None
        assert ctx.json_rpc_commands == []
        pretest.assert_not_awaited()

    def test_lpc_fbuild_uses_pyserial_for_rpc(self) -> None:
        args = _make_args(skip_schema=True)
        ctx = _make_ctx(
            args=args,
            final_environment="lpc845brk",
            use_fbuild=True,
        )
        mock_serial = MagicMock()
        with patch(
            "ci.util.serial_interface.create_serial_interface",
            return_value=mock_serial,
        ) as create_serial:
            rc = asyncio.run(_run_schema_and_pin_setup(ctx))
        assert rc is None
        assert ctx.serial_iface is mock_serial
        create_serial.assert_called_once_with(port="COM5", use_pyserial=True)

    def test_auto_discover_pins_success(self) -> None:
        args = _make_args(auto_discover_pins=True, skip_schema=True)
        ctx = _make_ctx(args=args)
        mock_discovery = MagicMock(success=True, tx_pin=5, rx_pin=6, client=AsyncMock())
        with patch(
            f"{_PATCH_MOD}.run_pin_discovery_segmented",
            new_callable=AsyncMock,
            return_value=mock_discovery,
        ):
            rc = asyncio.run(_run_schema_and_pin_setup(ctx))
        assert rc is None
        assert ctx.effective_tx_pin == 5
        assert ctx.effective_rx_pin == 6
        assert ctx.discovery_client is not None
        assert ctx.pins_discovered is True
        assert ctx.json_rpc_commands[0] == {
            "method": "setPins",
            "params": [{"txPin": 5, "rxPin": 6}],
        }

    def test_auto_discover_pins_failure_uses_platform_defaults(self) -> None:
        args = _make_args(auto_discover_pins=True, skip_schema=True)
        ctx = _make_ctx(args=args, final_environment="esp32p4")
        mock_client = AsyncMock()
        mock_discovery = MagicMock(
            success=False, tx_pin=None, rx_pin=None, client=mock_client
        )
        with (
            patch(
                f"{_PATCH_MOD}.run_pin_discovery_segmented",
                new_callable=AsyncMock,
                return_value=mock_discovery,
            ),
            patch(
                f"{_PATCH_MOD}.run_gpio_pretest",
                new_callable=AsyncMock,
                return_value=True,
            ),
        ):
            rc = asyncio.run(_run_schema_and_pin_setup(ctx))
        assert rc is None
        assert ctx.effective_tx_pin == 5
        assert ctx.effective_rx_pin == 6
        assert ctx.json_rpc_commands[0] == {
            "method": "setPins",
            "params": [{"txPin": 5, "rxPin": 6}],
        }
        mock_client.close.assert_awaited_once()


# ============================================================
# Tests: _run_tests_or_special_mode
# ============================================================


class TestRunTestsOrSpecialMode:
    """Test _run_tests_or_special_mode (mocks RPC client)."""

    def test_gpio_only_returns_zero(self) -> None:
        ctx = _make_ctx(gpio_only_mode=True)
        qctx = QuietContext(quiet=False)
        rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))
        assert rc == 0

    def test_rp_spi_loopback_delegates_on_rp2040(self) -> None:
        ctx = _make_ctx(
            args=_make_args(parlio=False, rp_spi_loopback=True),
            drivers=[],
            gpio_only_mode=False,
            final_environment="rp2040",
        )
        qctx = QuietContext(quiet=False)
        with patch(
            f"{_PATCH_MOD}._run_rp_spi_loopback_tests",
            new_callable=AsyncMock,
            return_value=0,
        ) as loopback:
            rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))
        assert rc == 0
        loopback.assert_awaited_once_with(ctx)

    @pytest.mark.parametrize("environment", ("rp2350", "rp2350w", "rpipico2w"))
    def test_rp_spi_loopback_runs_on_rp2350_aliases(self, environment: str) -> None:
        ctx = _make_ctx(
            args=_make_args(parlio=False, rp_spi_loopback=True),
            drivers=[],
            gpio_only_mode=False,
            final_environment=environment,
            upload_port="COM17",
        )
        completed = MagicMock(returncode=0)
        with patch(f"{_PATCH_MOD}.RunningProcess.run", return_value=completed) as run:
            rc = asyncio.run(_run_rp_spi_loopback_tests(ctx))
        assert rc == 0
        command = run.call_args.args[0]
        assert command[:4] == ["uv", "run", "python", "-m"]
        assert command[4] == "ci.autoresearch.test_rp_spi_loopback"
        assert run.call_args.kwargs == {"check": False, "timeout": 60.0}

    @pytest.mark.parametrize("environment", ("rp2350", "rp2350w", "rpipico2w"))
    def test_rp_spi_public_api_runs_on_rp2350_aliases(self, environment: str) -> None:
        ctx = _make_ctx(
            args=_make_args(parlio=False, rp_spi_public_api=True),
            drivers=[],
            gpio_only_mode=False,
            final_environment=environment,
            upload_port="COM17",
        )
        completed = MagicMock(returncode=0)
        with patch(f"{_PATCH_MOD}.RunningProcess.run", return_value=completed) as run:
            rc = asyncio.run(_run_rp_spi_public_api_tests(ctx))
        assert rc == 0
        command = run.call_args.args[0]
        assert command[:4] == ["uv", "run", "python", "-m"]
        assert command[4] == "ci.autoresearch.test_rp_spi_public_api"
        assert run.call_args.kwargs == {"check": False, "timeout": 60.0}

    def test_simd_mode_pass(self) -> None:
        ctx = _make_ctx(simd_test_mode=True)
        qctx = QuietContext(quiet=False)

        mock_client = AsyncMock()
        simd_response = MagicMock()
        simd_response.get = lambda k, d=None: {
            "totalTests": 10,
            "passedTests": 10,
            "failedTests": 0,
            "failures": [],
            "passed": True,
        }.get(k, d)

        bench_response = MagicMock()
        bench_response.get = lambda k, d=None: {"success": True}.get(k, d)
        bench_response.data = {"success": True, "multiply_ns": 42}

        mock_client.send_and_match = AsyncMock(
            side_effect=[simd_response, bench_response]
        )
        mock_client.connect = AsyncMock()
        mock_client.close = AsyncMock()

        with patch(f"{_PATCH_MOD}.RpcClient", return_value=mock_client):
            rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))
        assert rc == 0

    def test_rpc_smoke_mode_validates_core_surface(self) -> None:
        ctx = _make_ctx(rpc_smoke_mode=True, final_environment="rp2350w")
        qctx = QuietContext(quiet=False)
        expected_payload = {
            "text": "rp2350w-rpc-smoke",
            "number": 2350,
            "nested": {"ok": True, "values": [1, 2, 3]},
        }

        required_methods = [
            "help",
            "ping",
            "debugTest",
            "status",
            "drivers",
            "testNoSerial",
            "testRpConcurrency",
        ]

        def response(data):
            item = MagicMock()
            item.success = True
            item.data = data
            return item

        mock_client = AsyncMock()
        mock_client.send = AsyncMock(
            side_effect=[
                response({"schema": [[name] for name in required_methods]}),
                response(
                    {
                        "success": True,
                        "functions": [
                            {"name": name}
                            for name in ["rpc.discover", *required_methods]
                        ],
                    }
                ),
                response({"uptimeMs": 1}),
                response({"uptimeMs": 2}),
                response({"received": expected_payload}),
                response(
                    {
                        "ready": True,
                        "platform": "Raspberry Pi Pico 2 W (RP2350)",
                        "rpcReady": True,
                        "ledRxAvailable": False,
                    }
                ),
                response([]),
                response(
                    {
                        "success": True,
                        "message": "RPC works from task context",
                        "serial_safe": False,
                    }
                ),
                response(
                    {
                        "success": True,
                        "supported": True,
                        "backend": "pico-sdk-mutex+arduino-core1",
                        "core1Ready": True,
                        "core1Done": True,
                        "recursiveMutexReady": True,
                        "expected": 2000,
                        "actual": 2000,
                    }
                ),
                RpcError(
                    "Method not found",
                    code=-32601,
                    data={"method": "__autoresearch_missing_method__"},
                ),
            ]
        )
        mock_client.connect = AsyncMock()
        mock_client.drain_boot_output = AsyncMock()
        mock_client.close = AsyncMock()

        with patch(f"{_PATCH_MOD}.RpcClient", return_value=mock_client):
            rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))

        assert rc == 0
        assert mock_client.send.await_count == 10
        debug_test_call = mock_client.send.await_args_list[4]
        assert debug_test_call.args == ("debugTest",)
        assert debug_test_call.kwargs["args"] == expected_payload

    def test_rpc_smoke_rejects_missing_required_discovery_method(self) -> None:
        ctx = _make_ctx(rpc_smoke_mode=True, final_environment="rp2350w")
        qctx = QuietContext(quiet=False)

        def response(data):
            item = MagicMock()
            item.success = True
            item.data = data
            return item

        mock_client = AsyncMock()
        mock_client.send = AsyncMock(
            return_value=response({"schema": [["ping"], ["status"]]})
        )
        mock_client.connect = AsyncMock()
        mock_client.drain_boot_output = AsyncMock()
        mock_client.close = AsyncMock()

        with patch(f"{_PATCH_MOD}.RpcClient", return_value=mock_client):
            rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))

        assert rc == 1

    def test_rpc_smoke_non_rp_target_skips_rp_concurrency(self) -> None:
        ctx = _make_ctx(rpc_smoke_mode=True, final_environment="esp32s3")
        qctx = QuietContext(quiet=False)
        required_methods = [
            "help",
            "ping",
            "debugTest",
            "status",
            "drivers",
            "testNoSerial",
        ]

        def response(data):
            item = MagicMock()
            item.success = True
            item.data = data
            return item

        payload = {
            "text": "rp2040-rpc-smoke",
            "number": 2040,
            "nested": {"ok": True, "values": [1, 2, 3]},
        }
        mock_client = AsyncMock()
        mock_client.send = AsyncMock(
            side_effect=[
                response({"schema": [[name] for name in required_methods]}),
                response(
                    {
                        "functions": [
                            {"name": name}
                            for name in ["rpc.discover", *required_methods]
                        ]
                    }
                ),
                response({"uptimeMs": 1}),
                response({"uptimeMs": 2}),
                response({"received": payload}),
                response(
                    {
                        "ready": True,
                        "platform": "ESP32-S3",
                        "rpcReady": True,
                        "ledRxAvailable": False,
                    }
                ),
                response([]),
                response(
                    {
                        "success": True,
                        "message": "RPC works from task context",
                        "serial_safe": False,
                    }
                ),
                RpcError("Method not found", code=-32601),
            ]
        )
        mock_client.connect = AsyncMock()
        mock_client.drain_boot_output = AsyncMock()
        mock_client.close = AsyncMock()

        with patch(f"{_PATCH_MOD}.RpcClient", return_value=mock_client):
            rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))

        assert rc == 0
        assert mock_client.send.await_count == 9
        assert all(
            call.args[0] != "testRpConcurrency"
            for call in mock_client.send.await_args_list
        )

    def test_rpc_smoke_rejects_timeout_for_unknown_method(self) -> None:
        ctx = _make_ctx(rpc_smoke_mode=True, final_environment="rp2350w")
        qctx = QuietContext(quiet=False)
        methods = [
            "help",
            "ping",
            "debugTest",
            "status",
            "drivers",
            "testNoSerial",
            "testRpConcurrency",
        ]

        def response(data):
            item = MagicMock()
            item.success = True
            item.data = data
            return item

        payload = {
            "text": "rp2350w-rpc-smoke",
            "number": 2350,
            "nested": {"ok": True, "values": [1, 2, 3]},
        }
        mock_client = AsyncMock()
        mock_client.send = AsyncMock(
            side_effect=[
                response({"schema": [[name] for name in methods]}),
                response(
                    {
                        "functions": [
                            {"name": name} for name in ["rpc.discover", *methods]
                        ]
                    }
                ),
                response({"uptimeMs": 1}),
                response({"uptimeMs": 2}),
                response({"received": payload}),
                response(
                    {
                        "ready": True,
                        "platform": "Raspberry Pi Pico 2 W (RP2350)",
                        "rpcReady": True,
                        "ledRxAvailable": False,
                    }
                ),
                response([]),
                response(
                    {
                        "success": True,
                        "message": "RPC works from task context",
                        "serial_safe": False,
                    }
                ),
                response(
                    {
                        "success": True,
                        "supported": True,
                        "core1Ready": True,
                        "core1Done": True,
                        "recursiveMutexReady": True,
                        "expected": 2000,
                        "actual": 2000,
                    }
                ),
                RpcTimeoutError("missing method timed out"),
            ]
        )
        mock_client.connect = AsyncMock()
        mock_client.drain_boot_output = AsyncMock()
        mock_client.close = AsyncMock()

        with patch(f"{_PATCH_MOD}.RpcClient", return_value=mock_client):
            rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))

        assert rc == 1
        assert mock_client.send.await_count == 10

    def test_rp_coroutine_mode_reports_null_backend_as_unsupported(self) -> None:
        ctx = _make_ctx(
            coroutine_test_mode=True,
            final_environment="rp2350w",
        )
        qctx = QuietContext(quiet=False)

        response = MagicMock()
        response.get = lambda key, default=None: {
            "success": False,
            "supported": False,
            "backend": "null",
            "reason": "RP2xxx currently selects the generic Arduino null coroutine backend",
            "passed": 0,
        }.get(key, default)

        mock_client = AsyncMock()
        mock_client.send_and_match = AsyncMock(return_value=response)
        mock_client.connect = AsyncMock()
        mock_client.close = AsyncMock()

        with patch(f"{_PATCH_MOD}.RpcClient", return_value=mock_client):
            rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))

        assert rc == 0

    def test_rp2350_watchdog_reacquires_active_environment_and_device(self) -> None:
        ctx = _make_ctx(
            final_environment="rp2350w",
            upload_port="COM17",
            use_fbuild=True,
            watchdog_soak_mode=True,
        )

        def response(data):
            item = MagicMock()
            item.success = True
            item.data = data
            return item

        initial_client = AsyncMock()
        initial_client.send = AsyncMock(
            side_effect=[
                response({"uptimeMs": 100}),
                response({"success": True}),
            ]
        )
        recovery_client = AsyncMock()
        recovery_client.send = AsyncMock(
            return_value=response({"uptimeMs": 1, "lastResetWasWatchdog": True})
        )
        detected = MagicMock(selected_port="COM18")

        with (
            patch(
                "ci.util.serial_interface.create_serial_interface",
                return_value=MagicMock(),
            ),
            patch(
                f"{_PATCH_MOD}.RpcClient",
                side_effect=[initial_client, recovery_client],
            ),
            patch(f"{_PATCH_MOD}.port_exists", return_value=False),
            patch(
                f"{_PATCH_MOD}.get_port_serial_number",
                return_value="2DCB876B587EA334",
            ),
            patch(
                f"{_PATCH_MOD}.auto_detect_upload_port", return_value=detected
            ) as auto_detect,
            patch(
                f"{_PATCH_MOD}._run_rpc_smoke_tests",
                new_callable=AsyncMock,
                return_value=0,
            ),
        ):
            rc = asyncio.run(_run_watchdog_soak(ctx))

        assert rc == 0
        assert ctx.upload_port == "COM18"
        auto_detect.assert_called_once_with(
            "rp2350w", expected_serial_number="2DCB876B587EA334"
        )

    def test_rp2350_watchdog_rejects_reenumeration_without_watchdog_reset(self) -> None:
        ctx = _make_ctx(
            final_environment="rp2350w",
            upload_port="COM17",
            use_fbuild=True,
            watchdog_soak_mode=True,
        )

        def response(data):
            item = MagicMock()
            item.success = True
            item.data = data
            return item

        initial_client = AsyncMock()
        initial_client.send = AsyncMock(
            side_effect=[
                response({"uptimeMs": 100}),
                response({"success": True}),
            ]
        )
        recovery_client = AsyncMock()
        recovery_client.send = AsyncMock(
            return_value=response({"uptimeMs": 1, "lastResetWasWatchdog": False})
        )
        detected = MagicMock(selected_port="COM18")

        with (
            patch(
                "ci.util.serial_interface.create_serial_interface",
                return_value=MagicMock(),
            ),
            patch(
                f"{_PATCH_MOD}.RpcClient",
                side_effect=[initial_client, recovery_client],
            ),
            patch(f"{_PATCH_MOD}.port_exists", return_value=False),
            patch(
                f"{_PATCH_MOD}.get_port_serial_number",
                return_value="2DCB876B587EA334",
            ),
            patch(f"{_PATCH_MOD}.auto_detect_upload_port", return_value=detected),
            patch(
                f"{_PATCH_MOD}._run_rpc_smoke_tests",
                new_callable=AsyncMock,
                return_value=0,
            ) as rpc_smoke,
        ):
            rc = asyncio.run(_run_watchdog_soak(ctx))

        assert rc == 1
        rpc_smoke.assert_not_awaited()

    def test_ble_mode_delegates(self) -> None:
        ctx = _make_ctx(ble_mode=True)
        qctx = QuietContext(quiet=False)

        with patch(
            "ci.autoresearch.ble.run_ble_autoresearch",
            new_callable=AsyncMock,
            return_value=0,
        ) as mock_ble:
            rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))
        assert rc == 0
        mock_ble.assert_called_once()

    def test_ota_mode_delegates(self) -> None:
        ctx = _make_ctx(ota_mode=True)
        qctx = QuietContext(quiet=False)

        with patch(
            "ci.autoresearch.ota.run_ota_autoresearch",
            new_callable=AsyncMock,
            return_value=0,
        ) as mock_ota:
            rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))
        assert rc == 0
        mock_ota.assert_called_once()

    def test_rpc_test_all_pass(self) -> None:
        ctx = _make_ctx()
        qctx = QuietContext(quiet=False)

        mock_client = AsyncMock()
        mock_response = MagicMock()
        mock_response.data = {
            "success": True,
            "passed": True,
            "driver": "PARLIO",
            "laneCount": 1,
            "laneSizes": [100],
            "duration_ms": 42,
            "passedTests": 1,
            "totalTests": 1,
        }
        mock_client.send = AsyncMock(return_value=mock_response)
        mock_client.connect = AsyncMock()
        mock_client.close = AsyncMock()

        with (
            patch(f"{_PATCH_MOD}.RpcClient", return_value=mock_client),
            patch(f"{_PATCH_MOD}.kill_port_users"),
            patch("time.sleep"),
        ):
            rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))
        assert rc == 0

    def test_rpc_test_failure_surfaces_rp_uart_diagnostics(
        self, capsys: pytest.CaptureFixture[str]
    ) -> None:
        ctx = _make_ctx(
            args=_make_args(parlio=False, uart=True),
            drivers=["UART0"],
            json_rpc_commands=[
                {
                    "method": "runSingleTest",
                    "params": {
                        "driver": "UART0",
                        "laneSizes": [100],
                        "pattern": "MSB_LSB_A",
                        "iterations": 1,
                        "timing": "WS2812B-V5",
                    },
                }
            ],
            final_environment="rp2350w",
        )
        qctx = QuietContext(quiet=False)

        mock_client = AsyncMock()
        mock_response = MagicMock()
        mock_response.data = {
            "success": True,
            "passed": False,
            "driver": "UART0",
            "laneCount": 1,
            "laneSizes": [100],
            "duration_ms": 42,
            "totalTests": 1,
            "passedTests": 0,
            "rpUartStartAttempted": True,
            "rpUartStartSucceeded": False,
            "rpUartEncodedSize": 0,
            "rpUartActualBaud": 0,
            "rpUartLastError": "RP UART: peripheral configure failed",
        }
        mock_client.send = AsyncMock(return_value=mock_response)
        mock_client.connect = AsyncMock()
        mock_client.close = AsyncMock()

        with (
            patch(f"{_PATCH_MOD}.RpcClient", return_value=mock_client),
            patch(f"{_PATCH_MOD}.kill_port_users"),
            patch("time.sleep"),
        ):
            rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))
        assert rc == 1
        output = capsys.readouterr().out
        assert "RP UART:" in output
        assert "attempted=True" in output
        assert "started=False" in output
        assert "encodedBytes=0" in output
        assert "actualBaud=0" in output
        assert "lastError='RP UART: peripheral configure failed'" in output

    def test_rpc_control_success_without_passed_field(self) -> None:
        ctx = _make_ctx()
        ctx.json_rpc_commands.insert(
            0, {"method": "setPins", "params": [{"txPin": 8, "rxPin": 9}]}
        )
        qctx = QuietContext(quiet=False)

        mock_client = AsyncMock()
        set_pins_response = MagicMock()
        set_pins_response.data = {
            "success": True,
            "txPin": 8,
            "rxPin": 9,
            "rxChannelRecreated": True,
        }
        driver_response = MagicMock()
        driver_response.data = {
            "success": True,
            "passed": True,
            "driver": "PARLIO",
            "laneCount": 1,
            "laneSizes": [100],
            "duration_ms": 42,
            "passedTests": 1,
            "totalTests": 1,
        }
        mock_client.send = AsyncMock(side_effect=[set_pins_response, driver_response])
        mock_client.connect = AsyncMock()
        mock_client.close = AsyncMock()

        with (
            patch(f"{_PATCH_MOD}.RpcClient", return_value=mock_client),
            patch(f"{_PATCH_MOD}.kill_port_users"),
            patch("time.sleep"),
        ):
            rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))
        assert rc == 0

    def test_rpc_control_error_without_success_false_stops_run(self) -> None:
        ctx = _make_ctx()
        ctx.json_rpc_commands.insert(
            0, {"method": "setPins", "params": [{"txPin": 8, "rxPin": 9}]}
        )
        qctx = QuietContext(quiet=False)

        mock_client = AsyncMock()
        set_pins_response = MagicMock()
        set_pins_response.data = {
            "error": "RxChannelCreationFailed",
            "message": "Failed to create RX channel",
        }
        mock_client.send = AsyncMock(return_value=set_pins_response)
        mock_client.connect = AsyncMock()
        mock_client.close = AsyncMock()

        with (
            patch(f"{_PATCH_MOD}.RpcClient", return_value=mock_client),
            patch(f"{_PATCH_MOD}.kill_port_users"),
            patch("time.sleep"),
        ):
            rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))
        assert rc == 1
        assert mock_client.send.await_count == 1

    def test_discovery_client_reuse(self) -> None:
        """Verify that discovery_client is reused instead of creating a new one."""
        mock_discovery = AsyncMock()
        mock_response = MagicMock()
        mock_response.data = {
            "success": True,
            "passed": True,
            "driver": "PARLIO",
            "laneCount": 1,
            "laneSizes": [100],
            "duration_ms": 42,
            "passedTests": 1,
            "totalTests": 1,
        }
        mock_discovery.send = AsyncMock(return_value=mock_response)
        mock_discovery.close = AsyncMock()

        ctx = _make_ctx(discovery_client=mock_discovery)
        qctx = QuietContext(quiet=False)

        with patch(f"{_PATCH_MOD}.RpcClient") as mock_rpc_cls:
            with (
                patch(f"{_PATCH_MOD}.kill_port_users"),
                patch("time.sleep"),
            ):
                rc = asyncio.run(_run_tests_or_special_mode(ctx, qctx))
            mock_rpc_cls.assert_not_called()
        assert rc == 0
        mock_discovery.close.assert_called_once()
