#!/usr/bin/env python3
"""
FastLED Build System Migration Example

This shows how to migrate from the old scattered build logic
to the new unified BuildAPI.
"""

from pathlib import Path
from typing import List

from ci.build_api import create_example_builder, create_unit_test_builder


def old_way_unit_test_building():
    """
    OLD WAY: Scattered logic with manual PCH and library building

    This is how unit test building worked before the refactor:
    - Manual PCH building in multiple places
    - Manual library building with duplicated logic
    - Mixed build flags between unit tests and examples
    - Manual directory management
    """

    # OLD CODE (before refactor) - DO NOT USE
    """
    # Manual PCH building (scattered in multiple files)
    from ci.compiler.test_example_compilation import create_fastled_compiler
    from ci.compiler.cpp_test_compile import create_unit_test_fastled_library
    
    compiler = create_fastled_compiler(use_pch=True, use_sccache=False, parallel=True)
    
    # Manual library building (duplicated logic)
    lib_file = create_unit_test_fastled_library(clean=False, use_pch=True)
    
    # Manual target compilation loop
    for test_file in test_files:
        success = compiler.compile_cpp_file(test_file, output_obj)
        if success:
            link_executable(output_obj, lib_file, output_exe)
    """


def new_way_unit_test_building():
    """
    NEW WAY: Unified BuildAPI with automatic PCH and library building

    This is how unit test building works after the refactor:
    - Automatic PCH building (once, reused by all targets)
    - Automatic library building (once, reused by all targets)
    - Clean separation of unit test build flags
    - Automatic directory organization
    """

    # NEW CODE (after refactor) - USE THIS
    unit_builder = create_unit_test_builder(
        build_dir=".build/unit", use_pch=True, parallel=True
    )

    test_files = [
        Path("tests/test_json.cpp"),
        Path("tests/test_color.cpp"),
        Path("tests/test_math.cpp"),
    ]

    # Single call handles everything:
    # - PCH building (automatically, once)
    # - Library building (automatically, once)
    # - Individual target compilation and linking
    results = unit_builder.build_targets(test_files)

    # Clean result handling
    for result in results:
        if result.success:
            print(f"✓ {result.target.name} built successfully")
        else:
            print(f"✗ {result.target.name} failed: {result.message}")


def old_way_example_building():
    """
    OLD WAY: Mixed build configuration and manual management

    Examples used the same build_flags.toml as unit tests, causing:
    - Wrong defines (FASTLED_FORCE_NAMESPACE=1 for examples)
    - Mixed configuration between unit tests and examples
    - Manual PCH and library building
    """

    # OLD CODE (before refactor) - DO NOT USE
    """
    # Used same build_flags.toml as unit tests (wrong!)
    compiler = create_fastled_compiler(use_pch=True, use_sccache=False, parallel=True)
    
    # Manual example compilation with wrong defines
    for example_file in example_files:
        success = compiler.compile_ino_file(example_file, output_obj)
        # ... manual linking logic ...
    """


def new_way_example_building():
    """
    NEW WAY: Clean separation with dedicated build configuration

    Examples now have their own build_example.toml with:
    - Correct defines (no FASTLED_FORCE_NAMESPACE=1)
    - Arduino compatibility defines
    - Automatic PCH and library building
    """

    # NEW CODE (after refactor) - USE THIS
    example_builder = create_example_builder(
        build_dir=".build/examples", use_pch=True, parallel=True
    )

    example_files = [
        Path("examples/Blink/Blink.ino"),
        Path("examples/DemoReel100/DemoReel100.ino"),
    ]

    # Single call handles everything with correct configuration
    results = example_builder.build_targets(example_files)

    for result in results:
        if result.success:
            print(f"✓ {result.target.name} built successfully")
        else:
            print(f"✗ {result.target.name} failed: {result.message}")


def migration_benefits():
    """
    Benefits of the new unified BuildAPI:
    """

    print("MIGRATION BENEFITS:")
    print("=" * 50)

    print("1. UNIFIED API:")
    print(
        "   Before: create_fastled_compiler() + create_unit_test_fastled_library() + manual loops"
    )
    print("   After:  create_unit_test_builder().build_targets(files)")

    print("\n2. CLEAN SEPARATION:")
    print("   Before: build_flags.toml used by both unit tests and examples")
    print("   After:  build_unit.toml vs build_example.toml")

    print("\n3. AUTOMATIC ARTIFACT BUILDING:")
    print("   Before: Manual PCH and library building in callers")
    print("   After:  Automatic PCH and library building in BuildAPI")

    print("\n4. ORGANIZED DIRECTORIES:")
    print("   Before: Mixed artifacts in .build/")
    print("   After:  Organized .build/unit/ and .build/examples/ with subdirectories")

    print("\n5. MULTIPLE TARGET EFFICIENCY:")
    print("   Before: PCH and library built for each target")
    print("   After:  PCH and library built once, reused by all targets")

    print("\n6. SIMPLE ERROR HANDLING:")
    print("   Before: Manual error handling in compilation loops")
    print("   After:  Structured SketchResult objects with clear success/failure")


def migration_checklist():
    """
    Checklist for migrating existing code to the new BuildAPI:
    """

    print("\nMIGRATION CHECKLIST:")
    print("=" * 50)

    print("✓ COMPLETED:")
    print("  - Split ci/build_flags.toml into build_unit.toml and build_example.toml")
    print("  - Created unified BuildAPI class")
    print("  - Implemented automatic PCH and library building")
    print("  - Implemented organized build directory structure")
    print(
        "  - Created convenience functions create_unit_test_builder() and create_example_builder()"
    )

    print("\n⚠ TODO:")
    print(
        "  - Update existing unit test compilation code to use create_unit_test_builder()"
    )
    print(
        "  - Update existing example compilation code to use create_example_builder()"
    )
    print("  - Remove old scattered PCH and library building logic")
    print("  - Update test scripts to use new API")
    print("  - Deprecate old build_flags.toml (marked as legacy)")


def show_api_simplification():
    """
    Show how the API is dramatically simplified:
    """

    print("\nAPI SIMPLIFICATION:")
    print("=" * 50)

    print("BEFORE (scattered across multiple files):")
    print("""
    # In ci/compiler/test_example_compilation.py
    compiler = create_fastled_compiler(use_pch=True, use_sccache=False, parallel=True)
    
    # In ci/compiler/cpp_test_compile.py  
    lib_file = create_unit_test_fastled_library(clean=False, use_pch=True)
    
    # In test runner
    for test_file in test_files:
        obj_file = build_dir / f"{test_file.stem}.o"
        success = compiler.compile_cpp_file(test_file, obj_file)
        if success:
            exe_file = build_dir / f"{test_file.stem}.exe"
            link_success = link_executable(obj_file, lib_file, exe_file)
    """)

    print("AFTER (unified API):")
    print("""
    # Single unified API
    builder = create_unit_test_builder()
    results = builder.build_targets(test_files)
    
    # That's it! Everything else is automatic:
    # - PCH building
    # - Library building  
    # - Individual compilation
    # - Linking
    # - Directory organization
    # - Error handling
    """)


if __name__ == "__main__":
    print("FastLED Build System Migration Example")
    print("=" * 60)

    migration_benefits()
    migration_checklist()
    show_api_simplification()

    print("\n" + "=" * 60)
    print("NEXT STEPS:")
    print("1. Update existing unit test compilation to use create_unit_test_builder()")
    print("2. Update existing example compilation to use create_example_builder()")
    print("3. Remove old scattered build logic")
    print("4. Test the new unified system")
    print("=" * 60)
