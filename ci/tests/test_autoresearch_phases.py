"""Unit tests for autoresearch phase functions.

Tests the refactored phase decomposition:
- _parse_args_and_build_commands (pure computation)
- _resolve_port_and_environment (mocked I/O)
- _run_build_deploy (mocked BuildDriver)
- _run_schema_and_pin_setup (mocked RPC)
- _run_tests_or_special_mode (mocked RPC)
"""

import asyncio
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from ci.autoresearch.args import Args
from ci.autoresearch.build_driver import BuildDriver
from ci.autoresearch.context import QuietContext, RunContext
from ci.autoresearch.phases import (
    _parse_args_and_build_commands,
    _resolve_port_and_environment,
    _run_build_deploy,
    _run_schema_and_pin_setup,
    _run_tests_or_special_mode,
)


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
        all=False,
        simd=False,
        coroutine=False,
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
        chipset="ws2812",
        net_server=False,
        net_client=False,
        net=False,
        ota=False,
        ble=False,
        parallel=False,
        decode=None,
        frames=None,
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
        assert len(result.drivers) == 8
        assert "PARLIO" in result.drivers
        assert "RMT" in result.drivers

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

    def test_lcd_forces_esp32s3(self, fake_project_dir: Path) -> None:
        args = _make_args(parlio=False, lcd=True, project_dir=fake_project_dir)
        result = _parse_args_and_build_commands(args)
        assert isinstance(result, RunContext)
        assert result.final_environment == "esp32s3"
        assert "LCD_CLOCKLESS" in result.drivers

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
            patch(f"{_PATCH_MOD}.auto_detect_upload_port", return_value=mock_result),
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
        assert ctx.json_rpc_commands[0]["method"] == "setPins"

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
        assert ctx.effective_tx_pin == 1  # PIN_TX default
        assert ctx.effective_rx_pin == 0  # PIN_RX default

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
