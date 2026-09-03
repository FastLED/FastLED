"""The int_asm macros must keep compiling to the instruction we chose.

`fl::math::mulshift32` exists because writing the C idiom and trusting the
optimiser to find `mulh` / `smull` is not a contract. These are inline functions rather than
macros, which adds a second thing to verify: a macro always expands, an inline
function is a request. FASTLED_FORCE_INLINE covers that -- measured, a plain
`inline` went out of line under -fno-inline and this does not.

Scope of the guarantee, stated honestly: the single-instruction form holds at
-O2 and -Os, with or without -fno-inline. It does NOT hold at -O0 or -O1,
because folding to `mulh` is an optimisation GCC only runs higher up; at -O1 the
same source emits `mul mul add mulhu add`. Nothing ships at -O0, and the
operation ledger counts IR-level multiplies rather than machine instructions, so
that is fine -- but it is a property worth knowing rather than assuming. It holds today on every toolchain here,
and when it stops holding it does so silently: the expression still produces the
right value, just via a full 32x32->64 multiply, a 64-bit shift and a narrowing.
Measured on riscv32-esp-elf-g++ -Os that is 40 instructions against 2.

A 20x regression with no diagnostic is precisely what a test is for. These
compile the macros for each cross-target available and fail if the emitted
sequence stops being a single arithmetic instruction plus a return.
"""

from __future__ import annotations

import re
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest
from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[2]

SOURCE = """
// Prelude copied from ci/codec_cpu/minimp3_fixed_codegen.cpp, which is the real
// consumer. Order matters: compiler_control.h has to precede the stdint shim,
// and the ESP toolchains need that shim reached with ESP32 defined. Getting
// either wrong produces conflicting size_t/ptrdiff_t typedefs rather than a
// codegen result, which is a property of the probe, not of the header.
#include "fl/stl/compiler_control.h"

#if defined(FL_CODEC_CPU_CODEGEN_ESP_TYPES)
#define ESP32
#include "fl/stl/stdint.h"
#undef ESP32
#else
#include "fl/stl/stdint.h"
#endif

#include "fl/math/int_asm.h"

fl::i32 fl_test_mulshift(fl::i32 a, fl::i32 b) { return fl::math::mulshift32(a, b); }
fl::i32 fl_test_wrap_add(fl::i32 a, fl::i32 b) { return fl::math::wrap_add32(a, b); }
fl::i32 fl_test_wrap_sub(fl::i32 a, fl::i32 b) { return fl::math::wrap_sub32(a, b); }
fl::i32 fl_test_round27(fl::i32 a, fl::i32 b) { return fl::math::mul_shift_round32<27>(a, b); }
"""

# (target, compiler basename, extra flags, expected mnemonic per function).
# The mnemonic differs by ISA -- riscv32 has mulh, Cortex-M4 uses smull and
# takes the high register for free -- so each target names its own.
TARGETS = [
    (
        "riscv32-esp",
        "riscv32-esp-elf-g++",
        ["-DFL_CODEC_CPU_CODEGEN_ESP_TYPES"],
        {"mulshift": "mulh", "wrap_add": "add", "wrap_sub": "sub"},
    ),
    (
        "cortex-m4",
        "arm-none-eabi-g++",
        # -DARDUINO_ARCH_NRF52 is what ci/codec_cpu/audit.py passes for its ARM
        # codegen targets; without a platform define the fl:: integer shims
        # collide with the toolchain's own size_t/ptrdiff_t. Same flags as the
        # audit so this measures the same build.
        ["-mcpu=cortex-m4", "-mthumb", "-DARDUINO_ARCH_NRF52"],
        {"mulshift": "smull", "wrap_add": "add", "wrap_sub": "sub"},
    ),
]

# One arithmetic instruction plus a return. Generous by one to tolerate a
# register move; the failure this guards against is 40, not 3.
MAX_INSTRUCTIONS = 3

# The rounded multiply cannot be one instruction anywhere -- it needs both
# halves of the product -- so it gets its own budget. These counts include the
# return, like MAX_INSTRUCTIONS above. On riscv32 the carry-free form is seven
# instructions (mul, mulh, srli, addi, srli, slli, add) plus ret. The obvious
# spelling, adding 2^(S-1) to the whole product, costs nine because the
# rounding term can overflow the low word and the carry has to be propagated
# with an sltu. Those two instructions were worth 1.14% on an ESP32-C6, which
# is why this is pinned rather than left to the optimiser. Cortex-M4 still
# takes the portable form and is budgeted where it lands today, so that a
# regression there is visible even though it is not yet optimised.
MAX_ROUND_INSTRUCTIONS = {"riscv32-esp": 8, "cortex-m4": 10}

# Proof that the rounded multiply is actually here rather than called out to:
# the target's 32x32->64 multiply must appear in the body. Cortex-M4 emits
# `smlal` and not `smull`, because the portable form is a multiply followed by
# an add of the rounding term and the core fuses the two into one
# multiply-accumulate -- which is why its budget is the larger of the two.
ROUND_MULTIPLY = {"riscv32-esp": ("mulh",), "cortex-m4": ("smull", "smlal")}


def _find(compiler: str) -> str | None:
    found = shutil.which(compiler)
    if found:
        return found
    for candidate in Path.home().glob(f".platformio/packages/**/bin/{compiler}"):
        return str(candidate)
    return None


def _emit(
    compiler_path: str, extra: list[str], out: Path, optimisation: str = "-Os"
) -> str:
    src = out / "probe.cpp"
    src.write_text(SOURCE)
    asm = out / "probe.s"
    RunningProcess.run(
        [
            compiler_path,
            f"-I{ROOT / 'src'}",
            f"-I{ROOT / 'src' / 'platforms' / 'stub'}",
            "-std=gnu++11",
            optimisation,
            "-fno-exceptions",
            "-fno-rtti",
            *extra,
            "-DFASTLED_USE_PROGMEM=0",
            "-DARDUINO=10808",
            "-DFASTLED_NO_AUTO_NAMESPACE",
            "-S",
            str(src),
            "-o",
            str(asm),
        ],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return asm.read_text()


def _body(assembly: str, symbol_fragment: str) -> list[str]:
    lines = assembly.splitlines()
    body: list[str] = []
    inside = False
    for line in lines:
        if not inside:
            if symbol_fragment in line and line.rstrip().endswith(":"):
                inside = True
            continue
        if line.startswith("\t.") or line.strip().startswith(".size"):
            break
        stripped = line.strip()
        if stripped and not stripped.startswith((".", "#", "@")):
            body.append(stripped.split()[0])
    return body


@pytest.mark.parametrize("target,compiler,extra,expected", TARGETS)
def test_int_asm_emits_one_instruction(
    target: str, compiler: str, extra: list[str], expected: dict[str, str]
) -> None:
    compiler_path = _find(compiler)
    if compiler_path is None:
        pytest.skip(f"{compiler} not installed; cannot check {target} codegen")

    with tempfile.TemporaryDirectory() as tmp:
        assembly = _emit(compiler_path, extra, Path(tmp))

    round_body = _body(assembly, "fl_test_round27")
    round_budget = MAX_ROUND_INSTRUCTIONS[target]
    assert round_body, f"{target}: no body found for fl_test_round27"
    assert len(round_body) <= round_budget, (
        f"{target}: fl_test_round27 compiled to {len(round_body)} "
        f"instructions ({' '.join(round_body)}), budget {round_budget}. The "
        f"rounded multiply regressed to a carry-propagating form; see the "
        f"identity in platforms/int_asm.h."
    )

    for name, mnemonic in expected.items():
        body = _body(assembly, f"fl_test_{name}")
        assert body, f"{target}: no body found for fl_test_{name}"
        assert len(body) <= MAX_INSTRUCTIONS, (
            f"{target}: fl_test_{name} compiled to {len(body)} instructions "
            f"({' '.join(body)}). The idiom stopped being recognised -- the "
            f"value is still correct but the cost is not. Spell the "
            f"instruction out for this target in platforms/int_asm.h, "
            f"or check whether the inline request was declined."
        )
        assert any(op.startswith(mnemonic) for op in body), (
            f"{target}: expected '{mnemonic}' in fl_test_{name}, got {' '.join(body)}"
        )


@pytest.mark.parametrize("target,compiler,extra,expected", TARGETS)
def test_int_asm_survives_no_inline(
    target: str, compiler: str, extra: list[str], expected: dict[str, str]
) -> None:
    """FASTLED_FORCE_INLINE has to beat -fno-inline, which is the whole reason
    these are force-inlined rather than plain `inline`. A plain inline function
    here compiled to a tail-jump into an out-of-line body; the two-instruction
    win was entirely lost."""
    compiler_path = _find(compiler)
    if compiler_path is None:
        pytest.skip(f"{compiler} not installed; cannot check {target} codegen")

    with tempfile.TemporaryDirectory() as tmp:
        assembly = _emit(
            compiler_path, [*extra, "-fno-inline"], Path(tmp), optimisation="-Os"
        )

    # The instruction budget has to hold here too. Checking only the mnemonic
    # would pass a body that kept its `mulh` but grew a stack frame around it,
    # which is exactly what a declined force-inline looks like.
    round_body = _body(assembly, "fl_test_round27")
    assert round_body, f"{target}: no body for fl_test_round27 under -fno-inline"
    assert len(round_body) <= MAX_ROUND_INSTRUCTIONS[target], (
        f"{target}: fl_test_round27 compiled to {len(round_body)} instructions "
        f"under -fno-inline ({' '.join(round_body)}), budget "
        f"{MAX_ROUND_INSTRUCTIONS[target]}."
    )
    # The budget alone would be satisfied by a body that just calls out to an
    # out-of-line copy -- a call and a return is two instructions. Requiring the
    # target's high-multiply proves the arithmetic is actually here, which is
    # what FASTLED_FORCE_INLINE is for and what -fno-inline is trying to break.
    wanted = ROUND_MULTIPLY[target]
    assert any(op.startswith(m) for op in round_body for m in wanted), (
        f"{target}: fl_test_round27 has none of {wanted} under -fno-inline "
        f"({' '.join(round_body)}). The force-inline was declined and the "
        f"multiply went out of line."
    )

    for name, mnemonic in expected.items():
        body = _body(assembly, f"fl_test_{name}")
        assert body, f"{target}: no body found for fl_test_{name} under -fno-inline"
        assert len(body) <= MAX_INSTRUCTIONS, (
            f"{target}: fl_test_{name} compiled to {len(body)} instructions "
            f"under -fno-inline ({' '.join(body)}), budget {MAX_INSTRUCTIONS}."
        )
        assert any(op.startswith(mnemonic) for op in body), (
            f"{target}: under -fno-inline, fl_test_{name} emitted "
            f"{' '.join(body)} instead of '{mnemonic}'. The force-inline was "
            f"declined and the body went out of line."
        )
