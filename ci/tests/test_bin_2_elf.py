import subprocess
import unittest
import warnings
from pathlib import Path

from ci.util.bin_2_elf import bin_to_elf
from ci.util.elf import dump_symbol_sizes
from ci.util.paths import PROJECT_ROOT
from ci.util.tools import Tools, load_tools


HERE = Path(__file__).resolve().parent.absolute()
UNO = HERE / "uno"
OUTPUT = HERE / "output"


BUILD_INFO_PATH = PROJECT_ROOT / ".build" / "examples" / "uno" / "build_info.json"
BUILD_INFO_PATH2 = (
    PROJECT_ROOT / ".build" / "fled" / "examples" / "uno" / "build_info.json"
)

DISABLED = True


class TestBinToElf(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if DISABLED:
            return
        uno_build = PROJECT_ROOT / ".build" / "uno"
        print(f"Checking for Uno build in: {uno_build}")
        if not uno_build.exists():
            print("Uno build not found. Running compilation...")
            try:
                subprocess.run(
                    "uv run python -m ci.ci-compile uno --examples Blink",
                    shell=True,
                    check=True,
                )
                print("Compilation completed successfully.")
            except subprocess.CalledProcessError as e:
                print(f"Error during compilation: {e}")
                raise

    @unittest.skip("Skip bin to elf conversion test")
    def test_bin_to_elf_conversion(self) -> None:
        if DISABLED:
            return
        tools: Tools
        try:
            tools = load_tools(BUILD_INFO_PATH)
        except FileNotFoundError as e:
            warnings.warn(f"Error while loading tools: {e}")
            tools = load_tools(BUILD_INFO_PATH2)
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
