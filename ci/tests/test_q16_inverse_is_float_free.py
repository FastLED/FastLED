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
kQ16BuildSymbol = (
    "_ZN2fl26buildRgbSolveMatrixFromQ16ERKNS_"
    "24EmitterChromaticitiesQ16EPNS_21EmitterSolveMatrixQ16E"
)
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
def _disassembly(objdump: str, obj: Path) -> dict[str, list[str]]:
    """Every function in the object, keyed by symbol, with relocations.

    `-r` matters. In an unlinked object a call to an undefined symbol
    disassembles as `bl 0 <name>`, and objdump resolves that `0` against the
    symbol table -- so when a *defined* function happens to sit at address
    zero, every unrelocated call in the object appears to go to it. Walking
    those printed names had `signedCbrtQ16` calling
    `isUsableSolveChromaticity`: a cube root calling a chromaticity
    validator, which is what gave it away. The relocation records say where
    the calls really go.
    """

    # `subprocess.run` and not `RunningProcess.run`: the latter merges stderr
    # into stdout, which would put objdump's warnings into what this parses.
    completed = subprocess.run(  # noqa: SRC001
        [objdump, "-d", "-r", "--no-show-raw-insn", str(obj)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=True,
    )
    bodies: dict[str, list[str]] = {}
    current: str | None = None
    for line in completed.stdout.splitlines():
        header = re.match(r"^[0-9a-f]+ <(.+)>:$", line)
        if header:
            current = header.group(1)
            bodies[current] = []
            continue
        if current is not None and line.strip():
            bodies[current].append(line)
        elif current is not None:
            current = None
    return bodies


@typechecked
def _callees(body: list[str]) -> set[str]:
    """Who this function actually calls.

    Two forms, and only two. A relocation record names the real target of a
    call to an undefined symbol; a resolved local call carries a non-zero
    address. A `bl 0 <name>` with no relocation beside it is neither -- that
    is the unrelocated case, and the name objdump prints for it is whatever
    happens to sit at address zero. Following those printed names had this
    walk believing `signedCbrtQ16` called `isUsableSolveChromaticity`: a cube
    root calling a chromaticity validator, which is what gave it away.
    """

    out: set[str] = set()
    for line in body:
        relocation = re.search(r"R_ARM_\w+\s+([A-Za-z_][A-Za-z0-9_.]*)", line)
        if relocation:
            out.add(relocation.group(1))
            continue
        call = re.search(
            r"\bbl(?:\.w)?\s+([0-9a-f]+) <([A-Za-z_][A-Za-z0-9_.]*)>", line
        )
        if call and int(call.group(1), 16) != 0:
            out.add(call.group(2))
    return out


@typechecked
def _reachable_helpers(objdump: str, obj: Path, symbol: str) -> set[str]:
    """Soft-float helpers reachable from `symbol`, following calls.

    Checking one body is not enough for a function that delegates:
    `buildRgbSolveMatrixFromQ16` does its arithmetic in helpers, so a float
    operation there would leave its own body clean.
    """

    bodies = _disassembly(objdump, obj)
    if symbol not in bodies:
        raise AssertionError(f"{symbol} not found in {obj.name}")
    seen: set[str] = set()
    pending = [symbol]
    found: set[str] = set()
    while pending:
        name = pending.pop()
        if name in seen:
            continue
        seen.add(name)
        for callee in _callees(bodies.get(name, [])):
            if kFloatHelper.match(callee):
                found.add(callee)
            elif callee in bodies and callee != name:
                pending.append(callee)
    return found


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


kPerPixelSymbol = "_ZN2fl15processPixelQ16ERKNS_20StreamingPipelineQ16EhhhRA3_l"
kBindSymbol = (
    "_ZN2fl25buildStreamingPipelineQ16ERKNS_13SourceProfileERKNS_"
    "21colorimetric_response14EmitterProfileENS_11GamutPolicyEPNS_"
    "20StreamingPipelineQ16E"
)

kPipelineSource = """
#include "fl/gfx/device_solve.cpp.hpp"
#include "fl/gfx/flux_scalar.cpp.hpp"
#include "fl/gfx/gamut_map.cpp.hpp"
#include "fl/gfx/oklab_q16.cpp.hpp"
#include "fl/gfx/pipeline.cpp.hpp"
#include "fl/gfx/source_xyz.cpp.hpp"
#include "fl/gfx/transfer.cpp.hpp"
#include "fl/gfx/white_allocation.cpp.hpp"
"""


@pytest.fixture(scope="module")
@typechecked
def compiled_pipeline(
    tmp_path_factory: pytest.TempPathFactory,
) -> tuple[ArmTools, Path]:
    """The whole streaming pipeline in one object, so calls can be followed.

    Every stage `processPixelQ16` reaches lives in a different translation
    unit, and reachability cannot cross an object boundary. Compiling them
    together is what makes the walk complete.
    """

    tools = _arm_tools()
    if tools is None:
        pytest.skip("no arm-none-eabi cross compiler on this machine")

    tmp_path = tmp_path_factory.mktemp("pipeline_float_free")
    source = tmp_path / "pipeline_tu.cpp"
    source.write_text(kPipelineSource, encoding="utf-8")
    obj = tmp_path / "pipeline_tu.o"
    subprocess.run(  # noqa: SRC001
        [
            tools.compiler,
            "-c",
            str(source),
            "-o",
            str(obj),
            "-I",
            str(PROJECT_ROOT / "src"),
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
def test_the_per_pixel_path_reaches_no_float_runtime(
    compiled_pipeline: tuple[ArmTools, Path],
) -> None:
    """P9's criterion, at the place it actually matters (FastLED#4043).

    The phase asks for a tier with no float symbols linked. `processPixelQ16`
    is the per-pixel path -- decode, source matrix, gamut map, device solve,
    flux -- and it runs for every pixel of every frame.

    It reaches none. Measured across the 19 functions the walk visits.
    """

    tools, obj = compiled_pipeline
    helpers = _reachable_helpers(tools.objdump, obj, kPerPixelSymbol)
    assert helpers == set(), (
        f"the per-pixel path reaches the float runtime: {sorted(helpers)}"
    )


@typechecked
def test_the_bind_path_is_where_the_float_still_is(
    compiled_pipeline: tuple[ArmTools, Path],
) -> None:
    """Control for the case above, and a statement of what remains.

    If the walk reported nothing for both, it would prove the traversal
    stopped rather than that the pipeline is clean. `buildStreamingPipelineQ16`
    is in the same object and derives its matrices in float, so it must show
    the helpers the per-pixel path does not.

    That float is confined to bind time is P9 item 2's remaining work rather
    than a defect here -- `buildRgbSolveMatrixFromQ16` is the float-free
    counterpart, and nothing routes to it yet.
    """

    tools, obj = compiled_pipeline
    helpers = _reachable_helpers(tools.objdump, obj, kBindSymbol)
    assert len(helpers) >= 8, (
        f"expected the bind path to reach the soft-float runtime, found {sorted(helpers)}"
    )


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
def test_the_q16_build_reaches_no_float_runtime(
    compiled: tuple[ArmTools, Path],
) -> None:
    """The other half of P9 item 2 (FastLED#4043).

    `buildRgbSolveMatrixFromQ16` derives the emitter matrix from Q16
    chromaticities and inverts it, so a float anywhere in that chain would
    defeat the point of having it. It delegates, so this follows the calls
    rather than reading one body.
    """

    tools, obj = compiled
    helpers = _reachable_helpers(tools.objdump, obj, kQ16BuildSymbol)
    assert helpers == set(), (
        f"buildRgbSolveMatrixFromQ16 reaches the float runtime: {sorted(helpers)}"
    )


@typechecked
def test_following_calls_is_what_makes_that_meaningful(
    compiled: tuple[ArmTools, Path],
) -> None:
    """Control for the case above.

    `buildRgbSolveMatrixQ16` is the float path and calls into
    `colorimetric_response` helpers. Reachability has to find those; if it
    reported nothing here, the clean result above would mean the traversal
    stopped, not that the chain is clean.
    """

    tools, obj = compiled
    helpers = _reachable_helpers(tools.objdump, obj, kFloatPathSymbol)
    assert len(helpers) >= 8, (
        f"expected the float path to reach the soft-float runtime, found {sorted(helpers)}"
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
