#!/usr/bin/env python3

"""
FastLED Program Linking Demonstration
=====================================

This script demonstrates the new program linking capabilities added to the
FastLED build system. It shows how to:

1. Compile .ino files to object files
2. Create static libraries from FastLED source
3. Link object files and libraries into executable programs
4. Use platform-specific linker arguments

Based on the FEATURE.md specification for Program Linking Support.
"""

import platform
import sys
import tempfile
from pathlib import Path


_HERE = Path(__file__).parent
_PROJECT_ROOT = _HERE.parent

from ci.clang_compiler import (
    Compiler,
    CompilerOptions,
    LinkOptions,
    add_library_paths,
    add_system_libraries,
    get_common_linker_args,
)


def demonstrate_basic_linking():
    """Demonstrate basic program linking functionality."""
    print("=" * 70)
    print("BASIC PROGRAM LINKING DEMONSTRATION")
    print("=" * 70)

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Step 1: Set up compiler with FastLED settings
        print("\n1. Setting up compiler...")
        compiler_options = CompilerOptions(
            include_path=str(_PROJECT_ROOT / "src"),
            defines=["STUB_PLATFORM", "ARDUINO=10808"],
            std_version="c++17",
            compiler_args=[f"-I{_PROJECT_ROOT}/src/platforms/stub"],
        )
        compiler = Compiler(compiler_options)
        print(f"   ✓ Compiler configured for {platform.system()}")

        # Step 2: Create dummy object files (since we can't compile without Arduino.h)
        print("\n2. Creating demo object files...")
        dummy_main = temp_path / "main.o"
        dummy_utils = temp_path / "utils.o"

        # Create dummy object files with minimal content for demonstration
        dummy_main.write_bytes(b"dummy object file content for main")
        dummy_utils.write_bytes(b"dummy object file content for utils")
        print(f"   ✓ Created {dummy_main.name}")
        print(f"   ✓ Created {dummy_utils.name}")

        # Step 3: Create dummy static library
        print("\n3. Creating demo static library...")
        dummy_lib_obj = temp_path / "lib.o"
        dummy_lib_obj.write_bytes(b"dummy library object content")

        static_lib = temp_path / "libfastled.a"
        archive_result = compiler.create_archive([dummy_lib_obj], static_lib).result()

        if archive_result.ok:
            print(f"   ✓ Created {static_lib.name}")
        else:
            print(f"   ✗ Library creation failed: {archive_result.stderr}")
            return False

        # Step 4: Demonstrate platform-specific linker arguments
        print("\n4. Generating platform-specific linker arguments...")

        # Get common arguments
        common_args = get_common_linker_args(debug=True, optimize=True)
        print(f"   Common args: {common_args}")

        # Add system libraries
        system_libs = []
        if platform.system() == "Windows":
            add_system_libraries(system_libs, ["kernel32", "user32"], "Windows")
        else:
            add_system_libraries(system_libs, ["pthread", "m"], platform.system())
        print(f"   System libraries: {system_libs}")

        # Add library paths
        lib_paths = []
        add_library_paths(lib_paths, ["/usr/local/lib"], platform.system())
        print(f"   Library paths: {lib_paths}")

        # Step 5: Set up linking options
        print("\n5. Configuring linking options...")
        executable_name = (
            "demo_program.exe" if platform.system() == "Windows" else "demo_program"
        )
        executable_path = temp_path / executable_name

        # Combine all arguments
        all_linker_args = common_args + system_libs + lib_paths

        link_options = LinkOptions(
            output_executable=str(executable_path),
            object_files=[str(dummy_main), str(dummy_utils)],
            static_libraries=[str(static_lib)],
            linker_args=all_linker_args,
        )

        print(f"   Output: {executable_name}")
        print(f"   Object files: {len(link_options.object_files)}")
        print(f"   Libraries: {len(link_options.static_libraries)}")
        print(f"   Linker args: {len(link_options.linker_args)}")

        # Step 6: Attempt linking (expected to fail with dummy files)
        print("\n6. Attempting linking...")
        link_result = compiler.link_program(link_options).result()

        if link_result.ok:
            print(f"   ✓ Program linked successfully!")
            print(f"   Executable: {executable_path}")
            if executable_path.exists():
                size = executable_path.stat().st_size
                print(f"   Size: {size:,} bytes")
        else:
            print(f"   ✗ Linking failed (expected with dummy files):")
            print(f"     {link_result.stderr[:200]}...")
            print("   ℹ️  This is expected since we used dummy object files")

        print("\n" + "=" * 70)
        print("DEMONSTRATION COMPLETED")
        print("=" * 70)

        return True


def demonstrate_helper_functions():
    """Demonstrate the helper functions for different platforms."""
    print("\n" + "=" * 70)
    print("HELPER FUNCTIONS DEMONSTRATION")
    print("=" * 70)

    # Test different platform configurations
    platforms = ["Windows", "Linux", "Darwin"]

    for platform_name in platforms:
        print(f"\n--- {platform_name} Configuration ---")

        # Common linker arguments
        args = get_common_linker_args(
            platform_name, debug=True, optimize=True, static_runtime=True
        )
        print(f"Common args: {args}")

        # System libraries
        if platform_name == "Windows":
            libs = ["kernel32", "user32", "gdi32", "winmm"]
        else:
            libs = ["pthread", "m", "dl", "rt"]

        lib_args = []
        add_system_libraries(lib_args, libs, platform_name)
        print(f"System libs: {lib_args}")

        # Library paths
        if platform_name == "Windows":
            paths = ["C:\\Program Files\\lib", "C:\\local\\lib"]
        else:
            paths = ["/usr/lib", "/usr/local/lib", "/opt/lib"]

        path_args = []
        add_library_paths(path_args, paths, platform_name)
        print(f"Library paths: {path_args}")


def demonstrate_linker_detection():
    """Demonstrate automatic linker detection."""
    print("\n" + "=" * 70)
    print("LINKER DETECTION DEMONSTRATION")
    print("=" * 70)

    from ci.clang_compiler import detect_linker

    try:
        linker = detect_linker()
        print(f"\n✓ Detected linker: {linker}")
        print(f"  Platform: {platform.system()}")

        # Show linker type
        if "lld-link" in linker:
            print("  Type: LLVM LLD (Windows)")
        elif "link" in linker and platform.system() == "Windows":
            print("  Type: Microsoft MSVC Linker")
        elif "mold" in linker:
            print("  Type: Modern Linker (mold)")
        elif "lld" in linker:
            print("  Type: LLVM LLD")
        elif "gold" in linker:
            print("  Type: GNU Gold Linker")
        elif "ld" in linker:
            print("  Type: GNU LD")
        else:
            print("  Type: Unknown")

    except RuntimeError as e:
        print(f"\n✗ Linker detection failed: {e}")


def main():
    """Main demonstration function."""
    print("FastLED Program Linking Support Demonstration")
    print(f"Platform: {platform.system()}")
    print(f"Python: {sys.version}")

    try:
        # Run demonstrations
        demonstrate_linker_detection()
        demonstrate_helper_functions()
        demonstrate_basic_linking()

        print("\n🎉 All demonstrations completed successfully!")
        print("\nNext steps:")
        print("- Run the full integration tests: bash test")
        print("- See ci/tests/test_archive_creation.py for comprehensive tests")
        print("- Use the linking API in your own build scripts")

        return True

    except Exception as e:
        print(f"\n❌ Demonstration failed: {e}")
        import traceback

        traceback.print_exc()
        return False


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
