#!/usr/bin/env python3

"""
Direct compilation tests for FastLED examples using clang++.
Tests the simple build system approach outlined in FEATURE.md.
"""

import os
import sys
from pathlib import Path

from ci.compiler.clang_compiler import (
    BuildFlags,
    Compiler,
    CompilerOptions,
    Result,
    check_clang_version,
    compile_ino_file,
    find_ino_files,
    test_clang_accessibility,
)

# MANDATORY: Import configuration functions - NO fallbacks allowed
from ci.compiler.test_example_compilation import (
    extract_stub_platform_defines_from_toml,
    extract_stub_platform_include_paths_from_toml,
    load_build_flags_toml,
)


def test_clang_accessibility_main():
    """Test that clang++ is accessible and working properly."""
    print("\n=== Testing clang++ accessibility ===")

    success = test_clang_accessibility()
    assert success, "Clang accessibility test failed"


def test_clang_accessibility_class():
    """Test the class-based clang accessibility."""
    print("\n=== Testing class-based clang accessibility ===")

    # Create compiler with stub platform defines from configuration
    import os
    import sys
    from pathlib import Path

    # Add the ci/compiler directory to path for imports
    current_dir = Path(__file__).parent.parent
    sys.path.insert(0, str(current_dir / "compiler"))

    # MANDATORY: Load build_flags.toml configuration - NO fallbacks allowed
    toml_path = current_dir / "build_flags.toml"
    if not toml_path.exists():
        raise RuntimeError(
            f"CRITICAL: build_flags.toml not found at {toml_path}. "
            f"This file is MANDATORY for all compiler operations."
        )

    # Load configuration with strict error handling - NO fallbacks
    build_config = load_build_flags_toml(str(toml_path))
    stub_defines = extract_stub_platform_defines_from_toml(build_config)
    stub_include_paths = extract_stub_platform_include_paths_from_toml(build_config)

    # Convert relative include paths to absolute and create compiler args
    compiler_args: list[str] = []
    for include_path in stub_include_paths:
        if os.path.isabs(include_path):
            compiler_args.append(f"-I{include_path}")
        else:
            # Convert relative path to absolute from project root
            abs_path = os.path.join(os.getcwd(), include_path)
            compiler_args.append(f"-I{abs_path}")

    # Create compiler with proper stub platform settings from configuration
    settings = CompilerOptions(
        include_path="./src",
        defines=stub_defines,
        std_version="c++17",
        compiler_args=compiler_args,  # Include paths from configuration
    )
    # Load build flags from TOML
    build_flags = BuildFlags.parse(toml_path, quick_build=False, strict_mode=False)

    compiler = Compiler(settings, build_flags)

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

    # Get correct path for build_flags.toml (in ci/ directory)
    current_file_dir = Path(__file__).parent.parent
    toml_path = current_file_dir / "build_flags.toml"

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
        # Load build flags from TOML
        build_flags = BuildFlags.parse(toml_path, quick_build=False, strict_mode=False)

        compiler = Compiler(settings, build_flags)

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
