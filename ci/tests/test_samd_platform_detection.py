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


def test_samd_does_not_route_into_the_arduino_spi_backend() -> None:
    """SAMD must not claim platforms/arm/sam/ hardware SPI while #4016 is open.

    That backend does `#include <SPI.h>`, and nothing available can supply it
    under fbuild: lib_ldf_mode is unimplemented, the scan reaches neither an
    `#if 0` LDF hint nor a conditional include, and `lib_deps = SPI` fails with
    "library 'SPI' not found in registry" because framework-bundled libraries
    are not registry packages (FastLED/fbuild#1371).

    It also had never compiled on any board -- FL_IS_SAMD21/FL_IS_SAMD51 never
    evaluated true until #4011 -- so routing SAMD to bit-bang SPI preserves the
    behaviour every SAMD build has actually had, rather than switching on
    untested code. #4016 tracks a real SERCOM backend.
    """
    for name in ("spi_device_proxy.h", "spi_output_template.h"):
        text = (SRC / "platforms" / name).read_text(encoding="utf-8")
        assert "defined(FL_IS_SAM) || defined(FL_IS_SAMD)" not in text, (
            f"platforms/{name} routes SAMD into platforms/arm/sam/, whose SPI "
            "backend needs Arduino <SPI.h>. SAMD builds fail with 'SPI.h: No "
            "such file or directory'. See #4011, #4016, FastLED/fbuild#1371."
        )
