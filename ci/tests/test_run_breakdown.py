"""Tests for the unit/example split in test-count reporting (issue #3779).

``bash test --cpp`` runs two populations -- unit tests and example compile
targets -- and both fed one anonymous denominator:

    ✅ All tests passed (357/357 in 48.00s)   <- unit + examples
    ✅ All tests passed (274/274 in 30.49s)   <- unit only, examples cached

The wording was byte-identical, so a reader could not tell "274 unit tests,
examples already cached" from "274 of the 357 things I expected". These tests
pin that each population is named, that a population which did not run is
called out rather than silently dropped from a total, and that the same split
survives into the replayed cache line.
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from ci.early_exit_cache import _cpp_run_breakdown
from ci.meson.streaming import StreamingResult, is_example_artifact
from ci.meson.test_execution import MesonTestResult, examples_are_included
from ci.util.fingerprint import FingerprintManager
from ci.util.test_runner import _describe_cpp_counts
from ci.util.test_types import (
    FingerprintResult,
    RunBreakdown,
    describe_test_counts,
    format_run_breakdown,
)


class TestArtifactClassification(unittest.TestCase):
    """The build directory is the only surviving unit/example marker."""

    def test_example_artifact_is_recognized(self) -> None:
        self.assertTrue(
            is_example_artifact(Path(".build/meson/examples/Blink.dll")),
        )

    def test_unit_test_artifact_is_not_an_example(self) -> None:
        self.assertFalse(
            is_example_artifact(Path(".build/meson/tests/fl_fixed_point_s16x16.dll")),
        )

    def test_a_build_dir_named_examples_does_not_confuse_a_unit_test(self) -> None:
        """Classification keys on the artifact's own parent, not any ancestor."""
        self.assertFalse(
            is_example_artifact(
                Path("/home/examples/fastled/.build/tests/test_fx.dll")
            ),
        )


class TestBreakdownWording(unittest.TestCase):
    def test_both_populations_ran(self) -> None:
        breakdown = format_run_breakdown(
            unit_passed=274,
            unit_run=274,
            examples_passed=83,
            examples_run=83,
            examples_included=True,
        )
        self.assertEqual(breakdown.counts, "274/274 unit, 83/83 examples")
        self.assertEqual(breakdown.notes, "")

    def test_cached_examples_are_named_not_silently_dropped(self) -> None:
        """The #3779 failure: this used to render as a bare `274/274`."""
        breakdown = format_run_breakdown(
            unit_passed=274,
            unit_run=274,
            examples_passed=0,
            examples_run=0,
            examples_included=True,
        )
        self.assertEqual(breakdown.counts, "274/274 unit")
        self.assertEqual(breakdown.notes, "examples cached, not re-run")

    def test_unit_only_mode_says_not_requested_not_cached(self) -> None:
        """`--unit` excludes the examples suite; that is not a cache hit."""
        breakdown = format_run_breakdown(
            unit_passed=274,
            unit_run=274,
            examples_passed=0,
            examples_run=0,
            examples_included=False,
        )
        self.assertEqual(breakdown.notes, "examples not requested")

    def test_cached_units_with_fresh_examples(self) -> None:
        breakdown = format_run_breakdown(
            unit_passed=0,
            unit_run=0,
            examples_passed=83,
            examples_run=83,
            examples_included=True,
        )
        self.assertEqual(breakdown.counts, "83/83 examples")
        self.assertEqual(breakdown.notes, "unit tests cached, not re-run")

    def test_nothing_executed_keeps_the_totals_visible(self) -> None:
        """A zero run must still show numbers -- "nothing executed passed in
        0.5s" was ungrammatical and hid the difference between an empty run
        and a corrupt cache entry."""
        breakdown = format_run_breakdown(
            unit_passed=0,
            unit_run=0,
            examples_passed=0,
            examples_run=0,
            examples_included=True,
        )
        self.assertEqual(breakdown.counts, "0/0")
        self.assertIn("unit tests cached", breakdown.notes)
        self.assertIn("examples cached", breakdown.notes)

    def test_a_filtered_run_never_claims_a_population_was_cached(self) -> None:
        """`bash test <name>` drops non-matching artifacts before submission.
        Reporting them as "cached, not re-run" asserts they were verified up
        to date -- the unearned-coverage claim #3779 is about."""
        breakdown = format_run_breakdown(
            unit_passed=1,
            unit_run=1,
            examples_passed=0,
            examples_run=0,
            examples_included=True,
            filtered=True,
        )
        self.assertEqual(breakdown.counts, "1/1 unit")
        self.assertEqual(breakdown.notes, "examples not matched by filter")
        self.assertNotIn("cached", breakdown.notes)

    def test_failures_keep_the_denominator_honest_per_population(self) -> None:
        breakdown = format_run_breakdown(
            unit_passed=272,
            unit_run=274,
            examples_passed=83,
            examples_run=83,
            examples_included=True,
        )
        self.assertEqual(breakdown.counts, "272/274 unit, 83/83 examples")


class TestStreamingResultAttribution(unittest.TestCase):
    def test_example_counts_are_a_subset_of_the_totals(self) -> None:
        sr = StreamingResult(
            success=True,
            num_passed=357,
            num_failed=0,
            num_passed_examples=83,
            num_failed_examples=0,
        )
        self.assertEqual(sr.num_passed - sr.num_passed_examples, 274)

    def test_defaults_attribute_everything_to_unit_tests(self) -> None:
        sr = StreamingResult(success=True, num_passed=274)
        self.assertEqual(sr.num_passed_examples, 0)
        self.assertEqual(sr.num_failed_examples, 0)


class TestCacheSummaryCarriesTheSplit(unittest.TestCase):
    def test_replayed_line_names_both_populations(self) -> None:
        summary = FingerprintResult(
            hash="abc123",
            num_tests_run=357,
            num_tests_passed=357,
            duration_seconds=48.0,
            scope="full",
            num_examples_run=83,
            num_examples_passed=83,
            examples_included=True,
        ).get_cache_summary()
        self.assertIn("274/274 unit, 83/83 examples", summary)

    def test_replayed_unit_only_run_cannot_read_as_a_full_pass(self) -> None:
        summary = FingerprintResult(
            hash="abc123",
            num_tests_run=274,
            num_tests_passed=274,
            duration_seconds=30.49,
            scope="full",
            num_examples_run=0,
            num_examples_passed=0,
            examples_included=True,
        ).get_cache_summary()
        self.assertIn("274/274 unit", summary)
        self.assertIn("examples cached, not re-run", summary)

    def test_pre_3779_fingerprint_admits_the_split_is_unknown(self) -> None:
        summary = FingerprintResult(
            hash="abc123",
            num_tests_run=357,
            num_tests_passed=357,
            scope="full",
        ).get_cache_summary()
        self.assertIn("357/357 passed", summary)
        self.assertIn("unit/example split unrecorded", summary)

    def test_split_round_trips_through_disk(self) -> None:
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            mgr.write(
                "cpp_test",
                FingerprintResult(
                    hash="abc123",
                    num_tests_run=357,
                    num_tests_passed=357,
                    scope="full",
                    num_examples_run=83,
                    num_examples_passed=83,
                    examples_included=True,
                ),
            )
            path = Path(tmp) / "fingerprint" / "cpp_test_quick.json"
            written = json.loads(path.read_text())
            self.assertEqual(written["num_examples_run"], 83)
            self.assertIs(written["examples_included"], True)

            reloaded = mgr.read("cpp_test")
            assert reloaded is not None
            self.assertEqual(reloaded.num_examples_passed, 83)
            self.assertIn("83/83 examples", reloaded.get_cache_summary())

    def test_split_travels_with_the_counts_on_a_cache_hit(self) -> None:
        """save_all() carries prior counts forward when nothing ran. The split
        must come along, or a unit-only result is relabelled as unattributed --
        or worse, as covering examples it never touched."""
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            mgr._prev_fingerprints["cpp_test"] = FingerprintResult(
                hash="abc123",
                num_tests_run=274,
                num_tests_passed=274,
                scope="full",
                num_examples_run=0,
                num_examples_passed=0,
                examples_included=True,
            )
            mgr._fingerprints["cpp_test"] = FingerprintResult(hash="abc123")

            mgr.save_all("success")

            reloaded = mgr.read("cpp_test")
            assert reloaded is not None
            self.assertEqual(reloaded.num_examples_run, 0)
            self.assertIs(reloaded.examples_included, True)
            self.assertIn("examples cached, not re-run", reloaded.get_cache_summary())


class TestMetadataPlumbing(unittest.TestCase):
    def test_update_test_metadata_records_the_split(self) -> None:
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            mgr._fingerprints["cpp_test"] = FingerprintResult(hash="abc123")
            mgr.update_test_metadata(
                "cpp_test",
                num_tests_run=357,
                num_tests_passed=357,
                duration_seconds=48.0,
                test_name="cpp_unit_tests",
                scope="full",
                num_examples_run=83,
                num_examples_passed=83,
                examples_included=True,
            )
            fp = mgr._fingerprints["cpp_test"]
            self.assertEqual(fp.num_examples_run, 83)
            self.assertIs(fp.examples_included, True)

    def test_omitting_the_split_leaves_it_unrecorded_rather_than_zero(self) -> None:
        """A path that cannot attribute its counts must not claim zero examples --
        that would read as an authoritative "examples cached" statement."""
        with TemporaryDirectory() as tmp:
            mgr = FingerprintManager(Path(tmp))
            mgr._fingerprints["cpp_test"] = FingerprintResult(hash="abc123")
            mgr.update_test_metadata(
                "cpp_test",
                num_tests_run=357,
                num_tests_passed=357,
                duration_seconds=48.0,
                scope="full",
            )
            fp = mgr._fingerprints["cpp_test"]
            self.assertIsNone(fp.num_examples_run)
            self.assertIn("split unrecorded", fp.get_cache_summary())


class TestResultRowLabel(unittest.TestCase):
    """The summary table carried the same anonymous denominator as the headline."""

    @staticmethod
    def _row(**kw: object) -> str:
        base: dict[str, object] = {
            "success": True,
            "duration": 48.0,
            "num_tests_run": 357,
            "num_tests_passed": 357,
            "num_tests_failed": 0,
        }
        base.update(kw)
        return _describe_cpp_counts(MesonTestResult(**base))  # type: ignore[arg-type]

    def test_attributed_row_names_both_populations(self) -> None:
        self.assertEqual(
            self._row(
                num_examples_run=83, num_examples_passed=83, examples_included=True
            ),
            "274/274 unit, 83/83 examples passed",
        )

    def test_unit_only_row_says_examples_were_not_requested(self) -> None:
        self.assertEqual(
            self._row(
                num_tests_run=275,
                num_tests_passed=275,
                num_examples_run=0,
                num_examples_passed=0,
                examples_included=False,
            ),
            "275/275 unit passed; examples not requested",
        )

    def test_unattributed_row_admits_it(self) -> None:
        """The meson-test fallback cannot split its total; it must not imply one."""
        self.assertEqual(self._row(), "357/357 passed; unit/example split unrecorded")


class TestSuiteExclusionDetection(unittest.TestCase):
    """Coverage must be read from the exclusion list, not guessed at."""

    def test_no_exclusions_means_examples_ran(self) -> None:
        self.assertTrue(examples_are_included(None))
        self.assertTrue(examples_are_included([]))

    def test_project_qualified_exclusion_is_detected(self) -> None:
        self.assertFalse(examples_are_included(["fastled:examples"]))

    def test_bare_suite_name_is_detected(self) -> None:
        """meson accepts `--no-suite examples`, and this repo's own docstrings
        advertise that form. Missing it would claim examples ran when they
        did not -- worse than the ambiguity #3779 is about."""
        self.assertFalse(examples_are_included(["examples"]))

    def test_unrelated_exclusions_leave_examples_included(self) -> None:
        self.assertTrue(examples_are_included(["fastled:slow", "integration"]))


class TestPartialAttribution(unittest.TestCase):
    """Knowing WHICH populations ran, but not their sizes, is its own case --
    it must not collapse into either 'fully attributed' or 'nothing known'."""

    def test_populations_known_sizes_not(self) -> None:
        b = describe_test_counts(
            num_passed=357,
            num_run=357,
            num_examples_passed=None,
            num_examples_run=None,
            examples_included=True,
        )
        self.assertEqual(b.counts, "357/357 unit + examples")
        self.assertEqual(b.notes, "")

    def test_nothing_known_admits_it(self) -> None:
        b = describe_test_counts(
            num_passed=357,
            num_run=357,
            num_examples_passed=None,
            num_examples_run=None,
            examples_included=None,
        )
        self.assertEqual(b.counts, "357/357")
        self.assertEqual(b.notes, "unit/example split unrecorded")

    def test_headline_and_table_describe_a_run_identically(self) -> None:
        """The two used to disagree: the headline claimed 'unit + examples'
        while the table two lines later said the split was unrecorded."""
        result = MesonTestResult(
            success=True,
            duration=48.0,
            num_tests_run=357,
            num_tests_passed=357,
            num_tests_failed=0,
            examples_included=True,
        )
        headline = describe_test_counts(
            num_passed=357,
            num_run=357,
            num_examples_passed=None,
            num_examples_run=None,
            examples_included=True,
        )
        self.assertIn(headline.counts, _describe_cpp_counts(result))

    def test_describe_uses_a_separator_that_survives_commas(self) -> None:
        b = RunBreakdown("274/274 unit", "examples cached, not re-run")
        self.assertEqual(
            b.describe(" passed"),
            "274/274 unit passed; examples cached, not re-run",
        )
        self.assertEqual(RunBreakdown("357/357", "").describe(), "357/357")


class TestEarlyExitMirrorMatchesCanonical(unittest.TestCase):
    """``ci/early_exit_cache.py`` re-implements the wording stdlib-only to stay
    under its import budget. That duplication is only safe while the two agree,
    so pin them against each other."""

    CASES = [
        # (num_passed, num_run, num_examples_passed, num_examples_run, included)
        (357, 357, 83, 83, True),
        (274, 274, 0, 0, True),
        (274, 274, 0, 0, False),
        (83, 83, 83, 83, True),
        (0, 0, 0, 0, True),
        (274, 274, 0, 0, True),
        (355, 357, 83, 83, True),
    ]

    def test_mirror_agrees_with_format_run_breakdown(self) -> None:
        # Sweep the filtered axis too: the canonical grew `filtered` after the
        # mirror was written, and the pair silently diverged until pinned.
        for passed, run, ex_passed, ex_run, included in self.CASES:
            for scope in ("full", "partial"):
                with self.subTest(
                    counts=(passed, run, ex_passed, ex_run, included), scope=scope
                ):
                    breakdown = format_run_breakdown(
                        unit_passed=passed - ex_passed,
                        unit_run=run - ex_run,
                        examples_passed=ex_passed,
                        examples_run=ex_run,
                        examples_included=included,
                        filtered=scope == "partial",
                    )
                    mirror = _cpp_run_breakdown(
                        {
                            "num_examples_run": ex_run,
                            "num_examples_passed": ex_passed,
                            "examples_included": included,
                            "scope": scope,
                        },
                        passed,
                        run,
                    )
                    self.assertEqual(mirror, (breakdown.counts, breakdown.notes))

    def test_mirror_flags_a_pre_3779_entry(self) -> None:
        self.assertEqual(
            _cpp_run_breakdown({}, 357, 357),
            ("357/357", "unit/example split unrecorded"),
        )


if __name__ == "__main__":
    unittest.main()
