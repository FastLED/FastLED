"""Unit tests for the normalize + diff core of check_backend_flag_drift.

The compile-and-snapshot wrapper around this is integration-tested by
actually running it on a board (see FastLED#2410). These tests exercise
just the pure-Python normalization so that drift logic can evolve
without paying a full teensy41 compile every time.
"""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path
from types import ModuleType

import pytest


# The script lives at ci/check_backend_flag_drift.py (no underscore in the
# filename → kebab-cased on disk, but importable here via importlib). Tests
# in this repo are run from the project root so the relative path is stable.
_REPO_ROOT = Path(__file__).resolve().parents[2]
_SCRIPT_PATH = _REPO_ROOT / "ci" / "check_backend_flag_drift.py"


def _load_script_module() -> ModuleType:
    spec = importlib.util.spec_from_file_location(
        "check_backend_flag_drift", _SCRIPT_PATH
    )
    assert spec is not None and spec.loader is not None, (
        f"Failed to load module spec for {_SCRIPT_PATH}: "
        f"spec or spec.loader was None (script missing or unreadable?)"
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


_M = _load_script_module()


# ---------------------------------------------------------------------------
# Sample build_info JSONs modeled after the real fbuild + PIO emissions for
# teensy41 (trimmed to the minimum needed to exercise the diff).
# ---------------------------------------------------------------------------

_FBUILD_TEENSY41_LIKE = {
    "teensy41": {
        "cc_flags": [
            "-mcpu=cortex-m7",
            "-mthumb",
            "-mfloat-abi=hard",
            "-mfpu=fpv5-d16",
            "-Wall",
            "-ffunction-sections",
            "-fdata-sections",
            "-Os",
            "-MMD",
            # -D and -I mixed into cc_flags (real fbuild behavior)
            "-DARDUINO=10819",
            "-DARDUINO_TEENSY41",
            "-DPLATFORMIO",
            "-D__IMXRT1062__",
            "-IC:\\Users\\me\\.fbuild\\prod\\cache\\platforms\\dl-framework-arduinoteensy-1.160.0\\abc\\1.160.0\\cores\\teensy4",
            "-IC:\\Users\\me\\dev\\fastled5\\src",
        ],
        "defines": [
            "ARDUINO=10819",
            "ARDUINO_TEENSY41",
            "PLATFORMIO",
            "__IMXRT1062__",
        ],
        "includes": [
            "C:\\Users\\me\\.fbuild\\prod\\cache\\platforms\\dl-framework-arduinoteensy-1.160.0\\abc\\1.160.0\\cores\\teensy4",
            "C:\\Users\\me\\dev\\fastled5\\src",
        ],
    }
}


_PIO_TEENSY41_LIKE = {
    "teensy41": {
        "cc_flags": [
            "-Wall",
            "-ffunction-sections",
            "-fdata-sections",
            "-mthumb",
            "-mcpu=cortex-m7",
            "-mfloat-abi=hard",
            "-mfpu=fpv5-d16",
            "-O2",  # PIO uses -O2 vs fbuild -Os: a real drift signal
        ],
        "cxx_flags": [
            "-fno-exceptions",
            "-std=gnu++17",
            "-Wall",
            "-ffunction-sections",
            "-fdata-sections",
            "-mthumb",
            "-mcpu=cortex-m7",
            "-mfloat-abi=hard",
            "-mfpu=fpv5-d16",
            "-O2",
        ],
        "defines": [
            "PLATFORMIO=60119",
            "__IMXRT1062__",
            "ARDUINO_TEENSY41",
            "ARDUINO=10805",  # version drift
        ],
        "includes": {
            "build": [
                "C:\\Users\\me\\.platformio\\packages\\framework-arduinoteensy\\cores\\teensy4",
                "C:\\Users\\me\\dev\\fastled5\\src",
            ],
            "compatlib": [],
            "toolchain": [],
        },
    }
}


# ---------------------------------------------------------------------------
# Pure normalization tests
# ---------------------------------------------------------------------------


def test_normalize_include_strips_fbuild_cache_root() -> None:
    fbuild_path = (
        "C:\\Users\\me\\.fbuild\\prod\\cache\\platforms\\"
        "dl-framework-arduinoteensy-1.160.0\\abc\\1.160.0\\cores\\teensy4"
    )
    pio_path = (
        "C:\\Users\\me\\.platformio\\packages\\framework-arduinoteensy\\cores\\teensy4"
    )
    # Both should land on the same tail after stripping the per-backend
    # cache prefix and the framework-version-hash component.
    assert _M._normalize_include(fbuild_path) == _M._normalize_include(pio_path)
    assert _M._normalize_include(fbuild_path).endswith("cores/teensy4")


def test_normalize_define_collapses_pio_version_stamp() -> None:
    assert _M._normalize_define("PLATFORMIO=60119") == "PLATFORMIO"
    assert _M._normalize_define("PLATFORMIO") == "PLATFORMIO"
    # Non-PIO-version defines pass through.
    assert _M._normalize_define("ARDUINO=10819") == "ARDUINO=10819"
    assert _M._normalize_define("USB_SERIAL") == "USB_SERIAL"


def test_should_ignore_dep_machinery() -> None:
    assert _M._should_ignore_flag("-MMD")
    assert _M._should_ignore_flag("-MF")
    assert _M._should_ignore_flag("-MFsome/dep.d")
    assert _M._should_ignore_flag("--target=arm-none-eabi")
    assert _M._should_ignore_flag("-o")
    # Real signal flags must not be ignored.
    assert not _M._should_ignore_flag("-O2")
    assert not _M._should_ignore_flag("-Os")
    assert not _M._should_ignore_flag("-mcpu=cortex-m7")


def test_split_fbuild_combined_flags() -> None:
    split = _M._split_fbuild_combined_flags(
        ["-Os", "-DFOO=1", "-Wall", "-I/path/to/inc", "-DBAR"]
    )
    assert split.pure == ["-Os", "-Wall"]
    assert split.defines == ["FOO=1", "BAR"]
    assert split.includes == ["/path/to/inc"]


def test_clean_flags_drops_operand_after_standalone_path_flag() -> None:
    """``-include header.h`` and friends must consume the operand token too.

    Without this, the trailing path (``header.h``, ``foo.o``, depfile path)
    survives normalization and shows up as false drift signal.
    """
    cleaned = _M._clean_flags(
        [
            "-O2",  # real signal
            "-include",
            "stdint_pre.h",  # operand: must be dropped
            "-o",
            "main.o",  # operand: must be dropped
            "-c",
            "main.cpp",  # operand: must be dropped
            "-MF",
            "main.d",  # -MF is a noise prefix; operand also drops
            "-mcpu=cortex-m7",  # real signal
        ]
    )
    assert cleaned == {"-O2", "-mcpu=cortex-m7"}, (
        f"Expected only the real-signal flags, got {sorted(cleaned)}"
    )


def test_normalize_build_info_fails_fast_on_missing_board() -> None:
    """If the requested board key is absent we must raise, not silently
    fall back to the first top-level record."""
    raw = {"some_other_board": {"cc_flags": ["-O2"]}}
    with pytest.raises(ValueError, match="missing board key 'teensy41'"):
        _M.normalize_build_info(raw, "teensy41", "fbuild")


# ---------------------------------------------------------------------------
# End-to-end normalize + diff
# ---------------------------------------------------------------------------


def test_drift_detects_optimization_level_difference() -> None:
    fbuild_norm = _M.normalize_build_info(_FBUILD_TEENSY41_LIKE, "teensy41", "fbuild")
    pio_norm = _M.normalize_build_info(_PIO_TEENSY41_LIKE, "teensy41", "platformio")
    report = _M.compute_drift(fbuild_norm, pio_norm, "teensy41")
    assert report.has_drift(), (
        "Expected -Os vs -O2 (and ARDUINO version) drift to be reported"
    )
    # -Os is the fbuild signal; -O2 is the pio signal. Both must appear.
    assert "-Os" in report.cc_only_in_fbuild
    assert "-O2" in report.cc_only_in_pio


def test_drift_clean_when_inputs_match_after_normalization() -> None:
    """Identical-after-normalization inputs must report no drift."""
    identical_fbuild = {
        "teensy41": {
            "cc_flags": [
                "-O2",
                "-mcpu=cortex-m7",
                "-DPLATFORMIO",
                "-DARDUINO_TEENSY41",
                "-IC:\\x\\.fbuild\\prod\\cache\\platforms\\dl-fw-arduinoteensy-1\\h\\1\\cores\\teensy4",
            ],
        }
    }
    identical_pio = {
        "teensy41": {
            "cc_flags": ["-O2", "-mcpu=cortex-m7"],
            "cxx_flags": ["-O2", "-mcpu=cortex-m7"],
            "defines": ["PLATFORMIO=60119", "ARDUINO_TEENSY41"],
            "includes": {
                "build": [
                    "C:\\x\\.platformio\\packages\\framework-arduinoteensy\\cores\\teensy4",
                ],
            },
        }
    }
    fbuild_norm = _M.normalize_build_info(identical_fbuild, "teensy41", "fbuild")
    pio_norm = _M.normalize_build_info(identical_pio, "teensy41", "platformio")
    report = _M.compute_drift(fbuild_norm, pio_norm, "teensy41")
    assert not report.has_drift(), (
        f"Normalization should have collapsed everything to equality, "
        f"but got drift: {report}"
    )


def test_render_report_marks_drift_and_clean() -> None:
    drift_report = _M.DriftReport(board="teensy41", cc_only_in_fbuild=["-Os"])
    rendered = _M.render_report(drift_report)
    assert "VERDICT: DRIFT DETECTED" in rendered
    assert "+ (fbuild only) -Os" in rendered

    clean = _M.DriftReport(board="teensy41")
    rendered_clean = _M.render_report(clean)
    assert "VERDICT: clean" in rendered_clean
    assert "OK (no drift)" in rendered_clean
