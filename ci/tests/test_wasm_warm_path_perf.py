"""
Tests for the WASM warm-path performance optimizations landed alongside
clang-tool-chain 1.5.5.

Three changes are covered:
  1. ``_ensure_emcc_native_launcher`` uses one ``os.scandir`` + set lookup
     instead of seven individual ``Path.exists()`` calls (~220 ms saved on
     cold filesystem cache).
  2. ``_compute_src_file_list_hash`` is memoized per-process by the src/
     directory's mtime + top-level entry count (~40 ms saved on repeat calls).
  3. ``_ensure_emscripten_wasm_ld_patch`` is no longer called from
     ``_render_wasm_cross_file`` (the warm-build hot path) — moved to
     ``run_emcc`` which is the only function that imports emcc.py in-process.
"""

from __future__ import annotations

import os
import unittest
from unittest.mock import patch

from ci.wasm_build import (
    _compute_src_file_list_hash,
    _render_wasm_cross_file,
    _src_file_list_hash_reset_for_tests,
)


class TestSrcFileListHashMemoization(unittest.TestCase):
    """The hash function should return the same digest on every call AND
    must skip the expensive directory walk on cache hits."""

    def setUp(self) -> None:
        _src_file_list_hash_reset_for_tests()

    def test_digest_is_stable_across_calls(self) -> None:
        h1 = _compute_src_file_list_hash()
        h2 = _compute_src_file_list_hash()
        self.assertEqual(h1, h2)

    def test_second_call_hits_memo_cache(self) -> None:
        """Deterministic check that the memo works: wrap os.scandir with a
        call counter and verify the warm call skips the recursive walk.

        Both cold and warm calls invoke os.scandir once (via iterdir() for
        top_count). The cold call additionally drives the recursive _scan
        which walks every src/ subdirectory — many extra scandir calls. The
        warm call must short-circuit and add no further calls beyond the
        single iterdir."""
        real_scandir = os.scandir
        call_count = [0]

        def counting_scandir(path):  # type: ignore[no-untyped-def]
            call_count[0] += 1
            return real_scandir(path)

        with patch("ci.wasm_build.os.scandir", side_effect=counting_scandir):
            h1 = _compute_src_file_list_hash()
            cold_calls = call_count[0]
            h2 = _compute_src_file_list_hash()
            warm_calls = call_count[0] - cold_calls

        self.assertEqual(h1, h2)
        # Cold walk must touch many subdirectories under src/.
        self.assertGreater(
            cold_calls,
            10,
            f"Cold call did not perform recursive walk (only {cold_calls} scandirs)",
        )
        # Warm call only does the iterdir() top-count probe (1 scandir).
        # Anything more means the memo missed and _scan ran again.
        self.assertEqual(
            warm_calls,
            1,
            f"Memo missed: warm call made {warm_calls} scandir calls "
            f"(expected exactly 1 for the top_count probe)",
        )

    def test_reset_forces_full_walk(self) -> None:
        """After reset, the next call must redo the recursive walk
        (observable as many additional os.scandir invocations)."""
        real_scandir = os.scandir
        call_count = [0]

        def counting_scandir(path):  # type: ignore[no-untyped-def]
            call_count[0] += 1
            return real_scandir(path)

        with patch("ci.wasm_build.os.scandir", side_effect=counting_scandir):
            _compute_src_file_list_hash()  # populate
            cold_calls = call_count[0]

            _src_file_list_hash_reset_for_tests()

            _compute_src_file_list_hash()  # must walk again
            post_reset_calls = call_count[0] - cold_calls

        # Post-reset must repeat the recursive walk, not just the 1-call probe.
        self.assertGreater(
            post_reset_calls,
            10,
            f"Post-reset call only made {post_reset_calls} scandir calls — "
            f"cache not actually cleared",
        )


class TestRenderWasmCrossFileNoLongerCallsEmscriptenPatch(unittest.TestCase):
    """The expensive ``_ensure_emscripten_wasm_ld_patch`` must not be called
    from ``_render_wasm_cross_file`` — that call moved to ``run_emcc`` so
    warm builds don't pay for the patch verification on every invocation."""

    def test_render_does_not_call_emscripten_patch(self) -> None:
        import tempfile
        from pathlib import Path

        with patch("ci.wasm_build._ensure_emscripten_wasm_ld_patch") as mock_patch:
            with tempfile.TemporaryDirectory() as td:
                _render_wasm_cross_file(Path(td))
            mock_patch.assert_not_called()


class TestNativeLauncherScandirOptimization(unittest.TestCase):
    """``_ensure_emcc_native_launcher`` should use a single ``os.scandir``
    rather than seven individual ``Path.exists`` calls for presence checks."""

    def test_uses_scandir_not_seven_stats(self) -> None:
        # Inspect the source of _ensure_emcc_native_launcher and assert it
        # contains the scandir-based presence check. Source-level assertion
        # is appropriate here because the timing on a warm-cache test
        # machine isn't reliable (both approaches are sub-ms).
        import inspect

        from ci.meson.build_config import _ensure_emcc_native_launcher

        src = inspect.getsource(_ensure_emcc_native_launcher)
        self.assertIn(
            "os.scandir",
            src,
            "Expected os.scandir-based presence check in _ensure_emcc_native_launcher",
        )
        self.assertIn("present_names", src, "Expected presence-set variable")


if __name__ == "__main__":
    unittest.main()
