import unittest

from ci.boards import ALL, SPARKFUN_XRP_CONTROLLER_2350B, Board
from ci.compiler.build_config import _apply_esp_release_exception_policy
from ci.compiler.project_ini import ProjectIni


class TestBoardToProjectIni(unittest.TestCase):
    """Tests for Board.to_project_ini().

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
        ini = board.to_project_ini()
        lines = self._ini_to_set(ini)
        expected = {
            "[env:uno]",
            "board = uno",
            "platform = atmelavr",
            "framework = arduino",
        }
        self.assertTrue(expected.issubset(lines))
        # Should not reference internal attributes
        self.assertNotIn("add_board_to_all", ini)

    def test_real_board_name(self) -> None:
        board = Board(
            board_name="esp32c3",
            real_board_name="esp32-c3-devkitm-1",
            platform="espressif32",
            add_board_to_all=False,
        )
        ini = board.to_project_ini()
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
        ini = board.to_project_ini()
        lines = self._ini_to_set(ini)
        # The build_flags are in multi-line format - check that both flags are present as separate lines
        self.assertIn("build_flags =", lines)
        self.assertIn("-DFASTLED_TEST=1", lines)
        self.assertIn("-O2", lines)

    def test_lib_deps_are_merged_into_one_option(self) -> None:
        board = Board(
            board_name="custom", lib_deps=["board-lib"], add_board_to_all=False
        )

        ini = board.to_project_ini(project_root=".", additional_libs=["extra-lib"])

        self.assertEqual(ini.count("lib_deps ="), 1)
        self.assertIn("lib_deps = board-lib,extra-lib", ini)

    def test_sparkfun_xrp_uses_supported_arduino_pico_framework(
        self: "TestBoardToProjectIni",
    ) -> None:
        ini = SPARKFUN_XRP_CONTROLLER_2350B.to_project_ini()

        self.assertIn("framework-arduinopico", ini)
        self.assertIn("rp2040-5.7.0.zip", ini)
        self.assertIn("board_build.core = earlephilhower", ini)


class TestEspReleaseExceptionPolicy(unittest.TestCase):
    """#4773: release staging and system debug policy remain distinct."""

    def test_registered_esp_release_profiles_disable_cpp_exceptions(self) -> None:
        esp_boards = [b for b in ALL if b.platform_family in {"esp32", "esp8266"}]
        self.assertTrue(esp_boards)
        for board in esp_boards:
            with self.subTest(board=board.board_name):
                project = ProjectIni.parseString(board.to_project_ini())
                original = project.get_build_flags(board.board_name)
                _apply_esp_release_exception_policy(board, project)
                self.assertEqual(
                    project.get_build_flags(board.board_name),
                    original + ["-fno-exceptions"],
                )

    def test_explicit_and_inherited_debug_policy_is_preserved(self) -> None:
        board = Board(
            board_name="debug-probe", platform="espressif32", add_board_to_all=False
        )
        for scope in ("env", "env:debug-probe"):
            with self.subTest(scope=scope):
                project = ProjectIni.parseString(board.to_project_ini())
                project.set_option(scope, "build_type", "debug")
                flags = ["-fexceptions", "-funwind-tables", "-DDEBUG=1"]
                project.set_build_flags(board.board_name, flags)
                _apply_esp_release_exception_policy(board, project)
                self.assertEqual(project.get_build_flags(board.board_name), flags)
                self.assertEqual(project.get_option(scope, "build_type"), "debug")

    def test_release_retains_sdk_unwind_flags_and_wins_exception_order(self) -> None:
        board = Board(
            board_name="release-probe",
            platform="espressif8266",
            build_flags=["-funwind-tables", "-fexceptions"],
            add_board_to_all=False,
        )
        project = ProjectIni.parseString(board.to_project_ini())
        project.set_option("env:release-probe", "build_type", "release")
        _apply_esp_release_exception_policy(board, project)
        self.assertEqual(
            project.get_build_flags(board.board_name),
            ["-funwind-tables", "-fexceptions", "-fno-exceptions"],
        )

    def test_release_preserves_inherited_diagnostics_and_local_precedence(self) -> None:
        board = Board(
            board_name="inherit-probe", platform="espressif32", add_board_to_all=False
        )
        inherited = ["-funwind-tables", "-DCONFIG_ESP_SYSTEM_PANIC_PRINT_HALT=1"]
        for local in (None, [], ["-fexceptions", "-DCORE_DEBUG_LEVEL=5"]):
            with self.subTest(local=local):
                project = ProjectIni.parseString(board.to_project_ini())
                project.set_option("env", "build_flags", "\n".join(inherited))
                if local is not None:
                    project.set_build_flags(board.board_name, local)
                _apply_esp_release_exception_policy(board, project)
                expected = inherited if local is None else local
                self.assertEqual(
                    project.get_build_flags(board.board_name),
                    expected + ["-fno-exceptions"],
                )
                self.assertEqual(
                    project.get_option("env", "build_flags"), "\n".join(inherited)
                )

    def test_non_esp_release_flags_are_preserved(self) -> None:
        board = Board(
            board_name="host-probe", platform="native", add_board_to_all=False
        )
        project = ProjectIni.parseString(board.to_project_ini())
        project.set_build_flags(board.board_name, ["-fexceptions"])
        _apply_esp_release_exception_policy(board, project)
        self.assertEqual(project.get_build_flags(board.board_name), ["-fexceptions"])


if __name__ == "__main__":
    unittest.main()
