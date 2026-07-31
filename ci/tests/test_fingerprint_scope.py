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
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from ci.util.fingerprint import FingerprintManager
from ci.util.test_types import FingerprintResult


def _result(**kw: object) -> FingerprintResult:
    base: dict[str, object] = {
        "hash": "abc123",
        "num_tests_run": 357,
        "num_tests_passed": 357,
        "duration_seconds": 55.2,
        "test_name": "cpp_unit_tests",
    }
    base.update(kw)
    return FingerprintResult(**base)  # type: ignore[arg-type]


class TestCacheSummaryProvenance(unittest.TestCase):
    def test_full_run_reads_as_a_clean_pass(self) -> None:
        summary = _result(scope="full").get_cache_summary()
        self.assertIn("357/357 passed", summary)
        self.assertNotIn("filtered", summary)
        self.assertNotIn("unrecorded", summary)

    def test_partial_run_is_marked_and_cannot_read_as_full(self) -> None:
        """The #3763 failure: `1/1 passed` looked like a full green suite."""
        summary = _result(
            num_tests_run=1, num_tests_passed=1, duration_seconds=16.98, scope="partial"
        ).get_cache_summary()
        self.assertIn("1/1 passed", summary)
        self.assertIn("NOT the full suite", summary)

    def test_missing_scope_is_reported_as_unverified(self) -> None:
        """A fingerprint written before this field existed must not claim full."""
        summary = _result(scope=None).get_cache_summary()
        self.assertIn("357/357 passed", summary)
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

    def test_scope_travels_with_the_counts_on_a_cache_hit(self) -> None:
        """save_all() carries prior counts forward when nothing ran — the scope
        that labels those counts must come with them, or a filtered result gets
        silently relabelled as a full pass on the very next run."""
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            mgr._prev_fingerprints["cpp_test"] = _result(
                num_tests_run=1, num_tests_passed=1, scope="partial"
            )
            # Current run produced no counts (tests were skipped).
            mgr._fingerprints["cpp_test"] = FingerprintResult(hash="abc123")

            mgr.save_all("success")

            reloaded = mgr.read("cpp_test")
            assert reloaded is not None
            self.assertEqual(reloaded.num_tests_run, 1)
            self.assertEqual(
                reloaded.scope,
                "partial",
                "counts carried forward without their scope would read as a full pass",
            )


if __name__ == "__main__":
    unittest.main()
