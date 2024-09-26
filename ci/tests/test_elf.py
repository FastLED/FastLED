import sys
import unittest
from dataclasses import dataclass
from pathlib import Path

from ci.elf import print_symbol_sizes

HERE = Path(__file__).resolve().parent.absolute()
UNO = HERE / "uno"
OUTPUT = HERE / "output"
ELF_FILE = UNO / "firmware.elf"


@dataclass
class Tools:
    as_path: Path
    ld_path: Path
    objcopy_path: Path
    objdump_path: Path


def load_tools() -> Tools:
    as_path = UNO / "avr-as"
    ld_path = UNO / "avr-ld"
    objcopy_path = UNO / "avr-objcopy"
    objdump_path = UNO / "avr-objdump"
    if sys.platform == "win32":
        as_path = as_path.with_suffix(".exe")
        ld_path = ld_path.with_suffix(".exe")
        objcopy_path = objcopy_path.with_suffix(".exe")
        objdump_path = objdump_path.with_suffix(".exe")
    out = Tools(as_path, ld_path, objcopy_path, objdump_path)
    tools = [as_path, ld_path, objcopy_path, objdump_path]
    for tool in tools:
        if not tool.exists():
            raise FileNotFoundError(f"Tool not found: {tool}")
    return out


TOOLS = load_tools()


class TestBinToElf(unittest.TestCase):
    def test_bin_to_elf_conversion(self):
        print_symbol_sizes(TOOLS.objdump_path, ELF_FILE)


if __name__ == "__main__":
    unittest.main()
