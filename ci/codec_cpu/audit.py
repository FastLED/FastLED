#!/usr/bin/env python3
"""Build and enforce the three-tier MP3 CPU audit."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from running_process import RunningProcess
from typeguard import typechecked


ROOT = Path(__file__).resolve().parents[2]
TREND_PATH = ROOT / "codec_cpu_trend.json"
BUILD = Path(os.environ.get("CODEC_CPU_BUILD", ROOT / ".build" / "codec-cpu-audit"))
REGRESSION_TOLERANCE = 0.05
PROFILE_RUNS = 30
# Both minimp3 builds. `minimp3-fixed` is what ships; `minimp3-float` is the
# reference the fixed-vs-float gates compare against, and it keeps its history
# rather than being replaced (FastLED#4110). They cannot share a translation
# unit, so each has its own source below and its own namespace.
BACKENDS = ("minimp3-float", "minimp3-fixed")
STAGES = (
    "scalefactors",
    "huffman",
    "dequant",
    "stereo",
    "reorder",
    "antialias",
    "imdct",
    "synthesis",
)
TARGETS = ("xtensa-esp32", "riscv32-esp", "cortex-m0plus", "cortex-m4")
KERNELS = ("dct32", "polyphase")
CORPUS = (
    "tests/data/codec/minimp3/l3-hecommon.bit",
    "tests/data/codec/minimp3/l3-compl-cut.mp3",
    "tests/data/codec/minimp3/l3-he_free.bit",
    "tests/data/codec/minimp3/l3-lame-vbrtag.bit",
    "tests/data/codec/minimp3/M2L3_bitrate_16_all.bit",
)


@typechecked
@dataclass(frozen=True, slots=True)
class CallgrindData:
    summary: dict[str, int]


@typechecked
@dataclass(frozen=True, slots=True)
class TargetTools:
    compiler: Path
    objdump: Path
    flags: list[str]


@typechecked
@dataclass(frozen=True, slots=True)
class InstrumentationResult:
    text: str
    operation_sites: int
    stage_sites: dict[str, int]


@typechecked
@dataclass(frozen=True, slots=True)
class InstrumentedDriver:
    binary: Path
    stage_coverage: dict[str, dict[str, dict[str, Any]]]


_STAGE_SYMBOLS: tuple[tuple[str, tuple[str, ...]], ...] = (
    ("scalefactors", ("UnpackScaleFactors", "L3_decode_scalefactors", "L12_apply_scf")),
    ("huffman", ("DecodeHuffman", "L3_huffman")),
    ("dequant", ("Dequantize", "L12_dequantize_granule")),
    (
        "stereo",
        (
            "MidSideProc",
            "IntensityProcMPEG1",
            "IntensityProcMPEG2",
            "L3_intensity_stereo",
            "L3_midside_stereo",
        ),
    ),
    ("reorder", ("L3_reorder",)),
    ("antialias", ("AntiAlias", "L3_antialias")),
    ("imdct", ("IMDCT", "L3_imdct_gr")),
    (
        "synthesis",
        ("Subband", "FDCT32", "Polyphase", "mp3d_synth_granule", "mp3d_DCT_II"),
    ),
)

_FUSED_STAGES = {
    "minimp3-float": {"dequant": "huffman"},
    "minimp3-fixed": {"dequant": "huffman"},
}

_LLVM_DEFINE_RE = re.compile(r'^\s*define\b.*@(?:"([^"]+)"|([^\s(]+))\(')
_LLVM_VALUE_RE = re.compile(r"^\s*(%[-\w.$]+)\s*=")
_LLVM_VECTOR_RE = re.compile(r"<\s*(\d+)\s+x\s+(?:half|float|double)\s*>")


def load_trend(path: Path = TREND_PATH) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def environment_key(environment: dict[str, Any]) -> str:
    fields = ("cpu_model", "compiler", "governor")
    if any(not isinstance(environment.get(field), str) for field in fields):
        raise RuntimeError("host environment is incomplete")
    return " | ".join(environment[field] for field in fields)


def classify_stage(symbol: str) -> str | None:
    for stage, needles in _STAGE_SYMBOLS:
        if any(needle in symbol for needle in needles):
            return stage
    return None


def cached_compiler_candidates(os_name: str) -> list[Path]:
    if os_name != "nt":
        return []
    candidates = [ROOT / ".cached" / "clang-native" / "ctc-clang++.exe"]
    if len(ROOT.parents) > 1:
        candidates.append(
            ROOT.parents[1] / ".cached" / "clang-native" / "ctc-clang++.exe"
        )
    return candidates


def compiler_path() -> Path:
    override = os.environ.get("CODEC_CPU_COMPILER")
    if override:
        compiler = Path(override)
        if compiler.exists() or shutil.which(override):
            return compiler
        raise RuntimeError(f"CODEC_CPU_COMPILER does not exist: {override}")
    candidates = cached_compiler_candidates(os.name)
    for candidate in candidates:
        if candidate.exists():
            return candidate
    compiler = shutil.which("clang++")
    if compiler:
        return Path(compiler)
    raise RuntimeError("clang++ is required for the codec CPU audit")


def _llvm_functions(lines: list[str]) -> list[tuple[int, int, str]]:
    functions: list[tuple[int, int, str]] = []
    start: int | None = None
    symbol = ""
    for index, line in enumerate(lines):
        if start is None:
            match = _LLVM_DEFINE_RE.search(line)
            if match:
                start = index
                symbol = match.group(1) or match.group(2)
        elif line.strip() == "}":
            functions.append((start, index, symbol))
            start = None
            symbol = ""
    if start is not None:
        raise RuntimeError(f"unterminated LLVM function: {symbol}")
    return functions


def _operation_stage(backend: str, symbol: str) -> str | None:
    if "L3_huffman" in symbol or "L3_pow_43" in symbol or "mp3d_pow43" in symbol:
        return "dequant"
    return classify_stage(symbol)


def _uses_llvm_value(line: str, value: str) -> bool:
    return re.search(rf"{re.escape(value)}(?:\s|,|$)", line) is not None


def _integer_product_lines(body: list[str]) -> tuple[set[int], set[str]]:
    """Q-format multiplies in the fixed-point pipeline, and the values they
    define.

    The float build counts `fmul`, which is unambiguous. `mul` is not: at -O0
    the decoder's integer arithmetic and the compiler's address and size
    arithmetic are the same opcode. Measured on the fixed-point audit TU, the
    discriminating shape is a 64-bit multiply with at least one operand sign
    extended from i32 -- that is what `(int64_t)a * b` lowers to and what index
    scaling never does -- whose product is not then consumed by a
    `getelementptr` or passed to a call.

    Those two exclusions are not decoration. Address scaling that does emit a
    `mul` feeds a GEP, and `L12_apply_scf_384` computes a `memcpy` byte count as
    `(total_bands - stereo_bands) * 18 * sizeof(...)`, which has the sign-extend
    shape but is a size, not a sample. On the current tree this rule selects 69
    multiplies and rejects exactly that one.
    """
    defs: dict[str, str] = {}
    for line in body:
        match = re.match(r"\s*(%[\w.]+) = (\w+)", line)
        if match:
            defs[match.group(1)] = match.group(2)

    joined = "\n".join(body)
    lines: set[int] = set()
    values: set[str] = set()
    for index, line in enumerate(body):
        match = re.search(
            r"(%[\w.]+) = mul (?:nsw |nuw )*i64 (%[\w.]+|-?\d+), (%[\w.]+|-?\d+)",
            line,
        )
        if not match:
            continue
        result, left, right = match.group(1), match.group(2), match.group(3)
        if defs.get(left) != "sext" and defs.get(right) != "sext":
            continue
        used = re.escape(result) + r"(?=[\s,)])"
        if re.search(r"getelementptr[^\n]*" + used, joined):
            continue
        if re.search(r"\bcall\b[^\n]*" + used, joined):
            continue
        lines.add(index)
        values.add(result)
    return lines, values


def _tainted_accumulate_lines(
    body: list[str], seed_values: set[str], add_pattern: str
) -> set[int]:
    """Find arithmetic fed by products across LLVM SSA casts and O0 spills."""
    tainted = set(seed_values)
    tainted_slots: set[str] = set()
    accumulates: set[int] = set()
    for index, line in enumerate(body):
        match = _LLVM_VALUE_RE.match(line)
        result = match.group(1) if match else None
        rhs = line.split("=", 1)[1] if match else line
        uses_tainted = any(_uses_llvm_value(rhs, value) for value in tainted)
        values = re.findall(r"%[-\w.$]+", rhs)
        if re.search(r"\bstore\b", rhs) and values:
            slot = values[-1]
            if uses_tainted:
                tainted_slots.add(slot)
            else:
                tainted_slots.discard(slot)
        if (
            result
            and re.search(r"\bload\b", rhs)
            and values
            and values[-1] in tainted_slots
        ):
            tainted.add(result)
            uses_tainted = True
        if not result or not uses_tainted:
            continue
        if re.search(add_pattern, rhs):
            accumulates.add(index)
        elif re.search(
            r"\b(?:sext|zext|trunc|bitcast|freeze|fpext|fptrunc|phi|select)\b",
            rhs,
        ):
            tainted.add(result)
    return accumulates


def instrument_llvm_ir(
    text: str, backend: str, *, operations: bool = True
) -> InstrumentationResult:
    """Inject dynamic stage timers and arithmetic counters into Clang IR."""
    lines = text.splitlines()
    functions = _llvm_functions(lines)
    inserts: dict[int, list[str]] = {}
    operation_sites = 0
    stage_sites = {stage: 0 for stage in STAGES}
    for start, end, symbol in functions:
        stage = classify_stage(symbol)
        stage_index = STAGES.index(stage) if stage else None
        if stage is not None and stage_index is not None:
            stage_sites[stage] += 1
            entry_label = next(
                (
                    index
                    for index in range(start + 1, end)
                    if re.match(r"^[-\w.$]+:\s*(?:;.*)?$", lines[index])
                ),
                None,
            )
            if entry_label is None:
                raise RuntimeError(f"basic-block entry missing for {symbol}")
            inserts.setdefault(entry_label + 1, []).append(
                f"  call void @fastled_mp3_cpu_stage_enter(i32 {stage_index})"
            )
            for index in range(start + 1, end):
                if lines[index].lstrip().startswith("ret "):
                    inserts.setdefault(index, []).append(
                        f"  call void @fastled_mp3_cpu_stage_exit(i32 {stage_index})"
                    )
        body = lines[start + 1 : end]
        operation_stage = _operation_stage(backend, symbol)
        operation_stage_index = STAGES.index(operation_stage) if operation_stage else -1
        fmul_values = {
            match.group(1)
            for line in body
            if re.search(r"\bfmul\b", line)
            if (match := _LLVM_VALUE_RE.match(line))
        }
        float_accumulates = _tainted_accumulate_lines(
            body, fmul_values, r"\bf(?:add|sub)\b"
        )
        integer_mul_lines, integer_mul_values = _integer_product_lines(body)
        integer_accumulates = _tainted_accumulate_lines(
            body, integer_mul_values, r"\b(?:add|sub)(?:\s+nsw|\s+nuw)*\s+i64\b"
        )
        for relative, line in enumerate(body, start=start + 1):
            if (
                operations
                and backend == "minimp3-float"
                and re.search(r"\bfmul\b", line)
            ):
                vector = _LLVM_VECTOR_RE.search(line)
                lanes = int(vector.group(1)) if vector else 1
                inserts.setdefault(relative, []).append(
                    "  call void @fastled_mp3_cpu_operation("
                    f"i32 {operation_stage_index}, i32 {lanes}, i32 0)"
                )
                operation_sites += 1
            elif (
                operations
                and backend == "minimp3-fixed"
                and relative - (start + 1) in integer_mul_lines
            ):
                inserts.setdefault(relative, []).append(
                    "  call void @fastled_mp3_cpu_operation("
                    f"i32 {operation_stage_index}, i32 1, i32 0)"
                )
                operation_sites += 1
            elif (
                operations
                and backend == "minimp3-fixed"
                and relative - (start + 1) in integer_accumulates
            ):
                inserts.setdefault(relative, []).append(
                    "  call void @fastled_mp3_cpu_operation("
                    f"i32 {operation_stage_index}, i32 0, i32 1)"
                )
                operation_sites += 1
            elif (
                operations
                and backend == "minimp3-float"
                and relative - (start + 1) in float_accumulates
            ):
                vector = _LLVM_VECTOR_RE.search(line)
                lanes = int(vector.group(1)) if vector else 1
                inserts.setdefault(relative, []).append(
                    "  call void @fastled_mp3_cpu_operation("
                    f"i32 {operation_stage_index}, i32 0, i32 {lanes})"
                )
                operation_sites += 1
    if operations and operation_sites == 0:
        raise RuntimeError(f"no arithmetic sites found in {backend} LLVM IR")
    declarations = [
        "declare void @fastled_mp3_cpu_stage_enter(i32)",
        "declare void @fastled_mp3_cpu_stage_exit(i32)",
        "declare void @fastled_mp3_cpu_operation(i32, i32, i32)",
        "",
    ]
    first_define = min(start for start, _, _ in functions)
    output: list[str] = []
    for index, line in enumerate(lines):
        if index == first_define:
            output.extend(declarations)
        output.extend(inserts.get(index, ()))
        output.append(line)
    return InstrumentationResult(
        text="\n".join(output) + "\n",
        operation_sites=operation_sites,
        stage_sites=stage_sites,
    )


def _common_compile_flags(optimization: str = "-O0") -> list[str]:
    return [
        f"-I{ROOT / 'src'}",
        f"-I{ROOT / 'src' / 'platforms' / 'stub'}",
        "-std=gnu++11",
        optimization,
        "-fno-inline",
        "-fno-exceptions",
        "-fno-rtti",
        "-fno-strict-aliasing",
        "-ffp-contract=off",
        "-fno-discard-value-names",
        "-DFASTLED_USE_PROGMEM=0",
        "-DSTUB_PLATFORM",
        "-DARDUINO=10808",
        "-DFASTLED_USE_STUB_ARDUINO",
        "-DFASTLED_STUB_IMPL",
        "-DFASTLED_TESTING",
        "-DFASTLED_NO_AUTO_NAMESPACE",
        "-DFASTLED_NO_PINMAP",
        "-DMINIMP3_NO_SIMD",
    ]


def build_instrumented_driver(*, operations: bool = True) -> InstrumentedDriver:
    BUILD.mkdir(parents=True, exist_ok=True)
    compiler = compiler_path()
    sources = {
        "minimp3-float": ROOT / "ci" / "codec_memory" / "minimp3_audit.cpp",
        "minimp3-fixed": ROOT / "ci" / "codec_cpu" / "minimp3_fixed_driver.cpp",
    }
    objects: list[Path] = []
    stage_coverage: dict[str, dict[str, dict[str, Any]]] = {}
    for backend, source in sources.items():
        stem = backend.replace("minimp3-", "minimp3_")
        mode = "operations" if operations else "timing"
        raw_ir = BUILD / f"{stem}.{mode}.ll"
        instrumented_ir = BUILD / f"{stem}.{mode}.instrumented.ll"
        obj = BUILD / f"{stem}.{mode}.instrumented.o"
        RunningProcess.run(
            [
                str(compiler),
                *_common_compile_flags("-O0" if operations else "-O1"),
                "-S",
                "-emit-llvm",
                str(source),
                "-o",
                str(raw_ir),
            ],
            check=True,
        )
        instrumentation = instrument_llvm_ir(
            raw_ir.read_text(encoding="utf-8"), backend, operations=operations
        )
        instrumented_ir.write_text(instrumentation.text, encoding="utf-8")
        print(
            f"LLVM_INSTRUMENT:{backend}:mode={mode}:"
            f"sites={instrumentation.operation_sites}"
        )
        fused = _FUSED_STAGES[backend]
        coverage: dict[str, dict[str, Any]] = {}
        for stage in STAGES:
            sites = instrumentation.stage_sites[stage]
            if stage in fused:
                coverage[stage] = {
                    "mode": "fused",
                    "fused_with": fused[stage],
                    "instrumentation_sites": 0,
                }
            else:
                if sites <= 0:
                    raise RuntimeError(
                        f"missing stage instrumentation for {backend}/{stage}"
                    )
                coverage[stage] = {
                    "mode": "dedicated",
                    "instrumentation_sites": sites,
                }
            print(
                f"STAGE_COVERAGE:backend={backend}:stage={stage}:"
                f"mode={coverage[stage]['mode']}:sites="
                f"{coverage[stage]['instrumentation_sites']}"
            )
        stage_coverage[backend] = coverage
        RunningProcess.run(
            [str(compiler), "-c", str(instrumented_ir), "-o", str(obj)],
            check=True,
        )
        objects.append(obj)
    suffix = ".exe" if os.name == "nt" else ""
    mode = "operations" if operations else "timing"
    binary = BUILD / f"codec_cpu_driver_{mode}{suffix}"
    RunningProcess.run(
        [
            str(compiler),
            *_common_compile_flags("-O0" if operations else "-O1"),
            str(Path(__file__).with_name("driver.cpp")),
            *(str(obj) for obj in objects),
            "-o",
            str(binary),
        ],
        check=True,
    )
    return InstrumentedDriver(binary=binary, stage_coverage=stage_coverage)


def build_plain_driver() -> Path:
    BUILD.mkdir(parents=True, exist_ok=True)
    compiler = compiler_path()
    objects: list[Path] = []
    plain_sources = {
        "minimp3": ROOT / "ci" / "codec_memory" / "minimp3_audit.cpp",
        "minimp3_fixed": ROOT / "ci" / "codec_cpu" / "minimp3_fixed_driver.cpp",
    }
    for backend, source in plain_sources.items():
        obj = BUILD / f"{backend}.plain.o"
        RunningProcess.run(
            [
                str(compiler),
                *_common_compile_flags("-O1"),
                "-g",
                "-gdwarf-4",
                "-c",
                str(source),
                "-o",
                str(obj),
            ],
            check=True,
        )
        objects.append(obj)
    suffix = ".exe" if os.name == "nt" else ""
    binary = BUILD / f"codec_cpu_driver_plain{suffix}"
    RunningProcess.run(
        [
            str(compiler),
            *_common_compile_flags("-O1"),
            "-g",
            "-gdwarf-4",
            str(Path(__file__).with_name("driver.cpp")),
            *(str(obj) for obj in objects),
            "-o",
            str(binary),
        ],
        check=True,
    )
    return binary


_OPS_RE = re.compile(r"OPS:backend=([^:]+):stage=([^:]+):multiplies=(\d+):macs=(\d+)")
_TIMING_RE = re.compile(r"TIMING:backend=([^:]+):stage=([^:]+):nanoseconds=(\d+)")
_RESULT_RE = re.compile(
    r"CPU_AUDIT_RESULT:backend=([^:]+):frames=(\d+):checksum=(\d+):cycles=(\d+)"
)


def run_operation_audit(binary: Path) -> dict[str, dict[str, Any]]:
    report: dict[str, dict[str, Any]] = {}
    for backend in BACKENDS:
        result = RunningProcess.run(
            [str(binary.resolve()), backend],
            cwd=ROOT,
            check=True,
            text=True,
            capture_output=True,
        )
        counts: dict[str, dict[str, int]] = {}
        for match in _OPS_RE.finditer(result.stdout):
            if match.group(1) == backend:
                counts[match.group(2)] = {
                    "multiplies": int(match.group(3)),
                    "macs": int(match.group(4)),
                }
        validate_operation_counts(counts)
        frame_match = _RESULT_RE.search(result.stdout)
        if not frame_match or frame_match.group(1) != backend:
            raise RuntimeError(f"operation profile result missing for {backend}")
        frames = int(frame_match.group(2))
        if frames <= 0:
            raise RuntimeError(f"operation profile decoded no frames for {backend}")
        stages: dict[str, dict[str, int | float]] = {}
        for stage, metrics in counts.items():
            stages[stage] = {
                "total_multiplies": metrics["multiplies"],
                "total_macs": metrics["macs"],
                "multiplies_per_frame": round(metrics["multiplies"] / frames, 6),
                "macs_per_frame": round(metrics["macs"] / frames, 6),
            }
        report[backend] = {"frames": frames, "stages": stages}
        print(result.stdout, end="")
    return report


def run_host_stage_profile(binary: Path) -> dict[str, dict[str, float]]:
    affinity = host_affinity_prefix()
    samples: dict[str, dict[str, list[float]]] = {
        backend: {stage: [] for stage in STAGES} for backend in BACKENDS
    }
    for backend in BACKENDS:
        expected_frames: int | None = None
        for _ in range(PROFILE_RUNS):
            result = RunningProcess.run(
                [*affinity, str(binary.resolve()), backend],
                cwd=ROOT,
                check=True,
                text=True,
                capture_output=True,
            )
            frame_match = _RESULT_RE.search(result.stdout)
            if not frame_match or frame_match.group(1) != backend:
                raise RuntimeError(f"timing profile result missing for {backend}")
            frames = int(frame_match.group(2))
            if expected_frames is None:
                expected_frames = frames
            elif frames != expected_frames:
                raise RuntimeError(f"non-deterministic frame count for {backend}")
            emitted: set[str] = set()
            for match in _TIMING_RE.finditer(result.stdout):
                if match.group(1) != backend:
                    continue
                stage = match.group(2)
                samples[backend][stage].append(int(match.group(3)) / frames)
                emitted.add(stage)
            if emitted != set(STAGES):
                raise RuntimeError(f"timing stages missing for {backend}")
    medians: dict[str, dict[str, float]] = {}
    for backend in BACKENDS:
        medians[backend] = {
            stage: round(statistics.median(samples[backend][stage]), 3)
            for stage in STAGES
        }
        for stage, value in medians[backend].items():
            print(
                f"HOST_STAGE:backend={backend}:stage={stage}:"
                f"median_ns_per_frame={value}:runs={PROFILE_RUNS}"
            )
    return medians


def run_cycle_profile(binary: Path) -> dict[str, float]:
    affinity = host_affinity_prefix()
    samples: dict[str, list[int]] = {backend: [] for backend in BACKENDS}
    for backend in BACKENDS:
        for _ in range(PROFILE_RUNS):
            result = RunningProcess.run(
                [
                    *affinity,
                    str(binary.resolve()),
                    backend,
                ],
                cwd=ROOT,
                check=True,
                text=True,
                capture_output=True,
            )
            match = _RESULT_RE.search(result.stdout)
            if not match or match.group(1) != backend:
                raise RuntimeError(f"cycle profile result missing for {backend}")
            samples[backend].append(int(match.group(4)))
    report: dict[str, float] = {}
    for backend in BACKENDS:
        report[backend] = round(statistics.median(samples[backend]), 6)
        print(
            f"HOST_CYCLES:backend={backend}:runs={PROFILE_RUNS}:"
            f"median={report[backend]}:source=clang-readcyclecounter"
        )
    return report


def parse_callgrind(path: Path) -> CallgrindData:
    events: list[str] = []
    summary_values: list[int] = []
    total_values: list[int] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("events:"):
            events = line.split()[1:]
        elif line.startswith("summary:") and events:
            summary_values = [int(value) for value in line.split()[1:]]
        elif line.startswith("totals:") and events:
            total_values = [int(value) for value in line.split()[1:]]
    values = total_values or summary_values
    summary = dict(zip(events, values, strict=True))
    if "Ir" not in summary:
        raise RuntimeError("callgrind output is missing instruction summary")
    return CallgrindData(summary=summary)


def parse_callgrind_attribution(text: str) -> dict[str, int]:
    functions: dict[str, int] = {}
    row_re = re.compile(r"^\s*([\d,]+)\s+\([^)]*\)\s+(.+)$")
    for line in text.splitlines():
        match = row_re.match(line)
        if not match:
            continue
        descriptor = match.group(2)
        marker = "src/third_party/"
        marker_index = descriptor.find(marker)
        if marker_index < 0:
            continue
        descriptor = descriptor[marker_index:]
        descriptor = re.sub(r"\s+\[[^]]+\]$", "", descriptor)
        instructions = int(match.group(1).replace(",", ""))
        functions[descriptor] = max(functions.get(descriptor, 0), instructions)
    if not functions:
        raise RuntimeError("callgrind output is missing codec function attribution")
    return functions


def run_callgrind_profile(binary: Path) -> dict[str, dict[str, Any]]:
    valgrind = shutil.which("valgrind")
    if not valgrind:
        raise RuntimeError("valgrind is required for host function attribution")
    annotate = shutil.which("callgrind_annotate")
    if not annotate:
        raise RuntimeError("callgrind_annotate is required for function attribution")
    report: dict[str, dict[str, Any]] = {}
    for backend in BACKENDS:
        output = BUILD / f"callgrind.{backend}.out"
        RunningProcess.run(
            [
                valgrind,
                "--quiet",
                "--tool=callgrind",
                "--branch-sim=yes",
                "--instr-atstart=no",
                f"--callgrind-out-file={output}",
                str(binary.resolve()),
                backend,
            ],
            cwd=ROOT,
            check=True,
            capture_output=True,
        )
        data = parse_callgrind(output)
        annotation = RunningProcess.run(
            [
                annotate,
                "--inclusive=yes",
                "--threshold=100",
                "--auto=no",
                "--show=Ir",
                str(output),
            ],
            cwd=ROOT,
            check=True,
            text=True,
            capture_output=True,
        )
        functions = parse_callgrind_attribution(annotation.stdout)
        top = sorted(functions.items(), key=lambda item: item[1], reverse=True)[:20]
        branches = data.summary.get("Bc", 0) + data.summary.get("Bi", 0)
        branch_misses = data.summary.get("Bcm", 0) + data.summary.get("Bim", 0)
        report[backend] = {
            "instructions": data.summary["Ir"],
            "branches": branches,
            "branch_misses": branch_misses,
            "functions": dict(top),
        }
        print(
            f"CALLGRIND:backend={backend}:instructions={data.summary['Ir']}:"
            f"branches={branches}:branch_misses={branch_misses}:functions={len(top)}"
        )
        for symbol, instructions in top:
            print(
                f"CALLGRIND_FUNCTION:backend={backend}:"
                f"instructions={instructions}:symbol={symbol}"
            )
    return report


def compose_host_counters(
    cycle_medians: dict[str, float], callgrind: dict[str, dict[str, Any]]
) -> dict[str, dict[str, int | float]]:
    report: dict[str, dict[str, int | float]] = {}
    for backend in BACKENDS:
        cycles = cycle_medians[backend]
        instructions = callgrind[backend]["instructions"]
        branch_misses = callgrind[backend]["branch_misses"]
        if cycles <= 0 or instructions <= 0 or branch_misses <= 0:
            raise RuntimeError(f"invalid deterministic host counters for {backend}")
        report[backend] = {
            "cycles": cycles,
            "instructions": instructions,
            "ipc": round(instructions / cycles, 6),
            "branch_misses": branch_misses,
        }
    return report


def validate_operation_counts(counts: dict[str, dict[str, int]]) -> None:
    for stage in STAGES:
        if stage not in counts:
            raise RuntimeError(f"missing operation stage: {stage}")
        metrics = counts[stage]
        for name in ("multiplies", "macs"):
            value = metrics.get(name)
            if not isinstance(value, int) or value < 0:
                raise RuntimeError(f"invalid {stage} {name}: {value}")
        if metrics["macs"] > metrics["multiplies"]:
            raise RuntimeError(f"MAC count exceeds multiplies for {stage}")


def check_regression(baseline: dict[str, int], current: dict[str, int]) -> None:
    for backend in BACKENDS:
        if backend not in baseline or backend not in current:
            raise RuntimeError(f"missing regression metric for {backend}")
        limit = baseline[backend] * (1.0 + REGRESSION_TOLERANCE)
        if current[backend] > limit:
            raise RuntimeError(f"{backend} regressed: {current[backend]} > {limit:.2f}")


def validate_operations(backend: str, operations: dict[str, Any]) -> None:
    frames = operations.get("frames")
    if not isinstance(frames, int) or frames <= 0:
        raise RuntimeError(f"invalid operation frame count for {backend}")
    stages = operations.get("stages")
    if not isinstance(stages, dict):
        raise RuntimeError(f"missing operation stages for {backend}")
    for stage in STAGES:
        metrics = stages.get(stage)
        if not isinstance(metrics, dict):
            raise RuntimeError(f"missing operation stage: {backend}/{stage}")
        multiplies = metrics.get("total_multiplies")
        macs = metrics.get("total_macs")
        if not isinstance(multiplies, int) or multiplies < 0:
            raise RuntimeError(f"invalid total multiplies for {backend}/{stage}")
        if not isinstance(macs, int) or macs < 0 or macs > multiplies:
            raise RuntimeError(f"invalid total MACs for {backend}/{stage}")
        expected = {
            "multiplies_per_frame": round(multiplies / frames, 6),
            "macs_per_frame": round(macs / frames, 6),
        }
        for name, value in expected.items():
            if metrics.get(name) != value:
                raise RuntimeError(
                    f"inexact derived operation metric {backend}/{stage}/{name}"
                )


def validate_host(backend: str, host: dict[str, Any]) -> None:
    counters = host.get("counter_median", {})
    for metric in ("cycles", "instructions", "ipc", "branch_misses"):
        if not isinstance(counters.get(metric), (int, float)) or counters[metric] <= 0:
            raise RuntimeError(f"missing host counter {backend}/{metric}")
    source = host.get("counter_source", {})
    if set(source) != {"cycles", "instructions", "ipc", "branch_misses"}:
        raise RuntimeError(f"missing host counter provenance for {backend}")
    if any(not isinstance(value, str) or not value for value in source.values()):
        raise RuntimeError(f"invalid host counter provenance for {backend}")
    callgrind = host.get("callgrind", {})
    for metric in ("instructions", "branches", "branch_misses"):
        if not isinstance(callgrind.get(metric), int) or callgrind[metric] <= 0:
            raise RuntimeError(f"missing callgrind {metric} for {backend}")
    if counters["instructions"] != callgrind["instructions"]:
        raise RuntimeError(f"host instruction sources disagree for {backend}")
    if counters["branch_misses"] != callgrind["branch_misses"]:
        raise RuntimeError(f"host branch-miss sources disagree for {backend}")
    expected_ipc = round(counters["instructions"] / counters["cycles"], 6)
    if counters["ipc"] != expected_ipc:
        raise RuntimeError(f"inexact derived host IPC for {backend}")
    functions = callgrind.get("functions")
    if not isinstance(functions, dict) or len(functions) < 8:
        raise RuntimeError(f"missing callgrind functions for {backend}")
    if any(not isinstance(value, int) or value <= 0 for value in functions.values()):
        raise RuntimeError(f"invalid callgrind function metric for {backend}")
    stage_ns = host.get("stage_ns_median", {})
    coverage = host.get("stage_coverage", {})
    for stage in STAGES:
        if not isinstance(stage_ns.get(stage), (int, float)) or stage_ns[stage] < 0:
            raise RuntimeError(f"missing stage timing {backend}/{stage}")
        stage_coverage = coverage.get(stage, {})
        mode = stage_coverage.get("mode")
        sites = stage_coverage.get("instrumentation_sites")
        if mode == "dedicated":
            if not isinstance(sites, int) or sites <= 0:
                raise RuntimeError(
                    f"missing dedicated stage coverage {backend}/{stage}"
                )
            if stage_ns[stage] <= 0:
                raise RuntimeError(f"zero dedicated stage timing {backend}/{stage}")
        elif mode == "fused":
            if sites != 0 or stage_coverage.get("fused_with") not in STAGES:
                raise RuntimeError(f"invalid fused stage coverage {backend}/{stage}")
            if stage_ns[stage] != 0:
                raise RuntimeError(
                    f"fused stage must not claim dedicated timing {backend}/{stage}"
                )
        else:
            raise RuntimeError(f"missing stage coverage {backend}/{stage}")


def validate_trend(trend: dict[str, Any]) -> None:
    if trend.get("schema_version") != 2:
        raise RuntimeError("codec CPU trend schema_version must be 2")
    if trend.get("profile_runs") != PROFILE_RUNS:
        raise RuntimeError(f"host profile must use N={PROFILE_RUNS}")
    if trend.get("regression_tolerance") != REGRESSION_TOLERANCE:
        raise RuntimeError("codec CPU trend tolerance must be 5 percent")
    environment = trend.get("environment")
    if not isinstance(environment, dict):
        raise RuntimeError("codec CPU trend is missing its host environment")
    if trend.get("host_key") != environment_key(environment):
        raise RuntimeError("codec CPU trend host_key does not match its environment")
    backends = trend.get("backends")
    if not isinstance(backends, dict):
        raise RuntimeError("codec CPU trend is missing backends")
    for backend in BACKENDS:
        entry = backends.get(backend)
        if not isinstance(entry, dict):
            raise RuntimeError(f"codec CPU trend is missing backend {backend}")
        validate_operations(backend, entry.get("operations", {}))
        validate_host(backend, entry.get("host", {}))
        codegen = entry.get("codegen", {})
        for target in TARGETS:
            target_entry = codegen.get(target, {})
            for kernel in KERNELS:
                metrics = target_entry.get(kernel, {})
                for name in ("instructions", "inner_loop_instructions"):
                    if not isinstance(metrics.get(name), int) or metrics[name] < 0:
                        raise RuntimeError(
                            f"missing codegen metric {backend}/{target}/{kernel}/{name}"
                        )
    host_baselines = trend.get("host_baselines", {})
    if not isinstance(host_baselines, dict):
        raise RuntimeError("codec CPU trend host_baselines must be an object")
    for key, profile in host_baselines.items():
        if not isinstance(profile, dict):
            raise RuntimeError(f"invalid host baseline {key}")
        profile_environment = profile.get("environment")
        if not isinstance(profile_environment, dict) or key != environment_key(
            profile_environment
        ):
            raise RuntimeError(f"host baseline key does not match environment: {key}")
        hosts = profile.get("hosts")
        if not isinstance(hosts, dict) or set(hosts) != set(BACKENDS):
            raise RuntimeError(f"host baseline is missing backends: {key}")
        for backend in BACKENDS:
            host = hosts[backend]
            if not isinstance(host, dict):
                raise RuntimeError(f"invalid host baseline {key}/{backend}")
            validate_host(backend, host)


def _check_upper_bound(path: str, baseline: float, current: float) -> None:
    if baseline == 0:
        if current != 0:
            raise RuntimeError(f"{path} changed from zero to {current}")
        return
    limit = baseline * (1.0 + REGRESSION_TOLERANCE)
    if current > limit:
        raise RuntimeError(f"{path} regressed: {current} > {limit:.6f}")


def _check_lower_bound(path: str, baseline: float, current: float) -> None:
    limit = baseline * (1.0 - REGRESSION_TOLERANCE)
    if current < limit:
        raise RuntimeError(f"{path} regressed: {current} < {limit:.6f}")


def check_trend(baseline: dict[str, Any], current: dict[str, Any]) -> None:
    """Enforce exact portable counts and a 5% CPU/codegen regression budget."""
    validate_trend(baseline)
    validate_trend(current)
    current_host_key = current.get("host_key")
    alternate_profile = None
    if baseline.get("host_key") != current_host_key:
        alternate_profile = baseline.get("host_baselines", {}).get(current_host_key)
        if alternate_profile is None:
            raise RuntimeError(f"host baseline is unknown: {current_host_key}")
    for backend in BACKENDS:
        baseline_entry = baseline["backends"][backend]
        current_entry = current["backends"][backend]
        baseline_operations = baseline_entry["operations"]
        current_operations = current_entry["operations"]
        if current_operations != baseline_operations:
            raise RuntimeError(f"exact operation ledger changed for {backend}")

        baseline_host = (
            baseline_entry["host"]
            if alternate_profile is None
            else alternate_profile["hosts"][backend]
        )
        current_host = current_entry["host"]
        for metric in ("cycles", "instructions", "branch_misses"):
            _check_upper_bound(
                f"{backend}/counter/{metric}",
                baseline_host["counter_median"][metric],
                current_host["counter_median"][metric],
            )
        _check_lower_bound(
            f"{backend}/counter/ipc",
            baseline_host["counter_median"]["ipc"],
            current_host["counter_median"]["ipc"],
        )
        for stage in STAGES:
            _check_upper_bound(
                f"{backend}/stage/{stage}",
                baseline_host["stage_ns_median"][stage],
                current_host["stage_ns_median"][stage],
            )
        for metric in ("instructions", "branches", "branch_misses"):
            _check_upper_bound(
                f"{backend}/callgrind/{metric}",
                baseline_host["callgrind"][metric],
                current_host["callgrind"][metric],
            )
        baseline_functions = baseline_host["callgrind"]["functions"]
        current_functions = current_host["callgrind"]["functions"]
        for symbol, baseline_instructions in baseline_functions.items():
            if symbol not in current_functions:
                raise RuntimeError(
                    f"callgrind attribution disappeared for {backend}/{symbol}"
                )
            baseline_share = (
                baseline_instructions / baseline_host["callgrind"]["instructions"]
            )
            current_share = (
                current_functions[symbol] / current_host["callgrind"]["instructions"]
            )
            _check_upper_bound(
                f"{backend}/callgrind-function/{symbol}",
                baseline_share,
                current_share,
            )

        for target in TARGETS:
            for kernel in KERNELS:
                for metric in ("instructions", "inner_loop_instructions"):
                    _check_upper_bound(
                        f"{backend}/codegen/{target}/{kernel}/{metric}",
                        baseline_entry["codegen"][target][kernel][metric],
                        current_entry["codegen"][target][kernel][metric],
                    )


_FUNCTION_RE = re.compile(r"^\s*[0-9a-fA-F]+\s+<([^>]+)>:\s*$")
_INSTRUCTION_RE = re.compile(
    r"^\s*([0-9a-fA-F]+):\s+(?:[0-9a-fA-F]{2,8}\s+)+([A-Za-z.][\w.]*)\s*(.*)$"
)
_TARGET_RE = re.compile(r"(?:0x)?([0-9a-fA-F]+)\s*<[^>]+>")
_CALL_MNEMONICS = {
    "bl",
    "blx",
    "call",
    "call0",
    "call4",
    "call8",
    "call12",
    "callx0",
    "callx4",
    "callx8",
    "callx12",
    "jal",
    "jalr",
}


def parse_disassembly(text: str, symbol: str) -> dict[str, int]:
    in_function = False
    instructions: list[tuple[int, str, str]] = []
    for line in text.splitlines():
        function = _FUNCTION_RE.match(line)
        if function:
            if in_function:
                break
            in_function = function.group(1) == symbol
            continue
        if not in_function:
            continue
        match = _INSTRUCTION_RE.match(line)
        if match:
            instructions.append(
                (int(match.group(1), 16), match.group(2), match.group(3))
            )
    if not instructions:
        raise RuntimeError(f"symbol missing from disassembly: {symbol}")
    loop_addresses: set[int] = set()
    addresses = {address for address, _, _ in instructions}
    for address, mnemonic, operands in instructions:
        lowered = mnemonic.lower()
        if lowered in _CALL_MNEMONICS:
            continue
        if not lowered.startswith(("b", "j", "loop")):
            continue
        targets = [int(value, 16) for value in _TARGET_RE.findall(operands)]
        if lowered.startswith("loop"):
            forwards = [target for target in targets if target > address]
            if forwards:
                end = min(forwards)
                loop_addresses.update(
                    item_address
                    for item_address, _, _ in instructions
                    if address < item_address < end
                )
            continue
        backwards = [
            target for target in targets if target < address and target in addresses
        ]
        if backwards:
            target = max(backwards)
            loop_addresses.update(
                item_address
                for item_address, _, _ in instructions
                if target <= item_address <= address
            )
    return {
        "instructions": len(instructions),
        "inner_loop_instructions": len(loop_addresses),
    }


def matching_disassembly_symbols(text: str, needles: tuple[str, ...]) -> list[str]:
    symbols: list[str] = []
    for line in text.splitlines():
        match = _FUNCTION_RE.match(line)
        if match:
            symbol = match.group(1)
            if not symbol.startswith(".") and any(
                needle in symbol for needle in needles
            ):
                symbols.append(symbol)
    return symbols


def aggregate_kernel_disassembly(text: str, needles: tuple[str, ...]) -> dict[str, int]:
    symbols = matching_disassembly_symbols(text, needles)
    if not symbols:
        raise RuntimeError(f"codegen kernel missing: {','.join(needles)}")
    totals = {"instructions": 0, "inner_loop_instructions": 0}
    for symbol in symbols:
        metrics = parse_disassembly(text, symbol)
        for name in totals:
            totals[name] += metrics[name]
    return totals


def _package_tool(package: str, executable: str) -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    names = (executable + suffix, executable)
    for name in names:
        located = shutil.which(name)
        if located:
            return Path(located)
    package_roots = []
    override = os.environ.get("PLATFORMIO_PACKAGES_DIR")
    if override:
        package_roots.append(Path(override))
    package_roots.append(Path.home() / ".platformio" / "packages")
    for root in package_roots:
        for name in names:
            candidate = root / package / "bin" / name
            if candidate.exists():
                return candidate
    raise RuntimeError(f"missing tool {executable} from PlatformIO package {package}")


def _target_tools(target: str) -> TargetTools:
    if target == "xtensa-esp32":
        prefix = "xtensa-esp32-elf"
        package = "toolchain-xtensa-esp32"
        flags: list[str] = [
            "-DFL_CODEC_CPU_CODEGEN_ESP_TYPES",
            "-D__thumb__",
        ]
    elif target == "riscv32-esp":
        prefix = "riscv32-esp-elf"
        package = "toolchain-riscv32-esp"
        flags = [
            "-march=rv32imc_zicsr",
            "-mabi=ilp32",
            "-DFL_CODEC_CPU_CODEGEN_ESP_TYPES",
            "-D__thumb__",
        ]
    elif target in ("cortex-m0plus", "cortex-m4"):
        prefix = "arm-none-eabi"
        package = "toolchain-gccarmnoneeabi"
        cpu = "cortex-m0plus" if target == "cortex-m0plus" else "cortex-m4"
        flags = [f"-mcpu={cpu}", "-mthumb", "-DARDUINO_ARCH_NRF52"]
    else:
        raise RuntimeError(f"unknown CPU audit target: {target}")
    return TargetTools(
        compiler=_package_tool(package, f"{prefix}-g++"),
        objdump=_package_tool(package, f"{prefix}-objdump"),
        flags=flags,
    )


def run_codegen_audit() -> dict[str, dict[str, dict[str, dict[str, int]]]]:
    BUILD.mkdir(parents=True, exist_ok=True)
    sources = {
        "minimp3-float": Path(__file__).with_name("minimp3_codegen.cpp"),
        "minimp3-fixed": Path(__file__).with_name("minimp3_fixed_codegen.cpp"),
    }
    kernel_needles = {
        "minimp3-float": {
            "dct32": ("mp3d_DCT_II",),
            "polyphase": ("mp3d_synth_pair",),
        },
        "minimp3-fixed": {
            "dct32": ("mp3d_DCT_II",),
            "polyphase": ("mp3d_synth_pair",),
        },
    }
    report: dict[str, dict[str, dict[str, dict[str, int]]]] = {
        backend: {} for backend in BACKENDS
    }
    for target in TARGETS:
        tools = _target_tools(target)
        for backend, source in sources.items():
            stem = backend.replace("minimp3-", "minimp3_")
            obj = BUILD / f"{stem}.{target}.o"
            cross_flags = [
                flag
                for flag in _common_compile_flags("-Os")
                if flag
                not in (
                    "-fno-discard-value-names",
                    "-fno-inline",
                    "-ffp-contract=off",
                )
            ]
            RunningProcess.run(
                [
                    str(tools.compiler),
                    *cross_flags,
                    "-ffunction-sections",
                    *tools.flags,
                    "-c",
                    str(source),
                    "-o",
                    str(obj),
                ],
                check=True,
            )
            result = RunningProcess.run(
                [str(tools.objdump), "-d", "-C", str(obj)],
                check=True,
                text=True,
                capture_output=True,
            )
            report[backend][target] = {}
            for kernel in KERNELS:
                metrics = aggregate_kernel_disassembly(
                    result.stdout, kernel_needles[backend][kernel]
                )
                report[backend][target][kernel] = metrics
                print(
                    f"CODEGEN:backend={backend}:target={target}:kernel={kernel}:"
                    f"instructions={metrics['instructions']}:"
                    "inner_loop_instructions="
                    f"{metrics['inner_loop_instructions']}"
                )
    return report


def _selected_affinity_cpu() -> int | None:
    if hasattr(os, "sched_getaffinity"):
        allowed = sorted(os.sched_getaffinity(0))
        if allowed:
            return allowed[0]
    return None


def make_affinity_prefix(taskset: str | None, cpu: int | None) -> list[str]:
    if not taskset or cpu is None:
        raise RuntimeError("host CPU audit requires taskset and a selectable CPU")
    return [taskset, "-c", str(cpu)]


def host_affinity_prefix() -> list[str]:
    return make_affinity_prefix(shutil.which("taskset"), _selected_affinity_cpu())


def host_environment() -> dict[str, Any]:
    cpu_model = platform.processor()
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.exists():
        match = re.search(
            r"^(?:model name|Hardware)\s*:\s*(.+)$",
            cpuinfo.read_text(encoding="utf-8", errors="replace"),
            re.MULTILINE,
        )
        if match:
            cpu_model = match.group(1).strip()
    if not cpu_model:
        raise RuntimeError("host CPU model is unavailable")
    affinity_cpu = _selected_affinity_cpu()
    governor = "not-exposed"
    if affinity_cpu is not None:
        governor_path = Path(
            f"/sys/devices/system/cpu/cpu{affinity_cpu}/cpufreq/scaling_governor"
        )
        if governor_path.exists():
            governor = governor_path.read_text(encoding="utf-8").strip()
    compiler = compiler_path()
    compiler_result = RunningProcess.run(
        [str(compiler), "--version"],
        check=True,
        text=True,
        capture_output=True,
    )
    return {
        "platform": platform.platform(),
        "cpu_model": cpu_model,
        "affinity_cpu": affinity_cpu,
        "governor": governor,
        "compiler": compiler_result.stdout.splitlines()[0],
    }


def corpus_provenance() -> list[dict[str, Any]]:
    provenance: list[dict[str, Any]] = []
    for relative in CORPUS:
        path = ROOT / relative
        data = path.read_bytes()
        provenance.append(
            {
                "path": relative,
                "bytes": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
            }
        )
    return provenance


def capture_audit() -> dict[str, Any]:
    operation_driver = build_instrumented_driver()
    operations = run_operation_audit(operation_driver.binary)
    timing_driver = build_instrumented_driver(operations=False)
    stage_timing = run_host_stage_profile(timing_driver.binary)
    plain = build_plain_driver()
    cycle_medians = run_cycle_profile(plain)
    callgrind = run_callgrind_profile(plain)
    counters = compose_host_counters(cycle_medians, callgrind)
    codegen = run_codegen_audit()
    environment = host_environment()
    report: dict[str, Any] = {
        "schema_version": 2,
        "profile_runs": PROFILE_RUNS,
        "regression_tolerance": REGRESSION_TOLERANCE,
        "environment": environment,
        "host_key": environment_key(environment),
        "corpus": corpus_provenance(),
        "backends": {},
    }
    for backend in BACKENDS:
        report["backends"][backend] = {
            "operations": operations[backend],
            "host": {
                "counter_median": counters[backend],
                "counter_source": {
                    "cycles": "Clang readcyclecounter median over 30 pinned runs",
                    "instructions": "Valgrind Callgrind deterministic Ir",
                    "ipc": "Callgrind Ir divided by median cycle counter",
                    "branch_misses": "Valgrind Callgrind simulated Bcm plus Bim",
                },
                "stage_ns_median": stage_timing[backend],
                "stage_coverage": timing_driver.stage_coverage[backend],
                "callgrind": callgrind[backend],
            },
            "codegen": codegen[backend],
        }
    validate_trend(report)
    return report


def write_report(report: dict[str, Any], path: Path) -> None:
    path.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(f"Wrote codec CPU audit report to {path}")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--codegen", action="store_true")
    parser.add_argument("--host", action="store_true")
    parser.add_argument("--callgrind", action="store_true")
    parser.add_argument("--linux-host", action="store_true")
    parser.add_argument("--operations", action="store_true")
    parser.add_argument("--cycles", action="store_true")
    parser.add_argument("--write", type=Path)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.all or args.check or args.write:
        current = capture_audit()
        if args.write:
            write_report(current, args.write)
        if args.check:
            check_trend(load_trend(), current)
            print("Codec CPU trend is complete and within the 5% budget")
        return 0
    if args.operations:
        run_operation_audit(build_instrumented_driver().binary)
    if args.host:
        run_host_stage_profile(build_instrumented_driver(operations=False).binary)
    if args.codegen:
        run_codegen_audit()
    if args.linux_host or args.cycles or args.callgrind:
        plain = build_plain_driver()
    if args.linux_host or args.cycles:
        run_cycle_profile(plain)
    if args.linux_host or args.callgrind:
        run_callgrind_profile(plain)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
