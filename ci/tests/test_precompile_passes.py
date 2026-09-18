"""A header edit must invalidate the PCH even when build.ninja is unchanged.

The pre-compile PCH staleness check was gated, together with the strict-path
normalize, on build.ninja's mtime (#3129 A5). That is a valid shortcut for the
normalize, which repairs what a meson reconfigure writes into build.ninja. It
is not valid for the PCH: editing a header the PCH was built from never
touches build.ninja, so the check was skipped in exactly the case it exists
for, and the next build died with "file ... has been modified since the
precompiled header was built" until `--clean`.

These drive the real `_run_precompile_passes` against a real build directory
laid out the way `ci/compile_pch.py` leaves one.
"""

from __future__ import annotations

import tempfile
import unittest
from dataclasses import dataclass
from pathlib import Path

from typeguard import typechecked

from ci.compile_pch import hash_input_files
from ci.meson.compile import (
    _precompile_passes_can_be_skipped,
    _run_precompile_passes,
    _write_precompile_mtime_marker,
)


@typechecked
@dataclass(slots=True)
class _PchLayout:
    build_dir: Path
    pch: Path
    header: Path


def _build_dir_with_pch(root: Path) -> _PchLayout:
    """A build dir whose PCH is fresh and whose A5 shortcut is armed."""
    build_dir = root / "build"
    build_dir.mkdir()
    build_ninja = build_dir / "build.ninja"
    build_ninja.write_text("# placeholder\n", encoding="utf-8")

    header = root / "cled_controller.h"
    header.write_text("int before;\n", encoding="utf-8")

    pch = build_dir / "test_pch.h.pch"
    pch.write_bytes(b"PCH")
    (build_dir / "test_pch.h.d.cache").write_text(
        f"test_pch.h.pch: {header.as_posix()}\n", encoding="utf-8"
    )
    Path(str(pch) + ".input_hash").write_text(
        hash_input_files([header]), encoding="utf-8"
    )
    Path(str(pch) + ".flags_hash").write_text("flags", encoding="utf-8")

    _write_precompile_mtime_marker(build_dir, build_ninja.stat().st_mtime)
    return _PchLayout(build_dir, pch, header)


class PrecompilePassesTest(unittest.TestCase):
    def test_a_header_edit_invalidates_the_pch_with_build_ninja_unchanged(
        self: "PrecompilePassesTest",
    ) -> None:
        with tempfile.TemporaryDirectory() as directory:
            layout = _build_dir_with_pch(Path(directory))
            build_dir, pch, header = layout.build_dir, layout.pch, layout.header
            # The shortcut is armed: nothing about build.ninja changed.
            self.assertTrue(_precompile_passes_can_be_skipped(build_dir))

            header.write_text("int after;\n", encoding="utf-8")
            _run_precompile_passes(build_dir)

            self.assertFalse(
                pch.exists(), "a stale PCH survived because build.ninja did not change"
            )

    def test_an_unchanged_header_keeps_the_pch(
        self: "PrecompilePassesTest",
    ) -> None:
        with tempfile.TemporaryDirectory() as directory:
            layout = _build_dir_with_pch(Path(directory))
            _run_precompile_passes(layout.build_dir)
            self.assertTrue(layout.pch.exists())


if __name__ == "__main__":
    unittest.main()
