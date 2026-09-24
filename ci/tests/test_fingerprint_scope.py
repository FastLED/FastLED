"""Tests for test-result scope provenance in the fingerprint cache (issue #3763).

A cached test result is replayed verbatim on later runs:

    ✓ Fingerprint cache valid - skipping C++ unit tests [357/357 passed in 55.20s]

Nothing in that line said whether the number came from a full-suite run or from
a filtered one, so a partial result was indistinguishable from a full pass and
was reported as authoritative until something invalidated the fingerprint. The
reporter of #3763 acted on a `1/1 passed` line once, believing it was evidence
of suite health.

The fix records the run's *scope* alongside the counts and renders it. These
tests pin that the annotation exists, survives a round trip, and — the part
that actually matters — that a partial result can never render as a clean full
pass.
"""

from __future__ import annotations

import json
import os
import sys
import time
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

import pytest

from ci.early_exit_cache import (
    FINGERPRINT_VALIDATION_VERSION,
    argv_ultra_early_exit,
    fingerprint_aux_hash,
)
from ci.util.fingerprint import FingerprintManager
from ci.util.test_types import FingerprintResult


def _result(**kw: object) -> FingerprintResult:
    """A fully-attributed cpp_test fingerprint: 274 unit + 83 examples = 357."""
    base: dict[str, object] = {
        "hash": "abc123",
        "num_tests_run": 357,
        "num_tests_passed": 357,
        "duration_seconds": 55.2,
        "test_name": "cpp_unit_tests",
        "num_examples_run": 83,
        "num_examples_passed": 83,
        "examples_included": True,
    }
    base.update(kw)
    return FingerprintResult(**base)  # type: ignore[arg-type]


class TestCacheSummaryProvenance(unittest.TestCase):
    def test_full_run_reads_as_a_clean_pass(self) -> None:
        summary = _result(scope="full").get_cache_summary()
        self.assertIn("274/274 unit", summary)
        self.assertIn("83/83 examples", summary)
        self.assertNotIn("filtered", summary)
        self.assertNotIn("unrecorded", summary)

    def test_partial_run_is_marked_and_cannot_read_as_full(self) -> None:
        """The #3763 failure: `1/1 passed` looked like a full green suite."""
        summary = _result(
            num_tests_run=1,
            num_tests_passed=1,
            num_examples_run=0,
            num_examples_passed=0,
            duration_seconds=16.98,
            scope="partial",
        ).get_cache_summary()
        self.assertIn("1/1 unit passed", summary)
        self.assertIn("NOT the full suite", summary)

    def test_missing_scope_is_reported_as_unverified(self) -> None:
        """A fingerprint written before this field existed must not claim full."""
        summary = _result(scope=None).get_cache_summary()
        self.assertIn("274/274 unit", summary)
        self.assertIn("scope unrecorded", summary)
        self.assertNotIn("NOT the full suite", summary)

    def test_no_counts_still_yields_empty_summary(self) -> None:
        self.assertEqual(
            FingerprintResult(hash="abc").get_cache_summary(),
            "",
        )


class TestScopePersistence(unittest.TestCase):
    def test_scope_round_trips_through_disk(self) -> None:
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            mgr.write("cpp_test", _result(scope="partial"))

            reloaded = mgr.read("cpp_test")
            self.assertIsNotNone(reloaded)
            assert reloaded is not None
            self.assertEqual(reloaded.scope, "partial")
            self.assertIn("NOT the full suite", reloaded.get_cache_summary())

    def test_scope_is_written_into_the_json(self) -> None:
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            mgr.write("cpp_test", _result(scope="full"))
            path = Path(tmp) / "fingerprint" / "cpp_test_quick.json"
            self.assertEqual(json.loads(path.read_text())["scope"], "full")

    def test_legacy_fingerprint_without_scope_loads_as_none(self) -> None:
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            path = Path(tmp) / "fingerprint" / "cpp_test_quick.json"
            path.write_text(
                json.dumps(
                    {
                        "hash": "abc123",
                        "status": "success",
                        "num_tests_run": 274,
                        "num_tests_passed": 274,
                    }
                )
            )
            reloaded = mgr.read("cpp_test")
            assert reloaded is not None
            self.assertIsNone(reloaded.scope)
            self.assertIn("scope unrecorded", reloaded.get_cache_summary())


class TestScopeCarriedForward(unittest.TestCase):
    def test_update_test_metadata_records_scope(self) -> None:
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            mgr._fingerprints["cpp_test"] = FingerprintResult(hash="abc123")
            mgr.update_test_metadata(
                "cpp_test",
                num_tests_run=1,
                num_tests_passed=1,
                duration_seconds=0.1,
                test_name="cpp_unit_tests",
                scope="partial",
            )
            self.assertEqual(mgr._fingerprints["cpp_test"].scope, "partial")

    def test_cache_hit_keeps_prior_counts_and_scope_untouched(self) -> None:
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            prior = _result(
                num_tests_run=1,
                num_tests_passed=1,
                num_examples_run=0,
                num_examples_passed=0,
                scope="partial",
                status="success",
            )
            mgr.write("cpp_test", prior)
            path = Path(tmp) / "fingerprint" / "cpp_test_quick.json"
            before = path.read_bytes()
            self.assertFalse(
                mgr.check("cpp_test", lambda: FingerprintResult(hash="abc123"))
            )
            mgr.save_success("cpp_test")

            reloaded = mgr.read("cpp_test")
            assert reloaded is not None
            self.assertEqual(reloaded.num_tests_run, 1)
            self.assertEqual(reloaded.scope, "partial")
            self.assertEqual(path.read_bytes(), before)

    def test_python_success_does_not_certify_changed_cpp(self) -> None:
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            mgr.write("cpp_test", FingerprintResult(hash="old-cpp", status="success"))
            self.assertTrue(
                mgr.check("cpp_test", lambda: FingerprintResult(hash="new-cpp"))
            )
            self.assertTrue(
                mgr.check("python_test", lambda: FingerprintResult(hash="new-py"))
            )
            mgr.save_success("python_test")

            next_run = FingerprintManager(Path(tmp))
            self.assertTrue(
                next_run.check("cpp_test", lambda: FingerprintResult(hash="new-cpp"))
            )
            self.assertFalse(
                next_run.check("python_test", lambda: FingerprintResult(hash="new-py"))
            )

    def test_cpp_success_does_not_certify_changed_python(self) -> None:
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            mgr.write("python_test", FingerprintResult(hash="old-py", status="success"))
            self.assertTrue(
                mgr.check("python_test", lambda: FingerprintResult(hash="new-py"))
            )
            self.assertTrue(
                mgr.check("cpp_test", lambda: FingerprintResult(hash="new-cpp"))
            )
            mgr.save_success("cpp_test")

            next_run = FingerprintManager(Path(tmp))
            self.assertTrue(
                next_run.check("python_test", lambda: FingerprintResult(hash="new-py"))
            )
            self.assertFalse(
                next_run.check("cpp_test", lambda: FingerprintResult(hash="new-cpp"))
            )

    def test_legacy_success_requires_revalidation(self) -> None:
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            path = Path(tmp) / "fingerprint" / "cpp_test_quick.json"
            path.write_text(json.dumps({"hash": "same", "status": "success"}))
            self.assertTrue(
                mgr.check("cpp_test", lambda: FingerprintResult(hash="same"))
            )

    def test_python_config_change_invalidates_cache(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "ci" / "tests").mkdir(parents=True)
            config = root / "pyproject.toml"
            config.write_text("[project]\nname = 'before'\n")
            previous = Path.cwd()
            try:
                os.chdir(root)
                mgr = FingerprintManager(root / ".cache")
                self.assertTrue(mgr.check_python())
                mgr.save_success("python_test")
                self.assertFalse(FingerprintManager(root / ".cache").check_python())
                config.write_text("[project]\nname = 'after'\n")
                self.assertTrue(FingerprintManager(root / ".cache").check_python())
            finally:
                os.chdir(previous)

    def test_failed_or_unexecuted_scope_is_not_rewritten(self) -> None:
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            mgr.write("examples", FingerprintResult(hash="old", status="success"))
            mgr.write("python_test", FingerprintResult(hash="same", status="success"))
            path = Path(tmp) / "fingerprint" / "examples_quick.json"
            before = path.read_bytes()
            self.assertFalse(
                mgr.check("python_test", lambda: FingerprintResult(hash="same"))
            )
            self.assertTrue(
                mgr.check("examples", lambda: FingerprintResult(hash="new"))
            )
            mgr.mark_failure("python_test")
            self.assertEqual(path.read_bytes(), before)
            next_run = FingerprintManager(Path(tmp))
            self.assertTrue(
                next_run.check("examples", lambda: FingerprintResult(hash="new"))
            )
            self.assertTrue(
                next_run.check("python_test", lambda: FingerprintResult(hash="same"))
            )


def test_default_early_exit_requires_validated_wasm(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.chdir(tmp_path)
    monkeypatch.setattr(sys, "argv", ["test.py"])
    monkeypatch.delenv("PYTEST_ADDOPTS", raising=False)
    for dirname in ("src", "tests", "examples", "ci", "examples/wasm"):
        (tmp_path / dirname).mkdir(parents=True, exist_ok=True)
    cache = tmp_path / ".cache" / "fingerprint"
    cache.mkdir(parents=True)
    baseline = time.time()
    record = {
        "hash": "ok",
        "status": "success",
        "validation_version": FINGERPRINT_VALIDATION_VERSION,
        "source_max_mtime": baseline,
    }
    for scope, name in (
        ("cpp_test", "cpp_test_quick"),
        ("examples", "examples_quick"),
        ("python_test", "python_test"),
    ):
        (cache / f"{name}.json").write_text(
            json.dumps({**record, "aux_hash": fingerprint_aux_hash(scope)})
        )

    argv_ultra_early_exit(0)
    (cache / "wasm.json").write_text(
        json.dumps({**record, "aux_hash": fingerprint_aux_hash("wasm")})
    )
    with pytest.raises(SystemExit) as hit:
        argv_ultra_early_exit(0)
    assert hit.value.code == 0

    wasm_source = tmp_path / "examples" / "wasm" / "changed.js"
    wasm_source.write_text("changed")
    os.utime(wasm_source, (baseline + 2, baseline + 2))
    argv_ultra_early_exit(0)


if __name__ == "__main__":
    unittest.main()
