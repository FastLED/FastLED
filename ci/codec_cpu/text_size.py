#!/usr/bin/env python3
"""riscv32 `.text` for the fixed-point decoder, optionally per function.

Every optimisation round so far has traded code size for speed -- +14% then
+3.9% -- and that budget is not unlimited on the parts this decoder ships to.
The figure was being produced by hand-building a cross-compiled translation unit
each time; this does it in one command so it can be quoted per change like the
Callgrind numbers are.

    uv run python ci/codec_cpu/text_size.py
    uv run python ci/codec_cpu/text_size.py --baseline HEAD --functions

`--compare` answers a different question. Host Callgrind reads 1.00x against
the retired Helix decoder while an ESP32-C6 reads 1.170x. That gap is register
pressure, 64-bit operations and spill traffic: work an x86-64 host with sixteen
64-bit registers never pays and Callgrind therefore cannot see. `--compare`
builds *both* decoders for *both* ISAs and prints, per stage and per function,
how much each one inflates going from x86-64 to riscv32.

    uv run python ci/codec_cpu/text_size.py --compare
    uv run python ci/codec_cpu/text_size.py --compare --top 25

Read it as a bound, not a measurement: static size and instruction counts say
where the target does structurally more work, not what that work costs. A
change removing 15 of 25 riscv32 instructions once delivered 4.6% on device
because the branches it removed were predictable. `device_profile.py` decides.

Attribution is inline-aware. Both decoders inline heavily at -Os -- minimp3's
`mp3dec_decode_frame_r` swallows the whole bitstream stage, Helix's polyphase
swallows its arithmetic macros -- so a plain symbol table compares two
different partitions of the same program. `--compare` and `--functions
--inlined` instead disassemble, ask addr2line for each address's inline stack,
and charge the bytes to the innermost frame that names a real stage. minimp3's
`fl::math` helpers are deliberately *not* a stage of their own: Helix spells
the same operations as macros, so counting minimp3's separately would have
scored an implementation detail as a difference between the decoders.
"""

from __future__ import annotations

import argparse
import collections
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]

# Everything the riscv32 object is compiled from that we may edit. The
# int_asm headers hold the decoder's multiply and wraparound primitives.
SHADOW_PATHS = (
    "src/third_party/minimp3",
    "src/platforms/int_asm.h",
    "src/fl/math/int_asm.h",
)
TU = ROOT / "ci" / "codec_cpu" / "minimp3_fixed_codegen.cpp"
HELIX_TU = ROOT / "ci" / "codec_cpu" / "helix_codegen.cpp"

FLAGS = [
    "-std=gnu++11",
    "-Os",
    "-fno-exceptions",
    "-fno-rtti",
    "-fno-strict-aliasing",
    "-DFL_CODEC_CPU_CODEGEN_ESP_TYPES",
    "-DMINIMP3_NO_SIMD",
    "-DFASTLED_USE_PROGMEM=0",
    "-DARDUINO=10808",
    "-DFASTLED_NO_AUTO_NAMESPACE",
]

# The host half of `--compare`. Same optimisation level, same scalar kernels,
# same source: only the ISA changes. -DFL_CODEC_CPU_CODEGEN_ESP_TYPES is
# dropped because on the host the platform headers already supply the integer
# types, and including the shim ahead of them is what creates a conflict.
HOST_FLAGS = [f for f in FLAGS if f != "-DFL_CODEC_CPU_CODEGEN_ESP_TYPES"]

DECODERS = {"minimp3-fixed": TU, "helix": HELIX_TU}


@dataclass(frozen=True)
class Toolchain:
    """The riscv32 cross tools. Named fields rather than a positional pair:
    `gpp, objdump = _toolchain()` reads the same whichever way round it is
    wrong, and both are strings."""

    gpp: str
    objdump: str


def _toolchain() -> Toolchain:
    found = list(Path.home().glob(".platformio/packages/**/bin/riscv32-esp-elf-g++"))
    if not found:
        raise SystemExit(
            "riscv32-esp-elf-g++ not found; install it with\n"
            "  uv run pio pkg install -g -t "
            "'espressif/toolchain-riscv32-esp@12.2.0+20230208'"
        )
    gpp = str(found[0])
    return Toolchain(
        gpp=gpp, objdump=str(Path(gpp).with_name("riscv32-esp-elf-objdump"))
    )


def _riscv_tool(name: str) -> str:
    gpp = _toolchain().gpp
    return str(Path(gpp).with_name(f"riscv32-esp-elf-{name}"))


def _host_tool(*candidates: str) -> str:
    for name in candidates:
        found = shutil.which(name)
        if found:
            return found
    raise SystemExit(f"none of {candidates} found on PATH")


def _compile(
    gpp: str,
    out: Path,
    include_first: Path | None,
    tu: Path = TU,
    name: str = "codec.o",
    flags: list[str] = FLAGS,
    debug: bool = False,
) -> Path:
    obj = out / name
    includes = ([f"-I{include_first}"] if include_first else []) + [
        f"-I{ROOT / 'src'}",
        f"-I{ROOT / 'src' / 'platforms' / 'stub'}",
    ]
    extra = ["-g"] if debug else []
    subprocess.run(
        [gpp, *includes, *flags, *extra, "-c", str(tu), "-o", str(obj)],
        check=True,
        capture_output=True,
    )
    return obj


def _exec_sections(objdump: str, obj: Path) -> list[str]:
    out = subprocess.run(
        [objdump, "-h", str(obj)], capture_output=True, text=True, check=True
    ).stdout
    names, pending = [], None
    for line in out.splitlines():
        parts = line.split()
        if len(parts) > 2 and parts[1].startswith("."):
            pending = parts[1]
        elif pending and "CODE" in line and "ALLOC" in line:
            names.append(pending)
            pending = None
    return names


def _flatten(driver: str, objdump: str, obj: Path, isa: str) -> Path:
    """Give every function a distinct address, linking only if it has to.

    `MP3D_KERNEL` is a `hot` attribute, and clang honours it by putting
    mp3d_DCT_II and L3_imdct36 in `.text.hot.` -- a second executable section
    that, in a relocatable object, starts at address 0 just like `.text` does.
    addr2line then resolves every address below 0x517 against whichever
    section the line table reaches first, and the entire DCT-32 gets charged
    to whatever happens to live at the same offset in `.text`. That is not a
    small error: it read as "the DCT-32 costs 71 x86 instructions".

    Linking merges them at distinct addresses and fixes it. The host g++ build
    has the same problem for a different reason -- one comdat section per
    inline function -- so this runs on whichever side needs it. riscv32 g++
    emits a single `.text` and normally needs no link, which is why this is
    conditional: relaxation would change the instruction stream, and these
    counts have to stay comparable to the `.text` figure the rest of this
    script reports straight from the object file. `--no-relax` is passed
    anyway, for the day that changes.
    """
    if len(_exec_sections(objdump, obj)) < 2:
        return obj
    image = obj.with_suffix(".img")
    cmd = [
        driver,
        "-nostdlib",
        "-nostartfiles",
        "-Wl,--unresolved-symbols=ignore-all",
        "-Wl,-e0",
        "-o",
        str(image),
        str(obj),
    ]
    # A plain non-PIE executable, not `-shared`: the decoders take the address
    # of their tables from non-PIC code, which a shared object rejects.
    cmd[1:1] = ["-Wl,--no-relax"] if isa == "riscv32" else ["-no-pie"]
    subprocess.run(cmd, check=True, capture_output=True)
    return image


def _sections(objdump: str, obj: Path) -> dict[str, int]:
    out = subprocess.run(
        [objdump, "-h", str(obj)], capture_output=True, text=True, check=True
    ).stdout
    sizes: dict[str, int] = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) > 2 and parts[1].startswith("."):
            try:
                sizes[parts[1]] = int(parts[2], 16)
            except KeyboardInterrupt:
                raise
            except ValueError:
                continue
    return sizes


def _demangle(names: list[str]) -> dict[str, str]:
    """Mangled -> readable, via c++filt when it is available.

    Done as a display step rather than by asking objdump for demangled symbols:
    a demangled name contains spaces, which would have to be parsed back out of
    a whitespace-delimited symbol table.
    """
    tool = shutil.which("llvm-cxxfilt") or shutil.which("c++filt")
    if not tool or not names:
        return {}
    out = subprocess.run(
        [tool], input="\n".join(names), capture_output=True, text=True
    ).stdout.splitlines()
    return dict(zip(names, out)) if len(out) == len(names) else {}


def _functions(objdump: str, obj: Path) -> dict[str, int]:
    out = subprocess.run(
        [objdump, "-t", str(obj)], capture_output=True, text=True, check=True
    ).stdout
    sizes: dict[str, int] = {}
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+\s+.*?\s+F\s+\.text\s+([0-9a-f]+)\s+(\S+)", line)
        if m:
            # Key on the whole mangled symbol. Truncating at the first "E" --
            # the end of an Itanium nested-name -- discards the parameter list,
            # so overloads collapse onto one key and the last one silently
            # overwrites the others' sizes. This decoder really has such a
            # pair: L3_dct3_9(int32_t*) and L3_dct3_9(float*).
            sizes[m.group(2)] = int(m.group(1), 16)
    return sizes


# ---------------------------------------------------------------------------
# Inline-aware disassembly
# ---------------------------------------------------------------------------

# objdump -d rows look like
#   "    2366:\t4522                \tlw\ta0,8(sp)"
# The byte column is hex pairs; its width is what gives the instruction length,
# which is the only way to get per-instruction sizes on a variable-length ISA.
_INSN = re.compile(r"^\s+([0-9a-f]+):\s+((?:[0-9a-f]{2} ?)+?)\s{2,}(\S+)\s*(.*)$")

_RV_MEM = {
    "lw",
    "lh",
    "lhu",
    "lb",
    "lbu",
    "sw",
    "sh",
    "sb",
    "c.lw",
    "c.sw",
    "c.lwsp",
    "c.swsp",
    "flw",
    "fsw",
    "fld",
    "fsd",
}
# 32x32->64 on a 32-bit target: the operation the host does in one instruction
# and riscv32 needs a pair for. Counting it separately is the closest static
# proxy there is for "64-bit work the host profile cannot see".
_RV_WIDE = {"mulh", "mulhu", "mulhsu", "mul", "mulw"}


class Disasm:
    """Per-instruction facts about one object file, keyed by address."""

    def __init__(self, objdump: str, obj: Path, isa: str) -> None:
        out = subprocess.run(
            [objdump, "-d", str(obj)], capture_output=True, text=True, check=True
        ).stdout
        self.isa = isa
        self.rows: list[tuple[int, int, str, str]] = []
        for line in out.splitlines():
            m = _INSN.match(line)
            if not m:
                continue
            addr = int(m.group(1), 16)
            nbytes = len(m.group(2).replace(" ", "")) // 2
            self.rows.append((addr, nbytes, m.group(3), m.group(4)))

    def classify(self, mnemonic: str, operands: str) -> tuple[bool, bool, bool]:
        """(memory, stack-relative memory, wide-multiply) for one instruction."""
        if self.isa == "riscv32":
            mem = mnemonic in _RV_MEM
            stack = mem and ("(sp)" in operands or "(s0)" in operands)
            wide = mnemonic in _RV_WIDE
            return mem, stack, wide
        # x86-64. Any operand of the form `disp(%reg...)` touches memory,
        # except `lea`, which only computes the address. `(%rip)` is a
        # constant-pool reference, which is a load like any other.
        mem = mnemonic != "lea" and bool(re.search(r"\(%[a-z0-9]+", operands))
        stack = mem and ("(%rsp" in operands or "(%rbp" in operands)
        wide = mnemonic in {"imul", "mul"}
        return mem, stack, wide


def _inline_stacks(addr2line: str, obj: Path, addrs: list[int]) -> list[list[str]]:
    """The inline frame stack for each address, innermost frame first.

    `-a` is what makes this parseable: `-i` emits a variable number of
    function/location pairs per address with no separator between addresses,
    so without the address markers the groups cannot be split apart.
    """
    inp = "\n".join(hex(a) for a in addrs)
    out = subprocess.run(
        [addr2line, "-a", "-f", "-i", "-C", "-e", str(obj)],
        input=inp,
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    # After each `0x...` marker the output strictly alternates function name
    # and source location. Sniffing for "looks like a path" instead was wrong:
    # a demangled name ending in `::mulshift32` reads as a location under any
    # colon heuristic, which silently dropped the innermost frame of every
    # arithmetic helper -- and, on the clang side, most of the DSP.
    stacks: list[list[str]] = []
    cur: list[str] | None = None
    expect_name = True
    for line in out.splitlines():
        if line.startswith("0x") and re.fullmatch(r"0x[0-9a-f]+", line.strip()):
            cur = []
            stacks.append(cur)
            expect_name = True
            continue
        if cur is None:
            continue
        if expect_name:
            cur.append(line)
        expect_name = not expect_name
    return stacks


def _short(name: str) -> str:
    """`fl::third_party::mp3d_synth(int*, short*)` -> `mp3d_synth`."""
    name = name.split("(")[0].strip()
    name = re.sub(r"<.*>", "", name)
    return name.split("::")[-1] or name


# ---------------------------------------------------------------------------
# Stage mapping
# ---------------------------------------------------------------------------
#
# The two decoders share an algorithm and share none of its names, so a stage
# table is the only way to line them up. Each entry is (stage, {function names}).
# Order matters only in that ARITH is consulted last -- see below.

_MINIMP3_STAGES: list[tuple[str, set[str]]] = [
    (
        "polyphase",
        {
            "mp3d_synth",
            "mp3d_synth_pair",
            "mp3d_synth_taps",
            "mp3d_scale_pcm",
            "mp3d_clamp_sample",
        },
    ),
    ("dct32", {"mp3d_DCT_II", "mp3d_synth_granule"}),
    (
        "imdct",
        {
            "L3_imdct36",
            "mp3d_imdct36_twiddle",
            "L3_dct3_9",
            "L3_imdct12",
            "L3_imdct_short",
            "L3_imdct_gr",
            "L3_change_sign",
            "L3_antialias",
            "L3_reorder",
        },
    ),
    (
        "bitstream",
        {
            "L3_huffman",
            "get_bits",
            "peek_bits",
            "bs_init",
            "L3_read_side_info",
            "L3_decode_scalefactors",
            "L3_read_scalefactors",
            "L3_restore_reservoir",
            "L3_save_reservoir",
            "mp3d_find_frame",
            "hdr_valid",
            "hdr_compare",
            "hdr_frame_bytes",
            "hdr_bitrate_kbps",
            "hdr_sample_rate_hz",
            "hdr_frame_samples",
            "hdr_padding",
            "L12_read_scale_info",
            "L12_subband_alloc_table",
            "L12_read_scalefactors",
        },
    ),
    (
        "dequant",
        {
            "mp3d_dequant",
            "mp3d_scale_to_q",
            "L3_ldexp_q",
            "mp3d_l12_scale",
            "L12_apply_scf_384",
            "L12_dequantize_granule",
            "mp3d_pow43",
        },
    ),
    (
        "stereo",
        {
            "L3_intensity_stereo",
            "L3_midside_stereo",
            "L3_stereo_process",
            "L3_intensity_stereo_band",
        },
    ),
    (
        "frame",
        {"mp3dec_decode_frame_r", "L3_decode", "mp3dec_init", "mp3dec_decode_frame"},
    ),
]

_HELIX_STAGES: list[tuple[str, set[str]]] = [
    ("polyphase", {"PolyphaseStereo", "PolyphaseMono"}),
    ("dct32", {"FDCT32", "Subband"}),
    (
        "imdct",
        {
            "IMDCT",
            "idct9",
            "imdct12",
            "WinPrevious",
            "WinNext",
            "FreqInvertRescale",
            "AntiAlias",
            "HybridTransform",
            "IMDCT36",
            "IMDCT12x3",
        },
    ),
    (
        "bitstream",
        {
            "DecodeHuffman",
            "UnpackScaleFactors",
            "UnpackSideInfo",
            "UnpackFrameHeader",
            "GetBits",
            "SetBitstreamPointer",
            "RefillBitstreamCache",
            "CalcBitsUsed",
            "DecodeHuffmanPairs",
            "DecodeHuffmanQuads",
            "UnpackSFMPEG1",
            "UnpackSFMPEG2",
            "MP3FindSyncWord",
            "CheckPadBit",
        },
    ),
    (
        "dequant",
        {
            "Dequantize",
            "DequantChannel",
            "DequantBlock",
            "Reorder",
            "PolyphaseCoefficients",
        },
    ),
    (
        "stereo",
        {"IntensityProcMPEG1", "IntensityProcMPEG2", "MidSideProc", "StereoProcess"},
    ),
    (
        "frame",
        {
            "MP3Decode",
            "MP3GetLastFrameInfo",
            "MP3GetNextFrameInfo",
            "AllocateBuffers",
            "FreeBuffers",
            "MP3InitDecoder",
            "MP3FreeDecoder",
            "MP3ClearBadFrame",
            "MP3GetLastFrameInfo",
        },
    ),
]

STAGE_ORDER = [
    "polyphase",
    "dct32",
    "imdct",
    "bitstream",
    "dequant",
    "stereo",
    "frame",
    "other",
]

# minimp3 spells its saturating arithmetic and fixed-point multiplies as
# inlinable functions; Helix spells the same operations as macros, which have
# no DWARF frame at all. Giving minimp3's a stage of their own would score a
# language choice as an algorithmic difference, so they are transparent: an
# address inside `mul_shift_round32` is charged to whichever caller inlined it.
_TRANSPARENT = {
    # minimp3
    "mul_shift_round32",
    "mulshift32",
    "wrap_add32",
    "wrap_sub32",
    "mp3d_mulshift",
    "mp3d_add_sat",
    "mp3d_sub_sat",
    "mp3d_shl_sat",
    "mp3d_saturate",
    "mp3d_narrow",
    # Helix. `assembly.h` is macros on the platforms with hand assembly and
    # small static inlines everywhere else; on riscv32 it is the inlines, and
    # they are the same arithmetic minimp3 spells with the names above.
    "MADD64",
    "MSUB64",
    "SAR64",
    "MULSHIFT32",
    "FASTABS",
    "CLZ",
    "Mul32x32to64",
    "xmadd",
    "xmsub",
    "SHL64",
    "MULSHIFT32_ROUND",
    # neither
    "memset",
    "memcpy",
    "memmove",
}


def _stage_of(stack: list[str], table: list[tuple[str, set[str]]]) -> str:
    """Innermost frame that names a stage, skipping transparent helpers."""
    for frame in stack:
        short = _short(frame)
        if short in _TRANSPARENT:
            continue
        for stage, names in table:
            if short in names:
                return stage
    return "other"


def _attribution_name(stack: list[str]) -> str:
    for frame in stack:
        short = _short(frame)
        if short not in _TRANSPARENT:
            return short
    return _short(stack[0]) if stack else "?"


class Profile:
    """Per-function and per-stage static facts for one decoder on one ISA."""

    def __init__(
        self,
        name: str,
        isa: str,
        objdump: str,
        addr2line: str,
        obj: Path,
        stages: list[tuple[str, set[str]]],
    ) -> None:
        self.name = name
        self.isa = isa
        d = Disasm(objdump, obj, isa)
        stacks = _inline_stacks(addr2line, obj, [r[0] for r in d.rows])
        if len(stacks) != len(d.rows):
            raise SystemExit(
                f"addr2line returned {len(stacks)} groups for {len(d.rows)} "
                f"instructions in {obj}; refusing to attribute"
            )
        self.fn_bytes: collections.Counter[str] = collections.Counter()
        self.fn_insns: collections.Counter[str] = collections.Counter()
        self.fn_mem: collections.Counter[str] = collections.Counter()
        self.fn_stack: collections.Counter[str] = collections.Counter()
        self.fn_wide: collections.Counter[str] = collections.Counter()
        self.fn_stage: dict[str, str] = {}
        self.st_bytes: collections.Counter[str] = collections.Counter()
        self.st_insns: collections.Counter[str] = collections.Counter()
        self.st_mem: collections.Counter[str] = collections.Counter()
        self.st_stack: collections.Counter[str] = collections.Counter()
        self.st_wide: collections.Counter[str] = collections.Counter()
        for (_, nbytes, mnemonic, operands), stack in zip(d.rows, stacks):
            fn = _attribution_name(stack)
            stage = _stage_of(stack, stages)
            mem, stk, wide = d.classify(mnemonic, operands)
            self.fn_bytes[fn] += nbytes
            self.fn_insns[fn] += 1
            self.fn_mem[fn] += mem
            self.fn_stack[fn] += stk
            self.fn_wide[fn] += wide
            self.fn_stage[fn] = stage
            self.st_bytes[stage] += nbytes
            self.st_insns[stage] += 1
            self.st_mem[stage] += mem
            self.st_stack[stage] += stk
            self.st_wide[stage] += wide

    @property
    def total_insns(self) -> int:
        return sum(self.fn_insns.values())


def _build_profile(decoder: str, isa: str, tmp: Path) -> Profile:
    stages = _MINIMP3_STAGES if decoder == "minimp3-fixed" else _HELIX_STAGES
    tu = DECODERS[decoder]
    tag = f"{decoder.replace('-', '_')}_{isa}.o"
    if isa == "riscv32":
        tools = _toolchain()
        gpp, objdump = tools.gpp, tools.objdump
        addr2line = _riscv_tool("addr2line")
        obj = _compile(gpp, tmp, None, tu=tu, name=tag, flags=FLAGS, debug=True)
    else:
        # g++ ahead of clang++ on purpose. The riscv32 side is g++ 14.2, and
        # the first version of this used the host clang: it reported
        # mp3d_DCT_II at 798 riscv32 instructions against 217 on x86-64 and
        # called it a 3.7x ISA penalty. It is not. g++ fully unrolls the
        # DCT-32's 8- and 4-iteration inner loops (the riscv32 body has two
        # backward branches in 798 instructions) and clang at -Os does not, so
        # most of that ratio was one compiler against another. Same compiler
        # family both sides, and the number means what the header says it
        # means.
        gpp = _host_tool("g++", "clang++")
        objdump = _host_tool("objdump", "llvm-objdump")
        addr2line = _host_tool("addr2line", "llvm-addr2line")
        obj = _compile(gpp, tmp, None, tu=tu, name=tag, flags=HOST_FLAGS, debug=True)
    obj = _flatten(gpp, objdump, obj, isa)
    return Profile(decoder, isa, objdump, addr2line, obj, stages)


def _ratio(a: float, b: float) -> str:
    return f"{a / b:.2f}x" if b else "   --"


def _print_compare(profiles: dict[tuple[str, str], Profile], top: int) -> None:
    mp_rv = profiles[("minimp3-fixed", "riscv32")]
    mp_x8 = profiles[("minimp3-fixed", "x86_64")]
    hx_rv = profiles[("helix", "riscv32")]
    hx_x8 = profiles[("helix", "x86_64")]

    print("\nriscv32 (esp-elf-g++ -Os) vs x86-64 (g++ -Os), same sources, same")
    print("compiler family both sides.")
    print("`infl` = riscv32 instructions / x86-64 instructions for the same")
    print("stage. The whole-decoder figure at the bottom is the ISA's baseline;")
    print("a stage above it is doing structurally more work on the target than")
    print("the host profile can charge it for.\n")

    head = (
        f"  {'stage':<11}"
        f"{'mp3 rv32':>9}{'mp3 x86':>9}{'infl':>7}   "
        f"{'hx rv32':>9}{'hx x86':>9}{'infl':>7}   {'mp3/hx':>7}"
    )
    print(head)
    print("  " + "-" * (len(head) - 2))
    for stage in STAGE_ORDER:
        a, b = mp_rv.st_insns[stage], mp_x8.st_insns[stage]
        c, d = hx_rv.st_insns[stage], hx_x8.st_insns[stage]
        if not (a or c):
            continue
        print(
            f"  {stage:<11}{a:>9,}{b:>9,}{_ratio(a, b):>7}   "
            f"{c:>9,}{d:>9,}{_ratio(c, d):>7}   {_ratio(a, c):>7}"
        )
    ta, tb = mp_rv.total_insns, mp_x8.total_insns
    tc, td = hx_rv.total_insns, hx_x8.total_insns
    print("  " + "-" * (len(head) - 2))
    print(
        f"  {'whole TU':<11}{ta:>9,}{tb:>9,}{_ratio(ta, tb):>7}   "
        f"{tc:>9,}{td:>9,}{_ratio(tc, td):>7}   {_ratio(ta, tc):>7}"
    )

    print("\n  riscv32 memory traffic and 32x32 multiplies, per stage.")
    print("  `stack` is the subset of loads/stores against sp/s0 -- frame")
    print("  traffic, which is spills plus locals. `mul*` counts the multiply")
    print("  family, the pair riscv32 needs where x86-64 needs one imul.\n")
    head2 = (
        f"  {'stage':<11}"
        f"{'mp3 mem':>9}{'stack':>8}{'mul*':>7}   "
        f"{'hx mem':>9}{'stack':>8}{'mul*':>7}"
    )
    print(head2)
    print("  " + "-" * (len(head2) - 2))
    for stage in STAGE_ORDER:
        if not (mp_rv.st_insns[stage] or hx_rv.st_insns[stage]):
            continue
        print(
            f"  {stage:<11}{mp_rv.st_mem[stage]:>9,}"
            f"{mp_rv.st_stack[stage]:>8,}{mp_rv.st_wide[stage]:>7,}   "
            f"{hx_rv.st_mem[stage]:>9,}{hx_rv.st_stack[stage]:>8,}"
            f"{hx_rv.st_wide[stage]:>7,}"
        )

    for prof_rv, prof_x8 in ((mp_rv, mp_x8), (hx_rv, hx_x8)):
        print(
            f"\n  {prof_rv.name}: functions by riscv32/x86-64 inflation "
            f"(>=100 riscv32 instructions)"
        )
        rows, unmatched = [], []
        for fn, insns in prof_rv.fn_insns.items():
            if insns < 100:
                continue
            host = prof_x8.fn_insns.get(fn, 0)
            if not host:
                unmatched.append((insns, fn))
                continue
            rows.append(
                (
                    insns / host,
                    fn,
                    insns,
                    host,
                    prof_rv.fn_stack[fn],
                    prof_rv.fn_stage.get(fn, "?"),
                )
            )
        rows.sort(reverse=True)
        print(
            f"    {'infl':>6} {'rv32':>7} {'x86':>7} {'stack':>7}  "
            f"{'stage':<10} function"
        )
        for infl, fn, insns, host, stack, stage in rows[:top]:
            print(
                f"    {infl:>5.2f}x {insns:>7,} {host:>7,} {stack:>7,}  "
                f"{stage:<10} {fn}"
            )
        if unmatched:
            # The two builds do not inline the same set -- same compiler
            # family, different targets and different cost models -- so a
            # function one kept out of line and the other folded away has no
            # frame to compare against. The stage rows above absorb that correctly --
            # the work is still counted, just under whichever caller swallowed
            # it -- which is why the stage table is the layer to trust and this
            # one is a pointer to candidates.
            unmatched.sort(reverse=True)
            names = ", ".join(f"{fn} ({n:,})" for n, fn in unmatched[:6])
            print(f"    no host frame (inlined on the host, not on riscv32): {names}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", help="git ref to compare against")
    parser.add_argument(
        "--functions", action="store_true", help="per-function .text, largest first"
    )
    parser.add_argument(
        "--inlined",
        action="store_true",
        help="with --functions, attribute inlined code to the "
        "function it was written in rather than to the "
        "symbol that absorbed it",
    )
    parser.add_argument(
        "--decoder",
        choices=sorted(DECODERS),
        default="minimp3-fixed",
        help="which decoder to size (default: minimp3-fixed)",
    )
    parser.add_argument(
        "--compare",
        action="store_true",
        help="both decoders on riscv32 and x86-64, per stage "
        "and per function, ranked by ISA inflation",
    )
    parser.add_argument("--top", type=int, default=12)
    args = parser.parse_args(argv)

    tools = _toolchain()
    gpp, objdump = tools.gpp, tools.objdump

    if args.compare:
        with tempfile.TemporaryDirectory() as tmp:
            profiles = {}
            for decoder in ("minimp3-fixed", "helix"):
                for isa in ("riscv32", "x86_64"):
                    profiles[(decoder, isa)] = _build_profile(decoder, isa, Path(tmp))
            _print_compare(profiles, args.top)
        return 0

    tu = DECODERS[args.decoder]
    with tempfile.TemporaryDirectory() as tmp:
        now = _compile(gpp, Path(tmp), None, tu=tu, debug=args.inlined)
        now_sections = _sections(objdump, now)
        if args.decoder != "minimp3-fixed":
            print(f"  decoder {args.decoder}")
        print(f"  .text  {now_sections.get('.text', 0):>9,} bytes")
        for name in (".rodata", ".data", ".bss"):
            if now_sections.get(name):
                print(f"  {name:<6} {now_sections[name]:>9,} bytes")

        if args.baseline:
            if args.decoder != "minimp3-fixed":
                raise SystemExit(
                    "--baseline shadows src/third_party/minimp3 only; it does "
                    "not apply to --decoder helix"
                )
            shadow = Path(tmp) / "baseline"
            (shadow / "third_party").mkdir(parents=True)
            # The decoder's arithmetic primitives live outside its directory
            # deliberately, so shadowing only src/third_party/minimp3 reports
            # +0 bytes for a change to them -- a wrong delta, silently. Shadow
            # every path the object is built from, and say so when a change
            # lands somewhere neither of them covers.
            tar = subprocess.run(
                ["git", "archive", args.baseline, *SHADOW_PATHS],
                cwd=ROOT,
                check=True,
                capture_output=True,
            ).stdout
            subprocess.run(
                ["tar", "-x", "-C", str(shadow), "--strip-components=1"],
                input=tar,
                check=True,
            )
            changed = subprocess.run(
                ["git", "diff", "--name-only", args.baseline, "--", "src"],
                cwd=ROOT,
                capture_output=True,
                text=True,
            ).stdout.split()
            unshadowed = [
                f for f in changed if not any(f.startswith(pfx) for pfx in SHADOW_PATHS)
            ]
            if unshadowed:
                print(
                    f"  NOTE: {len(unshadowed)} changed file(s) under src/ are "
                    "not shadowed, so the delta below does not include them:"
                )
                for f in unshadowed[:5]:
                    print(f"          {f}")
            base = _compile(gpp, Path(tmp), shadow, tu=tu, name="baseline.o")
            base_text = _sections(objdump, base).get(".text", 0)
            delta = now_sections.get(".text", 0) - base_text
            pct = (delta / base_text * 100) if base_text else 0.0
            print(f"  baseline {args.baseline}: {base_text:,} bytes")
            print(f"  delta    {delta:+,} bytes ({pct:+.2f}%)")

        if args.functions and args.inlined:
            stages = (
                _MINIMP3_STAGES if args.decoder == "minimp3-fixed" else _HELIX_STAGES
            )
            # Link the relocatable first. MP3D_KERNEL carries `hot`, so the
            # kernels land in .text.hot.* -- a separate section that, in an
            # unlinked object, starts at address 0 like .text does. addr2line
            # then resolves those addresses against the wrong section and
            # misattributes the entire DCT-32. _build_profile already flattens
            # for exactly this reason; this path was constructing Profile on
            # the raw object and quietly getting a different answer.
            prof = Profile(
                args.decoder,
                "riscv32",
                objdump,
                _riscv_tool("addr2line"),
                _flatten(gpp, objdump, now, "riscv32"),
                stages,
            )
            print("\n  per-function .text (inline-aware):")
            for size, name in sorted(
                ((v, k) for k, v in prof.fn_bytes.items()), reverse=True
            )[: args.top]:
                print(f"    {size:>7,}  {prof.fn_stage.get(name, '?'):<10} {name}")
        elif args.functions:
            print("\n  per-function .text:")
            _by_size = _functions(objdump, now)
            readable = _demangle(list(_by_size))
            for size, name in sorted(
                ((v, k) for k, v in _by_size.items()), reverse=True
            )[: args.top]:
                print(f"    {size:>7,}  {readable.get(name, name)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
