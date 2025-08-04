#!/usr/bin/env python3
"""
FastLED Dynamic Library Builder
Builds FastLED as a shared/dynamic library for unit testing
"""

import os
import subprocess
import sys
from pathlib import Path
from typing import List

from ci.compiler.clang_compiler import (
    BuildFlags,
    Compiler,
    CompilerOptions,
    Result,
)
from ci.util.paths import PROJECT_ROOT


def build_fastled_dynamic_library(build_dir: Path) -> Path:
    """Build FastLED as a dynamic library"""
    print("Building FastLED dynamic library...")

    # Define library path with appropriate extension
    if sys.platform == "win32":
        fastled_lib_path = build_dir / "fastled.dll"
    else:
        fastled_lib_path = build_dir / "libfastled.so"

    # Configure compiler with shared library flags
    settings = CompilerOptions(
        include_path=str(Path(PROJECT_ROOT) / "src"),
        defines=[
            "STUB_PLATFORM",
            "ARDUINO=10808",
            "FASTLED_USE_STUB_ARDUINO",
            "SKETCH_HAS_LOTS_OF_MEMORY=1",
            "FASTLED_STUB_IMPL",
            "FASTLED_FORCE_NAMESPACE=1",
            "FASTLED_TESTING",
            "FASTLED_NO_AUTO_NAMESPACE",
            "FASTLED_NO_PINMAP",
            "HAS_HARDWARE_PIN_SUPPORT",
            # "FASTLED_DEBUG_LEVEL=1",
            # "BUILDING_FASTLED_DLL",  # New define for DLL exports
            # "FASTLED_EXPORT=__declspec(dllexport)" if sys.platform == "win32" else "FASTLED_EXPORT=",  # DLL exports for Windows
        ],
        std_version="c++17",
        compiler="clang++",
        compiler_args=[
            # NOTE: All compiler flags should come from build_unit.toml
            # Keep only platform-specific include paths and shared library flags
            "-I" + str(Path(PROJECT_ROOT) / "src/platforms/stub"),
            "-I" + str(Path(PROJECT_ROOT) / "tests"),
            # Add shared library flags - platform specific
            "-shared" if sys.platform != "win32" else "/DLL",
            "-fPIC" if sys.platform != "win32" else "",
        ],
        use_pch=True,
        parallel=True,
    )

    # Load build flags from TOML
    build_flags_path = Path(PROJECT_ROOT) / "ci" / "build_unit.toml"
    build_flags = BuildFlags.parse(
        build_flags_path, quick_build=False, strict_mode=False
    )

    compiler = Compiler(settings, build_flags)

    # Compile all FastLED source files
    fastled_src_dir = Path(PROJECT_ROOT) / "src"
    all_cpp_files = list(fastled_src_dir.rglob("*.cpp"))

    print(f"Found {len(all_cpp_files)} FastLED source files")

    # Compile to object files
    object_files: List[Path] = []
    for src_file in all_cpp_files:
        relative_path = src_file.relative_to(fastled_src_dir)
        safe_name = (
            str(relative_path.with_suffix("")).replace("/", "_").replace("\\", "_")
        )
        obj_path = build_dir / f"{safe_name}_fastled.o"

        future = compiler.compile_cpp_file(
            src_file,
            output_path=obj_path,
            additional_flags=[
                "-c",
                "-DFASTLED_STUB_IMPL",
                "-DFASTLED_FORCE_NAMESPACE=1",
                "-DFASTLED_NO_AUTO_NAMESPACE",
                "-DFASTLED_NO_PINMAP",
                "-DPROGMEM=",
                "-DHAS_HARDWARE_PIN_SUPPORT",
                "-DFASTLED_ENABLE_JSON=1",
                "-fno-exceptions",
                "-fno-rtti",
                "-fPIC",  # Position Independent Code for shared library
            ],
        )
        result: Result = future.result()
        if not result.ok:
            print(f"ERROR: Failed to compile {src_file}: {result.stderr}")
            continue

        object_files.append(obj_path)

    print(f"Linking {len(object_files)} object files into dynamic library...")

    # Link as shared library
    if sys.platform == "win32":
        # Use lld-link for Windows DLL creation
        link_cmd = [
            "lld-link",
            "/DLL",
            "/NOLOGO",
            "/OUT:" + str(fastled_lib_path),
            "/LIBPATH:C:/Program Files (x86)/Windows Kits/10/Lib/10.0.19041.0/um/x64",
            "/LIBPATH:C:/Program Files (x86)/Windows Kits/10/Lib/10.0.19041.0/ucrt/x64",
            "/LIBPATH:C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.37.32822/lib/x64",
            *[str(obj) for obj in object_files],
            "msvcrt.lib",
            "legacy_stdio_definitions.lib",
            "kernel32.lib",
            "user32.lib",
        ]
    else:
        link_cmd = [
            "clang++",
            "-shared",
            "-o",
            str(fastled_lib_path),
            *[str(obj) for obj in object_files],
        ]

    try:
        # Use streaming to prevent buffer overflow
        process = subprocess.Popen(
            link_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
            encoding="utf-8",
            errors="replace",
        )

        stdout_lines: list[str] = []
        stderr_lines: list[str] = []

        while True:
            stdout_line = process.stdout.readline() if process.stdout else ""
            stderr_line = process.stderr.readline() if process.stderr else ""

            if stdout_line:
                stdout_lines.append(stdout_line.rstrip())
            if stderr_line:
                stderr_lines.append(stderr_line.rstrip())

            if process.poll() is not None:
                remaining_stdout = process.stdout.read() if process.stdout else ""
                remaining_stderr = process.stderr.read() if process.stderr else ""

                if remaining_stdout:
                    for line in remaining_stdout.splitlines():
                        stdout_lines.append(line.rstrip())
                if remaining_stderr:
                    for line in remaining_stderr.splitlines():
                        stderr_lines.append(line.rstrip())
                break

        stderr_result = "\n".join(stderr_lines)
        if process.returncode != 0:
            raise Exception(f"Failed to create dynamic library: {stderr_result}")

        print(f"Successfully created dynamic library: {fastled_lib_path}")
        return fastled_lib_path

    except Exception as e:
        print(f"ERROR: Exception during library creation: {e}")
        raise


if __name__ == "__main__":
    # Create build directory in temp
    import tempfile

    build_dir = Path(tempfile.gettempdir()) / "fastled_test_build"
    build_dir.mkdir(parents=True, exist_ok=True)

    # Build the dynamic library
    build_fastled_dynamic_library(build_dir)
