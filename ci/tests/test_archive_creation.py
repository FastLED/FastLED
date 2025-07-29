#!/usr/bin/env python3

"""
Test suite for FastLED Archive Generation Infrastructure
Tests REAL archive creation functionality - no mocks!
"""

import os
import sys
import tempfile
import unittest
from pathlib import Path


# Add the ci directory to the path to import our modules
current_dir = Path(__file__).parent
project_root = current_dir.parent.parent
sys.path.insert(0, str(project_root))
sys.path.insert(0, str(project_root / "ci"))

from ci.clang_compiler import (
    Compiler,
    CompilerOptions,
    LibarchiveOptions,
    Result,
    create_archive_sync,
    detect_archiver,
)


class TestRealArchiveCreation(unittest.TestCase):
    """Test suite for REAL archive creation functionality."""

    def setUp(self):
        """Set up test fixtures."""
        self.temp_dir = Path(tempfile.mkdtemp())
        self.project_root = project_root

        # Set working directory to project root
        os.chdir(self.project_root)

    def tearDown(self):
        """Clean up test fixtures."""
        import shutil

        if self.temp_dir.exists():
            shutil.rmtree(self.temp_dir)

    def test_archiver_detection_real(self):
        """Test real archiver detection on the system."""
        try:
            archiver = detect_archiver()
            self.assertTrue(archiver)  # Should return a valid path
            self.assertTrue(Path(archiver).exists() or Path(archiver + ".exe").exists())
            print(f"Found archiver: {archiver}")
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

        compiler = Compiler(settings)

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

        compiler = Compiler(settings)

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

        # Use real archiver tool to list contents
        archiver = detect_archiver()

        import subprocess

        list_result = subprocess.run(
            [archiver, "t", str(archive_path)], capture_output=True, text=True
        )

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
        empty_objects = []
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

        compiler = Compiler(settings)

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

        compiler = Compiler(settings)

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

        compiler = Compiler(settings)

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

        compiler = Compiler(settings)

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

        compiler = Compiler(settings)

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


if __name__ == "__main__":
    print("=== FastLED REAL Archive Creation Tests (No Mocks!) ===")
    unittest.main(verbosity=2)
