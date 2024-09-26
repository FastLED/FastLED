import sys
import unittest
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
UNO = HERE / "uno"


@dataclass
class Tools:
    as_path: Path
    ld_path: Path
    objcopy_path: Path
    objdump_path: Path


def load_tools() -> Tools:
    as_path = UNO / "bin" / "avr-as"
    ld_path = UNO / "bin" / "avr-ld"
    objcopy_path = UNO / "bin" / "avr-objcopy"
    objdump_path = UNO / "bin" / "avr-objdump"
    if sys.platform == "win32":
        as_path = as_path.with_suffix(".exe")
        ld_path = ld_path.with_suffix(".exe")
        objcopy_path = objcopy_path.with_suffix(".exe")
        objdump_path = objdump_path.with_suffix(".exe")
    return Tools(as_path, ld_path, objcopy_path, objdump_path)


TOOLS = load_tools()


class TestMapParser(unittest.TestCase):
    def test_parse_map_file(self):
        pass


if __name__ == "__main__":
    unittest.main()
