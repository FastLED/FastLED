#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""
Unit test file.
"""

import unittest


class ApiTester(unittest.TestCase):
    """Main tester class."""

    def test_build_all_examples(self) -> None:
        """Test command line interface (CLI)."""
        from fastled import Api, Test  # type: ignore

        with Api.server(auto_updates=True) as server:
            exception_map = Test.test_examples(host=server)
            if len(exception_map) > 0:
                exception: Exception
                msg: str = ""
                for example, exception in exception_map.items():
                    msg += f"Failed to compile example: {example}, error: {exception}\n"
                self.fail(msg)

            # self.assertEqual(0, len(out), f"Failed tests: {out}")


if __name__ == "__main__":
    unittest.main()
