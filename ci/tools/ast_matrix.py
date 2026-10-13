"""Run the required stub and provisioned ESP semantic matrix without builds."""

import argparse
import json
import os
import shutil
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import cast

import psutil

from ci.lint_cpp.ast_cache import AstInputSnapshot, AstUnitResult, cached_ast_unit
from ci.tools.ast_profiles import (
    ESP_PROFILES,
    ProfileEnvironment,
    load_host_profile,
    load_profile,
    target_process_environment,
)
from ci.tools.check_array_params import (
    ArrayParamFinding,
    ArrayParamHit,
    diff_against_baseline,
    load_baseline,
)
from ci.tools.check_ast_combined import analyze_translation_unit
from ci.tools.check_noexcept import NoexceptHit, _scope_tus
from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.paths import PROJECT_ROOT


@dataclass(frozen=True)
class AstJob:
    profile: str
    source: Path
    query: tuple[str, ...]
    arguments: tuple[str, ...]
    file_regex: str
    expected_records: tuple[str, ...] = ()
    identity_files: tuple[Path, ...] = ()
    include_roots: tuple[Path, ...] = ()
    metadata: dict[str, object] = field(default_factory=dict)
    profiles: tuple[str, ...] = ()
    dependency_roots: tuple[Path, ...] = ()


@dataclass
class MatrixResult:
    stub_noexcept: list[NoexceptHit]
    stub_array_params: list[ArrayParamHit]
    esp_noexcept: list[NoexceptHit]
    esp_array_params: list[ArrayParamHit]
    report: dict[str, object]
    success: bool


class ProcessTreeRssSampler:
    """Sample aggregate resident memory without scanning unrelated processes.

    RSS counts shared pages per process. Short-lived children may escape the
    sampling interval, and independent daemons are outside this process tree.
    This is a measurement, not an enforced memory budget.
    """

    def __init__(self, interval_seconds: float = 0.1) -> None:
        if interval_seconds <= 0:
            raise ValueError("RSS sampling interval must be positive")
        self.interval_seconds = interval_seconds
        self._process = psutil.Process()
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self._peak_rss_bytes = 0
        self._samples = 0
        self._unavailable = 0

    @property
    def running(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    def sample_once(self) -> None:
        try:
            descendants = self._process.children(recursive=True)
        except psutil.Error:
            descendants = []
            self._unavailable += 1
        rss = 0
        seen: set[int] = set()
        for process in (self._process, *descendants):
            if process.pid in seen:
                continue
            seen.add(process.pid)
            try:
                rss += process.memory_info().rss
            except psutil.Error:
                self._unavailable += 1
        self._peak_rss_bytes = max(self._peak_rss_bytes, rss)
        self._samples += 1

    def _poll(self) -> None:
        while not self._stop.wait(self.interval_seconds):
            self.sample_once()

    def start(self) -> None:
        if self.running:
            raise RuntimeError("RSS sampler already running")
        self._stop.clear()
        self.sample_once()
        self._thread = threading.Thread(
            target=self._poll, name="ast-rss-sampler", daemon=True
        )
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join()
            self._thread = None
        self.sample_once()

    def __enter__(self) -> "ProcessTreeRssSampler":
        self.start()
        return self

    def __exit__(self, *_exception: object) -> None:
        self.stop()

    def report(self) -> dict[str, object]:
        return {
            "scope": "current_process_and_descendants",
            "peak_rss_bytes": self._peak_rss_bytes,
            "samples": self._samples,
            "interval_seconds": self.interval_seconds,
            "unavailable_process_observations": self._unavailable,
            "note": "Sampled aggregate RSS; shared pages may be counted per process, "
            "short-lived processes may be missed, independent daemons excluded",
        }


def owned_translation_units(source_root: Path) -> tuple[Path, ...]:
    """Include separately built implementation units as well as unity routers."""
    return tuple(
        sorted(
            path
            for path in source_root.rglob("*.cpp")
            if "third_party" not in path.relative_to(source_root).parts
        )
    )


def target_clang_query() -> list[str]:
    """Use the provisioned target parser without host include injection."""
    from ci.tools.ast_profiles import load_target_query

    return [str(load_target_query().executable)]


def worker_count(
    job_count: int,
    *,
    max_workers: int = 2,
    memory_budget_mb: int = 4096,
    worker_memory_mb: int = 2048,
) -> int:
    """Bound concurrent parses using a configurable per-worker memory estimate."""
    if max_workers < 1 or worker_memory_mb < 1 or memory_budget_mb < worker_memory_mb:
        raise ValueError("Invalid worker or memory budget for AST matrix")
    return max(1, min(job_count, max_workers, memory_budget_mb // worker_memory_mb))


def deduplicate_jobs(jobs: list[AstJob]) -> list[AstJob]:
    unique: dict[str, AstJob] = {}
    for job in jobs:
        key = json.dumps(
            [
                str(job.source.resolve()),
                job.query,
                job.arguments,
                job.file_regex,
                job.expected_records,
                [str(path) for path in job.identity_files],
                [str(path) for path in job.include_roots],
                [str(path) for path in job.dependency_roots],
                job.metadata,
            ],
            sort_keys=True,
        )
        previous = unique.get(key)
        profiles = (*previous.profiles, job.profile) if previous else (job.profile,)
        unique[key] = AstJob(
            job.profile,
            job.source,
            job.query,
            job.arguments,
            job.file_regex,
            job.expected_records,
            job.identity_files,
            job.include_roots,
            job.metadata,
            tuple(dict.fromkeys(profiles)),
            job.dependency_roots,
        )
    return list(unique.values())


def _include_roots(arguments: tuple[str, ...]) -> tuple[Path, ...]:
    roots: list[Path] = []
    for index, argument in enumerate(arguments):
        if argument in ("-I", "-isystem", "-iquote", "-idirafter"):
            roots.append(Path(arguments[index + 1]).resolve())
        elif argument.startswith("-I") and len(argument) > 2:
            roots.append(Path(argument[2:]).resolve())
    return tuple(roots)


def esp_jobs(environment: ProfileEnvironment) -> list[AstJob]:
    from ci.tools.ast_profiles import load_target_query

    target_query = load_target_query()
    query = (str(target_query.executable),)
    jobs: list[AstJob] = []
    for source in owned_translation_units(PROJECT_ROOT / "src"):
        is_sd = source.name == "fl.fs.sd+.cpp"
        for backend in ("sd", "sdfat") if is_sd else ("sd",):
            expected = (
                (
                    ("fl::SdFatFileHandle",)
                    if backend == "sdfat"
                    else ("fl::SDFileHandle",)
                )
                if is_sd
                else ()
            )
            if source.name == "platforms+.cpp":
                expected = ("fl::SPISingleESP32",)
            jobs.append(
                AstJob(
                    environment.profile.name + ("-sdfat" if backend == "sdfat" else ""),
                    source,
                    query,
                    environment.backend_arguments(backend),
                    r".*src.*",
                    expected,
                    (
                        environment.compiler,
                        target_query.identity_file,
                        *environment.identity_files,
                    ),
                    environment.include_paths,
                    {
                        "profile": asdict(environment.profile),
                        "compiler": str(environment.compiler),
                        "compiler_version": environment.compiler_version,
                        "query_version": target_query.version,
                        "backend": backend,
                    },
                    dependency_roots=environment.dependency_roots,
                )
            )
    return jobs


def build_jobs(
    scope: str = "all", profile_names: tuple[str, ...] | None = None
) -> list[AstJob]:
    host = load_host_profile()
    query = host.query
    arguments = host.arguments
    jobs = [
        AstJob(
            "stub",
            PROJECT_ROOT / source,
            query,
            arguments,
            regex,
            identity_files=host.identity_files,
            include_roots=tuple(
                dict.fromkeys((*_include_roots(arguments), *host.include_paths))
            ),
            metadata={"query_version": host.version, "target": "host"},
        )
        for source, regex in _scope_tus(scope)
    ]
    known = {profile.name for profile in ESP_PROFILES}
    if profile_names is not None and set(profile_names) - known:
        raise ValueError(f"Unknown ESP profiles: {set(profile_names) - known}")
    for profile in ESP_PROFILES:
        if profile_names is None or profile.name in profile_names:
            jobs.extend(esp_jobs(load_profile(profile)))
    return deduplicate_jobs(jobs)


def _query_files(query: tuple[str, ...]) -> tuple[Path, ...]:
    files: list[Path] = []
    for argument in query:
        candidate = Path(shutil.which(argument) or argument)
        if candidate.is_file():
            executable = candidate.resolve()
            files.append(executable)
            unwrapped = executable.with_name(executable.name + "-unwrapped")
            if unwrapped.is_file():
                files.append(unwrapped)
    return tuple(files)


def _run_job(job: AstJob, snapshot: AstInputSnapshot) -> dict[str, object]:
    start = time.monotonic()
    parse_count = 0
    process_env = None if job.profile == "stub" else target_process_environment()
    include_environment = os.environ if process_env is None else process_env
    tools = tuple(
        PROJECT_ROOT / "ci" / "tools" / name
        for name in (
            "ast_matrix.py",
            "ast_profiles.py",
            "check_ast_combined.py",
            "check_noexcept.py",
            "check_array_params.py",
        )
    )
    files = (
        job.source,
        *job.identity_files,
        *_query_files(job.query),
        *tools,
        PROJECT_ROOT / "ci/lint_cpp/ast_cache.py",
    )
    row: dict[str, object] = {
        "profiles": job.profiles or (job.profile,),
        "source": str(job.source),
        "arguments": job.arguments,
        "query": job.query,
        "metadata": job.metadata,
        "expected_records": job.expected_records,
        "identity_files": [str(path) for path in job.identity_files],
        "include_roots": [str(path) for path in job.include_roots],
        "dependency_roots": [str(path) for path in job.dependency_roots],
    }

    def parse() -> AstUnitResult:
        nonlocal parse_count
        parse_count += 1
        analysis = analyze_translation_unit(
            list(job.query),
            str(job.source),
            job.file_regex,
            list(job.arguments),
            expected_records=job.expected_records,
            process_env=process_env,
        )
        payload: dict[str, object] = {
            "noexcept": [asdict(hit) for hit in analysis.noexcept_hits],
            "array_params": [asdict(hit) for hit in analysis.array_param_hits],
            "coverage": [asdict(location) for location in analysis.coverage],
            "present_records": analysis.present_records,
        }
        return AstUnitResult(payload, analysis.dependencies)

    try:
        cached = cached_ast_unit(
            "combined",
            identity={
                "arguments": job.arguments,
                "query": job.query,
                "regex": job.file_regex,
                "expected": job.expected_records,
                "metadata": job.metadata,
                "environment": {
                    name: include_environment.get(name, "")
                    for name in (
                        "CPATH",
                        "CPLUS_INCLUDE_PATH",
                        "C_INCLUDE_PATH",
                        "NIX_CFLAGS_COMPILE",
                        "SDKROOT",
                        "INCLUDE",
                    )
                },
            },
            input_files=files,
            include_roots=(
                *job.include_roots,
                *job.dependency_roots,
                job.source.parent,
            ),
            runner=parse,
            snapshot=snapshot,
        )
        row.update(cached.payload)
        missing = sorted(
            set(job.expected_records)
            - set(cast(list[str], cached.payload.get("present_records", [])))
        )
        row.update(
            status="missing_expectations" if missing else "parsed",
            cache_hit=cached.cache_hit,
            missing_expectations=missing,
        )
    except KeyboardInterrupt as interrupt:
        handle_keyboard_interrupt(interrupt)
        raise
    except Exception as error:
        row.update(status="failed", error=str(error), cache_hit=False)
        if "expected backend definitions missing:" in str(error):
            row.update(
                status="missing_expectations",
                missing_expectations=list(job.expected_records),
            )
    row["elapsed_seconds"] = time.monotonic() - start
    row["parse_count"] = parse_count
    return row


def summarize(
    rows: list[dict[str, object]], elapsed: float, workers: int
) -> MatrixResult:
    locations: set[tuple[str, int, int]] = set()
    profiles: dict[str, set[tuple[str, int, int]]] = {}
    diagnostics: dict[tuple[str, str, int, str], dict[str, object]] = {}
    stub_noexcept: dict[tuple[str, int], NoexceptHit] = {}
    esp_noexcept: dict[tuple[str, int], NoexceptHit] = {}
    stub_array: dict[tuple[str, int], ArrayParamHit] = {}
    esp_array: dict[tuple[str, int], ArrayParamHit] = {}
    for row in rows:
        names = cast(list[str], row["profiles"])
        if row["status"] != "parsed":
            continue
        for location in cast(list[dict[str, object]], row["coverage"]):
            key = (
                str(location["path"]),
                int(str(location["line"])),
                int(str(location["column"])),
            )
            locations.add(key)
            for name in names:
                profiles.setdefault(name, set()).add(key)
        for check in ("noexcept", "array_params"):
            for entry in cast(list[dict[str, object]], row[check]):
                path, line = str(entry["path"]), int(str(entry["line"]))
                line_text, signature = str(entry["line_text"]), str(entry["signature"])
                if check == "array_params":
                    findings = tuple(
                        ArrayParamFinding(
                            str(finding["name"]),
                            str(finding["spelling"]),
                            bool(finding["is_iram"]),
                        )
                        for finding in cast(list[dict[str, object]], entry["findings"])
                    )
                    array_hit = ArrayParamHit(
                        path, line, line_text, signature, findings
                    )
                    for name in names:
                        (stub_array if name == "stub" else esp_array)[(path, line)] = (
                            array_hit
                        )
                else:
                    noexcept_hit = NoexceptHit(path, line, line_text, signature)
                    for name in names:
                        (stub_noexcept if name == "stub" else esp_noexcept)[
                            (path, line)
                        ] = noexcept_hit
                diagnostic_key = (check, path, line, signature)
                diagnostic = diagnostics.setdefault(
                    diagnostic_key, {"check": check, **entry, "profiles": []}
                )
                attribution = cast(list[str], diagnostic["profiles"])
                attribution.extend(name for name in names if name not in attribution)
    failures = [row for row in rows if row["status"] != "parsed"]
    hits = sum(row.get("cache_hit") is True for row in rows)
    report: dict[str, object] = {
        "schema": 1,
        "elapsed_seconds": elapsed,
        "workers": workers,
        "jobs": rows,
        "job_count": len(rows),
        "parse_count": sum(
            int(str(row.get("parse_count", 0 if row.get("cache_hit") else 1)))
            for row in rows
        ),
        "cache_hits": hits,
        "cache_hit_rate": hits / len(rows) if rows else 0,
        "failures": failures,
        "diagnostics": list(diagnostics.values()),
        "missing_expectations": [
            row for row in rows if row.get("missing_expectations")
        ],
        "coverage_union": [
            {"path": path, "line": line, "column": column}
            for path, line, column in sorted(locations)
        ],
        "profile_coverage": {
            name: len(locations) for name, locations in sorted(profiles.items())
        },
        "known_uncovered": [
            "Platforms and feature configurations outside the listed profiles",
            "Inactive conditional branches within each listed configuration",
        ],
    }
    return MatrixResult(
        list(stub_noexcept.values()),
        list(stub_array.values()),
        list(esp_noexcept.values()),
        list(esp_array.values()),
        report,
        not failures,
    )


def run_matrix(
    scope: str = "all",
    *,
    profile_names: tuple[str, ...] | None = None,
    report_path: Path | None = None,
    max_workers: int = 2,
    memory_budget_mb: int = 4096,
    worker_memory_mb: int = 2048,
) -> MatrixResult:
    start = time.monotonic()
    jobs: list[AstJob] = []
    rows: list[dict[str, object]] = []
    workers = 0
    snapshot_verified = False
    memory = ProcessTreeRssSampler()
    memory.start()
    try:
        jobs = build_jobs(scope, profile_names)
        workers = worker_count(
            len(jobs),
            max_workers=max_workers,
            memory_budget_mb=memory_budget_mb,
            worker_memory_mb=worker_memory_mb,
        )
        snapshot = AstInputSnapshot()
        with ThreadPoolExecutor(max_workers=workers) as pool:
            futures = {
                pool.submit(_run_job, job, snapshot): index
                for index, job in enumerate(jobs)
            }
            completed: dict[int, dict[str, object]] = {}
            for future in as_completed(futures):
                index = futures[future]
                row = future.result()
                completed[index] = row
                job = jobs[index]
                source = (
                    job.source.relative_to(PROJECT_ROOT)
                    if job.source.is_relative_to(PROJECT_ROOT)
                    else job.source
                )
                cache_status = "cache-hit" if row.get("cache_hit") else "cache-miss"
                print(
                    f"AST [{len(completed)}/{len(jobs)}] "
                    f"{','.join(job.profiles or (job.profile,))} {source}: "
                    f"{row['status']} {cache_status} {float(str(row['elapsed_seconds'])):.2f}s",
                    flush=True,
                )
                if row.get("error") or row.get("missing_expectations"):
                    print(
                        f"  {row.get('error', row.get('missing_expectations'))}",
                        flush=True,
                    )
            rows = [completed[index] for index in range(len(jobs))]
        try:
            snapshot.verify_final()
            snapshot_verified = True
        except ValueError as error:
            rows.append(
                {
                    "profiles": ["required-matrix"],
                    "status": "final_snapshot_failure",
                    "error": str(error),
                    "parse_count": 0,
                    "cache_hit": False,
                }
            )
            print(f"AST final input audit failed: {error}", flush=True)
    except KeyboardInterrupt as interrupt:
        memory.stop()
        handle_keyboard_interrupt(interrupt)
        raise
    except Exception as error:
        rows.append(
            {
                "profiles": ["required-matrix"],
                "status": "prerequisite_failure",
                "error": str(error),
                "parse_count": 0,
                "cache_hit": False,
            }
        )
    try:
        result = summarize(rows, time.monotonic() - start, workers)
    finally:
        memory.stop()
    result.report["memory_measurement"] = memory.report()
    result.report["snapshot_verified"] = snapshot_verified
    result.report["requested_profiles"] = [
        "stub",
        *(
            profile_names
            if profile_names is not None
            else (profile.name for profile in ESP_PROFILES)
        ),
    ]
    result.report["memory_scheduling"] = {
        "budget_mb": memory_budget_mb,
        "estimated_worker_mb": worker_memory_mb,
        "note": "Scheduling estimate, not a measured or enforced memory ceiling",
    }
    destination = report_path or PROJECT_ROOT / ".build/ast-coverage.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("w", encoding="utf-8") as output:
        json.dump(result.report, output, indent=2)
        output.write("\n")
    print(
        f"AST: {len(jobs)} jobs, {result.report['cache_hits']} cached, "
        f"{len(cast(list[object], result.report['coverage_union']))} declarations, "
        f"{len(cast(list[object], result.report['failures']))} failures; "
        f"{float(str(result.report['elapsed_seconds'])):.2f}s analysis, "
        f"{int(str(memory.report()['peak_rss_bytes'])) / (1024 * 1024):.1f} MiB sampled peak RSS; "
        f"{destination}"
    )
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--profile",
        action="append",
        choices=[profile.name for profile in ESP_PROFILES],
        help="Select ESP profile for investigation; default is the complete required matrix",
    )
    parser.add_argument(
        "--scope",
        default="all",
        help="Existing stub scope; ESP always enumerates all owned units",
    )
    parser.add_argument("--report", type=Path)
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--memory-budget-mb", type=int, default=4096)
    parser.add_argument("--worker-memory-mb", type=int, default=2048)
    args = parser.parse_args()
    result = run_matrix(
        args.scope,
        profile_names=tuple(args.profile) if args.profile else None,
        report_path=args.report,
        max_workers=args.workers,
        memory_budget_mb=args.memory_budget_mb,
        worker_memory_mb=args.worker_memory_mb,
    )
    if not result.success:
        for failure in cast(list[dict[str, object]], result.report["failures"]):
            print(
                f"ERROR [{failure['profiles']}]: {failure.get('error', failure.get('missing_expectations'))}"
            )
        return 1
    stub_arrays, _ = diff_against_baseline(result.stub_array_params, load_baseline())
    esp_arrays, _ = diff_against_baseline(result.esp_array_params, load_baseline())
    actionable = {
        ("noexcept", hit.path, hit.line)
        for hit in (*result.stub_noexcept, *result.esp_noexcept)
    }
    actionable.update(
        ("array_params", hit.path, hit.line) for hit in (*stub_arrays, *esp_arrays)
    )
    for diagnostic in cast(list[dict[str, object]], result.report["diagnostics"]):
        if (diagnostic["check"], diagnostic["path"], diagnostic["line"]) in actionable:
            print(
                f"{diagnostic['path']}:{diagnostic['line']}: {diagnostic['check']} "
                f"[{', '.join(cast(list[str], diagnostic['profiles']))}]: {diagnostic['signature']}"
            )
    return int(bool(actionable))


if __name__ == "__main__":
    raise SystemExit(main())
