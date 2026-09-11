"""The 4-wide fixed-point headers compile on their own.

`s16x16x4.h` reaches `s0x32x4.h` through `simd_ops.h`, and that include sits
*inside* `namespace fl` -- deliberately, because `simd_ops.h` defines
cross-type operators that need both types complete and so does not open a
namespace of its own.

On the usual path this is harmless: something else has already included
`s0x32x4.h`, so `#pragma once` makes it a no-op. In a translation unit that
reaches `s16x16x4.h` first it is not, because the whole of `s0x32x4.h` -- and
`s0x32.h`, and `fl/stl/type_traits.h` under it -- is then processed one
namespace deeper. `fl::enable_if` becomes `fl::fl::enable_if` and the header
does not compile.

Nothing in the tree hits that order today, which is exactly why it is worth a
test: the failure appears when someone adds the first include that does.

`simd_ops.h` itself is not checked. It documents that it must be included
inside an open `fl` namespace, so failing standalone is its contract.
"""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

import pytest
from typeguard import typechecked


PROJECT_ROOT = Path(__file__).resolve().parents[2]

kSelfContainedHeaders = [
    "fl/math/fixed_point/s16x16x4.h",
    "fl/math/fixed_point/s0x32x4.h",
    "fl/math/fixed_point/s16x16.h",
    "fl/math/fixed_point/s0x32.h",
    # Same defect, different cause: these spelled `EmitterProfile`
    # unqualified, which only resolves once some *other* header has pulled in
    # the `using` from `fl/channels/color_profile.h`. FastLED#4043, and then
    # the same thing again in its neighbour -- which is the argument for
    # listing the whole group rather than adding them one incident at a time.
    # The eight per-pixel stages, kept whole on purpose. This list held six
    # of them. `flux_scalar.h` and `source_xyz.h` were covered by accident --
    # `pipeline.h` is listed and includes both, so a break failed, blamed on
    # `pipeline.h`. `transfer.h` and `chromatic_adaptation.h` are included by
    # no listed header and had no coverage at all: dropping an include from
    # `transfer.h` left this file green at 15 passed (FastLED#4337).
    #
    # Two sibling guards scan the same eight -- test_no_rgb8_intermediate.py
    # and test_no_iterative_solver_per_pixel.py. Three hand-maintained lists
    # over one path; keep them in step.
    "fl/gfx/transfer.h",
    "fl/gfx/source_xyz.h",
    "fl/gfx/chromatic_adaptation.h",
    "fl/gfx/flux_scalar.h",
    "fl/gfx/device_solve.h",
    "fl/gfx/white_allocation.h",
    "fl/gfx/gamut_map.h",
    "fl/gfx/colorimetric_response.h",
    "fl/gfx/pipeline.h",
    "fl/gfx/oklab_q16.h",
    # The channels layer reaches the same type through its own
    # `fl/channels/color_profile.h`, so these are fixed by including that
    # rather than by qualifying.
    "fl/channels/color_profile.h",
    "fl/channels/color_managed_source.h",
    "fl/channels/cled_controller.h",
    "fl/channels/options.h",
    "fl/channels/channel.h",
]


@typechecked
def _compiler() -> str:
    found = shutil.which("c++") or shutil.which("clang++") or shutil.which("g++")
    if found is None:
        pytest.skip("no C++ compiler available")
    return found


@pytest.mark.parametrize("header", kSelfContainedHeaders)
@typechecked
def test_header_compiles_as_the_only_include(header: str, tmp_path: Path) -> None:
    source = tmp_path / "only_include.cpp"
    source.write_text(f'#include "{header}"\n', encoding="utf-8")

    # `subprocess.run` and not `RunningProcess.run`: the latter merges stderr
    # into stdout, and the diagnostics being reported here arrive on stderr.
    completed = subprocess.run(  # noqa: SRC001
        [
            _compiler(),
            "-fsyntax-only",
            "-I",
            str(PROJECT_ROOT / "src"),
            "-std=gnu++17",
            str(source),
        ],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    assert completed.returncode == 0, (
        f"{header} does not compile as a translation unit's only include:\n"
        f"{completed.stderr}"
    )
