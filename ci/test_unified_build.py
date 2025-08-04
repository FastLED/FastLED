#!/usr/bin/env python3
"""
Test script to validate the new unified build system.

This script tests the core functionality of the BuildAPI to ensure
the refactored build system works correctly.
"""

import tempfile
from pathlib import Path

from ci.build_api import BuildType, create_example_builder, create_unit_test_builder


def test_build_api_creation():
    """Test that BuildAPI instances can be created successfully"""
    print("Testing BuildAPI creation...")

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Test unit test builder creation
        unit_builder = create_unit_test_builder(
            build_dir=temp_path / "unit", use_pch=True, parallel=True, clean=False
        )

        assert unit_builder.build_type == BuildType.UNIT_TEST
        assert unit_builder.build_dir == temp_path / "unit"
        assert unit_builder.use_pch == True

        print("✓ Unit test builder created successfully")

        # Test example builder creation
        example_builder = create_example_builder(
            build_dir=temp_path / "examples", use_pch=True, parallel=True, clean=False
        )

        assert example_builder.build_type == BuildType.EXAMPLE
        assert example_builder.build_dir == temp_path / "examples"
        assert example_builder.use_pch == True

        print("✓ Example builder created successfully")


def test_build_configuration_loading():
    """Test that build configurations are loaded correctly"""
    print("\nTesting build configuration loading...")

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Test unit test configuration
        unit_builder = create_unit_test_builder(build_dir=temp_path / "unit")

        # Check that unit test specific defines are present
        unit_defines = unit_builder.compiler_settings.defines or []
        assert "FASTLED_FORCE_NAMESPACE=1" in unit_defines
        assert "FASTLED_TESTING=1" in unit_defines
        assert "STUB_PLATFORM" in unit_defines

        print("✓ Unit test configuration loaded correctly")

        # Test example configuration
        example_builder = create_example_builder(build_dir=temp_path / "examples")

        # Check that example specific defines are present
        example_defines = example_builder.compiler_settings.defines or []
        assert "STUB_PLATFORM" in example_defines
        assert "ARDUINO=10808" in example_defines
        # Unit test specific defines should NOT be present
        assert "FASTLED_FORCE_NAMESPACE=1" not in example_defines
        assert "FASTLED_TESTING=1" not in example_defines

        print("✓ Example configuration loaded correctly")


def test_build_directory_structure():
    """Test that build directory structure is created correctly"""
    print("\nTesting build directory structure...")

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        unit_builder = create_unit_test_builder(build_dir=temp_path / "unit")

        # Check that all required directories are created
        assert unit_builder.build_dir.exists()
        assert unit_builder.artifacts_dir.exists()
        assert unit_builder.targets_dir.exists()
        assert unit_builder.cache_dir.exists()

        # Check directory organization
        expected_structure = [
            unit_builder.build_dir / "artifacts",
            unit_builder.build_dir / "targets",
            unit_builder.build_dir / "cache",
        ]

        for directory in expected_structure:
            assert directory.exists(), f"Directory {directory} should exist"

        print("✓ Build directory structure created correctly")


def test_build_info():
    """Test that build info is returned correctly"""
    print("\nTesting build info...")

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        unit_builder = create_unit_test_builder(build_dir=temp_path / "unit")
        info = unit_builder.get_build_info()

        # Check required fields
        assert "config_file" in info
        assert "build_dir" in info
        assert "build_type" in info
        assert "use_pch" in info
        assert "parallel" in info

        # Check values
        assert info["build_type"] == "unit_test"
        assert info["use_pch"] == True
        assert str(temp_path / "unit") in info["build_dir"]
        assert "build_unit.toml" in info["build_flags_toml"]

        print("✓ Build info returned correctly")


def test_clean_functionality():
    """Test that clean functionality works"""
    print("\nTesting clean functionality...")

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        unit_builder = create_unit_test_builder(build_dir=temp_path / "unit")

        # Create some fake files to test cleaning
        test_file = unit_builder.build_dir / "test_file.txt"
        test_file.write_text("test content")
        assert test_file.exists()

        # Test clean functionality
        unit_builder.clean_build_artifacts()

        # Build directory should still exist but be empty (except for subdirs)
        assert unit_builder.build_dir.exists()
        assert not test_file.exists()

        # Subdirectories should be recreated
        assert unit_builder.artifacts_dir.exists()
        assert unit_builder.targets_dir.exists()
        assert unit_builder.cache_dir.exists()

        print("✓ Clean functionality works correctly")


def test_configuration_separation():
    """Test that unit test and example configurations are properly separated"""
    print("\nTesting configuration separation...")

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Create both builders
        unit_builder = create_unit_test_builder(build_dir=temp_path / "unit")
        example_builder = create_example_builder(build_dir=temp_path / "examples")

        # Get build info for comparison
        unit_info = unit_builder.get_build_info()
        example_info = example_builder.get_build_info()

        # Configurations should use different files
        assert "build_unit.toml" in unit_info["build_flags_toml"]
        assert "build_example.toml" in example_info["build_flags_toml"]

        # Build types should be different
        assert unit_info["build_type"] == "unit_test"
        assert example_info["build_type"] == "example"

        # Build directories should be different
        assert unit_info["build_dir"] != example_info["build_dir"]

        print("✓ Configuration separation works correctly")


def run_all_tests():
    """Run all validation tests"""
    print("=" * 60)
    print("FASTLED UNIFIED BUILD SYSTEM VALIDATION")
    print("=" * 60)

    try:
        test_build_api_creation()
        test_build_configuration_loading()
        test_build_directory_structure()
        test_build_info()
        test_clean_functionality()
        test_configuration_separation()

        print("\n" + "=" * 60)
        print("ALL TESTS PASSED ✓")
        print("=" * 60)
        print("The unified build system is working correctly!")
        print("\nKey features validated:")
        print("✓ BuildAPI creation for both unit tests and examples")
        print("✓ Proper configuration loading from separate TOML files")
        print("✓ Organized build directory structure")
        print("✓ Clean separation between unit test and example builds")
        print("✓ Build info reporting")
        print("✓ Clean functionality")

        return True

    except Exception as e:
        print(f"\n❌ TEST FAILED: {e}")
        import traceback

        traceback.print_exc()
        return False


if __name__ == "__main__":
    success = run_all_tests()
    exit(0 if success else 1)
