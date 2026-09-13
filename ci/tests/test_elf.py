import json
import unittest
import warnings
from pathlib import Path

from running_process import CalledProcessError, RunningProcess

from ci.util.elf import dump_symbol_sizes
from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.paths import PROJECT_ROOT
from ci.util.tools import Tools, load_tools


HERE = Path(__file__).resolve().parent.absolute()
UNO = HERE / "uno"
OUTPUT = HERE / "output"
ELF_FILE = UNO / "firmware.elf"
BUILD_INFO_PATH = PROJECT_ROOT / ".build" / "fbuild" / "uno" / "build_info_Blink.json"
BUILD_INFO_PATH2 = PROJECT_ROOT / ".build" / "fbuild" / "uno" / "build_info.json"


def _toolchain_present(build_info_path: Path) -> bool:
    """True when the `nm` the build_info names is on disk."""
    if not build_info_path.exists():
        return False
    try:
        data = json.loads(build_info_path.read_text())
        board_info = data[next(iter(data))]
        nm = board_info["aliases"]["nm"]
    except (KeyError, StopIteration, ValueError, TypeError):
        return False
    return isinstance(nm, str) and Path(nm).exists()


def init() -> None:
    uno_build = BUILD_INFO_PATH.parent
    print(f"Checking for Uno build in: {uno_build}")
    if not _toolchain_present(BUILD_INFO_PATH):
        print("Uno build not found. Running compilation...")
        try:
            RunningProcess.run(
                "uv run python -m ci.ci-compile uno --examples Blink",
                shell=True,
                check=True,
                cwd=str(PROJECT_ROOT),
            )
            print("Compilation completed successfully.")
        except CalledProcessError as e:
            print(f"Error during compilation: {e}")
            raise


class TestBinToElf(unittest.TestCase):
    def test_bin_to_elf_conversion(self) -> None:
        # Skip test if the UNO build metadata is missing. The board directory
        # can exist without a finished build (a staged project, an aborted
        # compile), so key off the metadata file rather than the directory.
        build_info = next(
            (p for p in (BUILD_INFO_PATH, BUILD_INFO_PATH2) if p.is_file()), None
        )
        if build_info is None:
            warnings.warn(
                "Skipping TestBinToElf::test_bin_to_elf_conversion because no uno "
                f"build_info was found in {BUILD_INFO_PATH.parent}. "
                "Run 'bash compile uno --examples Blink' to generate it."
            )
            self.skipTest("uno build_info missing; skipping ELF conversion test")
        tools: Tools
        try:
            tools = load_tools(build_info)
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        msg = dump_symbol_sizes(tools.nm_path, tools.cpp_filt_path, ELF_FILE)
        print(msg)


if __name__ == "__main__":
    unittest.main()
