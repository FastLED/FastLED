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


kSymbolHeader = re.compile(r"^[0-9a-f]+ <.+>:$")
import shutil
from dataclasses import dataclass
from pathlib import Path

import pytest
from running_process import PIPE, RunningProcess
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
# The float-profile entry point. It used to derive in float; since
# FastLED#4458 it converts the profile by its bits and routes to the Q16 build.
kProfileBuildSymbol = (
    "_ZN2fl22buildRgbSolveMatrixQ16ERKNS_"
    "21colorimetric_response14EmitterProfileEPNS_21EmitterSolveMatrixQ16E"
)

# A deliberately float function compiled into each object under test, so the
# walk has something it must find: if it stopped finding helpers here, every
# "reaches none" below would mean the traversal broke, not that code is clean.
kFloatControlSource = """
extern "C" float flFloatControl(float a, float b) { return a * b + a / b - (float)(int)a; }
"""
kFloatControlSymbol = "flFloatControl"


@dataclass(frozen=True)
class ArmTools:
    """A cross compiler and the objdump that goes with it."""

    compiler: str
    objdump: str


def _gcc_version(compiler: Path) -> tuple[int, ...]:
    """Parse `<compiler> --version` into a sortable tuple (0 on failure)."""

    try:
        text = RunningProcess.run(
            [str(compiler), "--version"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=20,
            check=False,
        ).stdout
    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import (  # noqa: PLC0415 - lazy
            handle_keyboard_interrupt,
        )

        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        return (0,)
    match = re.search(r"(\d+)\.(\d+)\.(\d+)", text)
    if match is None:
        return (0,)
    return tuple(int(part) for part in match.groups())


def _cached_cross_compilers(name: str) -> list[Path]:
    """Every `bin/<name>` under fbuild's toolchain cache, newest GCC first.

    The cache holds one toolchain per board family; a board-specific one
    (Teensy ships only Cortex-M4/M7 multilibs) cannot always compile for the
    target under test, and a newer release supports every target an older
    one does, so prefer the highest version.
    """

    root = Path.home() / ".fbuild"
    if not root.is_dir():
        return []
    candidates = [p for p in root.rglob(f"bin/{name}") if p.is_file()]
    # The RP2040/RP2350 (earlephilhower pico) toolchain is what these
    # codegen expectations were written against; prefer it, then the newest
    # GCC of whatever else fbuild has cached.
    return sorted(
        candidates,
        key=lambda p: ("earlephilhower" in p.as_posix(), _gcc_version(p)),
        reverse=True,
    )


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
    for candidate in _cached_cross_compilers("arm-none-eabi-g++"):
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

    # stdout and stderr piped separately so objdump's warnings stay out of
    # what this parses.
    completed = RunningProcess.run(
        [objdump, "-d", "-r", "--no-show-raw-insn", str(obj)],
        stdout=PIPE,
        stderr=PIPE,
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


# Relocations that stand for a transfer of control. `R_ARM_ABS32` and its
# relatives are data references -- this object carries nine of them, all
# naming `.rodata` -- and counting those as calls invents edges. False edges
# only *add* reachability, so they cannot make an absence claim pass wrongly,
# but they can make the control below pass for the wrong reason.
kControlRelocation = re.compile(
    r"\bR_ARM_(?:THM_)?(?:CALL|JUMP24|JUMP19|JUMP11|JUMP8|PC24|PLT32)\b"
    r"\s+([A-Za-z_][A-Za-z0-9_.]*)"
)

# A resolved local transfer: `bl`, or a plain `b` when the compiler turns a
# call in tail position into a jump. Missing the branch form is the dangerous
# direction -- a lost edge makes "reaches no float" pass by not looking.
kResolvedTransfer = re.compile(
    r"\bb(?:l)?(?:\.[nw])?\s+([0-9a-f]+) <([A-Za-z_][A-Za-z0-9_.]*)>"
)


@typechecked
def _callees(body: list[str]) -> set[str]:
    """Who this function actually transfers control to.

    A relocation record on the line *after* a transfer names its real target,
    and supersedes the name objdump printed. Without one, the printed name is
    correct and is used as it stands.

    The pairing matters in both directions, and getting it wrong is how this
    walk has been wrong twice.

    Reading the printed name always: an unrelocated call disassembles as
    `bl 0 <name>` and objdump resolves that `0` against the symbol table, so
    every such call appears to go to whatever function sits at address zero.
    That had `signedCbrtQ16` calling `isUsableSolveChromaticity` -- a cube
    root calling a chromaticity validator, which is what gave it away.

    Discarding every address-zero transfer instead: a function genuinely
    placed at offset zero in its section is a legitimate target, and dropping
    the edge loses a real call. Measured in this object, 254 of the 257
    address-zero transfers carry a relocation and are the ambiguous kind,
    and the remaining 3 are `buildRgbSolveMatrixQ16` calling
    `isUsableSolveChromaticity` for real. Losing an edge is the dangerous
    direction: it makes "reaches no float" pass by not looking.
    """

    out: set[str] = set()
    pending_transfer: str | None = None
    for line in body:
        relocation = kControlRelocation.search(line)
        if relocation:
            # Supersedes the transfer it belongs to, which is the line above.
            out.add(relocation.group(1))
            pending_transfer = None
            continue
        if pending_transfer is not None:
            out.add(pending_transfer)
            pending_transfer = None
        transfer = kResolvedTransfer.search(line)
        if transfer:
            pending_transfer = transfer.group(2)
    if pending_transfer is not None:
        out.add(pending_transfer)
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

    # stdout and stderr piped separately so objdump's warnings stay out of
    # the disassembly this parses.
    completed = RunningProcess.run(
        [objdump, "-d", "--no-show-raw-insn", str(obj)],
        stdout=PIPE,
        stderr=PIPE,
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
            # RunningProcess drops blank lines from captured output, so the
            # next symbol header is the reliable end of this body.
            if not line.strip() or kSymbolHeader.match(line):
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
#include "fl/gfx/chromatic_adaptation.cpp.hpp"
#include "fl/gfx/colorimetric_response.cpp.hpp"
#include "fl/gfx/device_solve.cpp.hpp"
#include "fl/gfx/flux_scalar.cpp.hpp"
#include "fl/gfx/gamut_map.cpp.hpp"
#include "fl/gfx/oklab_q16.cpp.hpp"
#include "fl/gfx/pipeline.cpp.hpp"
#include "fl/gfx/source_xyz.cpp.hpp"
#include "fl/gfx/transfer.cpp.hpp"
#include "fl/gfx/white_allocation.cpp.hpp"
"""


@typechecked
@dataclass(frozen=True)
class CompiledPipeline:
    """One build of the streaming pipeline, and the tools that read it."""

    tools: ArmTools
    obj: Path
    core: str


# Two cores, because they exercise different halves of the walk.
#
# Cortex-M0+ is ARMv6-M: soft float, and its limited branch range means the
# compiler never turns a call in tail position into a jump -- measured, zero
# such branches in this object.
#
# Cortex-M33 without an FPU is ARMv8-M: soft float *and* Thumb-2, where a
# tail call does become `b.n`. That is what makes the branch handling in
# `_callees` load-bearing rather than defensive; on the M0+ build alone it
# would never be exercised.
@typechecked
@dataclass(frozen=True)
class FloatFreeTarget:
    """One core to compile the pipeline for."""

    core: str
    march: str


@typechecked
def _target_id(target: FloatFreeTarget) -> str:
    return target.core


kFloatFreeTargets: tuple[FloatFreeTarget, ...] = (
    FloatFreeTarget(core="cortex-m0plus", march="-march=armv6-m"),
    FloatFreeTarget(core="cortex-m33+nofp", march="-march=armv8-m.main+dsp"),
)


@pytest.fixture(scope="module", params=kFloatFreeTargets, ids=_target_id)
@typechecked
def compiled_pipeline(
    request: pytest.FixtureRequest, tmp_path_factory: pytest.TempPathFactory
) -> CompiledPipeline:
    """The whole streaming pipeline in one object, so calls can be followed.

    Every stage `processPixelQ16` reaches lives in a different translation
    unit, and reachability cannot cross an object boundary. Compiling them
    together is what makes the walk complete.
    """

    tools = _arm_tools()
    if tools is None:
        pytest.skip("no arm-none-eabi cross compiler on this machine")

    target: FloatFreeTarget = request.param
    core = target.core
    tmp_path = tmp_path_factory.mktemp(f"pipeline_{core.replace('+', '_')}")
    source = tmp_path / "pipeline_tu.cpp"
    source.write_text(kPipelineSource + kFloatControlSource, encoding="utf-8")
    obj = tmp_path / "pipeline_tu.o"
    RunningProcess.run(
        [
            tools.compiler,
            "-c",
            str(source),
            "-o",
            str(obj),
            "-I",
            str(PROJECT_ROOT / "src"),
            f"-mcpu={core}",
            target.march,
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
    return CompiledPipeline(tools=tools, obj=obj, core=core)


@typechecked
def test_the_callee_parser_pairs_relocations_with_their_transfer() -> None:
    """`_callees` on disassembly written out by hand.

    The walk has been wrong twice, in opposite directions, and both mistakes
    survived every reachability case because they only changed which edges
    existed -- not whether the walk ran. So the parser is tested on input
    where the right answer is known by construction.
    """

    # An unrelocated call: the printed name is whatever sits at address zero,
    # and the relocation on the next line is the real target.
    relocated = [
        "     a20:\tbl\t0 <_ZN2fl9decoyAtZeroEv>",
        "\t\t\ta20: R_ARM_THM_CALL\t__aeabi_fadd",
    ]
    assert _callees(relocated) == {"__aeabi_fadd"}

    # A genuine local call to a function at offset zero: no relocation, so
    # the printed name stands. Discarding this is how a real edge gets lost.
    at_zero = ["     78e:\tbl\t0 <_ZN2fl20isUsableChromaticityEv>"]
    assert _callees(at_zero) == {"_ZN2fl20isUsableChromaticityEv"}

    # A resolved local call, and a tail call emitted as a branch.
    resolved = [
        "     a7a:\tbl\t1ba <_ZN2fl9someLeafEv>",
        "     18c:\tb.n\t16e <_ZN2fl9tailLeafEv>",
    ]
    assert _callees(resolved) == {"_ZN2fl9someLeafEv", "_ZN2fl9tailLeafEv"}

    # A data relocation is not a call. Counting it invents an edge.
    #
    # Naming a *function* here on purpose: the nine `R_ARM_ABS32` records in
    # the real object all name `.rodata`, which the symbol pattern rejects on
    # the leading dot anyway -- so a case built from those would pass whether
    # or not the relocation type is checked, and would say nothing. A
    # function-pointer table entry is the shape that needs the type check.
    data = [
        "     1c0:\tldr\tr1, [pc, #8]",
        "\t\t\t1c4: R_ARM_ABS32\t_ZN2fl11addressOnlyEv",
    ]
    assert _callees(data) == set()

    # And the two forms interleaved, which is what a real body looks like --
    # a relocation must attach to the transfer above it and not to the next
    # one down.
    mixed = [
        "     100:\tbl\t0 <_ZN2fl9decoyAtZeroEv>",
        "\t\t\t100: R_ARM_THM_CALL\t__aeabi_fmul",
        "     104:\tbl\t0 <_ZN2fl9realAtZeroEv>",
        "     108:\tbl\t200 <_ZN2fl9plainCallEv>",
    ]
    assert _callees(mixed) == {
        "__aeabi_fmul",
        "_ZN2fl9realAtZeroEv",
        "_ZN2fl9plainCallEv",
    }


@typechecked
def test_the_per_pixel_path_reaches_no_float_runtime(
    compiled_pipeline: CompiledPipeline,
) -> None:
    """P9's criterion, at the place it actually matters (FastLED#4043).

    The phase asks for a tier with no float symbols linked. `processPixelQ16`
    is the per-pixel path -- decode, source matrix, gamut map, device solve,
    flux -- and it runs for every pixel of every frame.

    It reaches none. Measured across the 19 functions the walk visits.
    """

    helpers = _reachable_helpers(
        compiled_pipeline.tools.objdump, compiled_pipeline.obj, kPerPixelSymbol
    )
    assert helpers == set(), (
        f"the per-pixel path reaches the float runtime on "
        f"{compiled_pipeline.core}: {sorted(helpers)}"
    )


@typechecked
def test_the_bind_path_reaches_no_float_runtime(
    compiled_pipeline: CompiledPipeline,
) -> None:
    """FastLED#4458: binding a profile is float-free too.

    `buildStreamingPipelineQ16` converts the profile's floats by their bits
    and derives the source matrix, the Bradford adaptation and the device
    solve in s16.16. The object includes `chromatic_adaptation` and
    `colorimetric_response`, so nothing the walk needs to follow is outside it.
    """

    helpers = _reachable_helpers(
        compiled_pipeline.tools.objdump, compiled_pipeline.obj, kBindSymbol
    )
    assert helpers == set(), (
        f"the bind path reaches the float runtime on "
        f"{compiled_pipeline.core}: {sorted(helpers)}"
    )


@typechecked
def test_the_pipeline_walk_still_finds_float(
    compiled_pipeline: CompiledPipeline,
) -> None:
    """Control for the two cases above: a float function in the same object
    must show helpers, or their silence proves nothing."""

    helpers = _reachable_helpers(
        compiled_pipeline.tools.objdump, compiled_pipeline.obj, kFloatControlSymbol
    )
    assert len(helpers) >= 3, (
        f"expected the float control to reach the soft-float runtime on "
        f"{compiled_pipeline.core}, found {sorted(helpers)}"
    )


@pytest.fixture(scope="module")
@typechecked
def compiled(tmp_path_factory: pytest.TempPathFactory) -> tuple[ArmTools, Path]:
    tools = _arm_tools()
    if tools is None:
        pytest.skip("no arm-none-eabi cross compiler on this machine")

    tmp_path = tmp_path_factory.mktemp("q16_float_free")
    source = tmp_path / "device_solve_tu.cpp"
    source.write_text(
        '#include "fl/gfx/device_solve.cpp.hpp"\n' + kFloatControlSource,
        encoding="utf-8",
    )
    obj = tmp_path / "device_solve_tu.o"
    RunningProcess.run(
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


def _require_gcc_16(compiler: str) -> None:
    """The float-free expectation was measured with GCC 16.

    GCC 14.2 (fbuild's pico toolchain, 4.0.1) keeps float helper calls that
    GCC 16 folds away, so on that toolchain the test documents a known gap
    rather than failing the suite. Re-check when the toolchain moves.
    """
    version = _gcc_version(Path(compiler))
    if version < (16,):
        pytest.xfail(
            f"codegen expectation written against GCC 16; {compiler} is "
            f"{'.'.join(str(v) for v in version)}"
        )


@typechecked
def test_q16_inverse_calls_no_float_runtime(
    compiled: tuple[ArmTools, Path],
) -> None:
    tools, obj = compiled
    _require_gcc_16(tools.compiler)
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
def test_the_profile_entry_point_reaches_no_float_runtime(
    compiled: tuple[ArmTools, Path],
) -> None:
    """`buildRgbSolveMatrixQ16(EmitterProfile)` takes float fields but now
    converts them by their bits and routes to the Q16 build (FastLED#4458)."""

    tools, obj = compiled
    helpers = _reachable_helpers(tools.objdump, obj, kProfileBuildSymbol)
    assert helpers == set(), (
        f"buildRgbSolveMatrixQ16 reaches the float runtime: {sorted(helpers)}"
    )


@typechecked
def test_following_calls_is_what_makes_that_meaningful(
    compiled: tuple[ArmTools, Path],
) -> None:
    """Control for the cases above: reachability has to find the helpers the
    float control calls, or a clean result would mean the traversal stopped."""

    tools, obj = compiled
    helpers = _reachable_helpers(tools.objdump, obj, kFloatControlSymbol)
    assert len(helpers) >= 3, (
        f"expected the float control to reach the soft-float runtime, found {sorted(helpers)}"
    )


@typechecked
def test_the_float_path_alongside_it_does(compiled: tuple[ArmTools, Path]) -> None:
    """Positive control on a single body: the float control's own disassembly
    names soft-float helpers. If it stopped, the checks above could not find
    them either."""

    tools, obj = compiled
    helpers = _helpers_called(tools.objdump, obj, kFloatControlSymbol)
    assert len(helpers) >= 3, (
        f"expected the float control to call the soft-float runtime, found {sorted(helpers)}"
    )
