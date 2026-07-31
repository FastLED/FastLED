"""Tests for the meson example-change detector (issue #3761).

The detector decides whether ``bash test`` must pay a full meson reconfigure
(~20-30s) because ``examples/`` changed. Several bugs made it fire on a clean
tree, or miss real changes:

  1. A missing cache was reported as "example files changed" — conflating
     "cannot prove unchanged" with "observed a change".
  2. Firing *deleted* the cache, and only meson's ``--update`` rewrites it. A
     following run that was fully fingerprint-cached never invoked meson, so
     the cache stayed missing and guaranteed another false positive.
  3. The hash was mtime-based, so ``git checkout`` / ``git worktree add``
     invalidated it even though the file contents were byte-identical.
  4. The mtime fast path consulted directory mtimes only, which do not move on
     an in-place edit — so a content edit to an existing example was reported
     as "unchanged" and the content hash was never consulted.
  5. Nothing re-armed the fast path after a hash match, so one git checkout
     left every later run paying the full content hash forever.
  6. The file set included build artifacts under ``examples/*/.build`` etc.,
     which a plain ``bash compile`` rewrites — a fresh false-positive source.
  7. The cache was written UTF-8 but read with the platform locale codec.

These tests pin all of those.
"""

from __future__ import annotations

import json
import os
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from ci.meson.example_metadata_cache import (
    compute_example_files_hash,
    iter_example_files,
)
from ci.meson.meson_setup_phases import detect_example_file_changes


def _make_source_tree(root: Path) -> Path:
    examples = root / "examples"
    (examples / "Blink").mkdir(parents=True)
    (examples / "Blink" / "Blink.ino").write_text("void setup() {}\n")
    (examples / "Blink" / "helper.h").write_text("#pragma once\n")
    return examples


def _write_cache(build_dir: Path, examples_dir: Path, metadata: str = "") -> Path:
    cache_file = build_dir / "examples" / "example_metadata.cache"
    cache_file.parent.mkdir(parents=True, exist_ok=True)
    cache_file.write_text(
        json.dumps(
            {
                "hash": compute_example_files_hash(examples_dir),
                "timestamp": 0.0,
                "metadata": metadata or "EXAMPLE|Blink|Blink",
            }
        ),
        encoding="utf-8",
    )
    return cache_file


def _age_tree(examples_dir: Path, cache_file: Path) -> None:
    """Make every example entry older than the cache, arming the fast path."""
    past = cache_file.stat().st_mtime - 10_000
    for f in list(examples_dir.rglob("*")) + [examples_dir]:
        os.utime(f, (past, past))


class TestExampleHashIsContentBased(unittest.TestCase):
    def test_mtime_bump_alone_does_not_change_hash(self) -> None:
        """git checkout rewrites mtimes with identical content — must not invalidate."""
        with TemporaryDirectory() as tmp:
            examples = _make_source_tree(Path(tmp))
            before = compute_example_files_hash(examples)

            for f in examples.rglob("*"):
                if f.is_file():
                    stat = f.stat()
                    os.utime(f, (stat.st_atime + 10_000, stat.st_mtime + 10_000))

            self.assertEqual(before, compute_example_files_hash(examples))

    def test_content_edit_changes_hash(self) -> None:
        with TemporaryDirectory() as tmp:
            examples = _make_source_tree(Path(tmp))
            before = compute_example_files_hash(examples)
            (examples / "Blink" / "Blink.ino").write_text("void setup() { x(); }\n")
            self.assertNotEqual(before, compute_example_files_hash(examples))

    def test_added_and_removed_files_change_hash(self) -> None:
        with TemporaryDirectory() as tmp:
            examples = _make_source_tree(Path(tmp))
            before = compute_example_files_hash(examples)

            new_file = examples / "Fade" / "Fade.ino"
            new_file.parent.mkdir()
            new_file.write_text("void setup() {}\n")
            self.assertNotEqual(before, compute_example_files_hash(examples))

            new_file.unlink()
            new_file.parent.rmdir()
            self.assertEqual(before, compute_example_files_hash(examples))

    def test_identical_content_under_different_names_differs(self) -> None:
        """Paths participate in the hash, so a rename is a change."""
        with TemporaryDirectory() as tmp:
            examples = _make_source_tree(Path(tmp))
            before = compute_example_files_hash(examples)
            (examples / "Blink" / "Blink.ino").rename(
                examples / "Blink" / "Renamed.ino"
            )
            self.assertNotEqual(before, compute_example_files_hash(examples))

    def test_build_artifacts_are_pruned(self) -> None:
        """`bash compile` writes .h/.cpp under examples/*/.build — not a source change."""
        with TemporaryDirectory() as tmp:
            examples = _make_source_tree(Path(tmp))
            before = compute_example_files_hash(examples)

            for artifact_dir in (".build", ".fbuild", "build", "bin", ".vscode"):
                generated = examples / "Blink" / artifact_dir / "generated.h"
                generated.parent.mkdir(parents=True)
                generated.write_text("// generated\n")

            self.assertEqual(before, compute_example_files_hash(examples))
            listed = {p.name for p in iter_example_files(examples)}
            self.assertNotIn("generated.h", listed)
            self.assertIn("Blink.ino", listed)


class TestDetectExampleFileChanges(unittest.TestCase):
    def test_clean_tree_with_valid_cache_reports_no_change(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            examples = _make_source_tree(root)
            build_dir = root / ".build" / "meson-quick"
            _write_cache(build_dir, examples)

            self.assertIsNone(detect_example_file_changes(root, build_dir))

    def test_missing_cache_is_not_reported_as_a_file_change(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            _make_source_tree(root)
            build_dir = root / ".build" / "meson-quick"
            build_dir.mkdir(parents=True)

            reason = detect_example_file_changes(root, build_dir)
            # Still reconfigures (safe default) but says why accurately.
            self.assertIsNotNone(reason)
            assert reason is not None
            self.assertIn("cache missing", reason)
            self.assertNotIn("files changed", reason)

    def test_detector_never_deletes_the_cache(self) -> None:
        """Deleting the cache is what made one false positive self-perpetuate."""
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            examples = _make_source_tree(root)
            build_dir = root / ".build" / "meson-quick"
            cache_file = _write_cache(build_dir, examples)

            target = examples / "Blink" / "Blink.ino"
            target.write_text("void setup() { y(); }\n")
            future = cache_file.stat().st_mtime + 10_000
            os.utime(target, (future, future))

            self.assertEqual(
                detect_example_file_changes(root, build_dir), "example files changed"
            )
            self.assertTrue(
                cache_file.exists(), "detector must leave the cache in place"
            )

    def test_in_place_content_edit_is_detected(self) -> None:
        """Directory mtimes do not move on an edit — the file mtime probe must."""
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            examples = _make_source_tree(root)
            build_dir = root / ".build" / "meson-quick"
            cache_file = _write_cache(build_dir, examples)
            _age_tree(examples, cache_file)
            self.assertIsNone(detect_example_file_changes(root, build_dir))

            # Edit content without touching any directory entry.
            target = examples / "Blink" / "Blink.ino"
            target.write_text("// @filter: esp32\nvoid setup() {}\n")
            future = cache_file.stat().st_mtime + 10_000
            os.utime(target, (future, future))

            self.assertEqual(
                detect_example_file_changes(root, build_dir), "example files changed"
            )

    def test_fast_path_is_rearmed_after_a_matching_hash(self) -> None:
        """A git checkout must cost one content hash, not one on every later run."""
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            examples = _make_source_tree(root)
            build_dir = root / ".build" / "meson-quick"
            cache_file = _write_cache(build_dir, examples)

            # Simulate `git checkout`: content identical, but the working tree's
            # mtimes are now newer than the cache, so the fast path cannot fire
            # and the detector must fall through to the content hash.
            past = cache_file.stat().st_mtime - 10_000
            os.utime(cache_file, (past, past))

            self.assertIsNone(detect_example_file_changes(root, build_dir))
            self.assertGreater(
                cache_file.stat().st_mtime,
                past,
                "cache mtime must be re-stamped so the fast path re-arms",
            )

            # The fast path is armed again. Prove the hash is now skipped: edit
            # content but keep every mtime older than the re-stamped marker —
            # only a run that skipped the hash reports "no change".
            target = examples / "Blink" / "Blink.ino"
            target.write_bytes(b"void setup() { z(); }\n")
            for f in (target, target.parent, examples):
                os.utime(f, (past, past))
            self.assertIsNone(detect_example_file_changes(root, build_dir))

    def test_unreadable_cache_is_distinguished_from_a_file_change(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            _make_source_tree(root)
            build_dir = root / ".build" / "meson-quick"
            cache_file = build_dir / "examples" / "example_metadata.cache"
            cache_file.parent.mkdir(parents=True)
            cache_file.write_text("{ not json")
            past = cache_file.stat().st_mtime - 10_000
            os.utime(cache_file, (past, past))

            reason = detect_example_file_changes(root, build_dir)
            self.assertIsNotNone(reason)
            assert reason is not None
            self.assertIn("unreadable", reason)

    def test_non_ascii_metadata_round_trips(self) -> None:
        """The cache is written UTF-8; reading it with the locale codec broke it."""
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            examples = _make_source_tree(root)
            build_dir = root / ".build" / "meson-quick"
            _write_cache(build_dir, examples, metadata="EXAMPLE|Blink|Blink — café")

            self.assertIsNone(detect_example_file_changes(root, build_dir))

    def test_no_examples_dir_means_no_reconfigure(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            build_dir = root / ".build" / "meson-quick"
            build_dir.mkdir(parents=True)
            self.assertIsNone(detect_example_file_changes(root, build_dir))


if __name__ == "__main__":
    unittest.main()
