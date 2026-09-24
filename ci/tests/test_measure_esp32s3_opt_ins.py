"""The opt-in audit must pass compile-time defines to fbuild itself."""

from __future__ import annotations

from tests.measure_esp32s3_opt_ins import (
    CONFIGS,
    BuildMeasurement,
    _run_compile_cmd,
    format_table,
)


def test_stage4_static_allocation_define_reaches_compile_command() -> None:
    command = _run_compile_cmd("Blink", CONFIGS["stage4"])

    assert "--defines" in command
    define_index = command.index("--defines")
    assert command[define_index + 1] == "FL_RMT_STATIC_ALLOCATION=1"


def test_baseline_compile_command_has_no_extra_defines() -> None:
    command = _run_compile_cmd("Blink", CONFIGS["baseline"])

    assert "--defines" not in command


def test_stacked_compile_command_passes_all_defines_together() -> None:
    command = _run_compile_cmd("Blink", CONFIGS["stack"])

    define_index = command.index("--defines")
    assert command[define_index + 1] == (
        "FASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1,FL_RMT_STATIC_ALLOCATION=1"
    )


def test_comparison_table_records_symbol_and_firmware_deltas() -> None:
    table = format_table(
        {
            "baseline": BuildMeasurement(1000, 400, 10, 1200),
            "stage4": BuildMeasurement(950, 380, 11, 1190),
        }
    )

    assert "Symbol delta" in table
    assert "firmware.bin" in table
    assert "-50 B" in table
    assert "-20 B" in table
    assert "+1" in table
    assert "-10 B" in table
    assert table.isascii()
