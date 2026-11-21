#!/usr/bin/env python3
"""Build and inspect isolated MP3 backend objects for memory auditing."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

from running_process import CalledProcessError, RunningProcess
from typeguard import typechecked

from ci.meson.native_launchers import resolve_soldr_clang


ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / ".build" / "codec-memory-audit"
BACKENDS = ("minimp3", "minimp3_fixed")
# Audit TU name -> the name the ledger uses for it.
LEDGER_NAMES = {
    "minimp3": "minimp3-float",
    "minimp3_fixed": "minimp3-fixed",
}
# Entry point each backend's decode call graph is rooted at.
CALLGRAPH_ROOTS = {
    "minimp3": "mp3dec_decode_frame_r",
    "minimp3_fixed": "mp3dec_decode_frame_r",
}
FRAME_LIMIT = 2048
RAM_LIMIT = 24 * 1024
# Retired Helix static-table total (12,744 bytes) + 20%. See check_ledger().
STATIC_TABLE_LIMIT = 15292
REGRESSION_FACTOR = 1.02
# Metrics that are measured and recorded but deliberately not regression-gated.
# Empty since FastLED#4106: `stack-watermark-observed` used to live here because
# it was not reproducible across CI runners. The cause turned out to be glibc --
# the harness forwarded the decoder's block moves to libc, so glibc's `mem*`
# frames sat inside the measured window and moved the figure by ~900 bytes
# depending on which implementation the host dispatched to. watermark.cpp now
# uses its own loops, the measurement is host-independent, and the metric is
# gated again like every other one.
INFORMATIONAL_METRICS: set[str] = set()

EXACT_METRICS = {
    "allocation-count",
    "decoder-state",
    "scratch",
    "stream-buffer",
    "pcm-output",
    "working-ram",
    "pipeline-peak",
}


@typechecked
@dataclass(frozen=True)
class AuditValues:
    summary: dict[tuple[str, str], int]
    tables: dict[tuple[str, str], int]


def compiler_from_meson() -> Path:
    soldr = resolve_soldr_clang()
    if soldr is not None:
        return Path(soldr.cxx)
    commands = ROOT / ".build" / "meson-quick" / "compile_commands.json"
    if commands.exists():
        entries = json.loads(commands.read_text(encoding="utf-8"))
        for entry in entries:
            if str(entry.get("file", "")).endswith("third_party+.cpp"):
                command = str(entry["command"])
                match = re.match(r'"([^"]+)"', command)
                if match:
                    candidate = Path(match.group(1))
                    is_native = os.name == "nt" or candidate.suffix != ".exe"
                    if is_native and candidate.exists():
                        return candidate
    fallback = ROOT / ".cached" / "clang-native" / "ctc-clang++.exe"
    if os.name == "nt" and fallback.exists():
        return fallback
    system_compiler = shutil.which("clang++")
    if system_compiler:
        return Path(system_compiler)
    raise RuntimeError("configure a native build first (bash test --cpp)")


def compile_objects() -> dict[str, Path]:
    BUILD.mkdir(parents=True, exist_ok=True)
    compiler = compiler_from_meson()
    common = [
        str(compiler),
        f"-I{ROOT / 'src'}",
        f"-I{ROOT / 'src' / 'platforms' / 'stub'}",
        "-std=gnu++11",
        "-Os",
        "-g",
        "-fno-exceptions",
        "-fno-rtti",
        "-fno-strict-aliasing",
        "-fstack-usage",
        f"-Wframe-larger-than={FRAME_LIMIT}",
        "-Werror=frame-larger-than",
        "-DFASTLED_USE_PROGMEM=0",
        "-DSTUB_PLATFORM",
        "-DARDUINO=10808",
        "-DFASTLED_USE_STUB_ARDUINO",
        "-DFASTLED_STUB_IMPL",
        "-DFASTLED_TESTING",
        "-DFASTLED_NO_AUTO_NAMESPACE",
        "-DFASTLED_NO_PINMAP",
        "-c",
    ]
    objects: dict[str, Path] = {}
    for backend in BACKENDS:
        source = Path(__file__).with_name(f"{backend}_audit.cpp")
        output = BUILD / f"{backend}_audit.o"
        RunningProcess.run(common + [str(source), "-o", str(output)], check=True)
        ir_output = BUILD / f"{backend}_audit.ll"
        ir_command = [arg for arg in common if arg not in ("-c", "-fstack-usage")]
        RunningProcess.run(
            ir_command + ["-S", "-emit-llvm", str(source), "-o", str(ir_output)],
            check=True,
        )
        objects[backend] = output
    return objects


def parse_stack_usage(path: Path) -> list[tuple[str, int, str]]:
    rows: list[tuple[str, int, str]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.rsplit("\t", 2)
        if len(parts) != 3:
            continue
        location_and_name, size_text, kind = parts
        location = re.match(r"^.+?:\d+:(?:\d+:)?(.*)$", location_and_name)
        if not location:
            continue
        name = location.group(1)
        rows.append((name, int(size_text), kind))
    return rows


def parse_llvm_callgraph(path: Path) -> dict[str, set[str]]:
    graph: dict[str, set[str]] = {}
    current: str | None = None
    symbol = r'@(?:"([^"]+)"|([^\s(]+))\('
    define_re = re.compile(rf"^\s*define\b.*{symbol}")
    call_re = re.compile(rf"\b(?:call|invoke)\b.*{symbol}")
    for line in path.read_text(encoding="utf-8").splitlines():
        define = define_re.search(line)
        if define:
            current = define.group(1) or define.group(2)
            graph.setdefault(current, set())
            continue
        if current is None:
            continue
        if line.strip() == "}":
            current = None
            continue
        call = call_re.search(line)
        if call:
            graph[current].add(call.group(1) or call.group(2))
    return graph


def longest_stack_path(
    frames: dict[str, int], graph: dict[str, set[str]], root: str
) -> int:
    if root not in graph:
        raise RuntimeError(f"decode root missing from LLVM IR: {root}")
    visiting: set[str] = set()
    memo: dict[str, int] = {}

    def visit(node: str) -> int:
        if node in memo:
            return memo[node]
        if node in visiting:
            raise RuntimeError(f"recursive decode call graph at {node}")
        if node not in frames:
            raise RuntimeError(f"stack frame missing for reachable function: {node}")
        visiting.add(node)
        children = [child for child in graph[node] if child in graph]
        child_depth = max((visit(child) for child in children), default=0)
        visiting.remove(node)
        memo[node] = frames[node] + child_depth
        return memo[node]

    return visit(root)


def resolve_callgraph_root(graph: dict[str, set[str]], api_name: str) -> str:
    candidates = [name for name in graph if name == api_name or api_name in name]
    if len(candidates) != 1:
        raise RuntimeError(
            f"expected one LLVM IR root for {api_name}, found {len(candidates)}"
        )
    return candidates[0]


def nm_path(compiler: Path) -> Path:
    candidate = compiler.with_name("ctc-llvm-nm.exe")
    if candidate.exists():
        return candidate
    system_nm = shutil.which("llvm-nm")
    if system_nm:
        return Path(system_nm)
    raise RuntimeError("llvm-nm is required for the static-table audit")


def size_path() -> Path:
    system_size = shutil.which("llvm-size")
    if system_size:
        return Path(system_size)
    raise RuntimeError("llvm-size is required for the codec object audit")


def print_report(
    objects: dict[str, Path],
) -> AuditValues:
    compiler = compiler_from_meson()
    nm = nm_path(compiler)
    size_tool = size_path() if os.name != "nt" else None
    metrics: dict[tuple[str, str], int] = {}
    tables: dict[tuple[str, str], int] = {}
    for backend, obj in objects.items():
        su = obj.with_suffix(".su")
        rows = parse_stack_usage(su)
        rows.sort(key=lambda row: row[1], reverse=True)
        max_frame = rows[0][1] if rows else 0
        print(f"STACK:{backend}:max-frame={max_frame}")
        ledger_backend = LEDGER_NAMES[backend]
        metrics[(ledger_backend, "stack-max-frame")] = max_frame
        for name, size, kind in rows[:12]:
            print(f"STACK_ITEM:{backend}:{size}:{kind}:{name}")
        result = RunningProcess.run(
            [str(nm), "--print-size", "--size-sort", "--demangle", str(obj)],
            check=True,
            text=True,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )
        table_total = 0
        for line in result.stdout.splitlines():
            fields = line.split(maxsplit=3)
            if len(fields) == 4 and fields[2].lower() == "r":
                size = int(fields[1], 16)
                if size:
                    table_total += size
                    print(f"TABLE:{backend}:{size}:{fields[3]}")
                    short_name = fields[3].rsplit("::", 1)[-1]
                    key = (ledger_backend, short_name)
                    if key in tables:
                        raise RuntimeError(
                            "duplicate static-table short name: "
                            f"{ledger_backend}/{short_name}"
                        )
                    tables[key] = size
        print(f"TABLE_TOTAL:{backend}:{table_total}")
        metrics[(ledger_backend, "static-tables")] = table_total
        if size_tool:
            size_result = RunningProcess.run(
                [str(size_tool), str(obj)],
                check=True,
                text=True,
                capture_output=True,
                encoding="utf-8",
                errors="replace",
            )
            fields = size_result.stdout.splitlines()[1].split()
            text_bytes, data_bytes, bss_bytes = map(int, fields[:3])
            metrics[(ledger_backend, "object-text")] = text_bytes
            metrics[(ledger_backend, "object-data")] = data_bytes
            metrics[(ledger_backend, "object-bss")] = bss_bytes
            print(
                f"OBJECT_SIZE:{ledger_backend}:text={text_bytes}:"
                f"data={data_bytes}:bss={bss_bytes}"
            )

    def frame_map(backend: str) -> dict[str, int]:
        return {
            name: size
            for name, size, _ in parse_stack_usage(objects[backend].with_suffix(".su"))
        }

    over_budget: list[str] = []
    for backend in BACKENDS:
        graph = parse_llvm_callgraph(objects[backend].with_suffix(".ll"))
        depth = longest_stack_path(
            frame_map(backend),
            graph,
            resolve_callgraph_root(graph, CALLGRAPH_ROOTS[backend]),
        )
        ledger_backend = LEDGER_NAMES[backend]
        metrics[(ledger_backend, "stack-callgraph")] = depth
        print(f"CALLGRAPH:{ledger_backend}:decode-depth={depth}")
        if depth > FRAME_LIMIT:
            over_budget.append(ledger_backend)
    if over_budget:
        raise RuntimeError(
            "static decode call graph exceeds the 2 KiB stack budget: "
            + ", ".join(over_budget)
        )
    return AuditValues(metrics, tables)


def parse_massif_codec_peak(path: Path) -> int:
    peak = 0
    snapshot_total = 0
    pattern = re.compile(r"^\s*n\d+:\s+(\d+)\s+.*Mp3MemoryAllocate")
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("snapshot="):
            peak = max(peak, snapshot_total)
            snapshot_total = 0
            continue
        match = pattern.search(line)
        if match:
            snapshot_total += int(match.group(1))
    peak = max(peak, snapshot_total)
    if peak == 0:
        raise RuntimeError("Massif did not attribute heap to Mp3MemoryAllocate")
    return peak


def print_captured(output: str) -> None:
    print(output, end="" if output.endswith("\n") else "\n")


def run_watermark(
    objects: dict[str, Path], hook_peaks: dict[str, int]
) -> dict[tuple[str, str], int]:
    if os.name == "nt":
        raise RuntimeError("the full watermark audit requires Linux and Valgrind")
    valgrind = shutil.which("valgrind")
    if not valgrind:
        raise RuntimeError("Valgrind is required for the full watermark audit")
    compiler = compiler_from_meson()
    binary = BUILD / "watermark"
    command = [
        str(compiler),
        f"-I{ROOT / 'src'}",
        "-std=gnu++11",
        "-Os",
        "-g",
        "-fno-exceptions",
        "-fno-rtti",
        "-mno-red-zone",
        str(Path(__file__).with_name("watermark.cpp")),
        str(objects["minimp3"]),
        "-pthread",
        "-o",
        str(binary),
    ]
    RunningProcess.run(command, check=True)
    result = RunningProcess.run(
        [str(binary)],
        cwd=ROOT,
        check=True,
        text=True,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    print_captured(result.stdout)
    metrics: dict[tuple[str, str], int] = {}
    for line in result.stdout.splitlines():
        match = re.fullmatch(
            r"WATERMARK:backend=([^ ]+) decode=(\d+)",
            line,
        )
        if match:
            metrics[(match.group(1), "stack-watermark-observed")] = int(match.group(2))
    for backend, hook_peak in hook_peaks.items():
        massif = BUILD / f"massif-{backend}.out"
        RunningProcess.run(
            [
                valgrind,
                "--quiet",
                "--tool=massif",
                "--stacks=no",
                "--time-unit=B",
                "--detailed-freq=1",
                "--threshold=0.0",
                "--max-snapshots=1000",
                f"--massif-out-file={massif}",
                str(binary),
                backend,
            ],
            cwd=ROOT,
            check=True,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )
        process_peak = 0
        for line in massif.read_text(encoding="utf-8").splitlines():
            if line.startswith("mem_heap_B="):
                process_peak = max(process_peak, int(line.split("=", 1)[1]))
        codec_peak = parse_massif_codec_peak(massif)
        if codec_peak != hook_peak:
            raise RuntimeError(
                f"{backend} Massif codec peak {codec_peak} differs from "
                f"hook peak {hook_peak}"
            )
        print(
            f"MASSIF:backend={backend}:process-peak={process_peak}:"
            f"codec-peak={codec_peak}:hook-peak={hook_peak}"
        )
        metrics[(backend, "massif-codec-peak")] = codec_peak
    return metrics


def parse_profile_output(output: str) -> dict[tuple[str, str], int]:
    metrics: dict[tuple[str, str], int] = {}
    for line in output.splitlines():
        if not line.startswith("MP3_MEMORY:"):
            continue
        values = dict(item.split("=", 1) for item in line.split()[0:])
        backend = values.pop("MP3_MEMORY:backend")
        metrics[(backend, "pipeline-peak")] = int(values["peak"])
        metrics[(backend, "allocation-count")] = int(values["allocations"])
        for metric in ("decoder-state", "scratch", "stream-buffer", "pcm-output"):
            metrics[(backend, metric)] = int(values[metric])
        metrics[(backend, "codec-core")] = (
            metrics[(backend, "decoder-state")] + metrics[(backend, "scratch")]
        )
        metrics[(backend, "working-ram")] = (
            metrics[(backend, "codec-core")] + metrics[(backend, "stream-buffer")]
        )
    return metrics


def profile_metrics(binary: Path) -> dict[tuple[str, str], int]:
    result = RunningProcess.run(
        [str(binary.resolve())],
        cwd=ROOT,
        check=True,
        text=True,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    print_captured(result.stdout)
    metrics = parse_profile_output(result.stdout)
    for backend in ("minimp3-float", "minimp3-fixed"):
        for metric in EXACT_METRICS:
            if (backend, metric) not in metrics:
                raise RuntimeError(f"production profile missing {backend}/{metric}")
    return metrics


def ledger_values() -> AuditValues:
    summary: dict[tuple[str, str], int] = {}
    tables: dict[tuple[str, str], int] = {}
    for line in (
        (ROOT / "codec_memory_ledger.md").read_text(encoding="utf-8").splitlines()
    ):
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) == 3 and cells[2].isdigit():
            summary[(cells[0], cells[1])] = int(cells[2])
        elif len(cells) == 4 and cells[3].isdigit():
            tables[(cells[0], cells[2])] = int(cells[3])
    if not summary or not tables:
        raise RuntimeError("codec_memory_ledger.md is missing machine-readable rows")
    return AuditValues(summary, tables)


def check_ledger(
    current: dict[tuple[str, str], int],
    current_tables: dict[tuple[str, str], int],
) -> None:
    expected_values = ledger_values()
    expected = expected_values.summary
    expected_tables = expected_values.tables
    errors: list[str] = []
    for key, baseline in expected.items():
        if key not in current:
            errors.append(f"missing current metric {key[0]}/{key[1]}")
            continue
        if key[1] in EXACT_METRICS and current[key] != baseline:
            errors.append(
                f"{key[0]}/{key[1]} changed: {current[key]} != {baseline}; "
                "update the ledger deliberately"
            )
            continue
        if key[1] in INFORMATIONAL_METRICS:
            if current[key] != baseline:
                print(
                    f"INFORMATIONAL:{key[0]}/{key[1]}={current[key]} "
                    f"(ledger {baseline}; not gated, see FastLED#4106)"
                )
            continue
        limit = int(baseline * REGRESSION_FACTOR)
        if current[key] > limit:
            errors.append(f"{key[0]}/{key[1]} regressed: {current[key]} > {limit}")
    for key in current.keys() - expected.keys():
        errors.append(f"new summary metric missing from ledger: {key[0]}/{key[1]}")
    for key, size in current_tables.items():
        if key not in expected_tables:
            errors.append(f"new static table missing from ledger: {key[0]}/{key[1]}")
        elif size > int(expected_tables[key] * REGRESSION_FACTOR):
            errors.append(f"static table regressed: {key[0]}/{key[1]}={size}")
    for key in expected_tables.keys() - current_tables.keys():
        errors.append(f"ledger table was not emitted: {key[0]}/{key[1]}")

    for backend in ("minimp3-float", "minimp3-fixed"):
        working_ram = current.get((backend, "working-ram"))
        if working_ram is not None and working_ram > RAM_LIMIT:
            errors.append(f"{backend} working RAM exceeds 24 KiB")
    for backend in ("minimp3-float", "minimp3-fixed"):
        stack_depth = current.get((backend, "stack-callgraph"))
        if stack_depth is not None and stack_depth > FRAME_LIMIT:
            errors.append(f"{backend} decode stack exceeds 2 KiB")
    # Was "below Helix's static tables +20%", computed live from the Helix
    # audit object. Helix is gone, so the referent is gone with it; the budget
    # it produced is kept as the absolute number it always evaluated to
    # (12,744 * 1.2), which is the figure the ledger prose already quotes.
    for backend in ("minimp3-float", "minimp3-fixed"):
        tables_total = current.get((backend, "static-tables"))
        if tables_total is not None and tables_total > STATIC_TABLE_LIMIT:
            errors.append(
                f"{backend} static tables exceed the {STATIC_TABLE_LIMIT}-byte "
                "budget (retired Helix baseline +20%)"
            )
    if errors:
        raise RuntimeError("; ".join(errors))
    print("LEDGER:PASS:all metrics within budgets and 2% regression allowance")


# --- AVR stack ratchet (FastLED#5332) ---------------------------------------
# Automates the manual "Stack inventory" recipe in
# agents/docs/binary-size-reference.md: compile every unity TU under
# src/fl/build/ for the ATmega328P with avr-gcc `-fstack-usage` and compare each
# frame against avr_stack_ledger.json. The ratchet is one-sided: a tracked frame
# that grows, or a new frame at or above AVR_STACK_FLOOR, fails; frames that
# shrink or disappear only print a note (run --avr-stack-update to tighten).
# Each unity TU emits every inline/template body it instantiates, so this is a
# per-TU compile inventory, not a link-time call graph.
AVR_STACK_LEDGER = Path(__file__).with_name("avr_stack_ledger.json")
# 128 B keeps the ledger to a few hundred frames and still covers every frame
# FastLED#5307 fixed (readLine 654 B, printf callers ~359 B, digitalMultiWrite).
AVR_STACK_FLOOR = 128
AVR_BUILD = BUILD / "avr-stack"
AVR_COMPILEDB_CANDIDATES = (
    ROOT / ".build" / "fbuild" / "uno" / ".fbuild" / "build" / "release",
    ROOT / ".build" / "fbuild" / "uno",
)


@typechecked
@dataclass(frozen=True)
class AvrToolchain:
    cxx: Path
    core: Path
    variant: Path


def avr_toolchain_from_compiledb(entries: list[dict[str, object]]) -> AvrToolchain:
    """Find the avr-g++ and arduino-avr core that `bash compile uno` used."""
    core: Path | None = None
    variant: Path | None = None
    cxx: Path | None = None
    for entry in entries:
        arguments = entry.get("arguments")
        if not isinstance(arguments, list):
            continue
        for argument in arguments:
            if not isinstance(argument, str) or not argument.startswith("-I"):
                continue
            include = Path(argument[2:])
            if include.parts[-2:] == ("cores", "arduino"):
                core = include
            elif include.parts[-2:] == ("variants", "standard"):
                variant = include
            elif include.parts[-2:] == ("avr", "include"):
                for exe in ("avr-g++", "avr-g++.exe"):
                    candidate = include.parent / "bin" / exe
                    if candidate.is_file():
                        cxx = candidate
                        break
        if core and variant and cxx:
            return AvrToolchain(cxx, core, variant)
    raise RuntimeError(
        "uno compile database has no avr-g++/arduino-avr core; "
        "run `bash compile uno --examples Blink` first"
    )


def load_avr_toolchain() -> AvrToolchain:
    for directory in AVR_COMPILEDB_CANDIDATES:
        database = directory / "compile_commands.json"
        if database.is_file():
            entries = json.loads(database.read_text(encoding="utf-8"))
            return avr_toolchain_from_compiledb(entries)
    raise RuntimeError(
        "no uno compile_commands.json; run `bash compile uno --examples Blink` first"
    )


def parse_avr_stack_usage(path: Path) -> list[tuple[str, int, str]]:
    """Parse avr-gcc `.su` rows, keyed `<file basename>:<function>`.

    avr-gcc 7.3 drops everything up to the last '.' of a function name, so
    `fl::function_list<void(Args ...)>::compact()` prints as `)>::compact()`.
    Keeping the declaring file (but not the line, which moves on every edit)
    stops those truncated names from colliding across headers.
    """
    rows: list[tuple[str, int, str]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.rsplit("\t", 2)
        if len(parts) != 3:
            continue
        location = re.match(r"^(.*?):\d+:\d+:(.*)$", parts[0])
        if not location:
            continue
        source = re.split(r"[\\/]", location.group(1))[-1]
        name = f"{source}:{location.group(2)}"
        rows.append((name, int(parts[1]), parts[2]))
    return rows


def avr_stack_frames(rows: list[tuple[str, int, str]]) -> dict[str, int]:
    """Largest frame per function name, dropping frames below the floor.

    Inline and template bodies are emitted by every TU that uses them, so the
    same name can appear more than once; the largest copy is the one that
    counts.
    """
    frames: dict[str, int] = {}
    for name, size, _kind in rows:
        if size >= AVR_STACK_FLOOR and size > frames.get(name, -1):
            frames[name] = size
    return frames


def compile_avr_stack(toolchain: AvrToolchain) -> dict[str, int]:
    if AVR_BUILD.exists():
        shutil.rmtree(AVR_BUILD)
    AVR_BUILD.mkdir(parents=True)
    source_root = ROOT / "src"
    common = [
        str(toolchain.cxx),
        "-mmcu=atmega328p",
        "-Os",
        "-std=gnu++11",
        "-fno-threadsafe-statics",
        "-fno-exceptions",
        "-ffunction-sections",
        "-fdata-sections",
        "-w",
        "-DF_CPU=16000000L",
        "-DARDUINO=10808",
        "-DARDUINO_ARCH_AVR",
        "-DARDUINO_AVR_UNO",
        f"-I{toolchain.core}",
        f"-I{toolchain.variant}",
        f"-I{source_root}",
        "-fstack-usage",
        "-c",
    ]
    sources = sorted((source_root / "fl" / "build").glob("*.cpp"))
    if not sources:
        raise RuntimeError("no unity TUs under src/fl/build")

    def compile_one(source: Path) -> list[tuple[str, int, str]]:
        output = AVR_BUILD / f"{source.stem}.o"
        # A partial inventory would silently pass, so any failure is fatal.
        RunningProcess.run(common + [str(source), "-o", str(output)], check=True)
        return parse_avr_stack_usage(output.with_suffix(".su"))

    rows: list[tuple[str, int, str]] = []
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 1) as pool:
        for source_rows in pool.map(compile_one, sources):
            rows.extend(source_rows)
    return avr_stack_frames(rows)


def avr_stack_ledger() -> dict[str, int]:
    data = json.loads(AVR_STACK_LEDGER.read_text(encoding="utf-8"))
    frames = data["frames"]
    if not isinstance(frames, dict) or not frames:
        raise RuntimeError(f"{AVR_STACK_LEDGER.name} has no frames")
    return {str(name): int(size) for name, size in frames.items()}


def check_avr_stack(
    current: dict[str, int], ledger: dict[str, int]
) -> tuple[list[str], list[str]]:
    """Return (errors, notes). Only growth is an error."""
    errors: list[str] = []
    notes: list[str] = []
    for name, size in sorted(current.items()):
        baseline = ledger.get(name)
        if baseline is None:
            errors.append(f"new AVR frame {name} = {size} B (floor {AVR_STACK_FLOOR})")
        elif size > baseline:
            errors.append(f"AVR frame grew: {name} {size} > {baseline}")
        elif size < baseline:
            notes.append(f"AVR frame shrank: {name} {size} < {baseline}")
    for name in sorted(ledger.keys() - current.keys()):
        notes.append(f"AVR frame gone or below floor: {name} (ledger {ledger[name]})")
    return errors, notes


def write_avr_stack_ledger(current: dict[str, int], toolchain: AvrToolchain) -> None:
    data = {
        "_comment": (
            "AVR (ATmega328P) stack-frame ratchet, FastLED#5332. Regenerate with "
            "`uv run python ci/codec_memory/audit.py --avr-stack-update` after "
            "`bash compile uno --examples Blink`."
        ),
        "toolchain": toolchain.cxx.parents[2].name,
        "floor": AVR_STACK_FLOOR,
        "frames": dict(sorted(current.items())),
    }
    AVR_STACK_LEDGER.write_text(
        json.dumps(data, indent=2, sort_keys=False) + "\n", encoding="utf-8"
    )


def run_avr_stack(update: bool) -> None:
    toolchain = load_avr_toolchain()
    print(f"AVR_STACK:toolchain={toolchain.cxx}")
    current = compile_avr_stack(toolchain)
    for name, size in sorted(current.items(), key=lambda item: -item[1])[:20]:
        print(f"AVR_STACK:frame {size:6d} {name}")
    if update:
        write_avr_stack_ledger(current, toolchain)
        print(f"AVR_STACK:wrote {len(current)} frames to {AVR_STACK_LEDGER.name}")
        return
    errors, notes = check_avr_stack(current, avr_stack_ledger())
    for note in notes:
        print(f"AVR_STACK:NOTE:{note} (--avr-stack-update tightens the ratchet)")
    if errors:
        raise RuntimeError(
            "; ".join(errors) + "; shrink the frame, or if the growth is "
            "intended rerun with --avr-stack-update and commit the ledger"
        )
    print(f"AVR_STACK:PASS:{len(current)} frames within the ratchet")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compile-only", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--profile-binary", type=Path)
    parser.add_argument(
        "--avr-stack",
        action="store_true",
        help="check ATmega328P stack frames against avr_stack_ledger.json",
    )
    parser.add_argument(
        "--avr-stack-update",
        action="store_true",
        help="rewrite avr_stack_ledger.json from the current AVR frames",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.avr_stack or args.avr_stack_update:
        try:
            run_avr_stack(update=args.avr_stack_update)
        except (OSError, RuntimeError, CalledProcessError, ValueError) as exc:
            print(f"AVR stack audit failed: {exc}", file=sys.stderr)
            return 1
        return 0
    try:
        objects = compile_objects()
        if not args.compile_only:
            report = print_report(objects)
            metrics = report.summary
            tables = report.tables
            if not args.profile_binary:
                raise RuntimeError("full audit requires --profile-binary")
            metrics.update(profile_metrics(args.profile_binary))
            hook_peaks = {
                backend: metrics[(backend, "pipeline-peak")]
                for backend in ("minimp3-float",)
            }
            metrics.update(run_watermark(objects, hook_peaks))
            if args.check:
                check_ledger(metrics, tables)
    except (OSError, RuntimeError, CalledProcessError, ValueError) as exc:
        print(f"codec memory audit failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
