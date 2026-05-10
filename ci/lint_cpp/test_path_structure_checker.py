#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker to ensure test files mirror the directory structure of source files.

This checker validates that test files in tests/ follow the same directory
structure as their corresponding source files in src/. For example:
- If src/fl/stl/flat_map.h exists, the test should be at tests/fl/stl/flat_map.cpp
- If the test is at tests/fl/flat_map.cpp, this is flagged as an error

This ensures test organization matches source organization for maintainability.
"""

from pathlib import Path

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT
from tests.test_config import EXCLUDED_TEST_DIRS


TESTS_ROOT = PROJECT_ROOT / "tests"
SRC_ROOT = PROJECT_ROOT / "src"

# Test files that are exempt from path matching (infrastructure/entry points)
# Also includes unity-build aggregators and sub-test HPP files that are
# #include'd by parent .cpp files (no 1:1 source mapping expected).
EXCLUDED_TEST_FILES = {
    "doctest_main.cpp",  # Test framework entry point
    # Unity-build aggregators (include multiple .hpp sub-tests)
    "audio.cpp",  # Aggregates tests/fl/audio/*.hpp
    "codec.cpp",  # Aggregates tests/fl/codec/*.hpp
    "log.cpp",  # Aggregates tests/fl/log/*.hpp
    "detectors.cpp",  # Aggregates tests/fl/audio/detector/*.hpp
    "encoders.cpp",  # Aggregates tests/fl/chipsets/encoders/*.hpp
    "2d.cpp",  # Aggregates tests/fl/fx/2d/*.hpp
    "validation.cpp",  # Aggregates tests/fl/channels/detail/validation/*.hpp
    # Sub-test HPP files included by parent CPP (gfx/ primitives → gfx.cpp)
    "draw_ring.hpp",
    "draw_thick_line.hpp",
    "draw_line.hpp",
    "draw_disc.hpp",
    "draw_disc_16.hpp",
    "perf_primitives.hpp",
    # Sub-test HPP files included by parent CPP (audio/ → audio.cpp)
    "gain.hpp",
    "test_helpers.hpp",
    "vocal_real_audio.hpp",  # → detectors.cpp (in detector/ dir, excluded as "detectors")
    # Sub-test HPP files included by parent CPP (misc locations)
    "map_range.hpp",  # → clamp.cpp
    "assume_aligned.hpp",  # → align.cpp
    "insert_result.hpp",  # → hash.cpp
    # Standalone tests that test cross-cutting functionality (no 1:1 source mapping)
    "active_strip_data_json.cpp",  # Tests JSON serialization of strip data
    "audio_url.cpp",  # Tests audio URL handling
    "bytestream.cpp",  # Tests pixel_stream/video byte streaming
    "clamp.cpp",  # Tests fl::clamp from fl/math/math.h
    "force_inline.cpp",  # Tests FASTLED_FORCE_INLINE from compiler_control.h
    "hsv2rgb_accuracy.cpp",  # Tests HSV→RGB conversion accuracy
    "noise_range.cpp",  # Tests noise function output ranges
    "noise_ring.cpp",  # Tests noise ring patterns
    "power_estimation.cpp",  # Tests power management estimation
    "slice.cpp",  # Tests span/slice operations
    "unused.cpp",  # Tests FASTLED_UNUSED macro
    # Channel tests (test channel subsystem, not individual headers)
    "channel_manager.cpp",  # Tests fl/channels/manager.h
    "spi_channel.cpp",  # Tests SPI channel integration
    "wave8_spi.cpp",  # Tests wave8 SPI encoding
    # Remote tests (test remote subsystem integration)
    "loopback.cpp",  # Tests remote loopback
    "rpc.cpp",  # Aggregates tests/fl/remote/rpc/*.hpp
    "rpc_http_stream.cpp",  # Tests RPC HTTP streaming
    # Audio sub-component tests
    "adversarial.cpp",  # Tests audio adversarial conditions
    "deficiencies.cpp",  # Tests audio processing deficiencies
    "sound_level_meter.cpp",  # Tests SoundLevelMeter
    # STL tests (test STL wrappers, not individual headers)
    "allocator_move.cpp",  # Tests allocator move semantics
    "cstdint.cpp",  # Tests cstddef/stdint compatibility
    "function_list.cpp",  # Tests function list patterns
    "strstream_integers.cpp",  # Tests strstream integer formatting
    # ASIO tests (test networking subsystem)
    "test_tcp_socket.cpp",  # Tests TCP socket
    "test_tcp_acceptor.cpp",  # Tests TCP acceptor
    "http_promise.cpp",  # Tests HTTP promise
    "http_transport.cpp",  # Tests HTTP transport
    "server_loopback.cpp",  # Tests HTTP server loopback
}


class TestPathStructureChecker(FileContentChecker):
    """Checker class for test file path structure validation."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for path structure validation."""
        # Only check files in tests directory
        if not file_path.startswith(str(TESTS_ROOT)):
            return False

        # Check .cpp and .hpp test files (sub-tests use .hpp extension)
        if not file_path.endswith((".cpp", ".hpp")):
            return False

        test_path = Path(file_path)

        # Skip excluded files (infrastructure/entry points)
        if test_path.name in EXCLUDED_TEST_FILES:
            return False

        # Skip tests/misc/ directory (these tests don't need to match source structure)
        # Skip tests/profile/ directory (standalone performance benchmarks)
        # Skip tests/shared/ directory (shared test infrastructure)
        # Skip any test_utils/ directories (test utilities)
        try:
            rel_path = test_path.relative_to(TESTS_ROOT)
            if rel_path.parts[0] in ("misc", "profile", "shared"):
                return False
            if "test_utils" in rel_path.parts:
                return False
        except (ValueError, IndexError):
            pass

        # Skip files in EXCLUDED_TEST_DIRS (consolidated sub-test directories
        # where .hpp files are #include'd from a parent .cpp test)
        for excluded_dir in EXCLUDED_TEST_DIRS:
            try:
                test_path.relative_to(excluded_dir)
                return False  # File is inside an excluded test directory
            except ValueError:
                continue

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check if test file path matches the source file directory structure.

        Rule: tests/**/file.cpp must match src/**/file.h or src/**/file.hpp
        Exception: Tests in tests/misc/ are exempt (don't need to match).
        Exception: Tests with '// ok standalone' comment are exempt.
        """
        test_path = Path(file_content.path)

        # Get the relative path from tests root: tests/fl/flat_map.cpp -> fl/flat_map
        rel_from_tests = test_path.relative_to(TESTS_ROOT)
        test_name_no_ext = rel_from_tests.with_suffix("")  # Remove .cpp

        # Check for source file at exact matching path
        # Check for .h, .hpp, or .cpp.hpp files (ESP32 implementations use .cpp.hpp)
        expected_source_h = SRC_ROOT / test_name_no_ext.with_suffix(".h")
        expected_source_hpp = SRC_ROOT / test_name_no_ext.with_suffix(".hpp")
        expected_source_cpp_hpp = SRC_ROOT / (str(test_name_no_ext) + ".cpp.hpp")

        # If matching source file exists at the expected location, no issue
        if (
            expected_source_h.exists()
            or expected_source_hpp.exists()
            or expected_source_cpp_hpp.exists()
        ):
            return []

        # Check if file has "// ok standalone" comment in first few lines
        for line in file_content.lines[:5]:  # Check first 5 lines
            if "// ok standalone" in line.lower():
                return []  # Exempt from path matching requirement

        # Source file doesn't exist at expected location
        # Flag as violation (test file has no corresponding source at matching path)
        rel_current_test = test_path.relative_to(PROJECT_ROOT)

        message = (
            f"Test file has no corresponding source file at matching path. "
            f"Test is at '{rel_current_test}' but no source file found at "
            f"'src/{rel_from_tests.with_suffix('.h')}', 'src/{rel_from_tests.with_suffix('.hpp')}', "
            f"or 'src/{rel_from_tests.with_name(rel_from_tests.stem + '.cpp.hpp')}'. "
            f"\n\n"
            f"REQUIRED ACTIONS (in order of preference):\n"
            f"  1. RENAME the test to match the source file it's testing (best option)\n"
            f"  2. MERGE this test into an existing test file that tests the same source — each test\n"
            f"     file costs compile time, so consolidating into fewer files is strongly preferred\n"
            f"  3. MOVE to 'tests/misc/{test_path.name}' if this truly doesn't test a specific source file\n\n"
            f"⚠️  DO NOT add '// ok standalone' unless absolutely necessary. This amnesty is a last\n"
            f"resort for rare infrastructure files that genuinely cannot be organized. AI agents\n"
            f"should NEVER add this comment — instead fix the path or consolidate tests.\n\n"
            f"Avoid creating tests in 'tests/misc/' - prefer mirroring source directory structure.\n"
            f"Test organization should mirror source organization for maintainability.\n"
            f"Note: Source matcher checks .h, .hpp, and .cpp.hpp files."
        )

        self.violations[file_content.path] = [(1, message)]
        return []  # We collect violations internally


def main() -> None:
    """Run test path structure checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = TestPathStructureChecker()
    run_checker_standalone(
        checker,
        [str(TESTS_ROOT)],
        "Found test files with incorrect path structure",
        extensions=[".cpp", ".hpp"],
    )


if __name__ == "__main__":
    main()
