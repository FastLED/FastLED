#!/usr/bin/env python3
"""Build and inspect isolated MP3 backend objects for memory auditing."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / ".build" / "codec-memory-audit"
BACKENDS = ("helix", "minimp3")
FRAME_LIMIT = 2048
RAM_LIMIT = 24 * 1024
REGRESSION_FACTOR = 1.02
EXACT_METRICS = {
    "allocation-count",
    "decoder-state",
    "scratch",
    "stream-buffer",
    "pcm-output",
    "working-ram",
    "pipeline-peak",
}


def compiler_from_meson() -> Path:
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
        name = location_and_name.split(":", 3)[-1]
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
) -> tuple[dict[tuple[str, str], int], dict[tuple[str, str], int]]:
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
        ledger_backend = "minimp3-float" if backend == "minimp3" else backend
        metrics[(ledger_backend, "stack-max-frame")] = max_frame
        for name, size, kind in rows[:12]:
            print(f"STACK_ITEM:{backend}:{size}:{kind}:{name}")
        result = RunningProcess.run(
            [str(nm), "--print-size", "--size-sort", "--demangle", str(obj)],
            check=True,
            text=True,
            capture_output=True,
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

    helix_graph = parse_llvm_callgraph(objects["helix"].with_suffix(".ll"))
    minimp3_graph = parse_llvm_callgraph(objects["minimp3"].with_suffix(".ll"))
    helix_depth = longest_stack_path(
        frame_map("helix"),
        helix_graph,
        resolve_callgraph_root(helix_graph, "MP3Decode"),
    )
    minimp3_depth = longest_stack_path(
        frame_map("minimp3"),
        minimp3_graph,
        resolve_callgraph_root(minimp3_graph, "mp3dec_decode_frame_r"),
    )
    metrics[("helix", "stack-callgraph")] = helix_depth
    metrics[("minimp3-float", "stack-callgraph")] = minimp3_depth
    print(f"CALLGRAPH:helix:decode-depth={helix_depth}")
    print(f"CALLGRAPH:minimp3-float:decode-depth={minimp3_depth}")
    if helix_depth > FRAME_LIMIT or minimp3_depth > FRAME_LIMIT:
        raise RuntimeError("static decode call graph exceeds the 2 KiB stack budget")
    return metrics, tables


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
        return {}
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
        str(objects["helix"]),
        str(objects["minimp3"]),
        "-pthread",
        "-o",
        str(binary),
    ]
    RunningProcess.run(command, check=True)
    result = RunningProcess.run(
        [str(binary)], cwd=ROOT, check=True, text=True, capture_output=True
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
    valgrind = shutil.which("valgrind")
    if not valgrind:
        return metrics
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
    )
    print_captured(result.stdout)
    metrics = parse_profile_output(result.stdout)
    for backend in ("helix", "minimp3-float"):
        for metric in EXACT_METRICS:
            if (backend, metric) not in metrics:
                raise RuntimeError(f"production profile missing {backend}/{metric}")
    return metrics


def ledger_values() -> tuple[dict[tuple[str, str], int], dict[tuple[str, str], int]]:
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
    return summary, tables


def check_ledger(
    current: dict[tuple[str, str], int],
    current_tables: dict[tuple[str, str], int],
) -> None:
    expected, expected_tables = ledger_values()
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

    working_ram = current.get(("minimp3-float", "working-ram"))
    if working_ram is not None and working_ram > RAM_LIMIT:
        errors.append("minimp3 working RAM exceeds 24 KiB")
    for backend in ("helix", "minimp3-float"):
        stack_depth = current.get((backend, "stack-callgraph"))
        if stack_depth is not None and stack_depth > FRAME_LIMIT:
            errors.append(f"{backend} decode stack exceeds 2 KiB")
    helix_tables = current.get(("helix", "static-tables"))
    minimp3_tables = current.get(("minimp3-float", "static-tables"))
    if (
        helix_tables is not None
        and minimp3_tables is not None
        and minimp3_tables > int(helix_tables * 1.2)
    ):
        errors.append("minimp3 static tables exceed Helix +20%")
    if errors:
        raise RuntimeError("; ".join(errors))
    print("LEDGER:PASS:all metrics within budgets and 2% regression allowance")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compile-only", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--profile-binary", type=Path)
    args = parser.parse_args()
    try:
        objects = compile_objects()
        if not args.compile_only:
            metrics, tables = print_report(objects)
            if not args.profile_binary:
                raise RuntimeError("full audit requires --profile-binary")
            metrics.update(profile_metrics(args.profile_binary))
            hook_peaks = {
                backend: metrics[(backend, "pipeline-peak")]
                for backend in ("helix", "minimp3-float")
            }
            metrics.update(run_watermark(objects, hook_peaks))
            if args.check:
                check_ledger(metrics, tables)
    except (OSError, RuntimeError, subprocess.CalledProcessError, ValueError) as exc:
        print(f"codec memory audit failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
