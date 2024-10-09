import subprocess
import unittest
import warnings
from pathlib import Path

from ci.bin_2_elf import bin_to_elf
from ci.elf import dump_symbol_sizes
from ci.paths import PROJECT_ROOT
from ci.tools import Tools, load_tools

HERE = Path(__file__).resolve().parent.absolute()
UNO = HERE / "uno"
OUTPUT = HERE / "output"


BUILD_INFO_PATH = PROJECT_ROOT / ".build" / "uno" / "build_info.json"


class TestBinToElf(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
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

    @unittest.skip("Skip bin to elf conversion test")
    def test_bin_to_elf_conversion(self) -> None:
        tools: Tools = load_tools(BUILD_INFO_PATH)
        bin_file = UNO / "firmware.hex"
        map_file = UNO / "firmware.map"
        output_elf = OUTPUT / "output.elf"
        try:
            bin_to_elf(
                bin_file,
                map_file,
                tools.as_path,
                tools.ld_path,
                tools.objcopy_path,
                output_elf,
            )
            stdout = dump_symbol_sizes(tools.nm_path, tools.cpp_filt_path, output_elf)
            print(stdout)
        except Exception as e:
            warnings.warn(f"Error while converting binary to ELF: {e}")


if __name__ == "__main__":
    unittest.main()
