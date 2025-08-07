import unittest
from pathlib import Path

from ci.util.map_dump import map_dump


HERE = Path(__file__).resolve().parent.absolute()
UNO = HERE / "uno"


class TestMapParser(unittest.TestCase):
    def test_map_parser(self):
        # If the UNO fixture directory is missing, skip with a warning
        if not UNO.exists():
            import warnings

            warnings.warn(
                "Skipping TestMapParser::test_map_parser because ci/tests/uno is missing."
            )
            self.skipTest("ci/tests/uno missing; skipping map parser test")
        map_dump(UNO / "firmware.map")


if __name__ == "__main__":
    unittest.main()
