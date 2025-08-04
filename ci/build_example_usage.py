#!/usr/bin/env python3
"""
FastLED Unified Build API - Example Usage

This script demonstrates how to use the new unified build API to replace
the old scattered build logic for both unit tests and examples.

Before (scattered logic):
- PCH building logic in multiple places
- libfastled.a building in callers
- Build flags mixed between unit tests and examples
- Manual directory management

After (unified API):
- Single API for both unit tests and examples
- Automatic PCH and library building
- Clean separation of build configurations
- Organized build directory structure
"""

from pathlib import Path

from ci.build_api import BuildType, create_example_builder, create_unit_test_builder


def demo_unit_test_building():
    """Demonstrate unit test building with the unified API"""
    print("=" * 60)
    print("UNIT TEST BUILDING DEMO")
    print("=" * 60)

    # Create unit test builder - automatically uses ci/build_unit.toml
    unit_builder = create_unit_test_builder(
        build_dir=".build/unit", use_pch=True, parallel=True, clean=False
    )

    # Get build info
    info = unit_builder.get_build_info()
    print(f"Build configuration: {info}")

    # Build multiple unit test targets efficiently
    # The API automatically handles:
    # - PCH building (once, reused by all targets)
    # - libfastled.a building (once, reused by all targets)
    # - Individual target compilation and linking
    test_files = [
        Path("tests/test_json.cpp"),
        Path("tests/test_color.cpp"),
        Path("tests/test_math.cpp"),
    ]

    print(f"\nBuilding {len(test_files)} unit test targets...")
    results = unit_builder.build_targets(test_files)

    # Report results
    for result in results:
        status = "SUCCESS" if result.success else "FAILED"
        print(f"  {result.target.name}: {status} ({result.build_time:.2f}s)")
        if not result.success:
            print(f"    Error: {result.message}")

    print(f"\nBuild artifacts organized in: {unit_builder.build_dir}")
    print(f"  - PCH file: {unit_builder.artifacts_dir}")
    print(f"  - Library: {unit_builder.artifacts_dir}")
    print(f"  - Targets: {unit_builder.targets_dir}")
    print(f"  - Cache: {unit_builder.cache_dir}")


def demo_example_building():
    """Demonstrate example building with the unified API"""
    print("\n" + "=" * 60)
    print("EXAMPLE BUILDING DEMO")
    print("=" * 60)

    # Create example builder - automatically uses ci/build_example.toml
    example_builder = create_example_builder(
        build_dir=".build/examples", use_pch=True, parallel=True, clean=False
    )

    # Get build info
    info = example_builder.get_build_info()
    print(f"Build configuration: {info}")

    # Build multiple example targets efficiently
    # The API automatically handles:
    # - PCH building (once, reused by all targets)
    # - libfastled.a building (once, with example-specific flags)
    # - Individual target compilation and linking
    example_files = [
        Path("examples/Blink/Blink.ino"),
        Path("examples/DemoReel100/DemoReel100.ino"),
        Path("examples/ColorPalette/ColorPalette.ino"),
    ]

    print(f"\nBuilding {len(example_files)} example targets...")
    results = example_builder.build_targets(example_files)

    # Report results
    for result in results:
        status = "SUCCESS" if result.success else "FAILED"
        print(f"  {result.target.name}: {status} ({result.build_time:.2f}s)")
        if not result.success:
            print(f"    Error: {result.message}")

    print(f"\nBuild artifacts organized in: {example_builder.build_dir}")
    print(f"  - PCH file: {example_builder.artifacts_dir}")
    print(f"  - Library: {example_builder.artifacts_dir}")
    print(f"  - Targets: {example_builder.targets_dir}")
    print(f"  - Cache: {example_builder.cache_dir}")


def demo_build_configuration_differences():
    """Demonstrate the differences between unit test and example configurations"""
    print("\n" + "=" * 60)
    print("BUILD CONFIGURATION DIFFERENCES")
    print("=" * 60)

    unit_builder = create_unit_test_builder()
    example_builder = create_example_builder()

    unit_info = unit_builder.get_build_info()
    example_info = example_builder.get_build_info()

    print("Unit Test Configuration:")
    print(f"  - Build flags TOML: {unit_info['build_flags_toml']}")
    print(f"  - Build directory: {unit_info['build_dir']}")
    print(f"  - Build type: {unit_info['build_type']}")
    print(f"  - Includes FASTLED_FORCE_NAMESPACE=1")
    print(f"  - Includes FASTLED_TESTING=1")

    print("\nExample Configuration:")
    print(f"  - Build flags TOML: {example_info['build_flags_toml']}")
    print(f"  - Build directory: {example_info['build_dir']}")
    print(f"  - Build type: {example_info['build_type']}")
    print(f"  - Uses global namespace (no FASTLED_FORCE_NAMESPACE)")
    print(f"  - Arduino compatibility defines")


def demo_clean_builds():
    """Demonstrate clean build functionality"""
    print("\n" + "=" * 60)
    print("CLEAN BUILD DEMO")
    print("=" * 60)

    # Create builders with clean=True to force rebuilding of all artifacts
    unit_builder = create_unit_test_builder(clean=True)
    example_builder = create_example_builder(clean=True)

    print("Unit test builder:")
    print(f"  - Will clean and rebuild all artifacts in {unit_builder.build_dir}")

    print("Example builder:")
    print(f"  - Will clean and rebuild all artifacts in {example_builder.build_dir}")

    # Demonstrate manual cleaning
    print("\nManual cleaning:")
    unit_builder.clean_build_artifacts()
    print(f"  - Cleaned unit test artifacts")

    example_builder.clean_build_artifacts()
    print(f"  - Cleaned example artifacts")


def demo_advanced_usage():
    """Demonstrate advanced usage patterns"""
    print("\n" + "=" * 60)
    print("ADVANCED USAGE DEMO")
    print("=" * 60)

    # Custom build directory structure
    unit_builder = create_unit_test_builder(
        build_dir=".build/my_custom_unit_tests",
        use_pch=False,  # Disable PCH for debugging
        parallel=False,  # Disable parallel for single-threaded debugging
    )

    print("Custom configuration:")
    print(f"  - Custom build directory: {unit_builder.build_dir}")
    print(f"  - PCH disabled for easier debugging")
    print(f"  - Parallel compilation disabled for sequential output")

    # Show the organized directory structure
    print(f"\nOrganized directory structure:")
    print(f"  {unit_builder.build_dir}/")
    print(f"    ├── artifacts/     # PCH files and libfastled.a")
    print(f"    ├── targets/       # Individual target builds")
    print(f"    │   ├── test_json/")
    print(f"    │   │   ├── test_json.o")
    print(f"    │   │   └── test_json.exe")
    print(f"    │   └── test_color/")
    print(f"    │       ├── test_color.o")
    print(f"    │       └── test_color.exe")
    print(f"    └── cache/         # Fingerprint caches")


if __name__ == "__main__":
    print("FastLED Unified Build API - Example Usage")
    print("This demonstrates the new unified build system")

    # Run all demos (commented out to avoid actual compilation during demo)

    # Uncomment these to run actual builds:
    # demo_unit_test_building()
    # demo_example_building()

    # These can run without actual building:
    demo_build_configuration_differences()
    demo_clean_builds()
    demo_advanced_usage()

    print("\n" + "=" * 60)
    print("DEMO COMPLETE")
    print("=" * 60)
    print("The unified build API provides:")
    print("✓ Single API for both unit tests and examples")
    print("✓ Automatic PCH and library building")
    print("✓ Clean separation of build configurations")
    print("✓ Organized build directory structure")
    print("✓ Efficient multiple target building")
    print("✓ Simple, elegant interface")
