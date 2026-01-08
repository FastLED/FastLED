#!/usr/bin/env python3
"""
Test suite for IWYU (Include-What-You-Use) integration.

This tests that the IWYU tool is properly integrated into the build system
and can be invoked correctly.
"""

import subprocess
import sys
import unittest
from pathlib import Path


class TestIWYUIntegration(unittest.TestCase):
    """Test IWYU integration in the FastLED project"""

    def test_iwyu_availability(self):
        """Test that IWYU is available via check_iwyu_available()"""
        # Import the function from ci-iwyu by loading the module directly
        import importlib.util

        ci_iwyu_path = Path(__file__).parent / "ci-iwyu.py"
        spec = importlib.util.spec_from_file_location("ci_iwyu", ci_iwyu_path)
        if spec is None or spec.loader is None:
            self.fail("Failed to load ci-iwyu.py module")
            return

        ci_iwyu = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(ci_iwyu)

        is_available, prefix = ci_iwyu.check_iwyu_available()

        # Assert that IWYU is available (either system or clang-tool-chain)
        self.assertTrue(
            is_available,
            "IWYU should be available either as system command or via clang-tool-chain",
        )

        # Check that prefix is valid (empty string or "uv run ")
        self.assertIn(prefix, ["", "uv run "], f"Unexpected IWYU prefix: '{prefix}'")

        print(f"✅ IWYU is available with prefix: '{prefix}'")

    def test_ci_iwyu_script_exists(self):
        """Test that the ci-iwyu.py script exists"""
        ci_iwyu_path = Path(__file__).parent / "ci-iwyu.py"
        self.assertTrue(ci_iwyu_path.exists(), "ci/ci-iwyu.py script should exist")
        print(f"✅ Found ci-iwyu.py at {ci_iwyu_path}")

    def test_iwyu_mapping_files_exist(self):
        """Test that IWYU mapping files exist"""
        ci_dir = Path(__file__).parent
        iwyu_dir = ci_dir / "iwyu"

        fastled_imp = iwyu_dir / "fastled.imp"
        stdlib_imp = iwyu_dir / "stdlib.imp"

        self.assertTrue(fastled_imp.exists(), "FastLED IWYU mapping file should exist")
        self.assertTrue(stdlib_imp.exists(), "stdlib IWYU mapping file should exist")

        print(f"✅ Found mapping files: {fastled_imp.name}, {stdlib_imp.name}")


def main():
    """Run the tests"""
    # Run unittest with verbose output
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromTestCase(TestIWYUIntegration)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    # Return 0 for success, 1 for failure
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
