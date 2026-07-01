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
    _parse_args_and_build_commands,
    _resolve_port_and_environment,
    _run_build_deploy,
    _run_schema_and_pin_setup,
    _run_tests_or_special_mode,
)
from ci.util.port_utils import ChipDetectionResult, auto_detect_upload_port


# Module path for patching symbols imported into phases.py
_PATCH_MOD = "ci.autoresearch.phases"


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
        wave2d_perf=None,
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

    def test_lpuart_is_reserved_not_selectable(self, fake_project_dir: Path) -> None:
        args = _make_args(
            parlio=False,
            lpuart=True,
            project_dir=fake_project_dir,
        )
        result = _parse_args_and_build_commands(args)
        assert result == 1

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
            "esp32s3", project_root=tmp_path.resolve(), verbose=False
        )
        assert result.build_dir == fake_build_dir

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

    def test_build_deploy_success(self) -> None:
        mock_driver = _make_mock_driver()
        ctx = _make_ctx(build_driver=mock_driver)
        qctx = QuietContext(quiet=False)
        rc = asyncio.run(_run_build_deploy(ctx, qctx))
        assert rc is None
        mock_driver.install_packages.assert_called_once()
        mock_driver.deploy.assert_called_once()

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
        with patch(f"{_PATCH_MOD}._build_and_flash_nxplpc", return_value=True) as flash:
            rc = asyncio.run(_run_build_deploy(ctx, qctx))
        assert rc is None
        assert _build_environment_for_mode(ctx) == "lpc845brk_ieee754"
        mock_driver.install_packages.assert_called_once_with(
            Path("/fake/project"), "lpc845brk_ieee754"
        )
        flash.assert_called_once()
        assert flash.call_args.kwargs["environment"] == "lpc845brk_ieee754"
        mock_driver.deploy.assert_not_called()


# ============================================================
# Tests: _run_schema_and_pin_setup
# ============================================================


class TestRunSchemaAndPinSetup:
    """Test _run_schema_and_pin_setup (mocks RPC client)."""

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
            f"{_PATCH_MOD}.run_pin_discovery",
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
                f"{_PATCH_MOD}.run_pin_discovery",
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

    def test_rpc_test_failure(self) -> None:
        ctx = _make_ctx()
        qctx = QuietContext(quiet=False)

        mock_client = AsyncMock()
        mock_response = MagicMock()
        mock_response.data = {
            "success": True,
            "passed": False,
            "driver": "PARLIO",
            "laneCount": 1,
            "laneSizes": [100],
            "duration_ms": 42,
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
