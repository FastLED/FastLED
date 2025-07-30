#!/usr/bin/env python3

"""
Direct compilation tests for FastLED examples using clang++.
Tests the simple build system approach outlined in FEATURE.md.
"""

import os
import sys
from pathlib import Path

from ci.ci.clang_compiler import (
    Compiler,
    CompilerOptions,
    Result,
    check_clang_version,
    compile_ino_file,
    find_ino_files,
    test_clang_accessibility,
)


def test_clang_accessibility_main():
    """Test that clang++ is accessible and working properly."""
    print("\n=== Testing clang++ accessibility ===")

    success = test_clang_accessibility()
    assert success, "Clang accessibility test failed"


def test_clang_accessibility_class():
    """Test the class-based clang accessibility."""
    print("\n=== Testing class-based clang accessibility ===")

    # Create compiler with default settings
    settings = CompilerOptions(
        include_path="./src", defines=["STUB_PLATFORM"], std_version="c++17"
    )
    compiler = Compiler(settings)

    # Test 1: Version check
    version_result = compiler.check_clang_version()
    assert version_result.success, f"Version check failed: {version_result.error}"
    print(f"[OK] clang++ version: {version_result.version}")

    # Test 2: Find .ino files
    ino_files = compiler.find_ino_files(
        "examples", filter_names=["Blink", "DemoReel100"]
    )
    assert len(ino_files) >= 2, (
        f"Expected at least 2 .ino files, found {len(ino_files)}"
    )
    print(f"[INFO] Found {len(ino_files)} .ino files")

    # Test 3: Basic compilation test
    blink_files = [f for f in ino_files if f.name == "Blink.ino"]
    assert len(blink_files) > 0, "Blink.ino not found"

    future_result = compiler.compile_ino_file(blink_files[0])
    result = future_result.result()  # Wait for completion and get result
    assert result.ok, f"Blink compilation failed: {result.stderr}"
    print(f"[OK] Blink.ino compilation: return code {result.return_code}")


def test_clang_accessibility_functions():
    """Test the backward-compatible function interface."""
    print("\n=== Testing backward-compatible functions ===")

    # Test the standalone function version
    success = test_clang_accessibility()
    assert success, "Backward-compatible function test failed"


def test_compiler_configuration():
    """
    Test different compiler configurations to ensure flexibility.
    """
    print("\n=== Testing compiler configuration options ===")

    # Test with different configurations
    configs: list[dict[str, str | CompilerOptions]] = [
        {
            "name": "Default",
            "settings": CompilerOptions(
                include_path="./src",
                defines=["STUB_PLATFORM"],
                std_version="c++17",
            ),
        },
        {
            "name": "C++20",
            "settings": CompilerOptions(
                include_path="./src",
                defines=["STUB_PLATFORM"],
                std_version="c++20",
            ),
        },
        {
            "name": "Custom Platform",
            "settings": CompilerOptions(
                include_path="./src",
                defines=["CUSTOM_PLATFORM"],
                std_version="c++17",
            ),
        },
    ]

    for test_config in configs:
        print(f"Testing {test_config['name']} configuration...")
        settings = test_config["settings"]
        assert isinstance(settings, CompilerOptions), (
            f"Invalid settings type: {type(settings)}"
        )
        compiler = Compiler(settings)

        # Just test version check for each config
        version_result = compiler.check_clang_version()
        if version_result.success:
            print(f"[OK] {test_config['name']}: {version_result.version}")
        else:
            print(f"[FAIL] {test_config['name']}: {version_result.error}")
            assert False, (
                f"Configuration {test_config['name']} failed: {version_result.error}"
            )


if __name__ == "__main__":
    print("FastLED Direct Compile Tests")
    print("=" * 50)

    try:
        test_clang_accessibility_main()
        test_clang_accessibility_class()
        test_clang_accessibility_functions()
        test_compiler_configuration()
        print("\n[SUCCESS] All tests passed!")
    except Exception as e:
        print(f"\n[ERROR] Test failed: {e}")
        sys.exit(1)
