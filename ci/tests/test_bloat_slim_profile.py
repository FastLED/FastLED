"""The slim ESP32-S3 bloat profile must be verifiable from build artifacts.

`verify_slim` checks that FASTLED_LOG_VERBOSITY=0 reached the compiler and
that coredump / diagnostic-log code is not linked (FastLED#4564).
"""

from __future__ import annotations

from typing import Any

from typeguard import typechecked

from ci.bloat import effective_log_verbosity, verify_slim


@typechecked
def _symbol(size: int, name: str, archive: str) -> dict[str, Any]:
    return {
        "region": "flash",
        "demangled": name,
        "archive": archive,
        "size": size,
        "source": "nm",
    }


@typechecked
def _report(symbols: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "symbols": symbols,
        "total_flash": sum(int(s["size"]) for s in symbols),
        "total_ram": 0,
    }


_CLEAN = _report([_symbol(1200, "fl::CFastLED::show()", "libFastLED.a")])

_NO_VERBOSITY = [
    {
        "file": "src/FastLED.cpp",
        "command": "xtensa-esp32s3-elf-g++ -Os -c src/FastLED.cpp",
    }
]

_VERBOSITY_ZERO = [
    {
        "file": "src/FastLED.cpp",
        "command": "xtensa-esp32s3-elf-g++ -DFASTLED_LOG_VERBOSITY=0 -c src/FastLED.cpp",
    },
    {
        "file": "src/fl/log.cpp",
        "arguments": [
            "xtensa-esp32s3-elf-g++",
            "-DFASTLED_LOG_VERBOSITY=0",
            "-c",
            "src/fl/log.cpp",
        ],
    },
]


@typechecked
def test_missing_verbosity_is_none_and_fails() -> None:
    assert effective_log_verbosity(_NO_VERBOSITY) is None
    failures = verify_slim(_CLEAN, _NO_VERBOSITY)
    assert any("FASTLED_LOG_VERBOSITY" in f for f in failures)


@typechecked
def test_verbosity_zero_in_command_and_arguments_forms() -> None:
    assert effective_log_verbosity(_VERBOSITY_ZERO) == "0"
    assert effective_log_verbosity(_VERBOSITY_ZERO[:1]) == "0"
    assert effective_log_verbosity(_VERBOSITY_ZERO[1:]) == "0"


@typechecked
def test_coredump_archive_fails() -> None:
    report = _report([_symbol(4096, "esp_core_dump_init", "/sdk/lib/libespcoredump.a")])
    failures = verify_slim(report, _VERBOSITY_ZERO)
    assert any("coredump" in f for f in failures)


@typechecked
def test_diag_log_add_fails() -> None:
    report = _report([_symbol(512, "diag_log_add", "libesp_diagnostics.a")])
    failures = verify_slim(report, _VERBOSITY_ZERO)
    assert any("diag_log_add" in f for f in failures)


@typechecked
def test_clean_slim_report_passes() -> None:
    assert verify_slim(_CLEAN, _VERBOSITY_ZERO) == []
