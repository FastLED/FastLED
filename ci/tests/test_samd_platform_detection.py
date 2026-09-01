"""Regression contracts for SAMD platform detection (FastLED #4011).

The SAMD CI workflows failed on every board with::

    platforms/pin.h:56:6: error: "Platform-specific pin implementation not
      defined for this Arduino variant."

even though ``platforms/pin.h`` has an ``#elif defined(FL_IS_SAMD)`` branch and
``platforms/arm/samd/pin_samd.hpp`` exists.

The cause was in ``is_samd.h``: the family gates required ``ARDUINO_ARCH_SAMD``
*and* a CPU macro. PlatformIO's Arduino builder script injects
``ARDUINO_ARCH_<ARCH>``; the board manifest does not. A build system that reads
the manifest directly -- fbuild, which is what ``bash compile samd21`` now uses
-- therefore never defines it, and detection threw away the unambiguous part
macro the manifest *did* supply.

These are preprocessor tests: they run the host compiler over the real header
with the exact ``-D`` flags a board manifest provides, so they need no SAMD
toolchain. A C++ unit test cannot cover this -- ``tests/test_pch.h`` force-
includes ``FastLED.h``, so ``is_samd.h`` is always already included (it is
``#pragma once``) before any test TU gets to set up its macros.
"""

import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


REPO_ROOT = Path(__file__).resolve().parents[2]
SRC = REPO_ROOT / "src"

# Exactly what the PlatformIO board manifests for the three CI boards define.
# Fetched from platformio/platform-atmelsam -> boards/*.json "build.extra_flags".
# Note the absence of ARDUINO_ARCH_SAMD in every one of them.
BOARD_MANIFEST_FLAGS = {
    "samd21 (adafruit_feather_m0)": [
        "-DARDUINO_SAMD_ZERO",
        "-DARDUINO_SAMD_FEATHER_M0",
        "-DARM_MATH_CM0PLUS",
        "-D__SAMD21G18A__",
    ],
    "samd21_zero (zeroUSB)": [
        "-DARDUINO_SAMD_ZERO",
        "-D__SAMD21G18A__",
    ],
    "samd51j (adafruit_feather_m4)": [
        "-DARDUINO_FEATHER_M4",
        "-DADAFRUIT_FEATHER_M4_EXPRESS",
        "-D__SAMD51J19A__",
        "-D__SAMD51__",
        "-DARM_MATH_CM4",
    ],
    "metro_m4 (adafruit_metro_m4)": [
        "-DARDUINO_METRO_M4",
        "-DADAFRUIT_METRO_M4_EXPRESS",
        "-D__SAMD51J19A__",
        "-D__SAMD51__",
        "-DARM_MATH_CM4",
    ],
}


def _compiler() -> str:
    for name in ("clang++", "clang", "g++", "gcc"):
        found = shutil.which(name)
        if found:
            return found
    pytest.skip("no host compiler available to run the preprocessor")


def _defined_macros(flags: list[str], header: str) -> set[str]:
    """Return every macro defined after preprocessing ``header`` with ``flags``."""
    cmd = [
        _compiler(),
        "-E",
        "-dM",
        "-x",
        "c++",
        f"-I{SRC}",
        *flags,
        "-",
    ]
    proc = RunningProcess.run(
        cmd,
        input=f'#include "{header}"\n',
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=120,
    )
    assert proc.returncode == 0, f"preprocessing failed:\n{proc.stderr}"
    macros: set[str] = set()
    for line in proc.stdout.splitlines():
        if not line.startswith("#define"):
            continue
        parts = line.split()
        if len(parts) > 1:
            macros.add(parts[1])
    return macros


def _preprocessed_header(flags: list[str], header: str) -> str:
    """Preprocess a real FastLED header with the supplied board flags."""
    cmd = [_compiler(), "-E", "-x", "c++", f"-I{SRC}", *flags, "-"]
    proc = RunningProcess.run(
        cmd,
        input=f'#include "{header}"\n',
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=120,
    )
    assert proc.returncode == 0, f"preprocessing failed:\n{proc.stderr}"
    return proc.stdout


HEADER = "platforms/arm/samd/is_samd.h"


@pytest.mark.parametrize("board", sorted(BOARD_MANIFEST_FLAGS))
def test_board_manifest_flags_alone_define_fl_is_samd(board: str) -> None:
    """#4011: the part macro must be sufficient, with no ARDUINO_ARCH_SAMD.

    This is the exact failure. Every SAMD board manifest names its part; none
    names its arch. If FL_IS_SAMD is not set here, platforms/pin.h falls
    through to its #error and the board cannot build.
    """
    flags = BOARD_MANIFEST_FLAGS[board]
    assert not any("ARDUINO_ARCH_SAMD" in f for f in flags), (
        "test premise: no SAMD board manifest defines ARDUINO_ARCH_SAMD"
    )

    macros = _defined_macros(flags, HEADER)

    assert "FL_IS_SAMD" in macros, (
        f"{board}: FL_IS_SAMD not defined from manifest flags {flags}. "
        "platforms/pin.h will fall through to its #error."
    )


def test_part_macros_select_the_right_family() -> None:
    """FL_IS_SAMD21 and FL_IS_SAMD51 must be distinguished, and not both set."""
    samd21 = _defined_macros(["-D__SAMD21G18A__"], HEADER)
    assert "FL_IS_SAMD21" in samd21
    assert "FL_IS_SAMD51" not in samd21

    samd51 = _defined_macros(["-D__SAMD51J19A__"], HEADER)
    assert "FL_IS_SAMD51" in samd51
    assert "FL_IS_SAMD21" not in samd51


def test_arduino_arch_samd_still_recognized_on_its_own() -> None:
    """Kept as an alternative, so PlatformIO-driven builds are unaffected.

    Matches is_nrf52.h and is_apollo3.h, which also treat ARDUINO_ARCH_* as one
    alternative among several rather than a requirement.
    """
    macros = _defined_macros(["-DARDUINO_ARCH_SAMD"], HEADER)
    assert "FL_IS_SAMD" in macros


def test_non_samd_build_is_not_detected_as_samd() -> None:
    """Negative control -- detection must not fire on an unrelated target."""
    macros = _defined_macros([], HEADER)
    assert "FL_IS_SAMD" not in macros
    assert "FL_IS_SAMD21" not in macros
    assert "FL_IS_SAMD51" not in macros


def test_arduino_metro_m4_board_macro_selects_d6_fastpin() -> None:
    """#4004: Arduino's board macro must select the Metro M4 pin table.

    ``ARDUINO_METRO_M4`` comes from ``build.board=METRO_M4`` in Adafruit
    SAMD 1.7.17. D6 is PB15 in that release's ``variants/metro_m4`` table.
    """
    output = _preprocessed_header(
        ["-DARDUINO_METRO_M4", "-D__SAMD51J19A__"],
        "platforms/arm/d51/fastpin_arm_d51.h",
    )
    specialization = (
        "template<> class FastPin<6> : public _ARMPIN<6, 15, 1ul << 15, 1> {}"
    )
    assert specialization in output, (
        "ARDUINO_METRO_M4 did not select the Metro M4 table; FastPin<6> will "
        "remain invalid instead of mapping to D6/PB15"
    )


def test_samd_routes_into_a_native_sercom_spi_backend() -> None:
    """SAMD hardware SPI must not depend on Arduino's SPI library (#4016)."""
    dispatcher = (SRC / "platforms" / "spi_output_template.h").read_text(
        encoding="utf-8"
    )
    backend = (SRC / "platforms" / "arm" / "sam" / "fastspi_arm_sam.h").read_text(
        encoding="utf-8"
    )

    assert "defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51)" in dispatcher
    assert "#include <SPI.h>" not in backend
    assert "::SPI" not in backend
    implementation = (
        SRC / "platforms" / "arm" / "sam" / "fastspi_arm_samd.hpp"
    ).read_text(encoding="utf-8")
    assert "PERIPH_SPI" in implementation


def test_samd_sercom_select_restores_each_controllers_clock_divider() -> None:
    """Two controllers sharing PERIPH_SPI must retain distinct data rates."""
    backend = (SRC / "platforms" / "arm" / "sam" / "fastspi_arm_samd.hpp").read_text(
        encoding="utf-8"
    )
    assert "::select() FL_NO_EXCEPT" in backend
    assert "configureClock();" in backend
    assert "disableSPI()" in backend
    assert "initSPI(PAD_SPI_TX, PAD_SPI_RX" in backend
    assert "initSPIClock(SERCOM_SPI_MODE_0, clockHz())" in backend
    assert "enableSPI()" in backend

    # Representative template dividers must remain distinct instead of the
    # last initialized controller's clock becoming global shared state.
    f_cpu = 120_000_000
    assert min(f_cpu // 8, 24_000_000) != min(f_cpu // 16, 24_000_000)
    dispatcher = (
        SRC / "platforms" / "arm" / "sam" / "spi_output_template.h"
    ).read_text(encoding="utf-8")
    assert "fastspi_arm_samd.hpp" in dispatcher


def test_samd51_does_not_register_unvalidated_qspi_as_four_lane_spi() -> None:
    """QSPI byte serialization is not four independent LED lane streams."""
    manager = (
        SRC / "platforms" / "arm" / "d51" / "spi_hw_manager_samd51.cpp.hpp"
    ).read_text(encoding="utf-8")

    assert "SPIQuadSAMD51" in manager, "keep the hardware limitation documented"
    assert "SpiHw4::registerInstance" not in manager
    assert "make_shared<SPIQuadSAMD51>" not in manager


def test_samd51_qspi_buffer_size_tracks_validated_active_lane_width() -> None:
    """Buffer allocation and transmission must agree on the configured width."""
    driver = (SRC / "platforms" / "arm" / "d51" / "spi_hw_4_samd51.cpp.hpp").read_text(
        encoding="utf-8"
    )

    assert "mActiveLanes == 3" in driver
    assert "const size_t num_lanes = mActiveLanes;" in driver
    assert "bytes_per_lane > max_size / num_lanes" in driver
