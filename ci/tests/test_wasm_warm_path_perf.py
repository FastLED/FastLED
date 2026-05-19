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

import time
import unittest
from unittest.mock import patch

from ci.wasm_build import (
    _compute_src_file_list_hash,
    _render_wasm_cross_file,
    _src_file_list_hash_reset_for_tests,
)


class TestSrcFileListHashMemoization(unittest.TestCase):
    """The hash function should return the same digest on every call AND
    take meaningfully less time on the second call within a process."""

    def setUp(self) -> None:
        _src_file_list_hash_reset_for_tests()

    def test_digest_is_stable_across_calls(self) -> None:
        h1 = _compute_src_file_list_hash()
        h2 = _compute_src_file_list_hash()
        self.assertEqual(h1, h2)

    def test_second_call_is_faster_than_first(self) -> None:
        t0 = time.perf_counter()
        _compute_src_file_list_hash()
        cold = time.perf_counter() - t0

        t0 = time.perf_counter()
        _compute_src_file_list_hash()
        warm = time.perf_counter() - t0

        # Memoized call must be at least 5× faster than the cold walk.
        # In practice it's ~100×, but 5× is a robust floor that won't
        # flake on slow CI runners.
        self.assertLess(
            warm * 5,
            cold,
            f"Memoized call not meaningfully faster: cold={cold * 1000:.2f} ms, warm={warm * 1000:.2f} ms",
        )

    def test_reset_clears_cache(self) -> None:
        _compute_src_file_list_hash()  # populate
        _src_file_list_hash_reset_for_tests()

        # After reset the next call must do the full walk again. We assert
        # this indirectly by checking the timing is comparable to a cold call.
        t0 = time.perf_counter()
        _compute_src_file_list_hash()
        post_reset = time.perf_counter() - t0
        self.assertGreater(
            post_reset * 1000,
            1.0,
            "Post-reset call too fast — cache not actually cleared",
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
