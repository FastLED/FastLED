import unittest
from pathlib import Path

from ci.map_dump import map_dump

HERE = Path(__file__).resolve().parent.absolute()
UNO = HERE / "uno"


class TestMapParser(unittest.TestCase):
    def test_map_parser(self):
        map_dump(UNO / "firmware.map")


if __name__ == "__main__":
    unittest.main()
