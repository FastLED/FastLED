import subprocess
import unittest
from pathlib import Path

from ci.elf import dump_symbol_sizes
from ci.paths import PROJECT_ROOT
from ci.tools import Tools, load_tools

HERE = Path(__file__).resolve().parent.absolute()
UNO = HERE / "uno"
OUTPUT = HERE / "output"
ELF_FILE = UNO / "firmware.elf"
BUILD_INFO_PATH = PROJECT_ROOT / ".build" / "uno" / "build_info.json"


class TestBinToElf(unittest.TestCase):

    @classmethod
    def setUpClass(cls) -> None:
        uno_build = PROJECT_ROOT / ".build" / "uno"
        print(f"Checking for Uno build in: {uno_build}")
        if not uno_build.exists():
            print("Uno build not found. Running compilation...")
            try:
                subprocess.run(
                    "uv run ci/ci-compile.py uno --examples Blink",
                    shell=True,
                    check=True,
                )
                print("Compilation completed successfully.")
            except subprocess.CalledProcessError as e:
                print(f"Error during compilation: {e}")
                raise

    def test_bin_to_elf_conversion(self) -> None:
        tools: Tools = load_tools(BUILD_INFO_PATH)
        msg = dump_symbol_sizes(tools.nm_path, tools.cpp_filt_path, ELF_FILE)
        print(msg)


if __name__ == "__main__":
    unittest.main()
