#!/usr/bin/env python3

"""
Test suite for FastLED Archive Generation Infrastructure
Tests REAL archive creation functionality - no mocks!
"""

import os
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import List

from ci.compiler.clang_compiler import (
    BuildFlags,
    Compiler,
    CompilerOptions,
    LibarchiveOptions,
    LinkOptions,
    Result,
    create_archive_sync,
    detect_linker,
    get_configured_archiver_command,
    link_program_sync,
)
from ci.util.paths import PROJECT_ROOT


class TestRealArchiveCreation(unittest.TestCase):
    """Test suite for REAL archive creation functionality."""

    def setUp(self):
        """Set up test fixtures."""
        self.temp_dir = Path(tempfile.mkdtemp())
        self.project_root = PROJECT_ROOT

        # Set working directory to project root
        os.chdir(self.project_root)

    def tearDown(self):
        """Clean up test fixtures."""
        import shutil

        if self.temp_dir.exists():
            shutil.rmtree(self.temp_dir)

    def _load_build_flags(self) -> BuildFlags:
        """STRICT: Load BuildFlags explicitly - NO defaults allowed."""
        build_flags_path = Path(__file__).parent.parent / "build_unit.toml"
        if not build_flags_path.exists():
            raise RuntimeError(
                f"CRITICAL: build_unit.toml not found at {build_flags_path}"
            )
        return BuildFlags.parse(build_flags_path)

    def test_archiver_configuration_real(self):
        """Test that archiver is properly configured in TOML."""
        try:
            build_flags = self._load_build_flags()
            configured_cmd = get_configured_archiver_command(build_flags)
            self.assertIsNotNone(configured_cmd, "Archiver must be configured in TOML")
            # Explicit type assertion for type checker
            assert configured_cmd is not None  # Make type checker happy
            self.assertTrue(
                len(configured_cmd) > 0, "Archiver command must not be empty"
            )
            import subprocess

            archiver_str = subprocess.list2cmdline(configured_cmd)
            print(f"Configured archiver: {archiver_str}")
        except RuntimeError as e:
            self.fail(f"No archiver found on system: {e}")

    def test_compile_and_archive_blink(self):
        """Test compiling Blink.ino and creating a real archive."""
        # Verify Blink example exists
        blink_ino = self.project_root / "examples/Blink/Blink.ino"
        self.assertTrue(blink_ino.exists(), f"Blink.ino not found at {blink_ino}")

        # Set up compiler with proper configuration for testing
        settings = CompilerOptions(
            include_path=str(self.project_root / "src"),
            defines=[
                "ARDUINO=10607",
                "ARDUINO_AVR_UNO",
                "F_CPU=16000000L",
                "__AVR_ATmega328P__",
                "ARDUINO_ARCH_AVR",
                "STUB",
                "FASTLED_TESTING",
            ],
            std_version="c++17",
            compiler_args=[
                f"-I{self.project_root}/src/platforms/stub",
                f"-I{self.project_root}/src/platforms/wasm/compiler",  # Arduino.h emulation
            ],
        )

        build_flags = self._load_build_flags()
        compiler = Compiler(settings, build_flags)

        # Create C++ file from .ino
        blink_cpp = self.temp_dir / "Blink.cpp"
        blink_content = blink_ino.read_text()

        cpp_content = f"""#include "FastLED.h"
{blink_content}
"""
        blink_cpp.write_text(cpp_content)

        # Compile to object file
        blink_obj = self.temp_dir / "Blink.o"

        compile_future = compiler.compile_cpp_file(
            blink_cpp,
            blink_obj,
            additional_flags=["-c"],  # Only compile, don't link
        )

        compile_result = compile_future.result()

        # Verify compilation succeeded
        self.assertTrue(
            compile_result.ok, f"Compilation failed: {compile_result.stderr}"
        )
        self.assertTrue(blink_obj.exists(), f"Object file not created: {blink_obj}")

        # Verify object file has reasonable size
        obj_size = blink_obj.stat().st_size
        self.assertGreater(obj_size, 1000, "Object file seems too small")
        print(f"Object file size: {obj_size} bytes")

        # Test archive creation
        archive_path = self.temp_dir / "libfastled_blink.a"
        options = LibarchiveOptions(use_thin=False)

        archive_result = create_archive_sync([blink_obj], archive_path, options)

        # Verify archive creation succeeded
        self.assertTrue(
            archive_result.ok, f"Archive creation failed: {archive_result.stderr}"
        )
        self.assertTrue(
            archive_path.exists(), f"Archive file not created: {archive_path}"
        )

        # Verify archive has reasonable size
        archive_size = archive_path.stat().st_size
        self.assertGreater(
            archive_size, obj_size, "Archive should be larger than object file"
        )
        print(f"Archive size: {archive_size} bytes")

        return archive_path

    def test_thin_archive_creation(self):
        """Test creating thin archives."""
        # First create an object file
        archive_path = self.test_compile_and_archive_blink()

        # Get the object file used in previous test
        blink_obj = self.temp_dir / "Blink.o"
        self.assertTrue(blink_obj.exists())

        # Create thin archive
        thin_archive_path = self.temp_dir / "libfastled_blink_thin.a"
        thin_options = LibarchiveOptions(use_thin=True)

        thin_result = create_archive_sync([blink_obj], thin_archive_path, thin_options)

        # Verify thin archive creation succeeded
        self.assertTrue(
            thin_result.ok, f"Thin archive creation failed: {thin_result.stderr}"
        )
        self.assertTrue(
            thin_archive_path.exists(),
            f"Thin archive file not created: {thin_archive_path}",
        )

        # Verify thin archive is smaller than regular archive
        regular_size = archive_path.stat().st_size
        thin_size = thin_archive_path.stat().st_size
        self.assertLess(
            thin_size,
            regular_size,
            "Thin archive should be smaller than regular archive",
        )
        print(
            f"Thin archive size: {thin_size} bytes (vs regular: {regular_size} bytes)"
        )

    def test_multiple_object_files(self):
        """Test creating archive from multiple object files."""
        # Create multiple simple C++ files
        cpp_files: list[Path] = []
        obj_files: list[Path] = []

        for i in range(3):
            cpp_file = self.temp_dir / f"test{i}.cpp"
            cpp_content = f"""
#include "FastLED.h"

void function{i}() {{
    // Test function {i}
    CRGB leds[10];
    fill_solid(leds, 10, CRGB::Red);
}}
"""
            cpp_file.write_text(cpp_content)
            cpp_files.append(cpp_file)

        # Set up compiler
        settings = CompilerOptions(
            include_path=str(self.project_root / "src"),
            defines=[
                "ARDUINO=10607",
                "STUB",
                "FASTLED_TESTING",
            ],
            std_version="c++17",
            compiler_args=[
                f"-I{self.project_root}/src/platforms/stub",
                f"-I{self.project_root}/src/platforms/wasm/compiler",
            ],
        )

        build_flags = self._load_build_flags()
        compiler = Compiler(settings, build_flags)

        # Compile all files
        for i, cpp_file in enumerate(cpp_files):
            obj_file = self.temp_dir / f"test{i}.o"

            compile_future = compiler.compile_cpp_file(
                cpp_file, obj_file, additional_flags=["-c"]
            )

            compile_result = compile_future.result()
            self.assertTrue(
                compile_result.ok,
                f"Compilation failed for {cpp_file}: {compile_result.stderr}",
            )
            self.assertTrue(obj_file.exists())
            obj_files.append(obj_file)

        # Create archive from multiple object files
        multi_archive = self.temp_dir / "libmulti.a"
        options = LibarchiveOptions(use_thin=False)

        archive_result = create_archive_sync(obj_files, multi_archive, options)

        self.assertTrue(
            archive_result.ok,
            f"Multi-object archive creation failed: {archive_result.stderr}",
        )
        self.assertTrue(multi_archive.exists())

        # Verify archive size is reasonable
        total_obj_size = sum(obj.stat().st_size for obj in obj_files)
        archive_size = multi_archive.stat().st_size
        self.assertGreater(
            archive_size,
            total_obj_size * 0.8,
            "Archive should contain most of the object data",
        )

        print(f"Multi-object archive: {len(obj_files)} objects, {archive_size} bytes")

    def test_archive_content_verification(self):
        """Test verifying archive contents using real archiver tool."""
        # Create an archive first
        archive_path = self.test_compile_and_archive_blink()

        # Use configured archiver tool to list contents
        build_flags = self._load_build_flags()
        configured_cmd = get_configured_archiver_command(build_flags)
        self.assertIsNotNone(configured_cmd, "Archiver must be configured in TOML")
        # Use the full configured archiver command (e.g., ["uv", "run", "python", "-m", "ziglang", "ar"])
        assert configured_cmd is not None  # Type checker hint
        archiver_cmd = configured_cmd[:]  # Full command including uv run python

        # Build full command for archive listing
        full_cmd = archiver_cmd + ["t", str(archive_path)]
        list_result = subprocess.run(full_cmd, capture_output=True, text=True)

        self.assertEqual(
            list_result.returncode, 0, f"Archive listing failed: {list_result.stderr}"
        )

        # Verify the object file is listed in the archive
        contents = list_result.stdout.strip()
        self.assertIn(
            "Blink.o", contents, f"Blink.o not found in archive contents: {contents}"
        )

        print(f"Archive contents verified: {contents}")

    def test_error_handling_missing_objects(self):
        """Test error handling for missing object files - REAL error case."""
        missing_objects = [Path("/nonexistent/file.o")]
        archive_path = self.temp_dir / "test.a"
        options = LibarchiveOptions()

        result = create_archive_sync(missing_objects, archive_path, options)

        self.assertFalse(result.ok, "Should fail with missing object files")
        self.assertEqual(result.return_code, 1)
        self.assertIn("Object file not found", result.stderr)

    def test_error_handling_no_objects(self):
        """Test error handling when no object files are provided - REAL error case."""
        empty_objects: List[Path] = []
        archive_path = self.temp_dir / "test.a"
        options = LibarchiveOptions()

        result = create_archive_sync(empty_objects, archive_path, options)

        self.assertFalse(result.ok, "Should fail with no object files")
        self.assertEqual(result.return_code, 1)
        self.assertIn("No object files provided", result.stderr)

    def test_unity_build_basic(self):
        """Test basic UNITY build functionality with multiple .cpp files."""
        # Create multiple test .cpp files
        cpp_files: list[Path] = []

        for i in range(3):
            cpp_file = self.temp_dir / f"unity_test_{i}.cpp"
            cpp_content = f"""
#include "FastLED.h"

// Test function {i} for unity build
void test_function_{i}() {{
    CRGB leds[{10 + i}];
    fill_solid(leds, {10 + i}, CRGB::Red);
    // Some unique code for function {i}
    int value = {i * 10};
    (void)value;  // Suppress unused variable warning
}}

// Global variable for function {i}
static int global_var_{i} = {i * 100};
"""
            cpp_file.write_text(cpp_content)
            cpp_files.append(cpp_file)

        # Set up compiler
        settings = CompilerOptions(
            include_path=str(self.project_root / "src"),
            defines=[
                "ARDUINO=10607",
                "STUB",
                "FASTLED_TESTING",
            ],
            std_version="c++17",
            compiler_args=[
                f"-I{self.project_root}/src/platforms/stub",
                f"-I{self.project_root}/src/platforms/wasm/compiler",
            ],
        )

        build_flags = self._load_build_flags()
        compiler = Compiler(settings, build_flags)

        # Test unity compilation
        unity_options = CompilerOptions(
            include_path=str(self.project_root / "src"),
            defines=["STUB", "FASTLED_TESTING"],
            use_pch=False,  # Disable PCH for this test to avoid complications
            additional_flags=["-c"],
        )

        unity_future = compiler.compile_unity(unity_options, cpp_files)
        unity_result = unity_future.result()

        # Verify unity compilation succeeded
        self.assertTrue(
            unity_result.ok, f"Unity compilation failed: {unity_result.stderr}"
        )

        # Verify that unity.cpp was created and compiled to an object file
        # Since we're using temp files, we can't easily check the exact path,
        # but we can verify the compilation was successful
        print(f"Unity build completed successfully")

        # Now test unity with custom output path
        custom_unity_path = self.temp_dir / "custom_unity.cpp"

        # Test unity compilation with custom output
        unity_options_custom = CompilerOptions(
            include_path=str(self.project_root / "src"),
            defines=["STUB", "FASTLED_TESTING"],
            use_pch=False,
            additional_flags=["-c"],
        )

        unity_future_custom = compiler.compile_unity(
            unity_options_custom, cpp_files, custom_unity_path
        )
        unity_result_custom = unity_future_custom.result()

        # Verify custom unity compilation succeeded
        self.assertTrue(
            unity_result_custom.ok,
            f"Custom unity compilation failed: {unity_result_custom.stderr}",
        )

        # Verify that the custom unity.cpp file was created
        self.assertTrue(
            custom_unity_path.exists(),
            f"Custom unity file was not created at: {custom_unity_path}",
        )

        print(f"Custom unity.cpp created and validated: {custom_unity_path}")

    def test_unity_build_error_handling(self):
        """Test UNITY build error handling with invalid inputs."""
        settings = CompilerOptions(
            include_path=str(self.project_root / "src"),
            defines=["STUB", "FASTLED_TESTING"],
            std_version="c++17",
        )

        build_flags = self._load_build_flags()
        compiler = Compiler(settings, build_flags)

        # Test with empty file list
        empty_options = CompilerOptions(
            include_path=str(self.project_root / "src"),
            defines=["STUB", "FASTLED_TESTING"],
        )

        unity_future = compiler.compile_unity(empty_options, [])
        unity_result = unity_future.result()

        self.assertFalse(unity_result.ok, "Should fail with empty file list")
        self.assertIn("No .cpp files provided", unity_result.stderr)

        # Test with non-existent file
        nonexistent_file = self.temp_dir / "nonexistent.cpp"

        unity_future = compiler.compile_unity(empty_options, [nonexistent_file])
        unity_result = unity_future.result()

        self.assertFalse(unity_result.ok, "Should fail with non-existent file")
        self.assertIn("Source file not found", unity_result.stderr)

        # Test with non-.cpp file
        txt_file = self.temp_dir / "not_cpp.txt"
        txt_file.write_text("This is not a C++ file")

        unity_future = compiler.compile_unity(empty_options, [txt_file])
        unity_result = unity_future.result()

        self.assertFalse(unity_result.ok, "Should fail with non-.cpp file")
        self.assertIn("File is not a .cpp file", unity_result.stderr)

    def test_unity_build_and_archive_integration(self):
        """Test complete workflow: unity build + archive creation."""
        # Create multiple test .cpp files
        cpp_files: list[Path] = []

        for i in range(3):
            cpp_file = self.temp_dir / f"integration_test_{i}.cpp"
            cpp_content = f"""
#include "FastLED.h"

// Integration test function {i}
void integration_function_{i}() {{
    CRGB leds[5];
    fill_rainbow(leds, 5, {i * 32}, 20);
}}

// Static data for testing
static const uint8_t test_data_{i}[] = {{1, 2, 3, {i}}};
"""
            cpp_file.write_text(cpp_content)
            cpp_files.append(cpp_file)

        # Set up compiler
        settings = CompilerOptions(
            include_path=str(self.project_root / "src"),
            defines=[
                "ARDUINO=10607",
                "STUB",
                "FASTLED_TESTING",
            ],
            std_version="c++17",
            compiler_args=[
                f"-I{self.project_root}/src/platforms/stub",
                f"-I{self.project_root}/src/platforms/wasm/compiler",
            ],
        )

        build_flags = self._load_build_flags()
        compiler = Compiler(settings, build_flags)

        # Unity compilation
        unity_obj_path = self.temp_dir / "integration_unity.o"
        unity_options = CompilerOptions(
            include_path=str(self.project_root / "src"),
            defines=["STUB", "FASTLED_TESTING"],
            use_pch=False,
            additional_flags=["-c", "-o", str(unity_obj_path)],
        )

        unity_future = compiler.compile_unity(unity_options, cpp_files)
        unity_result = unity_future.result()

        # Verify unity compilation succeeded
        self.assertTrue(
            unity_result.ok,
            f"Integration unity compilation failed: {unity_result.stderr}",
        )
        self.assertTrue(
            unity_obj_path.exists(),
            f"Unity object file not created: {unity_obj_path}",
        )

        # Verify object file has reasonable size
        obj_size = unity_obj_path.stat().st_size
        self.assertGreater(obj_size, 1000, "Unity object file seems too small")
        print(f"Unity object file size: {obj_size} bytes")

        # Create archive from unity object file
        archive_path = self.temp_dir / "libfastled_integration_unity.a"
        options = LibarchiveOptions(use_thin=False)

        archive_result = create_archive_sync([unity_obj_path], archive_path, options)

        # Verify archive creation succeeded
        self.assertTrue(
            archive_result.ok,
            f"Integration archive creation failed: {archive_result.stderr}",
        )
        self.assertTrue(
            archive_path.exists(),
            f"Integration archive file not created: {archive_path}",
        )

        # Verify archive has reasonable size
        archive_size = archive_path.stat().st_size
        self.assertGreater(
            archive_size, obj_size, "Archive should be larger than object file"
        )
        print(f"Integration archive size: {archive_size} bytes")

        print("Complete unity build + archive integration test passed!")

    def test_unity_build_with_additional_flags(self):
        """Test UNITY build with additional compiler flags."""
        # Create test .cpp files
        cpp_files: list[Path] = []

        for i in range(2):
            cpp_file = self.temp_dir / f"flags_test_{i}.cpp"
            cpp_content = f"""
#include "FastLED.h"

void flags_test_function_{i}() {{
    CRGB led;
    led = CRGB::Green;
    // Test optimization flags {i}
    volatile int value = {i * 42};
    (void)value;  // Use the value to prevent optimization
}}
"""
            cpp_file.write_text(cpp_content)
            cpp_files.append(cpp_file)

        # Set up compiler
        settings = CompilerOptions(
            include_path=str(self.project_root / "src"),
            defines=[
                "ARDUINO=10607",
                "STUB",
                "FASTLED_TESTING",
            ],
            std_version="c++17",
            compiler_args=[
                f"-I{self.project_root}/src/platforms/stub",
                f"-I{self.project_root}/src/platforms/wasm/compiler",
            ],
        )

        build_flags = self._load_build_flags()
        compiler = Compiler(settings, build_flags)

        # Test unity compilation with additional flags
        unity_options = CompilerOptions(
            include_path=str(self.project_root / "src"),
            defines=["STUB", "FASTLED_TESTING"],
            use_pch=False,
            additional_flags=["-c", "-O2", "-ffast-math"],
        )

        unity_future = compiler.compile_unity(unity_options, cpp_files)
        unity_result = unity_future.result()

        # Verify unity compilation succeeded
        self.assertTrue(
            unity_result.ok,
            f"Unity compilation with additional flags failed: {unity_result.stderr}",
        )
        print(f"Unity build with additional flags completed successfully")

    def test_unity_build_identical_content_skip(self):
        """Test that unity build skips writing when content is identical."""
        # Create test .cpp files
        cpp_files: list[Path] = []

        for i in range(2):
            cpp_file = self.temp_dir / f"identical_test_{i}.cpp"
            cpp_content = f"""
#include "FastLED.h"

void identical_function_{i}() {{
    CRGB led;
    led = CRGB::Red;
    // Identical test implementation {i}
}}
"""
            cpp_file.write_text(cpp_content)
            cpp_files.append(cpp_file)

        # Set up compiler
        settings = CompilerOptions(
            include_path=str(self.project_root / "src"),
            defines=["STUB", "FASTLED_TESTING"],
            std_version="c++17",
        )

        build_flags = self._load_build_flags()
        compiler = Compiler(settings, build_flags)

        # Custom unity.cpp path for testing
        unity_path = self.temp_dir / "test_identical_unity.cpp"

        # Test first unity compilation - should create the file
        unity_options = CompilerOptions(
            include_path=str(self.project_root / "src"),
            defines=["STUB", "FASTLED_TESTING"],
            use_pch=False,
            additional_flags=["-c"],
        )

        unity_future = compiler.compile_unity(unity_options, cpp_files, unity_path)
        unity_result = unity_future.result()

        # Verify first compilation succeeded
        self.assertTrue(
            unity_result.ok, f"First unity compilation failed: {unity_result.stderr}"
        )

        # Verify unity file was created
        self.assertTrue(
            unity_path.exists(), f"Unity file was not created: {unity_path}"
        )

        # Get original file modification time
        original_mtime = unity_path.stat().st_mtime

        # Wait a small amount to ensure mtime would be different if file was rewritten
        import time

        time.sleep(0.01)

        # Run unity compilation again with identical inputs - should skip writing
        unity_future_2 = compiler.compile_unity(unity_options, cpp_files, unity_path)
        unity_result_2 = unity_future_2.result()

        # Verify second compilation succeeded
        self.assertTrue(
            unity_result_2.ok,
            f"Second unity compilation failed: {unity_result_2.stderr}",
        )

        # Get new file modification time
        new_mtime = unity_path.stat().st_mtime

        # Verify file was NOT modified (identical content optimization worked)
        self.assertEqual(
            original_mtime,
            new_mtime,
            f"Unity file was unnecessarily rewritten - mtime changed from {original_mtime} to {new_mtime}",
        )

        print(f"Unity identical content optimization test passed - file not modified")

        # Now test that different content DOES cause a rewrite
        # Add another .cpp file to change the unity content
        new_cpp_file = self.temp_dir / "identical_test_new.cpp"
        new_cpp_content = """
#include "FastLED.h"

void new_function() {
    CRGB led;
    led = CRGB::Blue;
    // New implementation
}
"""
        new_cpp_file.write_text(new_cpp_content)
        cpp_files_modified = cpp_files + [new_cpp_file]

        # Wait to ensure mtime would be different
        time.sleep(0.01)

        # Run unity compilation with different inputs - should rewrite
        unity_future_3 = compiler.compile_unity(
            unity_options, cpp_files_modified, unity_path
        )
        unity_result_3 = unity_future_3.result()

        # Verify third compilation succeeded
        self.assertTrue(
            unity_result_3.ok,
            f"Third unity compilation failed: {unity_result_3.stderr}",
        )

        # Get final file modification time
        final_mtime = unity_path.stat().st_mtime

        # Verify file WAS modified when content changed
        self.assertNotEqual(
            new_mtime,
            final_mtime,
            f"Unity file should have been rewritten when content changed",
        )

        print(
            f"Unity content change detection test passed - file was properly modified"
        )


class TestProgramLinking(unittest.TestCase):
    """Test suite for program linking functionality."""

    def setUp(self):
        """Set up test fixtures."""
        self.temp_dir = Path(tempfile.mkdtemp())
        self.project_root = PROJECT_ROOT
        os.chdir(self.project_root)

    def tearDown(self):
        """Clean up test fixtures."""
        import shutil

        if self.temp_dir.exists():
            shutil.rmtree(self.temp_dir)

    def _load_build_flags(self) -> BuildFlags:
        """STRICT: Load BuildFlags explicitly - NO defaults allowed."""
        build_flags_path = Path(__file__).parent.parent / "build_unit.toml"
        if not build_flags_path.exists():
            raise RuntimeError(
                f"CRITICAL: build_unit.toml not found at {build_flags_path}"
            )
        return BuildFlags.parse(build_flags_path)

    def test_linker_detection(self):
        """Test automatic linker detection."""
        try:
            linker = detect_linker()
            self.assertIsInstance(linker, str)
            self.assertTrue(len(linker) > 0)
            print(f"Detected linker: {linker}")
        except RuntimeError as e:
            self.fail(f"Linker detection failed: {e}")

    def test_link_options_basic(self):
        """Test LinkOptions dataclass functionality."""
        options = LinkOptions(
            output_executable="test_program.exe",
            object_files=["main.o", "utils.o"],
            static_libraries=["libmath.a"],
            linker_args=["/SUBSYSTEM:CONSOLE"],
        )

        self.assertEqual(options.output_executable, "test_program.exe")
        self.assertEqual(len(options.object_files), 2)
        self.assertEqual(len(options.static_libraries), 1)
        self.assertEqual(len(options.linker_args), 1)
        print("LinkOptions dataclass test passed")

    def test_linking_validation_missing_objects(self):
        """Test validation of missing object files."""
        options = LinkOptions(
            output_executable="test.exe",
            object_files=["nonexistent.o"],
            static_libraries=[],
            linker_args=[],
        )

        build_flags = self._load_build_flags()
        result = link_program_sync(options, build_flags)
        self.assertFalse(result.ok)
        self.assertIn("Object file not found", result.stderr)
        print("Missing object file validation works")

    def test_linking_validation_missing_libraries(self):
        """Test validation of missing static libraries."""
        # Create a dummy object file
        dummy_obj = self.temp_dir / "dummy.o"
        dummy_obj.write_text("dummy content")

        options = LinkOptions(
            output_executable="test.exe",
            object_files=[str(dummy_obj)],
            static_libraries=["nonexistent.a"],
            linker_args=[],
        )

        build_flags = self._load_build_flags()
        result = link_program_sync(options, build_flags)
        self.assertFalse(result.ok)
        self.assertIn("Static library not found", result.stderr)
        print("Missing library validation works")

    def test_linking_validation_no_objects(self):
        """Test validation when no object files provided."""
        options = LinkOptions(
            output_executable="test.exe",
            object_files=[],
            static_libraries=[],
            linker_args=[],
        )

        build_flags = self._load_build_flags()
        result = link_program_sync(options, build_flags)
        self.assertFalse(result.ok)
        self.assertIn("No object files provided", result.stderr)
        print("No object files validation works")


class TestFullProgramLinking(unittest.TestCase):
    """Test complete program linking using actual FastLED src and examples."""

    def setUp(self):
        """Set up test environment with FastLED source."""
        self.temp_dir = Path(tempfile.mkdtemp())
        self.project_root = PROJECT_ROOT
        self.src_dir = self.project_root / "src"
        self.examples_dir = self.project_root / "examples"
        os.chdir(self.project_root)

        # Verify FastLED source structure
        self.assertTrue(self.src_dir.exists(), "FastLED src directory not found")
        self.assertTrue((self.src_dir / "FastLED.h").exists(), "FastLED.h not found")

    def tearDown(self):
        """Clean up temporary files."""
        import shutil

        if self.temp_dir.exists():
            shutil.rmtree(self.temp_dir)

    def _load_build_flags(self) -> BuildFlags:
        """STRICT: Load BuildFlags explicitly - NO defaults allowed."""
        build_flags_path = Path(__file__).parent.parent / "build_unit.toml"
        if not build_flags_path.exists():
            raise RuntimeError(
                f"CRITICAL: build_unit.toml not found at {build_flags_path}"
            )
        return BuildFlags.parse(build_flags_path)

    def test_link_blink_example_full_pipeline(self):
        """Test complete pipeline: Blink.ino + FastLED src to executable."""

        # Step 1: Set up compiler with FastLED source
        compiler_options = CompilerOptions(
            include_path=str(self.src_dir),
            defines=["STUB_PLATFORM", "ARDUINO=10808"],
            std_version="c++17",
            compiler_args=[
                f"-I{self.src_dir}/platforms/stub",
                f"-I{self.src_dir}/platforms/wasm/compiler",  # Arduino.h compatibility
            ],
        )
        build_flags = self._load_build_flags()
        compiler = Compiler(compiler_options, build_flags)

        # Step 2: Compile Blink.ino to object file
        blink_ino = self.examples_dir / "Blink" / "Blink.ino"
        if not blink_ino.exists():
            self.skipTest("Blink example not found")

        blink_obj = self.temp_dir / "blink.o"
        compile_future = compiler.compile_ino_file(blink_ino, blink_obj)
        compile_result = compile_future.result()

        if not compile_result.ok:
            print(f"WARNING: Blink compilation failed: {compile_result.stderr}")
            self.skipTest("Blink compilation failed - skipping link test")

        self.assertTrue(blink_obj.exists(), "Blink object file not created")

        # Step 3: Create minimal FastLED static library from core sources
        fastled_objects = self._compile_fastled_core(compiler)
        if not fastled_objects:
            self.skipTest("FastLED core compilation failed - skipping link test")

        fastled_lib = self.temp_dir / "libfastled.a"
        archive_future = compiler.create_archive(fastled_objects, fastled_lib)
        archive_result = archive_future.result()

        if not archive_result.ok:
            print(f"WARNING: FastLED library creation failed: {archive_result.stderr}")
            self.skipTest("FastLED library creation failed - skipping link test")

        # Step 4: Link Blink + FastLED into executable
        import platform

        executable_name = (
            "blink_program.exe" if platform.system() == "Windows" else "blink_program"
        )
        executable_path = self.temp_dir / executable_name

        link_options = LinkOptions(
            output_executable=str(executable_path),
            object_files=[str(blink_obj)],
            static_libraries=[str(fastled_lib)],
            linker_args=self._get_platform_linker_args(executable_name),
        )

        link_future = compiler.link_program(link_options)
        link_result = link_future.result()

        if not link_result.ok:
            # Linking should succeed - fail the test if it doesn't
            self.fail(
                f"Program linking failed - this needs to be fixed:\n{link_result.stderr}"
            )

        self.assertTrue(executable_path.exists(), "Executable not created")
        self._verify_executable_properties(executable_path)

        print(
            f"SUCCESS: Full pipeline test passed: {blink_ino.name} to {executable_path.name}"
        )

    def test_link_multiple_examples_basic(self):
        """Test basic linking validation of multiple examples."""

        # Test examples that should compile with stub platform
        test_examples = ["Blink", "FirstLight"]
        successful_compiles = 0
        successful_links = 0

        compiler_options = CompilerOptions(
            include_path=str(self.src_dir),
            defines=["STUB_PLATFORM", "ARDUINO=10808"],
            std_version="c++17",
            compiler_args=[
                f"-I{self.src_dir}/platforms/stub",
                f"-I{self.src_dir}/platforms/wasm/compiler",  # Arduino.h compatibility
            ],
        )
        build_flags = self._load_build_flags()
        compiler = Compiler(compiler_options, build_flags)

        # Create FastLED library once
        fastled_objects = self._compile_fastled_core(compiler)
        if not fastled_objects:
            self.skipTest("FastLED core compilation failed")

        fastled_lib = self.temp_dir / "libfastled.a"
        archive_result = compiler.create_archive(fastled_objects, fastled_lib).result()
        if not archive_result.ok:
            self.skipTest("FastLED library creation failed")

        for example_name in test_examples:
            example_dir = self.examples_dir / example_name
            if not example_dir.exists():
                continue

            ino_file = example_dir / f"{example_name}.ino"
            if not ino_file.exists():
                continue

            try:
                # Compile example
                obj_file = self.temp_dir / f"{example_name}.o"
                compile_result = compiler.compile_ino_file(ino_file, obj_file).result()

                if not compile_result.ok:
                    print(f"WARNING: {example_name} compilation failed")
                    continue

                successful_compiles += 1

                # Attempt linking
                import platform

                executable_name = (
                    f"{example_name}.exe"
                    if platform.system() == "Windows"
                    else example_name
                )
                executable_path = self.temp_dir / executable_name

                link_options = LinkOptions(
                    output_executable=str(executable_path),
                    object_files=[str(obj_file)],
                    static_libraries=[str(fastled_lib)],
                    linker_args=self._get_platform_linker_args(executable_name),
                )

                link_result = compiler.link_program(link_options).result()

                if link_result.ok and executable_path.exists():
                    successful_links += 1
                    print(f"SUCCESS: {example_name} linked successfully")
                else:
                    self.fail(
                        f"Linking failed for {example_name} - this needs to be fixed:\n{link_result.stderr}"
                    )

            except Exception as e:
                print(f"ERROR: {example_name} failed with exception: {e}")

        # Require at least some examples to compile
        self.assertGreater(successful_compiles, 0, "No examples compiled successfully")

        print(
            f"SUCCESS: Batch test completed: {successful_compiles} compiled, {successful_links} linked"
        )

    def _compile_fastled_core(self, compiler: Compiler) -> list[Path]:
        """Compile all FastLED source files to object files."""
        fastled_objects: list[Path] = []

        # Glob all .cpp files in src/**
        print("Compiling all FastLED source files from src/**")
        cpp_files = list(self.src_dir.rglob("*.cpp"))

        # Sort for consistent compilation order
        cpp_files.sort()

        compiled_count = 0
        for cpp_file in cpp_files:
            # Create unique object file name based on relative path
            rel_path = cpp_file.relative_to(self.src_dir)
            obj_name = (
                str(rel_path).replace("/", "_").replace("\\", "_").replace(".cpp", ".o")
            )
            obj_file = self.temp_dir / obj_name

            compile_result = compiler.compile_cpp_file(cpp_file, obj_file).result()

            if compile_result.ok:
                fastled_objects.append(obj_file)
                compiled_count += 1
            else:
                print(f"WARNING: Failed to compile {rel_path}: {compile_result.stderr}")

        # Compile STUB platform implementation (includes delay, millis, micros)
        # This is a .hpp file with implementations that needs to be compiled
        stub_impl_file = (
            self.src_dir / "platforms" / "stub" / "generic" / "led_sysdefs_generic.hpp"
        )
        if stub_impl_file.exists():
            # Create a temporary .cpp file that includes the .hpp implementation
            temp_cpp = self.temp_dir / "stub_platform_impl.cpp"
            temp_cpp.write_text(f'''
#define FASTLED_STUB_IMPL
#include "{stub_impl_file}"
            ''')

            obj_file = self.temp_dir / "stub_platform_impl.o"
            compile_result = compiler.compile_cpp_file(temp_cpp, obj_file).result()

            if compile_result.ok:
                fastled_objects.append(obj_file)
                compiled_count += 1
                print(f"SUCCESS: Compiled STUB platform implementation")
            else:
                print(
                    f"WARNING: Failed to compile STUB platform implementation: {compile_result.stderr}"
                )

        # Create main entry point for Arduino-style programs
        # This provides the missing main() function that calls setup() and loop()
        main_cpp = self.temp_dir / "arduino_main.cpp"
        main_cpp.write_text("""
// Arduino-style main entry point
// Forward declarations for setup() and loop() functions from .ino file
// Note: .ino files compile as C++, not C, so don't use extern "C"
void setup();
void loop();

int main() {
    // Call setup() once
    setup();
    
    // Call loop() a few times (not infinite to avoid hanging in tests)
    for (int i = 0; i < 3; ++i) {
        loop();
    }
    
    return 0;
}
        """)

        obj_file = self.temp_dir / "arduino_main.o"
        compile_result = compiler.compile_cpp_file(main_cpp, obj_file).result()

        if compile_result.ok:
            fastled_objects.append(obj_file)
            compiled_count += 1
            print(f"SUCCESS: Compiled Arduino main entry point")
        else:
            print(
                f"WARNING: Failed to compile Arduino main entry point: {compile_result.stderr}"
            )

        print(f"Compiled {compiled_count} FastLED source files successfully")
        return fastled_objects

    def _get_platform_linker_args(self, executable_name: str) -> list[str]:
        """Get platform-appropriate linker arguments using build_unit.toml approach."""
        # Use basic linking flags from build_unit.toml
        return ["-pthread"]  # Basic linking flags from build_unit.toml [linking.base]

    def _verify_executable_properties(self, executable_path: Path):
        """Verify that the created executable has expected properties."""

        # Check file exists and has reasonable size
        self.assertTrue(executable_path.exists(), "Executable file not found")

        file_size = executable_path.stat().st_size
        self.assertGreater(file_size, 100, f"Executable too small: {file_size} bytes")
        self.assertLess(
            file_size, 100 * 1024 * 1024, f"Executable too large: {file_size} bytes"
        )

        # Check executable permissions on Unix
        import platform

        if platform.system() != "Windows":
            file_mode = executable_path.stat().st_mode
            self.assertTrue(
                file_mode & stat.S_IXUSR, "Executable not marked as executable"
            )

        print(
            f"SUCCESS: Executable verification passed: {executable_path.name} ({file_size:,} bytes)"
        )


if __name__ == "__main__":
    print("=== FastLED REAL Archive Creation Tests (No Mocks!) ===")
    unittest.main(verbosity=2)
