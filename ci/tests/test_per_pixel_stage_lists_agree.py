"""The per-pixel stage lists must agree with each other.

Three guards scan the same eight-stage pipeline, each with its own
hand-maintained list, and all three drifted independently:

* FastLED#4330 -- `test_no_rgb8_intermediate.py` claimed to cover "the
  streaming path" and covered the pipeline's interior.
* FastLED#4335 -- `test_no_iterative_solver_per_pixel.py` scanned five of the
  eight, missing `gamut_map.cpp.hpp`: the one stage A3 is about, and the only
  place anyone would put an iterative solver.
* FastLED#4337 -- `test_fixed_point_headers_are_self_contained.py` listed six
  of the eight headers. `transfer.h` and `chromatic_adaptation.h` were
  included by no listed header and had no coverage at all.

Each list was defensible read alone. The gaps only appeared reading them side
by side, which nothing did -- a guard's list was itself a thing nothing
checked.

This is the cheap half of the fix. It does not restructure the three files or
introduce a shared stage module; it asserts they agree, so the next
divergence fails here instead of waiting to be noticed. FastLED#4339 showed
the value of that shape: the reference corpus names its source set in five
places too, and every one of them failed loudly when a source was added,
which is why that duplication never silently rotted.
"""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path
from types import ModuleType


TESTS_DIR = Path(__file__).resolve().parent
GFX = TESTS_DIR.parent.parent / "src" / "fl" / "gfx"


def _load(name: str) -> ModuleType:
    """Import a sibling guard by path, without needing a package."""

    path = TESTS_DIR / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise ImportError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    # Registered before exec_module, not after: the self-containment guard
    # decorates with typeguard's @typechecked, which reads its own module
    # source out of sys.modules while the decorator runs and raises KeyError
    # if it is not there yet.
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


class TestPerPixelStageListsAgree(unittest.TestCase):
    def test_the_two_stage_scanners_cover_the_same_files(self) -> None:
        rgb8 = _load("test_no_rgb8_intermediate")
        solver = _load("test_no_iterative_solver_per_pixel")

        self.assertEqual(
            tuple(rgb8.PER_PIXEL_STAGES),
            tuple(solver.PER_PIXEL_STAGES),
            msg=(
                "the RGB8 and iterative-solver guards scan the same per-pixel "
                "path and must scan the same files; see FastLED#4335, where "
                "one of them was missing the gamut mapper"
            ),
        )

    def test_every_scanned_stage_exists(self) -> None:
        rgb8 = _load("test_no_rgb8_intermediate")
        for name in rgb8.PER_PIXEL_STAGES:
            with self.subTest(stage=name):
                self.assertTrue((GFX / name).is_file(), msg=f"missing {name}")

    def test_each_stage_header_is_checked_for_self_containment(self) -> None:
        # The self-containment guard lists headers, not stage bodies, and
        # carries entries this file has no opinion about -- fixed-point and
        # channels headers. What it must not do is skip a stage: that is
        # exactly FastLED#4337, where `transfer.h` compiled nowhere on its own
        # and nothing noticed.
        rgb8 = _load("test_no_rgb8_intermediate")
        headers = _load("test_fixed_point_headers_are_self_contained")
        listed = set(headers.kSelfContainedHeaders)

        for stage in rgb8.PER_PIXEL_STAGES:
            header = "fl/gfx/" + stage.replace(".cpp.hpp", ".h")
            with self.subTest(stage=stage, header=header):
                self.assertIn(
                    header,
                    listed,
                    msg=(
                        f"{header} backs a per-pixel stage and is not checked "
                        "for self-containment (FastLED#4337)"
                    ),
                )


if __name__ == "__main__":
    unittest.main()
