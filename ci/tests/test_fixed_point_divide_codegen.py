"""Q16.16 division stops calling libgcc on a core with a 32-bit divider.

FastLED#4307 measured `s16x16` division at 47x the cost of `s8x8` on an
RP2350, and named the cause: `s8x8`'s 32-bit intermediate lowers to a single
`SDIV`, while the 16.16 types need a 64-bit numerator that Cortex-M33 has no
instruction for, so the compiler calls `__aeabi_ldivmod`.

The runtime figures need the board. What can be checked here is the thing the
issue identifies as the cause -- whether that call is emitted -- so this
compiles both operators for several ARM configurations and reads the
disassembly.

**These cases skip in CI.** No workflow installs an `arm-none-eabi` toolchain,
so what runs there is the skip, and the evidence is local. Making the
toolchain a CI prerequisite is a change to the CI image rather than to this
file, and failing hard instead of skipping would break every contributor
machine without it. Tracked separately.

The Cortex-M0+ case is not a second example, it is the gate's control. That
core has no hardware divider, so the replacement would make two libgcc calls
where the wide path makes one, and the gate is supposed to leave it alone. A
test that only looked at the M33 would pass just as happily if the gate were
`#define ... 1`.
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

# 64-bit integer division helpers, in libgcc's ARM EABI and generic spellings.
# Not `__aeabi_uidiv`, which is the *32*-bit helper a core without a divider
# uses and which is not what this is about.
kWideDivideHelper = re.compile(r"__aeabi_u?ldivmod|__u?divdi3")

kSignedSymbol = "s_div"
kUnsignedSymbol = "u_div"

kProbeSource = """
#include "fl/math/fixed_point/s16x16.h"
#include "fl/math/fixed_point/u16x16.h"
extern "C" fl::i32 s_div(fl::i32 a, fl::i32 b) {
    return (fl::s16x16::from_raw(a) / fl::s16x16::from_raw(b)).raw();
}
extern "C" fl::u32 u_div(fl::u32 a, fl::u32 b) {
    return (fl::u16x16::from_raw(a) / fl::u16x16::from_raw(b)).raw();
}
"""


@typechecked
@dataclass(frozen=True)
class ArmTools:
    """A cross compiler and the objdump that goes with it."""

    compiler: str
    objdump: str


@typechecked
@dataclass(frozen=True)
class Emitted:
    """What one function's disassembly contains."""

    instructions: int
    wide_helpers: frozenset[str]
    hardware_divides: int


@typechecked
def _arm_tools() -> ArmTools | None:
    """A complete pair, from PATH if it has one and the caches otherwise."""

    found = shutil.which("arm-none-eabi-g++")
    dump = shutil.which("arm-none-eabi-objdump")
    if found is not None and dump is not None:
        return ArmTools(compiler=found, objdump=dump)
    for root in (Path.home() / ".platformio" / "packages", Path.home() / ".fbuild"):
        if not root.is_dir():
            continue
        for candidate in root.rglob("bin/arm-none-eabi-g++"):
            beside = candidate.with_name("arm-none-eabi-objdump")
            if beside.exists():
                return ArmTools(compiler=str(candidate), objdump=str(beside))
    return None


@typechecked
def _compile_for(
    tools: ArmTools, tmp_path: Path, cpu_flags: list[str], standard: str = "gnu++17"
) -> Path:
    source = tmp_path / "divide_probe.cpp"
    source.write_text(kProbeSource, encoding="utf-8")
    obj = tmp_path / f"divide_probe_{standard.replace('+', 'p')}.o"
    subprocess.run(  # noqa: SRC001
        [
            tools.compiler,
            "-c",
            str(source),
            "-o",
            str(obj),
            "-I",
            str(PROJECT_ROOT / "src"),
            *cpu_flags,
            "-mthumb",
            "-Os",
            f"-std={standard}",
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
    return obj


@typechecked
def _emitted(tools: ArmTools, obj: Path, symbol: str) -> Emitted:
    completed = subprocess.run(  # noqa: SRC001
        [tools.objdump, "-d", "--no-show-raw-insn", str(obj)],
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
    text = "\n".join(body)
    return Emitted(
        instructions=sum(1 for line in body if re.match(r"\s+[0-9a-f]+:", line)),
        wide_helpers=frozenset(kWideDivideHelper.findall(text)),
        hardware_divides=len(re.findall(r"\b[su]div\b", text)),
    )


@pytest.fixture(scope="module")
@typechecked
def tools() -> ArmTools:
    found = _arm_tools()
    if found is None:
        pytest.skip("no arm-none-eabi cross compiler on this machine")
    return found


@typechecked
def test_cortex_m33_divides_in_hardware(tools: ArmTools, tmp_path: Path) -> None:
    """The core FastLED#4307 measured on."""

    flags = ["-mcpu=cortex-m33", "-march=armv8-m.main+fp+dsp", "-mfloat-abi=softfp"]
    # Both ends of the range the gate allows: C++14 is its minimum, and
    # gnu++17 is what the Arduino-Pico core passes for the RP2350 the issue
    # measured on. Checking only the latter would leave the minimum untested.
    for standard in ("gnu++14", "gnu++17"):
        obj = _compile_for(tools, tmp_path, flags, standard)
        for symbol in (kSignedSymbol, kUnsignedSymbol):
            emitted = _emitted(tools, obj, symbol)
            assert emitted.wide_helpers == frozenset(), (
                f"{symbol} at {standard} still calls {sorted(emitted.wide_helpers)}"
            )
            # Two, one per base-2^16 digit. Zero would mean the division moved
            # somewhere this cannot see.
            assert emitted.hardware_divides == 2, (
                f"{symbol} at {standard} emitted {emitted.hardware_divides} "
                "hardware divides"
            )


@typechecked
def test_cpp11_keeps_the_wide_path(tools: ArmTools, tmp_path: Path) -> None:
    """The gate's other boundary.

    `operator/` is `constexpr`, and the replacement cannot be under C++11's
    rule against local variables in a constexpr function -- the repo builds at
    C++11 to match AVR. So a C++11 build for a core that *does* have a divider
    still takes the wide path.

    This is not hypothetical: combining `FL_CONSTEXPR14` with
    `FASTLED_FORCE_INLINE` produced `inline inline` and broke the `clearcore`
    platform build, which is exactly this configuration.
    """

    obj = _compile_for(
        tools,
        tmp_path,
        ["-mcpu=cortex-m4"],
        "gnu++11",
    )
    for symbol in (kSignedSymbol, kUnsignedSymbol):
        emitted = _emitted(tools, obj, symbol)
        assert emitted.wide_helpers != frozenset(), (
            f"{symbol} took the narrow path at C++11, where it cannot be constexpr"
        )


@typechecked
def test_cortex_m0plus_is_left_alone(tools: ArmTools, tmp_path: Path) -> None:
    """The gate's control.

    No hardware divider, so the replacement would be two libgcc calls where
    the wide path makes one. The gate must leave this core on the wide path,
    and the only way to see that is that the helper is still called.
    """

    obj = _compile_for(tools, tmp_path, ["-mcpu=cortex-m0plus", "-march=armv6-m"])
    for symbol in (kSignedSymbol, kUnsignedSymbol):
        emitted = _emitted(tools, obj, symbol)
        assert emitted.wide_helpers != frozenset(), (
            f"{symbol} stopped using the wide divide on a core with no divider"
        )
        assert emitted.hardware_divides == 0, (
            f"{symbol} emitted a hardware divide on ARMv6-M, which has none"
        )
