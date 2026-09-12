"""`bash bloat` must analyse the ELF the build produced, not an older one.

`find_elf` used to pick by a fixed layout priority and ignore mtime. The
layouts coexist under one board directory -- `--build` writes the `.pio` one
while the `.fbuild` one outranked it -- so a stale artifact could be analysed
with no indication anything was wrong.

That is how the command reported five-day-old numbers for a change made
minutes earlier: two runs across a real code change produced byte-identical
output, `total_flash` included, and the totals did not even match CI's for the
same board and example because they described different binaries.
FastLED#4384.
"""

from __future__ import annotations

import os
import tempfile
import time
import unittest
from dataclasses import dataclass
from pathlib import Path

from typeguard import typechecked

from ci.bloat import ElfLocation, _assert_fresh, find_elf


def _touch(path: Path, mtime: float) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"\x7fELF")
    os.utime(path, (mtime, mtime))


@typechecked
@dataclass(frozen=True, slots=True)
class Layouts:
    """The two ELF layouts that coexist under one board directory."""

    fbuild: Path
    pio: Path


class TestFindElfPrefersTheNewest(unittest.TestCase):
    def _layouts(self: "TestFindElfPrefersTheNewest", root: Path) -> Layouts:
        """The two that coexist in practice, and did here."""

        fbuild = root / "pio" / "esp32s3" / ".fbuild" / "build" / "release"
        pio = root / "pio" / "esp32s3" / ".pio" / "build" / "esp32s3"
        return Layouts(fbuild=fbuild / "firmware.elf", pio=pio / "firmware.elf")

    def test_the_newer_pio_elf_wins_over_a_stale_fbuild_one(
        self: "TestFindElfPrefersTheNewest",
    ) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            layouts = self._layouts(root)
            now = time.time()
            _touch(layouts.fbuild, now - 5 * 86400)
            _touch(layouts.pio, now)

            # Priority alone would return the fbuild one, which is the bug.
            self.assertEqual(find_elf("esp32s3", root).elf, layouts.pio)

    def test_the_newer_fbuild_elf_still_wins_when_it_is_newer(
        self: "TestFindElfPrefersTheNewest",
    ) -> None:
        """The change is "newest", not "always prefer PIO"."""

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            layouts = self._layouts(root)
            now = time.time()
            _touch(layouts.pio, now - 5 * 86400)
            _touch(layouts.fbuild, now)

            location = find_elf("esp32s3", root)
            self.assertEqual(location.elf, layouts.fbuild)
            self.assertTrue(location.fbuild_native)

    def test_a_missing_build_still_reports_where_it_looked(
        self: "TestFindElfPrefersTheNewest",
    ) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(SystemExit) as caught:
                find_elf("esp32s3", Path(tmp))
            self.assertIn("no firmware.elf found", str(caught.exception))


class TestFreshnessGuard(unittest.TestCase):
    """`--build` promises a fresh ELF; this is the promise being checked."""

    def _location(self: "TestFreshnessGuard", tmp: str, mtime: float) -> ElfLocation:
        elf = Path(tmp) / "firmware.elf"
        _touch(elf, mtime)
        return ElfLocation(elf=elf, fbuild_native=False)

    def test_without_build_nothing_is_asserted(self: "TestFreshnessGuard") -> None:
        with tempfile.TemporaryDirectory() as tmp:
            _assert_fresh(self._location(tmp, time.time() - 86400), None)

    def test_an_elf_newer_than_the_build_passes(self: "TestFreshnessGuard") -> None:
        with tempfile.TemporaryDirectory() as tmp:
            started = time.time() - 60
            _assert_fresh(self._location(tmp, time.time()), started)

    def test_an_elf_rounded_just_below_the_build_still_passes(
        self: "TestFreshnessGuard",
    ) -> None:
        """Coarse filesystem mtime must not read as stale.

        `time.time()` is sub-microsecond; a filesystem may store mtimes to the
        second (ext3, HFS+) or two seconds (FAT), so an ELF written moments
        after the build began can carry a timestamp rounded below it. Refusing
        that would turn this guard into a false alarm on exactly the fresh
        build it is meant to accept.
        """

        with tempfile.TemporaryDirectory() as tmp:
            started = time.time()
            _assert_fresh(self._location(tmp, started - 1.5), started)

    def test_an_elf_older_than_the_build_is_refused(self: "TestFreshnessGuard") -> None:
        with tempfile.TemporaryDirectory() as tmp:
            location = self._location(tmp, time.time() - 5 * 86400)
            with self.assertRaises(SystemExit) as caught:
                _assert_fresh(location, time.time())
            message = str(caught.exception)
            # Says what happened and by how much, so the next person does not
            # have to rediscover it from byte-identical reports.
            self.assertIn("predates it", message)
            self.assertIn("120.0 h", message)


if __name__ == "__main__":
    unittest.main()
