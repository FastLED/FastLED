import unittest

from ci.boards import SPARKFUN_XRP_CONTROLLER_2350B, Board


class TestBoardToPlatformioIni(unittest.TestCase):
    """Tests for Board.to_platformio_ini().

    Every Board built here passes add_board_to_all=False. Board.__post_init__
    appends to the module-level ci.boards.ALL registry by default, so a fixture
    board constructed without it leaks into that list for the rest of the pytest
    process -- which made test_readme_badge_wall fail with a stray "custom"
    alias when the two files ran in the same session, and pass when either ran
    alone.
    """

    def _ini_to_set(self, ini: str) -> set[str]:
        """Return a set with each non-empty, stripped line of the ini snippet."""
        return {line.strip() for line in ini.splitlines() if line.strip()}

    def test_basic_fields(self) -> None:
        board = Board(
            board_name="uno",
            platform="atmelavr",
            framework="arduino",
            add_board_to_all=False,
        )
        ini = board.to_platformio_ini()
        lines = self._ini_to_set(ini)
        expected = {
            "[env:uno]",
            "board = uno",
            "platform = atmelavr",
            "framework = arduino",
        }
        self.assertTrue(expected.issubset(lines))
        # Should not reference internal attributes
        self.assertNotIn("platform_needs_install", ini)

    def test_real_board_name(self) -> None:
        board = Board(
            board_name="esp32c3",
            real_board_name="esp32-c3-devkitm-1",
            platform="espressif32",
            add_board_to_all=False,
        )
        ini = board.to_platformio_ini()
        lines = self._ini_to_set(ini)
        self.assertIn("[env:esp32c3]", lines)
        self.assertIn("board = esp32-c3-devkitm-1", lines)

    def test_flags(self) -> None:
        board = Board(
            board_name="custom",
            defines=["FASTLED_TEST=1"],
            build_flags=["-O2"],
            add_board_to_all=False,
        )
        ini = board.to_platformio_ini()
        lines = self._ini_to_set(ini)
        # The build_flags are in multi-line format - check that both flags are present as separate lines
        self.assertIn("build_flags =", lines)
        self.assertIn("-DFASTLED_TEST=1", lines)
        self.assertIn("-O2", lines)

    def test_lib_deps_are_merged_into_one_option(self) -> None:
        board = Board(
            board_name="custom", lib_deps=["board-lib"], add_board_to_all=False
        )

        ini = board.to_platformio_ini(project_root=".", additional_libs=["extra-lib"])

        self.assertEqual(ini.count("lib_deps ="), 1)
        self.assertIn("lib_deps = board-lib,extra-lib", ini)

    def test_sparkfun_xrp_uses_supported_arduino_pico_framework(
        self: "TestBoardToPlatformioIni",
    ) -> None:
        ini = SPARKFUN_XRP_CONTROLLER_2350B.to_platformio_ini()

        self.assertIn("framework-arduinopico", ini)
        self.assertIn("rp2040-5.7.0.zip", ini)
        self.assertIn("board_build.core = earlephilhower", ini)


if __name__ == "__main__":
    unittest.main()
