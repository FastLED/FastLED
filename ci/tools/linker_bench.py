"""Replay an existing Linux Meson unit build's shared-object links safely.

Example: uv run python ci/tools/linker_bench.py --build-dir .build/meson-debug-thin \
    --linker lld=/usr/bin/ld.lld --linker mold=/usr/bin/mold --output /tmp/links.json
"""

import argparse
import hashlib
import json
import os
import re
import shlex
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from statistics import median
from typing import cast

from running_process import RunningProcess


CORE = "ci/meson/native/fastled.so"
TARGET_RE = re.compile(
    r"^build (ci/meson/native/fastled\.so|tests/[^ :]+\.so|examples/example-[^ :]+\.so):",
    re.MULTILINE,
)


@dataclass(frozen=True)
class LinkCommand:
    target: str
    argv: tuple[str, ...]


@dataclass(frozen=True)
class Candidate:
    name: str
    path: Path
    version: str
    sha256: str


def target_major_order(
    targets: list[str], names: list[str], repetitions: int
) -> list[tuple[int, str, list[str]]]:
    """Keep candidates adjacent for each target; rotate first place per block."""
    return [
        (
            repetition,
            target,
            names[offset:] + names[:offset],
        )
        for repetition in range(repetitions)
        for index, target in enumerate(targets)
        for offset in [(repetition * len(targets) + index) % len(names)]
    ]


def sweep_totals(
    samples: list[dict[str, object]], repetitions: int
) -> list[dict[str, float | int]]:
    """Aggregate measured link durations, excluding scheduler gaps."""
    result: list[dict[str, float | int]] = []
    for repetition in range(repetitions):
        selected = [item for item in samples if item["repetition"] == repetition]
        seconds = sum(cast(float, item["seconds"]) for item in selected)
        core = sum(
            cast(float, item["seconds"]) for item in selected if item["target"] == CORE
        )
        modules = sum(
            cast(float, item["seconds"])
            for item in selected
            if str(item["target"]).startswith("tests/")
        )
        examples = sum(
            cast(float, item["seconds"])
            for item in selected
            if str(item["target"]).startswith("examples/")
        )
        result.append(
            {
                "repetition": repetition,
                "seconds": seconds,
                "core_seconds": core,
                "modules_seconds": modules,
                "examples_seconds": examples,
            }
        )
    return result


def scratch_output(root: Path, repetition: int, name: str, target: str) -> Path:
    """Put each candidate's deploy side effects in its own directory."""
    return root / str(repetition) / name / Path(target).name


def select_links(
    links: list[LinkCommand],
    sample: str | None,
    *,
    include_examples: bool = False,
    max_tests: int | None = None,
) -> list[LinkCommand]:
    if max_tests is not None and max_tests < 1:
        raise ValueError("--max-tests must be positive")
    by_target = {link.target: link for link in links}
    if CORE not in by_target:
        raise ValueError(f"Core target {CORE} not found in Ninja graph")
    tests = sorted(
        target
        for target in by_target
        if target.startswith("tests/") and target.endswith(".so")
    )
    if not tests:
        raise ValueError("No test shared objects found in Ninja graph")
    if sample:
        tests = [target for target in tests if sample in target]
        if not tests:
            raise ValueError(f"Test sample {sample!r} not found in Ninja graph")
    if max_tests is not None:
        tests = tests[:max_tests]
    examples: list[str] = []
    if include_examples:
        examples = sorted(
            target
            for target in by_target
            if target.startswith("examples/example-") and target.endswith(".so")
        )
        if not examples:
            raise ValueError("No example shared objects found in Ninja graph")
    return [
        by_target[CORE],
        *(by_target[target] for target in tests),
        *(by_target[target] for target in examples),
    ]


def replay_argv(link: LinkCommand, linker: str, output: Path) -> list[str]:
    argv = list(link.argv)
    if argv and Path(argv[0]).name == "zccache":
        argv.pop(0)
    output_locations = [
        i for i in range(len(argv) - 1) if argv[i : i + 2] == ["-o", link.target]
    ]
    if len(output_locations) != 1 or not any(
        value.startswith("-fuse-ld=") for value in argv
    ):
        raise ValueError(f"{link.target}: expected one output and one linker selection")
    rewritten: list[str] = []
    skip_path = False
    for value in argv:
        if skip_path:
            skip_path = False
            continue
        if value.startswith("-fuse-ld=") or value.startswith("--ld-path="):
            continue
        if value == "--ld-path":
            skip_path = True
            continue
        rewritten.append(value)
    if skip_path:
        raise ValueError(f"{link.target}: --ld-path requires a value")
    output_index = next(
        i
        for i in range(len(rewritten) - 1)
        if rewritten[i : i + 2] == ["-o", link.target]
    )
    rewritten[output_index + 1] = str(output)
    rewritten[output_index:output_index] = ["-fuse-ld=lld", f"--ld-path={linker}"]
    return rewritten


def _run(argv: list[str], cwd: Path, *, env: dict[str, str] | None = None) -> str:
    result = RunningProcess.run(
        argv,
        cwd=str(cwd),
        env=env,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        text=True,
        check=False,
        timeout=120,
    )
    if result.returncode:
        raise RuntimeError(
            f"Command failed ({result.returncode}): {shlex.join(argv)}\n{result.stdout}\n{result.stderr}"
        )
    return f"{result.stdout or ''}\n{result.stderr or ''}"


def discover_links(build_dir: Path) -> list[LinkCommand]:
    graph = (build_dir / "build.ninja").read_text(encoding="utf-8")
    targets = set(TARGET_RE.findall(graph))
    if CORE not in targets or not any(
        target.startswith("tests/") for target in targets
    ):
        raise ValueError(
            "Ninja graph must contain the native core and test shared objects"
        )
    commands = _run(["ninja", "-t", "commands", *sorted(targets)], build_dir)
    found: dict[str, LinkCommand] = {}
    for line in commands.splitlines():
        argv = shlex.split(line)
        for i, value in enumerate(argv[:-1]):
            if value == "-o" and argv[i + 1] in targets:
                target = argv[i + 1]
                if target in found:
                    raise ValueError(f"Duplicate Ninja link command for {target}")
                found[target] = LinkCommand(target, tuple(argv))
    missing = targets - found.keys()
    if missing:
        raise ValueError(f"Missing Ninja commands: {sorted(missing)}")
    return [found[target] for target in sorted(found)]


def _inputs_exist(link: LinkCommand, build_dir: Path) -> None:
    for arg in link.argv:
        if arg.endswith((".o", ".a")) and not (build_dir / arg).is_file():
            raise FileNotFoundError(f"Missing link input for {link.target}: {arg}")
    if not (build_dir / link.target).is_file():
        raise FileNotFoundError(f"Original target is not built: {link.target}")


def _candidate(spec: str) -> Candidate:
    if "=" not in spec:
        raise ValueError(f"Expected NAME=ABS_PATH: {spec}")
    name, raw_path = spec.split("=", 1)
    path = Path(raw_path)
    if (
        not re.fullmatch(r"[A-Za-z0-9_-]+", name)
        or not path.is_absolute()
        or not path.is_file()
        or not os.access(path, os.X_OK)
    ):
        raise ValueError(f"Missing or non-executable linker candidate: {spec}")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    version = _run([str(path), "--version"], Path.cwd()).splitlines()[0]
    return Candidate(name, path, version, digest)


def _elf(path: Path) -> None:
    with path.open("rb") as stream:
        if stream.read(4) != b"\x7fELF":
            raise ValueError(f"Link output is not ELF: {path}")


def classify_reld(log: str) -> str:
    """RELD reports the chosen engine on stderr with RELD_LOG_ENGINE=1."""
    match = re.search(r"reld:\s*engine=(reld|lld)\s*\((native|bridge)\b", log)
    if not match:
        return "unclassified"
    if match.groups() == ("reld", "native"):
        return "native"
    if match.groups() == ("lld", "bridge"):
        return "bridge"
    return "unclassified"


def benchmark(
    build_dir: Path,
    specs: list[str],
    repetitions: int,
    sample: str | None,
    *,
    include_examples: bool = False,
    max_tests: int | None = None,
) -> dict[str, object]:
    if sys.platform != "linux":
        raise RuntimeError("Native link replay requires Linux")
    if repetitions < 1:
        raise ValueError("--repetitions must be positive")
    build_dir = build_dir.resolve(strict=True)
    links = select_links(
        discover_links(build_dir),
        sample,
        include_examples=include_examples,
        max_tests=max_tests,
    )
    for link in links:
        _inputs_exist(link, build_dir)
    candidates = [_candidate(spec) for spec in specs]
    if not candidates or len({item.name for item in candidates}) != len(candidates):
        raise ValueError("Supply at least one uniquely named --linker candidate")
    names = [item.name for item in candidates]
    by_name = {item.name: item for item in candidates}
    samples: dict[str, list[dict[str, object]]] = {name: [] for name in names}
    run_order: list[dict[str, object]] = []
    blocks = target_major_order([link.target for link in links], names, repetitions)
    by_target = {link.target: link for link in links}
    with tempfile.TemporaryDirectory(prefix="fastled-linker-bench-") as scratch:
        scratch_path = Path(scratch)
        if scratch_path.is_relative_to(build_dir):
            raise RuntimeError("Temporary outputs must be outside the build tree")
        for repetition in range(repetitions):
            for name in names:
                (scratch_path / str(repetition) / name).mkdir(parents=True)
        for repetition, target, order in blocks:
            link = by_target[target]
            for name in order:
                candidate = by_name[name]
                output = scratch_output(scratch_path, repetition, name, link.target)
                argv = replay_argv(link, str(candidate.path), output)
                env = dict(os.environ)
                if name.lower() == "reld":
                    env["RELD_LOG_ENGINE"] = "1"
                    if "lld" in by_name:
                        env["RELD_BRIDGE_LINKER"] = str(by_name["lld"].path)
                link_start = time.perf_counter()
                log = _run(argv, build_dir, env=env)
                elapsed = time.perf_counter() - link_start
                _elf(output)
                engine = "unknown"
                if name.lower() == "reld":
                    engine = classify_reld(log)
                sample_record: dict[str, object] = {
                    "repetition": repetition,
                    "target": link.target,
                    "seconds": elapsed,
                    "bytes": output.stat().st_size,
                    "engine": engine,
                    "command": argv,
                    "log": log,
                }
                samples[name].append(sample_record)
                run_order.append(
                    {"repetition": repetition, "target": link.target, "linker": name}
                )
                output.unlink()
    sweeps = [
        {"linker": name, **total}
        for name in names
        for total in sweep_totals(samples[name], repetitions)
    ]
    summary: dict[str, dict[str, object]] = {}
    for name in names:
        values = [cast(float, item["seconds"]) for item in samples[name]]
        totals = [
            cast(float, item["seconds"]) for item in sweeps if item["linker"] == name
        ]
        core_by_sweep = [
            cast(float, item["seconds"])
            for item in samples[name]
            if item["target"] == CORE
        ]
        modules_by_sweep = [
            sum(
                cast(float, item["seconds"])
                for item in samples[name]
                if item["repetition"] == repetition
                and str(item["target"]).startswith("tests/")
            )
            for repetition in range(repetitions)
        ]
        examples_by_sweep = [
            sum(
                cast(float, item["seconds"])
                for item in samples[name]
                if item["repetition"] == repetition
                and str(item["target"]).startswith("examples/")
            )
            for repetition in range(repetitions)
        ]
        candidate = by_name[name]
        summary[name] = {
            "path": str(candidate.path),
            "version": candidate.version,
            "sha256": candidate.sha256,
            "samples": samples[name],
            "link_median_seconds": median(values),
            "link_spread_seconds": max(values) - min(values),
            "sweep_median_seconds": median(totals),
            "sweep_spread_seconds": max(totals) - min(totals),
            "core_median_seconds": median(core_by_sweep),
            "modules_total_median_seconds": median(modules_by_sweep),
            "examples_total_median_seconds": median(examples_by_sweep),
            "engine_counts": {
                engine: sum(item["engine"] == engine for item in samples[name])
                for engine in sorted({str(item["engine"]) for item in samples[name]})
            },
        }
    return {
        "build_dir": str(build_dir),
        "targets": [link.target for link in links],
        "original_commands": {link.target: list(link.argv) for link in links},
        "repetitions": repetitions,
        "include_examples": include_examples,
        "max_tests": max_tests,
        "order": [
            {"repetition": repetition, "target": target, "linkers": order}
            for repetition, target, order in blocks
        ],
        "run_order": run_order,
        "sweep_seconds_kind": "sum_of_link_durations",
        "sweeps": sweeps,
        "linkers": summary,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument(
        "--linker", action="append", required=True, metavar="NAME=ABS_PATH"
    )
    parser.add_argument("--repetitions", type=int, default=3)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sample", help="Substring of test shared-object target name")
    parser.add_argument("--include-examples", action="store_true")
    parser.add_argument("--max-tests", type=int, metavar="N")
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(args.build_dir.resolve()):
        parser.error("--output must be outside the build tree")
    result = benchmark(
        args.build_dir,
        args.linker,
        args.repetitions,
        args.sample,
        include_examples=args.include_examples,
        max_tests=args.max_tests,
    )
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
