"""Contracts for matrix enumeration, attribution, and bounded scheduling."""

from pathlib import Path
from types import SimpleNamespace
from typing import cast

import psutil
import pytest

from ci.tools.ast_matrix import (
    AstJob,
    deduplicate_jobs,
    owned_translation_units,
    summarize,
    worker_count,
)


def test_owned_units_include_split_platform_routers(tmp_path: Path) -> None:
    for relative in [
        "fl/build/platforms+.cpp",
        "platforms/esp/32/rmt.cpp",
        "third_party/vendor.cpp",
    ]:
        path = tmp_path / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("// translation unit\n", encoding="utf-8")
    assert owned_translation_units(tmp_path) == (
        tmp_path / "fl/build/platforms+.cpp",
        tmp_path / "platforms/esp/32/rmt.cpp",
    )


def test_job_dedup_retains_profile_attribution(tmp_path: Path) -> None:
    first = AstJob(
        "idf4", tmp_path / "router.cpp", ("clang-query",), ("-DFL_ESP",), "src/.*"
    )
    second = AstJob(
        "alias", first.source, first.query, first.arguments, first.file_regex
    )
    distinct = AstJob(
        "idf5", first.source, first.query, ("-DFL_OTHER",), first.file_regex
    )
    jobs = deduplicate_jobs([first, second, distinct])
    assert len(jobs) == 2
    assert jobs[0].profiles == ("idf4", "alias")


def test_worker_limit_combines_memory_and_process_caps() -> None:
    assert (
        worker_count(50, max_workers=2, memory_budget_mb=4096, worker_memory_mb=2048)
        == 2
    )
    assert (
        worker_count(50, max_workers=8, memory_budget_mb=4096, worker_memory_mb=3072)
        == 1
    )
    assert (
        worker_count(1, max_workers=2, memory_budget_mb=4096, worker_memory_mb=2048)
        == 1
    )
    with pytest.raises(ValueError, match="memory"):
        worker_count(50, max_workers=2, memory_budget_mb=1024, worker_memory_mb=2048)


def test_report_deduplicates_locations_and_attributes_findings() -> None:
    location: dict[str, object] = {
        "path": "src/platforms/esp/driver.h",
        "line": 4,
        "column": 2,
    }
    hit: dict[str, object] = {
        "path": location["path"],
        "line": 4,
        "line_text": "void f();",
        "signature": "void f();",
    }
    rows: list[dict[str, object]] = [
        {
            "profiles": [name],
            "status": "parsed",
            "cache_hit": cached,
            "coverage": [location],
            "noexcept": [hit],
            "array_params": [],
        }
        for name, cached in [("esp32-idf4", False), ("esp32-idf5", True)]
    ]
    result = summarize(rows, 1.0, 2)
    assert result.success
    assert len(result.esp_noexcept) == 1
    assert result.report["coverage_union"] == [location]
    assert result.report["parse_count"] == 1
    assert result.report["diagnostics"] == [
        {"check": "noexcept", **hit, "profiles": ["esp32-idf4", "esp32-idf5"]}
    ]


def test_report_cannot_claim_success_when_backend_is_absent() -> None:
    row: dict[str, object] = {
        "profiles": ["esp32-idf4"],
        "status": "missing_expectations",
        "missing_expectations": ["fl::SDFileHandle"],
        "cache_hit": True,
    }
    result = summarize([row], 0.1, 1)
    assert not result.success
    assert result.report["coverage_union"] == []
    assert result.report["missing_expectations"] == [row]


def test_report_cannot_claim_success_after_parse_error() -> None:
    row: dict[str, object] = {
        "profiles": ["esp32-idf4"],
        "status": "failed",
        "error": "compiler error",
    }
    result = summarize([row], 0.1, 1)
    assert not result.success
    assert result.report["failures"] == [row]


def test_memory_measurement_sums_live_descendants_and_keeps_peak(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from ci.tools.ast_matrix import ProcessTreeRssSampler

    class Process:
        def __init__(self, pid: int, rss: int) -> None:
            self.pid = pid
            self.rss = rss

        def memory_info(self):
            if self.rss < 0:
                raise psutil.NoSuchProcess(self.pid)
            return SimpleNamespace(rss=self.rss)

        def children(self, recursive: bool):
            assert recursive
            return descendants

    root = Process(1, 100)
    child = Process(2, 200)
    descendants = [child, Process(3, -1)]
    monkeypatch.setattr(psutil, "Process", lambda: root)
    sampler = ProcessTreeRssSampler()
    sampler.sample_once()
    child.rss = 10
    sampler.sample_once()
    report = sampler.report()
    assert report["peak_rss_bytes"] == 300
    assert report["samples"] == 2
    assert report["unavailable_process_observations"] == 2
    assert report["scope"] == "current_process_and_descendants"


def test_memory_sampler_stops_on_exception() -> None:
    from ci.tools.ast_matrix import ProcessTreeRssSampler

    sampler = ProcessTreeRssSampler(interval_seconds=0.1)
    with pytest.raises(ValueError, match="analysis failed"):
        with sampler:
            assert sampler.running
            raise ValueError("analysis failed")
    assert not sampler.running
    assert int(str(sampler.report()["samples"])) >= 2


def test_matrix_report_includes_memory_and_elapsed_measurements(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from ci.tools import ast_matrix

    monkeypatch.setattr(ast_matrix, "build_jobs", lambda *_: [])
    result = ast_matrix.run_matrix(report_path=tmp_path / "report.json")
    assert float(str(result.report["elapsed_seconds"])) >= 0
    measurement = cast(dict[str, object], result.report["memory_measurement"])
    assert int(str(measurement["peak_rss_bytes"])) > 0
    assert int(str(measurement["samples"])) >= 2
    assert result.report["job_count"] == 0
    assert result.report["parse_count"] == 0
    assert result.report["cache_hits"] == 0
    assert result.report["coverage_union"] == []
