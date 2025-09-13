import json
import subprocess
import threading
import time
import unittest
from pathlib import Path

import pytest


# OPTIMIZED: Disabled by default to avoid expensive imports during test discovery
_ENABLED = True

from ci.util.paths import PROJECT_ROOT
from ci.util.symbol_analysis import (
    SymbolInfo,
    analyze_map_file,
    analyze_symbols,
    build_reverse_call_graph,
    find_board_build_info,
    generate_report,
)
from ci.util.tools import Tools, load_tools


HERE = Path(__file__).resolve().parent.absolute()
UNO = HERE / "uno"
OUTPUT = HERE / "output"
ELF_FILE = UNO / "firmware.elf"
BUILD_INFO_PATH = (
    PROJECT_ROOT / ".build" / "fled" / "examples" / "uno" / "build_info.json"
)


PLATFORMIO_PATH = Path.home() / ".platformio"
PLATFORMIO_PACKAGES_PATH = PLATFORMIO_PATH / "packages"
TOOLCHAIN_AVR = PLATFORMIO_PACKAGES_PATH / "toolchain-atmelavr"

# Global lock to prevent multiple threads from running compilation simultaneously
_compilation_lock = threading.Lock()
_compilation_done = False


def init() -> None:
    global _compilation_done

    # Use lock to ensure only one thread runs compilation
    with _compilation_lock:
        if _compilation_done:
            print("Compilation already completed by another thread, skipping.")
            return

        uno_build = PROJECT_ROOT / ".build" / "uno"
        print(
            f"Thread {threading.current_thread().ident}: Checking for Uno build in: {uno_build}"
        )
        print(f"BUILD_INFO_PATH: {BUILD_INFO_PATH}")
        print(f"TOOLCHAIN_AVR: {TOOLCHAIN_AVR}")
        print(f"BUILD_INFO_PATH exists: {BUILD_INFO_PATH.exists()}")
        print(f"TOOLCHAIN_AVR exists: {TOOLCHAIN_AVR.exists()}")

        if not BUILD_INFO_PATH.exists() or not TOOLCHAIN_AVR.exists():
            print("Uno build not found. Running compilation...")
            print(f"Working directory: {PROJECT_ROOT}")
            try:
                print(
                    "Starting compilation command: uv run python-m ci.ci-compile uno --examples Blink"
                )
                start_time = time.time()
                result = subprocess.run(
                    "uv run python -m ci.ci-compile uno --examples Blink",
                    shell=True,
                    check=True,
                    cwd=str(PROJECT_ROOT),
                    capture_output=True,
                    text=True,
                )
                end_time = time.time()
                print(
                    f"Compilation completed successfully in {end_time - start_time:.2f} seconds."
                )
                print(f"STDOUT: {result.stdout}")
                if result.stderr:
                    print(f"STDERR: {result.stderr}")
                _compilation_done = True
            except subprocess.CalledProcessError as e:
                print(f"Error during compilation (returncode: {e.returncode}): {e}")
                if e.stdout:
                    print(f"STDOUT: {e.stdout}")
                if e.stderr:
                    print(f"STDERR: {e.stderr}")
                raise
        else:
            print("Uno build found, skipping compilation.")
            _compilation_done = True


@pytest.mark.full
class TestSymbolAnalysis(unittest.TestCase):
    @classmethod
    @unittest.skipUnless(_ENABLED, "Tests disabled - set _ENABLED = True to run")
    def setUpClass(cls):
        """Set up test fixtures before running tests."""
        if not _ENABLED:
            return
        init()
        # Import Tools dynamically to avoid import errors when disabled
        from ci.util.tools import Tools, load_tools

        cls.tools: Tools = load_tools(BUILD_INFO_PATH)

        # Load build info for testing
        with open(BUILD_INFO_PATH) as f:
            cls.build_info = json.load(f)

        # Get the board key (should be 'uno' for UNO board)
        cls.board_key = "uno"
        if cls.board_key not in cls.build_info:
            # Fallback to first available key
            cls.board_key = list(cls.build_info.keys())[0]

        cls.board_info = cls.build_info[cls.board_key]

    @unittest.skipUnless(_ENABLED, "Tests disabled - set _ENABLED = True to run")
    def test_analyze_symbols_basic(self) -> None:
        """Test basic symbol analysis functionality."""
        print("Testing basic symbol analysis...")

        # Test with the test ELF file
        symbols = analyze_symbols(
            str(ELF_FILE), str(self.tools.nm_path), str(self.tools.cpp_filt_path)
        )

        # Verify we got some symbols
        self.assertGreater(len(symbols), 0, "Should find some symbols in ELF file")

        # Verify symbol structure
        symbols_with_size = 0
        for symbol in symbols:
            self.assertIsInstance(symbol.address, str)
            self.assertIsInstance(symbol.size, int)
            self.assertIsInstance(symbol.type, str)
            self.assertIsInstance(symbol.name, str)
            self.assertIsInstance(symbol.demangled_name, str)
            self.assertGreaterEqual(
                symbol.size, 0, "Symbol size should be non-negative"
            )
            if symbol.size > 0:
                symbols_with_size += 1

        # Ensure we have at least some symbols with actual size (not all zero-size)
        self.assertGreater(
            symbols_with_size, 0, "Should have at least some symbols with positive size"
        )

        print(f"Found {len(symbols)} symbols")

        # Check that we have some common symbols we'd expect in a compiled program
        symbol_names = [s.demangled_name for s in symbols]

        # Should have main function
        main_symbols = [name for name in symbol_names if "main" in name.lower()]
        self.assertGreater(len(main_symbols), 0, "Should find main function")

        print(f"Sample symbols: {symbol_names[:5]}")

    @unittest.skipUnless(_ENABLED, "Tests disabled - set _ENABLED = True to run")
    def test_analyze_symbols_from_build_info(self) -> None:
        """Test symbol analysis using actual build info paths."""
        print("Testing symbol analysis with build_info.json paths...")

        # Get paths from build info
        nm_path = self.board_info["aliases"]["nm"]
        cppfilt_path = self.board_info["aliases"]["c++filt"]
        elf_file = self.board_info["prog_path"]

        # If the ELF file from build_info doesn't exist, try the actual build location
        if not Path(elf_file).exists():
            # Fallback to the actual ELF file location
            actual_elf_file = (
                PROJECT_ROOT
                / ".build"
                / "fled"
                / "examples"
                / "uno"
                / "Blink"
                / ".pio"
                / "build"
                / "uno"
                / "firmware.elf"
            )
            if actual_elf_file.exists():
                elf_file = str(actual_elf_file)
                print(f"Using actual ELF file location: {elf_file}")

        # Verify the ELF file exists
        self.assertTrue(Path(elf_file).exists(), f"ELF file should exist: {elf_file}")

        # Run analysis
        symbols = analyze_symbols(elf_file, nm_path, cppfilt_path)

        # Verify results
        self.assertGreater(len(symbols), 0, "Should find symbols")

        # Check that we have symbols with reasonable sizes
        sizes = [s.size for s in symbols]
        symbols_with_size = sum(1 for size in sizes if size > 0)
        self.assertGreater(
            symbols_with_size, 0, "Should have at least some symbols with positive size"
        )
        # All sizes should be non-negative (zero-size symbols are valid for undefined symbols, labels, etc.)
        self.assertTrue(
            all(size >= 0 for size in sizes),
            "All symbols should have non-negative sizes",
        )

        print(f"Analyzed {len(symbols)} symbols from build ELF: {elf_file}")

    @unittest.skipUnless(_ENABLED, "Tests disabled - set _ENABLED = True to run")
    def test_generate_report_basic(self) -> None:
        """Test report generation functionality."""
        print("Testing basic report generation...")

        # Create some test symbols using SymbolInfo dataclass
        test_symbols = [
            SymbolInfo(
                address="0x1000",
                size=1000,
                type="T",
                name="test_function_1",
                demangled_name="test_function_1()",
                source="test",
            ),
            SymbolInfo(
                address="0x2000",
                size=500,
                type="T",
                name="_Z12test_func_2v",
                demangled_name="test_function_2()",
                source="test",
            ),
            SymbolInfo(
                address="0x3000",
                size=200,
                type="D",
                name="test_data",
                demangled_name="test_data",
                source="test",
            ),
        ]

        test_dependencies = {"test_module.o": ["test_function_1", "test_data"]}

        # Generate report
        report = generate_report("TEST_BOARD", test_symbols, test_dependencies)

        # Verify report structure - use dataclass field access
        self.assertIsInstance(report.board, str)
        self.assertIsInstance(report.total_symbols, int)
        self.assertIsInstance(report.total_size, int)
        self.assertIsInstance(report.largest_symbols, list)
        self.assertIsInstance(report.type_breakdown, list)
        self.assertIsInstance(report.dependencies, dict)

        # Verify values
        self.assertEqual(report.board, "TEST_BOARD")
        self.assertEqual(report.total_symbols, 3)
        self.assertEqual(report.total_size, 1700)  # 1000 + 500 + 200

        # Verify type breakdown - it's now a list of TypeBreakdown dataclasses
        type_breakdown_dict = {tb.type: tb for tb in report.type_breakdown}
        self.assertIn("T", type_breakdown_dict)
        self.assertIn("D", type_breakdown_dict)
        self.assertEqual(type_breakdown_dict["T"].count, 2)
        self.assertEqual(type_breakdown_dict["D"].count, 1)

        print("Report generation test passed")

    @unittest.skipUnless(_ENABLED, "Tests disabled - set _ENABLED = True to run")
    def test_find_board_build_info(self) -> None:
        """Test the board build info detection functionality."""
        print("Testing board build info detection...")

        # Test finding UNO board specifically
        try:
            build_info_path, board_name = find_board_build_info("uno")
            self.assertEqual(board_name, "uno")
            self.assertTrue(build_info_path.exists())
            self.assertEqual(build_info_path.name, "build_info.json")
            print(f"Found UNO build info: {build_info_path}")
        except SystemExit:
            self.skipTest("UNO build not available for testing")

        # Test auto-detection (should find any available board)
        try:
            build_info_path, board_name = find_board_build_info(None)
            self.assertTrue(build_info_path.exists())
            self.assertEqual(build_info_path.name, "build_info.json")
            print(f"Auto-detected board: {board_name} at {build_info_path}")
        except SystemExit:
            self.skipTest("No builds available for auto-detection testing")

    @unittest.skipUnless(_ENABLED, "Tests disabled - set _ENABLED = True to run")
    def test_analyze_map_file(self) -> None:
        """Test map file analysis if available."""
        print("Testing map file analysis...")

        # Try to find a map file
        elf_file_path = Path(self.board_info["prog_path"])
        map_file = elf_file_path.with_suffix(".map")

        if not map_file.exists():
            print(f"Map file not found at {map_file}, skipping map analysis test")
            return

        # Analyze the map file
        dependencies = analyze_map_file(map_file)

        # Verify result structure
        self.assertIsInstance(dependencies, dict)

        if dependencies:
            print(f"Found {len(dependencies)} modules in map file")
            # Verify structure of dependencies
            for module, symbols in dependencies.items():
                self.assertIsInstance(module, str)
                self.assertIsInstance(symbols, list)

            # Print a sample for debugging
            sample_modules = list(dependencies.keys())[:3]
            for module in sample_modules:
                print(f"  {module}: {len(dependencies[module])} symbols")
        else:
            print("No dependencies found in map file (this may be normal)")

    @unittest.skipUnless(_ENABLED, "Tests disabled - set _ENABLED = True to run")
    def test_build_reverse_call_graph(self) -> None:
        """Test reverse call graph building."""
        print("Testing reverse call graph building...")

        # Create test call graph
        test_call_graph = {
            "function_a": ["function_b", "function_c"],
            "function_b": ["function_c", "function_d"],
            "function_c": ["function_d"],
        }

        # Build reverse call graph
        reverse_graph = build_reverse_call_graph(test_call_graph)

        # Verify structure
        expected_reverse = {
            "function_b": ["function_a"],
            "function_c": ["function_a", "function_b"],
            "function_d": ["function_b", "function_c"],
        }

        self.assertEqual(reverse_graph, expected_reverse)
        print("Reverse call graph building test passed")

    @unittest.skipUnless(_ENABLED, "Tests disabled - set _ENABLED = True to run")
    def test_full_symbol_analysis_workflow(self) -> None:
        """Test the complete symbol analysis workflow."""
        print("Testing complete symbol analysis workflow...")

        # Run full analysis on actual build
        elf_file = self.board_info["prog_path"]

        # If the ELF file from build_info doesn't exist, try the actual build location
        if not Path(elf_file).exists():
            # Fallback to the actual ELF file location
            actual_elf_file = (
                PROJECT_ROOT
                / ".build"
                / "fled"
                / "examples"
                / "uno"
                / "Blink"
                / ".pio"
                / "build"
                / "uno"
                / "firmware.elf"
            )
            if actual_elf_file.exists():
                elf_file = str(actual_elf_file)
                print(f"Using actual ELF file location: {elf_file}")

        nm_path = self.board_info["aliases"]["nm"]
        cppfilt_path = self.board_info["aliases"]["c++filt"]

        # Analyze symbols
        symbols = analyze_symbols(elf_file, nm_path, cppfilt_path)

        # Analyze map file if available
        map_file = Path(elf_file).with_suffix(".map")
        dependencies = analyze_map_file(map_file)

        # Generate report
        report = generate_report("UNO", symbols, dependencies)

        # Verify the complete workflow produced valid results - use dataclass field access
        self.assertGreater(report.total_symbols, 0)
        self.assertGreater(report.total_size, 0)
        self.assertGreater(len(report.largest_symbols), 0)
        self.assertGreater(len(report.type_breakdown), 0)

        # Print summary for verification
        print("Complete analysis results:")
        print(f"  Board: {report.board}")
        print(f"  Total symbols: {report.total_symbols}")
        print(
            f"  Total size: {report.total_size} bytes ({report.total_size / 1024:.1f} KB)"
        )
        print(
            f"  Largest symbol: {report.largest_symbols[0].demangled_name} ({report.largest_symbols[0].size} bytes)"
        )

        # Verify we have expected symbol types for a typical embedded program
        type_breakdown_dict = {tb.type: tb for tb in report.type_breakdown}
        self.assertIn("T", type_breakdown_dict, "Should have text/code symbols")

        print("Full workflow test completed successfully")


if __name__ == "__main__":
    unittest.main()
