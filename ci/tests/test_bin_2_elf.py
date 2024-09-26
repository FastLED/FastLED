import subprocess
import sys
import unittest
from dataclasses import dataclass
from pathlib import Path

from ci.bin_2_elf import bin_to_elf
from ci.elf import print_symbol_sizes
from ci.paths import PROJECT_ROOT

HERE = Path(__file__).resolve().parent.absolute()
UNO = HERE / "uno"
OUTPUT = HERE / "output"


@dataclass
class Tools:
    as_path: Path
    ld_path: Path
    objcopy_path: Path
    objdump_path: Path
    cpp_filt_path: Path


def load_tools() -> Tools:
    as_path = UNO / "avr-as"
    ld_path = UNO / "avr-ld"
    objcopy_path = UNO / "avr-objcopy"
    objdump_path = UNO / "avr-objdump"
    cpp_filt_path = UNO / "avr-c++filt"
    if sys.platform == "win32":
        as_path = as_path.with_suffix(".exe")
        ld_path = ld_path.with_suffix(".exe")
        objcopy_path = objcopy_path.with_suffix(".exe")
        objdump_path = objdump_path.with_suffix(".exe")
        cpp_filt_path = cpp_filt_path.with_suffix(".exe")
    out = Tools(as_path, ld_path, objcopy_path, objdump_path, cpp_filt_path)
    tools = [as_path, ld_path, objcopy_path, objdump_path, cpp_filt_path]
    for tool in tools:
        if not tool.exists():
            raise FileNotFoundError(f"Tool not found: {tool}")
    return out


TOOLS = load_tools()


class TestBinToElf(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        uno_build = PROJECT_ROOT / ".build" / "uno"
        print(f"Checking for Uno build in: {uno_build}")
        if not uno_build.exists():
            print("Uno build not found. Running compilation...")
            try:
                subprocess.run(
                    "uv run ci/compile.py uno --examples Blink", shell=True, check=True
                )
                print("Compilation completed successfully.")
            except subprocess.CalledProcessError as e:
                print(f"Error during compilation: {e}")
                raise

    def test_bin_to_elf_conversion(self):

        bin_file = UNO / "firmware.hex"
        map_file = UNO / "firmware.map"
        output_elf = OUTPUT / "output.elf"
        bin_to_elf(
            bin_file,
            map_file,
            TOOLS.as_path,
            TOOLS.ld_path,
            TOOLS.objcopy_path,
            output_elf,
        )
        print_symbol_sizes(TOOLS.objdump_path, TOOLS.cpp_filt_path, output_elf)


if __name__ == "__main__":
    unittest.main()
