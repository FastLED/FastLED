"""`resolve_firmware_elf` picks the ELF a build actually wrote. FastLED#4402.

`find_fbuild_elf` is covered by `test_compiled_size_fbuild.py`, which is where
it used to live. These cover the layer the binary-size diagnostics added on top
of it: the fallback to `prog_path` for PlatformIO builds, and returning None
instead of a path that does not exist.

The last one is the whole point. `ci/inspect_binary.py`, `ci/inspect_elf.py`
and `ci/util/symbol_analysis.py` used to read `prog_path` and hand it to
`readelf`, `nm` and `objdump` in turn. On an fbuild board that path is never
written, so each tool failed separately and the symbol report came out empty --
under `continue-on-error: true`, so the log showed a wall of tool errors and a
report of `Total symbols: 0` while the actual ELF sat under `.fbuild/`.
"""

from __future__ import annotations

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from ci.util.firmware_elf import find_fbuild_elf, resolve_firmware_elf


class TestResolveFirmwareElf(unittest.TestCase):
    def setUp(self: "TestResolveFirmwareElf") -> None:
        self._tmp = TemporaryDirectory()
        self.build_dir = Path(self._tmp.name)

    def tearDown(self: "TestResolveFirmwareElf") -> None:
        self._tmp.cleanup()

    def _write(self: "TestResolveFirmwareElf", relative: str) -> Path:
        path = self.build_dir / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"\x7fELF")
        return path

    def test_the_fbuild_artifact_wins_over_a_stale_prog_path(
        self: "TestResolveFirmwareElf",
    ) -> None:
        # The esp32dev case: `prog_path` names a PlatformIO location that the
        # fbuild build never wrote, and the real ELF is under `.fbuild/`.
        elf = self._write(".fbuild/build/esp32dev/release/firmware.elf")
        board_info = {
            "prog_path": str(
                self.build_dir / ".pio" / "build" / "esp32dev" / "firmware.elf"
            )
        }
        self.assertEqual(resolve_firmware_elf(board_info, self.build_dir), elf)

    def test_prog_path_is_used_when_there_is_no_fbuild_build(
        self: "TestResolveFirmwareElf",
    ) -> None:
        # A PlatformIO-driven board must keep working exactly as before.
        elf = self._write(".pio/build/uno/firmware.elf")
        board_info = {"prog_path": str(elf)}
        self.assertIsNone(find_fbuild_elf(board_info, self.build_dir))
        self.assertEqual(resolve_firmware_elf(board_info, self.build_dir), elf)

    def test_a_bin_prog_path_falls_back_to_its_elf_sibling(
        self: "TestResolveFirmwareElf",
    ) -> None:
        elf = self._write(".pio/build/uno/firmware.elf")
        board_info = {"prog_path": str(elf.with_suffix(".bin"))}
        self.assertEqual(resolve_firmware_elf(board_info, self.build_dir), elf)

    def test_nothing_on_disk_returns_none_rather_than_a_bad_path(
        self: "TestResolveFirmwareElf",
    ) -> None:
        # Returning the non-existent path is what produced a wall of separate
        # tool failures and an empty report instead of one clear message.
        board_info = {
            "prog_path": str(self.build_dir / ".pio" / "build" / "x" / "firmware.elf")
        }
        self.assertIsNone(resolve_firmware_elf(board_info, self.build_dir))

    def test_a_missing_prog_path_key_is_not_an_exception(
        self: "TestResolveFirmwareElf",
    ) -> None:
        self.assertIsNone(resolve_firmware_elf({}, self.build_dir))
        elf = self._write(".fbuild/build/release/firmware.elf")
        self.assertEqual(resolve_firmware_elf({}, self.build_dir), elf)

    def test_the_newest_fbuild_layout_wins(self: "TestResolveFirmwareElf") -> None:
        import os
        import time

        old = self._write(".fbuild/build/release/firmware.elf")
        new = self._write(".fbuild/build/esp32dev/release/firmware.elf")
        # Explicit mtimes rather than write order: a same-second filesystem
        # makes write order prove nothing.
        now = time.time()
        os.utime(old, (now - 100, now - 100))
        os.utime(new, (now, now))
        self.assertEqual(resolve_firmware_elf({}, self.build_dir), new)

        os.utime(old, (now, now))
        os.utime(new, (now - 100, now - 100))
        self.assertEqual(resolve_firmware_elf({}, self.build_dir), old)


if __name__ == "__main__":
    unittest.main()
