"""`invert3x3Q16` links no floating-point runtime.

P9 (FastLED#4043) asks for a colorimetric build and solve that no float
symbol reaches, on tiers where float is soft. Saying so is easy and checking
it on the host is worthless: x86 has hardware float, so a float operation
leaves no call behind to find.

So this compiles the implementation for a Cortex-M0+ -- no FPU, every float
operation a call into `libgcc` -- and reads the calls out of the two
functions' disassembly. On that target the difference is not subtle.

The positive control matters as much as the assertion: `buildRgbSolveMatrixQ16`
is the existing float path in the same translation unit, and it must show the
helpers this test says `invert3x3Q16` does not. Without it, a typo in a symbol
name would make the check pass by disassembling nothing.
"""

from __future__ import annotations

import re
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

import pytest
from typeguard import typechecked


PROJECT_ROOT = Path(__file__).resolve().parents[2]

# Soft-float entry points in libgcc's ARM EABI naming: `__aeabi_f*` for
# float, `__aeabi_d*` for double, plus the generic `__*sf3` / `__*df3` names
# a non-EABI build would emit. Deliberately not `__aeabi_l*`, which is 64-bit
# *integer* -- `invert3x3Q16` uses `__aeabi_ldivmod` and `__aeabi_lmul` and is
# meant to.
kFloatHelper = re.compile(r"__aeabi_[fd][a-z0-9]*|__[a-z]+(?:sf|df)3")

kQ16Symbol = "_ZN2fl12invert3x3Q16ERA3_A3_KlRA3_A3_l"
kFloatPathSymbol = (
    "_ZN2fl22buildRgbSolveMatrixQ16ERKNS_"
    "21colorimetric_response14EmitterProfileEPNS_21EmitterSolveMatrixQ16E"
)


@dataclass(frozen=True)
class ArmTools:
    """A cross compiler and the objdump that goes with it."""

    compiler: str
    objdump: str


@typechecked
def _arm_tools() -> ArmTools | None:
    """A complete pair, from PATH if it has one and the caches otherwise.

    Both halves are needed, so a `PATH` carrying only the compiler falls
    through to the caches rather than reporting no toolchain -- which would
    skip this test on a machine that has a usable one.
    """

    found = shutil.which("arm-none-eabi-g++")
    dump = shutil.which("arm-none-eabi-objdump")
    if found is not None and dump is not None:
        return ArmTools(compiler=found, objdump=dump)
    for root in (Path.home() / ".platformio" / "packages", Path.home() / ".fbuild"):
        if not root.is_dir():
            continue
        for candidate in root.rglob("bin/arm-none-eabi-g++"):
            dump = candidate.with_name("arm-none-eabi-objdump")
            if dump.exists():
                return ArmTools(compiler=str(candidate), objdump=str(dump))
    return None


@typechecked
def _helpers_called(objdump: str, obj: Path, symbol: str) -> set[str]:
    """Soft-float helpers named inside one function's disassembly.

    Raises if the symbol is absent, so a rename cannot turn this into a test
    that passes by reading an empty body.
    """

    # `subprocess.run` and not `RunningProcess.run`: the latter merges stderr
    # into stdout, which would put objdump's warnings into the disassembly
    # this parses.
    completed = subprocess.run(  # noqa: SRC001
        [objdump, "-d", "--no-show-raw-insn", str(obj)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=True,
    )
    body: list[str] = []
    inside = False
    for line in completed.stdout.splitlines():
        if line.endswith(f"<{symbol}>:"):
            inside = True
            continue
        if inside:
            if not line.strip():
                break
            body.append(line)
    if not body:
        raise AssertionError(f"{symbol} not found in {obj.name}")
    return set(kFloatHelper.findall("\n".join(body)))


@pytest.fixture(scope="module")
@typechecked
def compiled(tmp_path_factory: pytest.TempPathFactory) -> tuple[ArmTools, Path]:
    tools = _arm_tools()
    if tools is None:
        pytest.skip("no arm-none-eabi cross compiler on this machine")

    tmp_path = tmp_path_factory.mktemp("q16_float_free")
    source = tmp_path / "device_solve_tu.cpp"
    source.write_text('#include "fl/gfx/device_solve.cpp.hpp"\n', encoding="utf-8")
    obj = tmp_path / "device_solve_tu.o"
    subprocess.run(  # noqa: SRC001
        [
            tools.compiler,
            "-c",
            str(source),
            "-o",
            str(obj),
            "-I",
            str(PROJECT_ROOT / "src"),
            # Cortex-M0+: ARMv6-M, no FPU, so every float operation becomes a
            # call. RP2040 and Teensy LC are this core.
            "-mcpu=cortex-m0plus",
            "-march=armv6-m",
            "-mthumb",
            "-Os",
            "-std=gnu++17",
            "-ffreestanding",
            "-fno-exceptions",
            "-fno-rtti",
            "-DARDUINO_ARCH_RP2040",
        ],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=True,
    )
    return tools, obj


@typechecked
def test_q16_inverse_calls_no_float_runtime(
    compiled: tuple[ArmTools, Path],
) -> None:
    tools, obj = compiled
    helpers = _helpers_called(tools.objdump, obj, kQ16Symbol)
    assert helpers == set(), (
        f"invert3x3Q16 reaches the float runtime: {sorted(helpers)}"
    )


@typechecked
def test_the_float_path_alongside_it_does(compiled: tuple[ArmTools, Path]) -> None:
    """Positive control.

    `buildRgbSolveMatrixQ16` is in the same object and does its work in float.
    If this stops finding helpers, the check above has stopped being able to
    find them either, and its silence means nothing.

    Measured on GCC 16.1 for cortex-m0plus at -Os: 11 distinct helpers.
    """

    tools, obj = compiled
    helpers = _helpers_called(tools.objdump, obj, kFloatPathSymbol)
    assert len(helpers) >= 8, (
        f"expected the float path to call the soft-float runtime, found {sorted(helpers)}"
    )
