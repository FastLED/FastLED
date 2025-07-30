#!/usr/bin/env python3

"""
Clang compiler accessibility and testing functions for FastLED.
Supports the simple build system approach outlined in FEATURE.md.
"""

import os
import platform
import shutil
import subprocess
import tempfile
from concurrent.futures import Future, ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path
from typing import Sequence, cast


def cpu_count() -> int:
    # get the nubmer of cpus on the machine
    import multiprocessing

    return multiprocessing.cpu_count() or 1


_EXECUTOR = ThreadPoolExecutor(max_workers=cpu_count() * 2)


@dataclass
class LibarchiveOptions:
    """Configuration options for static library archive generation."""

    use_thin: bool = False  # Use thin archives (ar T flag) for faster linking
    # Future expansion points:
    # use_deterministic: bool = True  # Deterministic archives (ar D flag)
    # symbol_table: bool = True       # Generate symbol table (ar s flag)
    # verbose: bool = False          # Verbose output (ar v flag)


@dataclass
class CompilerOptions:
    """
    Configuration options for compilation operations.
    Merged from former CompileOptions and CompilerSettings.
    """

    # Core compiler settings (formerly CompilerSettings)
    include_path: str
    compiler: str = "clang++"
    defines: list[str] | None = None
    std_version: str = "c++17"
    compiler_args: list[str] = field(default_factory=list)

    # PCH (Precompiled Headers) settings
    use_pch: bool = False
    pch_header_content: str | None = None
    pch_output_path: str | None = None

    # Archive generation settings
    archiver: str = "ar"  # Default archiver tool
    archiver_args: list[str] = field(default_factory=list)

    # Compilation operation settings (formerly CompileOptions, unity options removed)
    additional_flags: list[str] = field(default_factory=list)  # Extra compiler flags
    parallel: bool = True  # Enable parallel compilation
    temp_dir: str | Path | None = None  # Custom temporary directory


@dataclass
class Result:
    """
    Result of a subprocess.run command execution.
    """

    ok: bool
    stdout: str
    stderr: str
    return_code: int


@dataclass
class CompileResult:
    """
    Result of a compile operation.
    """

    success: bool
    output_path: str
    stderr: str
    size: int
    command: list[str]


@dataclass
class VersionCheckResult:
    """
    Result of a clang version check.
    """

    success: bool
    version: str
    error: str


@dataclass
class LinkOptions:
    """Configuration options for program linking operations."""

    # Output configuration
    output_executable: str | Path  # Output executable path

    # Input files
    object_files: list[str | Path] = field(default_factory=list)  # .o files to link
    static_libraries: list[str | Path] = field(default_factory=list)  # .a files to link

    # Linker configuration
    linker: str | None = None  # Custom linker path (auto-detected if None)
    linker_args: list[str] = field(
        default_factory=list
    )  # All linker command line arguments

    # Platform-specific defaults can be added via helper functions
    temp_dir: str | Path | None = (
        None  # Custom temporary directory for intermediate files
    )


class Compiler:
    """
    Compiler wrapper for FastLED .ino file compilation.
    """

    def __init__(self, settings: CompilerOptions):
        self.settings = settings
        self._pch_file_path: Path | None = None
        self._pch_header_path: Path | None = None
        self._pch_ready: bool = False

    def get_compiler_args(self) -> list[str]:
        """
        Get a copy of the complete compiler arguments that would be used for compilation.

        Returns:
            list[str]: Copy of compiler arguments including compiler, flags, defines, and settings
        """
        cmd = [self.settings.compiler]

        # Handle cache-wrapped compilers (sccache/ccache)
        if (
            len(self.settings.compiler_args) > 0
            and self.settings.compiler_args[0] == "clang++"
        ):
            # This is a cache-wrapped compiler, add clang++ first, then remaining cache args
            cmd.append("clang++")
            # Skip the first "clang++" argument from compiler_args since we already added it
            remaining_cache_args = self.settings.compiler_args[1:]
        else:
            # This is a direct compiler call, use all compiler_args as-is
            remaining_cache_args = self.settings.compiler_args

        # Add standard clang arguments
        cmd.extend(
            [
                "-x",
                "c++",  # Force C++ compilation of .ino files
                f"-std={self.settings.std_version}",
                f"-I{self.settings.include_path}",  # FastLED include path
            ]
        )

        # Add defines if specified
        if self.settings.defines:
            for define in self.settings.defines:
                cmd.append(f"-D{define}")

        # Add remaining compiler args (after skipping clang++ for cache-wrapped compilers)
        cmd.extend(remaining_cache_args)

        # Add PCH flag if available
        if self.settings.use_pch and self._pch_ready and self._pch_file_path:
            cmd.extend(["-include-pch", str(self._pch_file_path)])

        # Add standard compilation flags
        cmd.extend(["-c"])

        return cmd.copy()  # Return a copy to prevent modification

    def generate_pch_header(self) -> str:
        """
        Generate the default PCH header content with commonly used FastLED headers.

        Returns:
            str: PCH header content with common includes
        """
        if self.settings.pch_header_content:
            return self.settings.pch_header_content

        # Default PCH content based on common FastLED example includes
        default_content = """// FastLED PCH - Common headers for faster compilation
#pragma once

// Core headers that are used in nearly all FastLED examples
#include <Arduino.h>
#include <FastLED.h>

// Common C++ standard library headers
#include <string>
#include <vector>
#include <stdio.h>
"""
        return default_content

    def create_pch_file(self) -> bool:
        """
        Create a precompiled header file for faster compilation.

        IMPORTANT: PCH generation bypasses sccache and uses direct clang++ compilation
        to avoid compatibility issues. Only the actual translation unit compiles go through sccache.

        Returns:
            bool: True if PCH creation was successful, False otherwise
        """
        if not self.settings.use_pch:
            return False

        try:
            # Create PCH header content
            pch_content = self.generate_pch_header()

            # Create temporary PCH header file
            pch_header_file = tempfile.NamedTemporaryFile(
                mode="w", suffix=".hpp", delete=False
            )
            pch_header_file.write(pch_content)
            pch_header_path = Path(pch_header_file.name)
            pch_header_file.close()

            # Create PCH output path
            if self.settings.pch_output_path:
                pch_output_path = Path(self.settings.pch_output_path)
            else:
                pch_output_path = pch_header_path.with_suffix(".hpp.pch")

            # Build PCH compilation command - BYPASS sccache for PCH generation
            # Use direct clang++ compiler, not the cache-wrapped version
            direct_compiler = "clang++"

            cmd = [
                direct_compiler,  # Always use direct clang++ for PCH
                "-x",
                "c++-header",
                f"-std={self.settings.std_version}",
                f"-I{self.settings.include_path}",
            ]

            # Add defines if specified
            if self.settings.defines:
                for define in self.settings.defines:
                    cmd.append(f"-D{define}")

            # Add compiler args but skip cache-related args
            # Filter out sccache/ccache wrapper arguments that don't apply to direct compilation
            filtered_args: list[str] = []
            skip_next = False

            for arg in self.settings.compiler_args:
                if skip_next:
                    skip_next = False
                    continue

                # Skip cache wrapper arguments
                if arg in ["clang++", "gcc", "g++"]:
                    # These are cache wrapper args, skip them for direct PCH compilation
                    continue
                elif arg.startswith("-include-pch"):
                    # Skip any existing PCH arguments
                    skip_next = True  # Skip the PCH file path argument too
                    continue
                else:
                    # Keep all other compiler arguments for PCH compatibility
                    filtered_args.append(arg)

            cmd.extend(filtered_args)
            cmd.extend([str(pch_header_path), "-o", str(pch_output_path)])

            # Compile PCH with direct compiler (no sccache)
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)

            if result.returncode == 0:
                self._pch_file_path = pch_output_path
                self._pch_header_path = (
                    pch_header_path  # Keep track of header file for cleanup
                )
                self._pch_ready = True
                return True
            else:
                # PCH compilation failed, clean up and continue without PCH
                # Print debug info to help diagnose PCH issues
                print(f"[DEBUG] PCH compilation failed with command: {' '.join(cmd)}")
                print(f"[DEBUG] PCH error output: {result.stderr}")
                os.unlink(pch_header_path)
                return False

        except Exception as e:
            # PCH creation failed, continue without PCH
            print(f"[DEBUG] PCH creation exception: {e}")
            return False

    def cleanup_pch(self):
        """Clean up PCH files."""
        if self._pch_file_path and self._pch_file_path.exists():
            try:
                os.unlink(self._pch_file_path)
            except:
                pass
        if self._pch_header_path and self._pch_header_path.exists():
            try:
                os.unlink(self._pch_header_path)
            except:
                pass
        self._pch_file_path = None
        self._pch_header_path = None
        self._pch_ready = False

    def check_clang_version(self) -> VersionCheckResult:
        """
        Check that clang++ is accessible and return version information.
        Handles cache-wrapped compilers (sccache/ccache) properly.

        Returns:
            VersionCheckResult: Result containing success status, version string, and error message
        """
        # Test uv run access first
        try:
            # Determine the correct command to check clang version
            if (
                len(self.settings.compiler_args) > 0
                and self.settings.compiler_args[0] == "clang++"
            ):
                # This is a cache-wrapped compiler (sccache/ccache clang++)
                version_cmd = [self.settings.compiler, "clang++", "--version"]
            else:
                # This is a direct compiler call
                version_cmd = [self.settings.compiler, "--version"]

            result = subprocess.run(
                version_cmd,
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode == 0 and "clang version" in result.stdout:
                version = (
                    result.stdout.split()[2]
                    if len(result.stdout.split()) > 2
                    else "unknown"
                )
                return VersionCheckResult(
                    success=True, version=f"uv:{version}", error=""
                )
            else:
                return VersionCheckResult(
                    success=False,
                    version="",
                    error=f"clang++ version check failed: {result.stderr}",
                )
        except Exception as e:
            return VersionCheckResult(
                success=False, version="", error=f"clang++ not accessible: {str(e)}"
            )

    def compile_ino_file(
        self,
        ino_path: str | Path,
        output_path: str | Path | None = None,
        additional_flags: list[str] | None = None,
        use_pch_for_this_file: bool | None = None,
    ) -> Future[Result]:
        """
        Compile a single .ino file using clang++ with optional temporary output.

        Args:
            ino_path (str|Path): Path to the .ino file to compile
            output_path (str|Path, optional): Output object file path. If None, uses temp file.
            additional_flags (list, optional): Additional compiler flags
            use_pch_for_this_file (bool, optional): Override PCH usage for this specific file.
                If None, uses global PCH setting. If True/False, forces PCH on/off for this file.

        Returns:
            Future[Result]: Future that will contain Result dataclass with subprocess execution details
        """
        # Submit the compilation task to a thread pool

        return _EXECUTOR.submit(
            self._compile_ino_file_sync,
            ino_path,
            output_path,
            additional_flags,
            use_pch_for_this_file,
        )

    def _compile_ino_file_sync(
        self,
        ino_path: str | Path,
        output_path: str | Path | None = None,
        additional_flags: list[str] | None = None,
        use_pch_for_this_file: bool | None = None,
    ) -> Result:
        """
        Internal synchronous implementation of compile_ino_file.

        Args:
            ino_path (str|Path): Path to the .ino file to compile
            output_path (str|Path, optional): Output object file path. If None, uses temp file.
            additional_flags (list, optional): Additional compiler flags
            use_pch_for_this_file (bool, optional): Override PCH usage for this specific file.
                If None, uses global PCH setting. If True/False, forces PCH on/off for this file.

        Returns:
            Result: Result dataclass containing subprocess execution details
        """
        ino_file_path = Path(ino_path).resolve()

        # Create output path if not provided
        if output_path is None:
            temp_file = tempfile.NamedTemporaryFile(suffix=".o", delete=False)
            output_path = temp_file.name
            temp_file.close()
            cleanup_temp = True
        else:
            cleanup_temp = False

        # Build compiler command
        cmd = [self.settings.compiler]

        # Handle cache-wrapped compilers (sccache/ccache)
        if (
            len(self.settings.compiler_args) > 0
            and self.settings.compiler_args[0] == "clang++"
        ):
            # This is a cache-wrapped compiler, add clang++ first, then remaining cache args
            cmd.append("clang++")
            # Skip the first "clang++" argument from compiler_args since we already added it
            remaining_cache_args = self.settings.compiler_args[1:]
        else:
            # This is a direct compiler call, use all compiler_args as-is
            remaining_cache_args = self.settings.compiler_args

        # Add standard clang arguments
        cmd.extend(
            [
                "-x",
                "c++",  # Force C++ compilation of .ino files
                f"-std={self.settings.std_version}",
                f"-I{self.settings.include_path}",  # FastLED include path
            ]
        )

        # Add defines if specified
        if self.settings.defines:
            for define in self.settings.defines:
                cmd.append(f"-D{define}")

        # Add remaining compiler args (after skipping clang++ for cache-wrapped compilers)
        cmd.extend(remaining_cache_args)

        # Determine whether to use PCH for this specific file
        should_use_pch = (
            use_pch_for_this_file
            if use_pch_for_this_file is not None
            else self.settings.use_pch
        )

        # Add PCH flag if available and should be used for this file
        if should_use_pch and self._pch_ready and self._pch_file_path:
            cmd.extend(["-include-pch", str(self._pch_file_path)])

        cmd.extend(
            [
                "-c",
                str(ino_file_path),  # Compile .ino file directly
                "-o",
                str(output_path),
            ]
        )

        if additional_flags:
            cmd.extend(additional_flags)

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)

            # Clean up temp file on failure if we created it
            if cleanup_temp and result.returncode != 0 and os.path.exists(output_path):
                try:
                    os.unlink(output_path)
                except:
                    pass

            return Result(
                ok=(result.returncode == 0),
                stdout=result.stdout,
                stderr=result.stderr,
                return_code=result.returncode,
            )

        except Exception as e:
            if cleanup_temp and os.path.exists(output_path):
                try:
                    os.unlink(output_path)
                except:
                    pass

            return Result(ok=False, stdout="", stderr=str(e), return_code=-1)

    def compile_cpp_file(
        self,
        cpp_path: str | Path,
        output_path: str | Path | None = None,
        additional_flags: list[str] | None = None,
        use_pch_for_this_file: bool | None = None,
    ) -> Future[Result]:
        """
        Compile a .cpp file asynchronously.

        Args:
            cpp_path (str|Path): Path to the .cpp file to compile
            output_path (str|Path, optional): Output object file path. If None, uses temp file.
            additional_flags (list, optional): Additional compiler flags
            use_pch_for_this_file (bool, optional): Override PCH usage for this specific file.
                If None, uses global PCH setting. If True/False, forces PCH on/off for this file.

        Returns:
            Future[Result]: Future object that will contain the compilation result
        """
        # Submit the compilation task to a thread pool
        return _EXECUTOR.submit(
            self._compile_cpp_file_sync,
            cpp_path,
            output_path,
            additional_flags,
            use_pch_for_this_file,
        )

    def _compile_cpp_file_sync(
        self,
        cpp_path: str | Path,
        output_path: str | Path | None = None,
        additional_flags: list[str] | None = None,
        use_pch_for_this_file: bool | None = None,
    ) -> Result:
        """
        Internal synchronous implementation of compile_cpp_file.

        Args:
            cpp_path (str|Path): Path to the .cpp file to compile
            output_path (str|Path, optional): Output object file path. If None, uses temp file.
            additional_flags (list, optional): Additional compiler flags
            use_pch_for_this_file (bool, optional): Override PCH usage for this specific file.
                If None, uses global PCH setting. If True/False, forces PCH on/off for this file.

        Returns:
            Result: Result dataclass containing subprocess execution details
        """
        cpp_file_path = Path(cpp_path).resolve()

        # Create output path if not provided
        if output_path is None:
            temp_file = tempfile.NamedTemporaryFile(suffix=".o", delete=False)
            output_path = temp_file.name
            temp_file.close()
            cleanup_temp = True
        else:
            cleanup_temp = False

        # Build compiler command
        cmd = [self.settings.compiler]

        # Handle cache-wrapped compilers (sccache/ccache)
        if (
            len(self.settings.compiler_args) > 0
            and self.settings.compiler_args[0] == "clang++"
        ):
            # This is a cache-wrapped compiler, add clang++ first, then remaining cache args
            cmd.append("clang++")
            # Skip the first "clang++" argument from compiler_args since we already added it
            remaining_cache_args = self.settings.compiler_args[1:]
        else:
            # This is a direct compiler call, use all compiler_args as-is
            remaining_cache_args = self.settings.compiler_args

        # Add standard clang arguments
        cmd.extend(
            [
                "-x",
                "c++",  # Force C++ compilation
                f"-std={self.settings.std_version}",
                f"-I{self.settings.include_path}",  # FastLED include path
            ]
        )

        # Add defines if specified
        if self.settings.defines:
            for define in self.settings.defines:
                cmd.append(f"-D{define}")

        # Add remaining compiler args (after skipping clang++ for cache-wrapped compilers)
        cmd.extend(remaining_cache_args)

        # Determine whether to use PCH for this specific file
        should_use_pch = (
            use_pch_for_this_file
            if use_pch_for_this_file is not None
            else self.settings.use_pch
        )

        # Add PCH flag if available and should be used for this file
        if should_use_pch and self._pch_ready and self._pch_file_path:
            cmd.extend(["-include-pch", str(self._pch_file_path)])

        cmd.extend(
            [
                "-c",
                str(cpp_file_path),  # Compile the .cpp file directly
                "-o",
                str(output_path),
            ]
        )

        if additional_flags:
            cmd.extend(additional_flags)

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)

            # Clean up temp file on failure if we created it
            if cleanup_temp and result.returncode != 0 and os.path.exists(output_path):
                try:
                    os.unlink(output_path)
                except:
                    pass

            return Result(
                ok=(result.returncode == 0),
                stdout=result.stdout,
                stderr=result.stderr,
                return_code=result.returncode,
            )

        except Exception as e:
            if cleanup_temp and os.path.exists(output_path):
                try:
                    os.unlink(output_path)
                except:
                    pass

            return Result(ok=False, stdout="", stderr=str(e), return_code=-1)

    def find_ino_files(
        self, examples_dir: str | Path, filter_names: list[str] | None = None
    ) -> list[Path]:
        """
        Find all .ino files in the specified examples directory.

        Args:
            examples_dir (str|Path): Directory containing .ino files
            filter_names (list, optional): Only include files with these stem names (case-insensitive)

        Returns:
            list[Path]: List of .ino file paths
        """
        examples_dir = Path(examples_dir)
        ino_files = list(examples_dir.rglob("*.ino"))

        if filter_names:
            # Create case-insensitive mapping for better user experience
            filter_lower = {name.lower() for name in filter_names}
            ino_files = [f for f in ino_files if f.stem.lower() in filter_lower]

        return ino_files

    def find_cpp_files_for_example(self, ino_file: Path) -> list[Path]:
        """
        Find all .cpp files in the same directory as the given .ino file and its subdirectories.

        Args:
            ino_file (Path): Path to the .ino file

        Returns:
            list[Path]: List of .cpp file paths in the example directory tree
        """
        example_dir = ino_file.parent
        # Search recursively for .cpp files in the example directory and all subdirectories
        cpp_files = list(example_dir.rglob("*.cpp"))
        return cpp_files

    def find_include_dirs_for_example(self, ino_file: Path) -> list[str]:
        """
        Find all directories that should be added as include paths for the given example.
        This includes the example root directory and any subdirectories that contain header files.

        Args:
            ino_file (Path): Path to the .ino file

        Returns:
            list[str]: List of directory paths that should be added as -I flags
        """
        example_dir = ino_file.parent
        include_dirs: list[str] = []

        # Always include the example root directory
        include_dirs.append(str(example_dir))

        # Check for header files in subdirectories
        for subdir in example_dir.iterdir():
            if subdir.is_dir() and subdir.name not in [
                ".git",
                "__pycache__",
                ".pio",
                ".vscode",
            ]:
                # Check if this directory contains header files
                header_files = [
                    f
                    for f in subdir.rglob("*")
                    if f.is_file() and f.suffix in [".h", ".hpp"]
                ]
                if header_files:
                    include_dirs.append(str(subdir))

        return include_dirs

    def test_clang_accessibility(self) -> bool:
        """
        Comprehensive test that clang++ is accessible and working properly.
        This is the foundational test for the simple build system approach.

        Returns:
            bool: True if all tests pass, False otherwise
        """
        print("Testing clang++ accessibility...")

        # Test 1: Check clang++ version
        version_result = self.check_clang_version()
        if not version_result.success:
            print(f"X clang++ version check failed: {version_result.error}")
            return False
        print(f"[OK] clang++ version: {version_result.version}")

        # Test 2: Simple compilation test with Blink example
        print("Testing Blink.ino compilation...")
        future_result = self.compile_ino_file("examples/Blink/Blink.ino")
        result = future_result.result()  # Wait for completion and get result

        if not result.ok:
            print(f"X Blink compilation failed: {result.stderr}")
            return False

        # Check if output file was created and has reasonable size
        # Since we use temp file, we can't easily check size here
        print(f"[OK] Blink.ino compilation: return code {result.return_code}")

        # Test 3: Complex example compilation test with DemoReel100
        print("Testing DemoReel100.ino compilation...")
        future_result = self.compile_ino_file("examples/DemoReel100/DemoReel100.ino")
        result = future_result.result()  # Wait for completion and get result

        if not result.ok:
            print(f"X DemoReel100 compilation failed: {result.stderr}")
            return False

        print(f"[OK] DemoReel100.ino compilation: return code {result.return_code}")

        print("[INFO] All clang accessibility tests passed!")
        print("[OK] Implementation ready: Simple build system can proceed")
        return True

    def _cleanup_file(self, file_path: str | Path) -> None:
        """Clean up a file, ignoring errors."""
        try:
            if os.path.exists(file_path):
                os.unlink(file_path)
        except:
            pass

    def analyze_ino_for_pch_compatibility(self, ino_path: str | Path) -> bool:
        """
        Analyze a .ino file to determine if it's compatible with PCH.

        PCH is incompatible if the file has any defines, includes, or other preprocessor
        directives before including FastLED.h, as this can cause conflicts with the
        precompiled header assumptions.

        Args:
            ino_path (str|Path): Path to the .ino file to analyze

        Returns:
            bool: True if compatible with PCH, False if PCH should be disabled
        """
        try:
            ino_file_path = Path(ino_path).resolve()

            with open(ino_file_path, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()

            lines = content.split("\n")
            found_fastled_include = False

            for line_num, line in enumerate(lines, 1):
                # Strip whitespace and comments for analysis
                stripped = line.strip()

                # Skip empty lines
                if not stripped:
                    continue

                # Skip single-line comments
                if stripped.startswith("//"):
                    continue

                # Skip multi-line comment starts (basic detection)
                if stripped.startswith("/*") and "*/" in stripped:
                    continue

                # Check for FastLED.h include (various formats)
                if "#include" in stripped and (
                    "FastLED.h" in stripped
                    or '"FastLED.h"' in stripped
                    or "<FastLED.h>" in stripped
                ):
                    found_fastled_include = True
                    break

                # Check for problematic constructs before FastLED.h
                problematic_patterns = [
                    stripped.startswith("#include"),  # Any other include
                    stripped.startswith("#define"),  # Any define
                    stripped.startswith("#ifdef"),  # Conditional compilation
                    stripped.startswith("#ifndef"),  # Conditional compilation
                    stripped.startswith("#if "),  # Conditional compilation
                    stripped.startswith("#pragma"),  # Pragma directives
                    stripped.startswith("using "),  # Using declarations
                    stripped.startswith("namespace"),  # Namespace declarations
                    "=" in stripped
                    and not stripped.startswith("//"),  # Variable assignments
                ]

                if any(problematic_patterns):
                    # Found problematic code before FastLED.h
                    return False

            # If we found FastLED.h and no problematic code before it, PCH is compatible
            # If we didn't find FastLED.h at all, assume PCH is still ok (might be included indirectly)
            return True

        except Exception:
            # If we can't analyze the file, err on the side of caution and disable PCH
            return False

    def compile_unity(
        self,
        compile_options: CompilerOptions,
        list_of_cpp_files: Sequence[str | Path],
        unity_output_path: str | Path | None = None,
    ) -> Future[Result]:
        """
        Compile multiple .cpp files as a unity build.

        Generates a single unity.cpp file that includes all specified .cpp files,
        then compiles this single file for improved compilation speed and optimization.

        Args:
            compile_options: Configuration for the compilation
            list_of_cpp_files: List of .cpp file paths to include in unity build
            unity_output_path: Custom unity.cpp output path. If None, uses temp file.

        Returns:
            Future[Result]: Future that will contain compilation result
        """
        return _EXECUTOR.submit(
            self._compile_unity_sync,
            compile_options,
            list_of_cpp_files,
            unity_output_path,
        )

    def _compile_unity_sync(
        self,
        compile_options: CompilerOptions,
        list_of_cpp_files: Sequence[str | Path],
        unity_output_path: str | Path | None = None,
    ) -> Result:
        """
        Synchronous implementation of unity compilation.

        Args:
            compile_options: Configuration for the compilation
            list_of_cpp_files: List of .cpp file paths to include in unity build
            unity_output_path: Custom unity.cpp output path. If None, uses temp file.

        Returns:
            Result: Success status and command output
        """
        if not list_of_cpp_files:
            return Result(
                ok=False,
                stdout="",
                stderr="No .cpp files provided for unity compilation",
                return_code=1,
            )

        # Convert to Path objects and validate all files exist
        cpp_paths: list[Path] = []
        for cpp_file in list_of_cpp_files:
            cpp_path = Path(cpp_file).resolve()
            if not cpp_path.exists():
                return Result(
                    ok=False,
                    stdout="",
                    stderr=f"Source file not found: {cpp_path}",
                    return_code=1,
                )
            if not cpp_path.suffix == ".cpp":
                return Result(
                    ok=False,
                    stdout="",
                    stderr=f"File is not a .cpp file: {cpp_path}",
                    return_code=1,
                )
            cpp_paths.append(cpp_path)

        # Determine unity.cpp output path
        if unity_output_path:
            unity_path = Path(unity_output_path).resolve()
            cleanup_unity = False
        else:
            # Create temporary unity.cpp file
            temp_dir = compile_options.temp_dir or tempfile.gettempdir()
            temp_file = tempfile.NamedTemporaryFile(
                suffix=".cpp", prefix="unity_", dir=temp_dir, delete=False
            )
            unity_path = Path(temp_file.name)
            temp_file.close()
            cleanup_unity = True

        # Generate unity.cpp content
        try:
            unity_content = self._generate_unity_content(cpp_paths)

            # Check if file already exists with identical content to avoid unnecessary modifications
            should_write = True
            if unity_path.exists():
                try:
                    existing_content = unity_path.read_text(encoding="utf-8")
                    if existing_content == unity_content:
                        should_write = False
                        # File already has identical content, skip writing
                except (IOError, UnicodeDecodeError):
                    # If we can't read the existing file, proceed with writing
                    should_write = True

            if should_write:
                unity_path.write_text(unity_content, encoding="utf-8")
        except Exception as e:
            if cleanup_unity and unity_path.exists():
                try:
                    unity_path.unlink()
                except:
                    pass
            return Result(
                ok=False,
                stdout="",
                stderr=f"Failed to generate unity.cpp: {e}",
                return_code=1,
            )

        # Determine output object file path
        if cleanup_unity:
            # If unity file is temporary, create temporary object file too
            temp_obj_file = tempfile.NamedTemporaryFile(suffix=".o", delete=False)
            output_path = temp_obj_file.name
            temp_obj_file.close()
            cleanup_obj = True
        else:
            # If unity file is permanent, create object file alongside it
            output_path = unity_path.with_suffix(".o")
            cleanup_obj = False

        try:
            # Compile the unity.cpp file
            result = self._compile_cpp_file_sync(
                unity_path,
                output_path,
                compile_options.additional_flags,
                compile_options.use_pch,
            )

            # Clean up temporary files on failure
            if not result.ok:
                if cleanup_unity and unity_path.exists():
                    try:
                        unity_path.unlink()
                    except:
                        pass
                if cleanup_obj and output_path and os.path.exists(output_path):
                    try:
                        os.unlink(output_path)
                    except:
                        pass

            return result

        except Exception as e:
            # Clean up temporary files on exception
            if cleanup_unity and unity_path.exists():
                try:
                    unity_path.unlink()
                except:
                    pass
            if cleanup_obj and output_path and os.path.exists(output_path):
                try:
                    os.unlink(output_path)
                except:
                    pass

            return Result(
                ok=False,
                stdout="",
                stderr=f"Unity compilation failed: {e}",
                return_code=-1,
            )

    def _generate_unity_content(self, cpp_paths: list[Path]) -> str:
        """
        Generate the content for unity.cpp file.

        Args:
            cpp_paths: List of .cpp file paths to include

        Returns:
            str: Content for unity.cpp file
        """
        lines = [
            "// Unity build file generated by FastLED CI system",
            "// This file includes all source .cpp files for unified compilation",
            "//",
            f"// Generated for {len(cpp_paths)} source files:",
        ]

        # Add comment listing all included files
        for cpp_path in cpp_paths:
            lines.append(f"//   - {cpp_path}")

        lines.extend(
            [
                "//",
                "",
                "// Include all source files",
            ]
        )

        # Add #include statements for all .cpp files
        for cpp_path in cpp_paths:
            # Use absolute paths to avoid include path issues
            lines.append(f'#include "{cpp_path}"')

        lines.append("")  # Final newline

        return "\n".join(lines)

    def create_archive(
        self,
        object_files: list[Path],
        output_archive: Path,
        options: LibarchiveOptions = LibarchiveOptions(),
    ) -> Future[Result]:
        """
        Create static library archive from object files.
        Submits archive creation to thread pool for parallel execution.

        Args:
            object_files: List of .o files to include in archive
            output_archive: Output .a file path
            options: Archive generation options

        Returns:
            Future[Result]: Future object that will contain the archive creation result
        """
        return _EXECUTOR.submit(
            self.create_archive_sync, object_files, output_archive, options
        )

    def create_archive_sync(
        self, object_files: list[Path], output_archive: Path, options: LibarchiveOptions
    ) -> Result:
        """
        Synchronous archive creation implementation.

        Args:
            object_files: List of .o files to include in archive
            output_archive: Output .a file path
            options: Archive generation options

        Returns:
            Result: Success status and command output
        """
        return create_archive_sync(
            object_files, output_archive, options, self.settings.archiver
        )

    def link_program(self, link_options: LinkOptions) -> Future[Result]:
        """
        Link object files and libraries into an executable program.

        Args:
            link_options: Configuration for the linking operation

        Returns:
            Future[Result]: Future that will contain linking result
        """
        return _EXECUTOR.submit(self._link_program_sync, link_options)

    def _link_program_sync(self, link_options: LinkOptions) -> Result:
        """
        Synchronous implementation of program linking.

        Args:
            link_options: Configuration for the linking operation

        Returns:
            Result: Success status and command output
        """
        return link_program_sync(link_options)

    def detect_linker(self) -> str:
        """
        Detect the best available linker for the current platform.
        Preference order varies by platform:
        - Windows: lld-link.exe > link.exe > ld.exe
        - Linux: mold > lld > gold > ld
        - macOS: ld64.lld > ld

        Returns:
            str: Path to the detected linker

        Raises:
            RuntimeError: If no suitable linker is found
        """
        system = platform.system()

        if system == "Windows":
            # Windows linker priority
            for linker in [
                "lld-link.exe",
                "lld-link",
                "link.exe",
                "link",
                "ld.exe",
                "ld",
            ]:
                linker_path = shutil.which(linker)
                if linker_path:
                    return linker_path
            raise RuntimeError("No linker found (lld-link, link, or ld required)")

        elif system == "Linux":
            # Linux linker priority
            for linker in ["mold", "lld", "gold", "ld"]:
                linker_path = shutil.which(linker)
                if linker_path:
                    return linker_path
            raise RuntimeError("No linker found (mold, lld, gold, or ld required)")

        elif system == "Darwin":  # macOS
            # macOS linker priority
            for linker in ["ld64.lld", "ld"]:
                linker_path = shutil.which(linker)
                if linker_path:
                    return linker_path
            raise RuntimeError("No linker found (ld64.lld or ld required)")

        else:
            # Default fallback for other platforms
            linker_path = shutil.which("ld")
            if linker_path:
                return linker_path
            raise RuntimeError(f"No linker found for platform {system}")


def link_program_sync(link_options: LinkOptions) -> Result:
    """
    Link object files and libraries into an executable program.

    Args:
        link_options: Configuration for the linking operation

    Returns:
        Result: Success status and command output
    """
    if not link_options.object_files:
        return Result(
            ok=False,
            stdout="",
            stderr="No object files provided for linking",
            return_code=1,
        )

    # Validate all object files exist
    for obj_file in link_options.object_files:
        obj_path = Path(obj_file)
        if not obj_path.exists():
            return Result(
                ok=False,
                stdout="",
                stderr=f"Object file not found: {obj_path}",
                return_code=1,
            )

    # Validate all static libraries exist
    for lib_file in link_options.static_libraries:
        lib_path = Path(lib_file)
        if not lib_path.exists():
            return Result(
                ok=False,
                stdout="",
                stderr=f"Static library not found: {lib_path}",
                return_code=1,
            )

    # Auto-detect linker if not provided
    linker = link_options.linker
    if linker is None:
        try:
            linker = detect_linker()
        except RuntimeError as e:
            return Result(ok=False, stdout="", stderr=str(e), return_code=1)

    # Build linker command
    cmd = [linker]

    # Add platform-specific output flag first
    system = platform.system()
    output_path = Path(link_options.output_executable)

    if system == "Windows" and ("lld-link" in linker or "link" in linker):
        # Windows (lld-link/link) style
        cmd.append(f"/OUT:{output_path}")
    else:
        # Unix-style (ld/mold/gold/etc.)
        cmd.extend(["-o", str(output_path)])

    # Add object files
    cmd.extend(str(obj) for obj in link_options.object_files)

    # Add static libraries
    cmd.extend(str(lib) for lib in link_options.static_libraries)

    # Add custom linker arguments
    cmd.extend(link_options.linker_args)

    # Ensure output directory exists
    output_path.parent.mkdir(parents=True, exist_ok=True)

    # Execute linker command
    try:
        process = subprocess.run(cmd, capture_output=True, text=True, cwd=None)

        return Result(
            ok=process.returncode == 0,
            stdout=process.stdout,
            stderr=process.stderr,
            return_code=process.returncode,
        )
    except Exception as e:
        return Result(
            ok=False, stdout="", stderr=f"Linker command failed: {e}", return_code=-1
        )


# Helper functions for common linker argument patterns


def get_common_linker_args(
    platform_name: str | None = None,
    debug: bool = False,
    optimize: bool = False,
    static_runtime: bool = False,
) -> list[str]:
    """Generate common linker arguments for different platforms and configurations."""
    if platform_name is None or platform_name == "auto":
        platform_name = platform.system()

    args: list[str] = []

    if platform_name == "Windows":
        # Windows (lld-link/link) arguments
        args.append("/SUBSYSTEM:CONSOLE")
        args.append("/NOLOGO")

        if debug:
            args.append("/DEBUG")

        if optimize:
            args.extend(["/OPT:REF", "/OPT:ICF"])

        if static_runtime:
            args.append("/MT")

    else:
        # Unix-style arguments (Linux/macOS)
        if debug:
            args.append("-g")

        if optimize:
            args.extend(["-O2", "-Wl,--gc-sections"])

        if static_runtime:
            args.extend(["-static-libgcc", "-static-libstdc++"])

    return args


def add_system_libraries(
    linker_args: list[str], libraries: list[str], platform_name: str | None = None
) -> None:
    """Add system libraries to linker arguments with platform-appropriate flags."""
    if platform_name is None or platform_name == "auto":
        platform_name = platform.system()

    for lib in libraries:
        if platform_name == "Windows":
            # Windows style
            if not lib.endswith(".lib"):
                lib += ".lib"
            linker_args.append(f"/DEFAULTLIB:{lib}")
        else:
            # Unix style
            linker_args.append(f"-l{lib}")


def add_library_paths(
    linker_args: list[str], paths: list[str], platform_name: str | None = None
) -> None:
    """Add library search paths to linker arguments with platform-appropriate flags."""
    if platform_name is None or platform_name == "auto":
        platform_name = platform.system()

    for path in paths:
        if platform_name == "Windows":
            # Windows style
            linker_args.append(f"/LIBPATH:{path}")
        else:
            # Unix style
            linker_args.append(f"-L{path}")


# Convenience functions for backward compatibility

# Default settings for backward compatibility
_DEFAULT_SETTINGS = CompilerOptions(
    include_path="./src", defines=["STUB_PLATFORM"], std_version="c++17"
)

# Shared compiler instance for backward compatibility functions
_default_compiler = None


def _get_default_compiler() -> Compiler:
    """Get or create a shared compiler instance with default settings."""
    global _default_compiler
    if _default_compiler is None:
        _default_compiler = Compiler(_DEFAULT_SETTINGS)
    return _default_compiler


def check_clang_version() -> tuple[bool, str, str]:
    """Backward compatibility wrapper."""
    compiler = _get_default_compiler()
    result = compiler.check_clang_version()
    return result.success, result.version, result.error


def compile_ino_file(
    ino_path: str | Path,
    output_path: str | Path | None = None,
    additional_flags: list[str] | None = None,
) -> Result:
    """Backward compatibility wrapper."""
    compiler = _get_default_compiler()
    future_result = compiler.compile_ino_file(ino_path, output_path, additional_flags)
    return future_result.result()  # Wait for completion and return result


def test_clang_accessibility() -> bool:
    """Backward compatibility wrapper."""
    compiler = _get_default_compiler()
    return compiler.test_clang_accessibility()


def find_ino_files(
    examples_dir: str = "examples", filter_names: list[str] | None = None
) -> list[Path]:
    """Backward compatibility wrapper."""
    compiler = _get_default_compiler()
    return compiler.find_ino_files(examples_dir, filter_names=filter_names)


def get_fastled_compile_command(
    ino_file: str | Path, output_file: str | Path
) -> list[str]:
    """Backward compatibility wrapper - DEPRECATED: Use FastLEDClangCompiler.compile() instead."""
    compiler = _get_default_compiler()
    # This is a bit hacky since we removed get_compile_command, but needed for backward compatibility
    settings = compiler.settings
    cmd = [
        settings.compiler,
        "-x",
        "c++",
        f"-std={settings.std_version}",
        f"-I{settings.include_path}",
    ]

    # Add defines if specified
    if settings.defines:
        for define in settings.defines:
            cmd.append(f"-D{define}")
    cmd.extend(settings.compiler_args)
    cmd.extend(
        [
            "-c",
            str(ino_file),
            "-o",
            str(output_file),
        ]
    )
    return cmd


def main() -> bool:
    """Test all functions in the module using the class-based approach."""
    print("=== Testing Compiler class ===")

    # Create compiler instance with custom settings
    settings = CompilerOptions(
        include_path="./src", defines=["STUB_PLATFORM"], std_version="c++17"
    )
    compiler = Compiler(settings)

    # Test version check
    version_result = compiler.check_clang_version()
    print(
        f"Version check: success={version_result.success}, version={version_result.version}"
    )
    if not version_result.success:
        print(f"Error: {version_result.error}")
        return False

    # Test finding ino files
    ino_files = compiler.find_ino_files(
        "examples", filter_names=["Blink", "DemoReel100"]
    )
    print(f"Found {len(ino_files)} ino files: {[f.name for f in ino_files]}")

    if ino_files:
        # Test compile_ino_file function
        future_result = compiler.compile_ino_file(ino_files[0])
        result = future_result.result()  # Wait for completion and get result
        print(
            f"Compile {ino_files[0].name}: success={result.ok}, return_code={result.return_code}"
        )
        if not result.ok:
            print(f"Error: {result.stderr}")
        else:
            print("[OK] Compilation successful")

        # Test compile_ino_file function with Result dataclass
        future_result_dc = compiler.compile_ino_file(ino_files[0])
        result_dc = future_result_dc.result()  # Wait for completion and get result
        print(
            f"compile_ino_file {ino_files[0].name}: ok={result_dc.ok}, return_code={result_dc.return_code}"
        )

    # Test compiler_args
    print("\n=== Testing compiler_args ===")
    settings_with_args = CompilerOptions(
        include_path="./src",
        defines=["STUB_PLATFORM"],
        std_version="c++17",
        compiler_args=["-Werror", "-Wall"],
    )
    compiler_with_args = Compiler(settings_with_args)
    if ino_files:
        # Test that compiler_args are included in the command
        with tempfile.NamedTemporaryFile(suffix=".o", delete=False) as temp_file:
            temp_output = temp_file.name

        # Get the command that would be executed
        cmd = get_fastled_compile_command(ino_files[0], temp_output)
        print(f"Command with compiler_args: {' '.join(cmd)}")

        if "-Werror" not in cmd or "-Wall" not in cmd:
            print("X compiler_args not found in command")
            return False
        print("[OK] compiler_args correctly passed to compile command.")

        # Test actual compilation
        future_result = compiler_with_args.compile_ino_file(ino_files[0])
        result = future_result.result()  # Wait for completion and get result
        print(f"Compile {ino_files[0].name} with extra args: success={result.ok}")
        if not result.ok:
            print(f"Error: {result.stderr}")

        # Clean up
        if os.path.exists(temp_output):
            os.unlink(temp_output)

    # Test full accessibility test
    print("\n=== Running full accessibility test ===")
    result = compiler.test_clang_accessibility()

    # Test backward compatibility functions
    print("\n=== Testing backward compatibility functions ===")
    success_compat, version_compat, error_compat = check_clang_version()
    print(
        f"Backward compatibility version check: success={success_compat}, version={version_compat}"
    )

    return result and success_compat


def detect_archiver() -> str:
    """
    Detect available archiver tool.
    Preference order: llvm-ar > ar

    Returns:
        str: Path to the detected archiver tool

    Raises:
        RuntimeError: If no archiver tool is found
    """
    llvm_ar = shutil.which("llvm-ar")
    if llvm_ar:
        return llvm_ar

    ar = shutil.which("ar")
    if ar:
        return ar

    raise RuntimeError("No archiver tool found (ar or llvm-ar required)")


def detect_linker() -> str:
    """
    Detect the best available linker for the current platform.
    Preference order varies by platform:
    - Windows: lld-link.exe > link.exe > ld.exe
    - Linux: mold > lld > gold > ld
    - macOS: ld64.lld > ld

    Returns:
        str: Path to the detected linker

    Raises:
        RuntimeError: If no suitable linker is found
    """
    system = platform.system()

    if system == "Windows":
        # Windows linker priority
        for linker in ["lld-link.exe", "lld-link", "link.exe", "link", "ld.exe", "ld"]:
            linker_path = shutil.which(linker)
            if linker_path:
                return linker_path
        raise RuntimeError("No linker found (lld-link, link, or ld required)")

    elif system == "Linux":
        # Linux linker priority
        for linker in ["mold", "lld", "gold", "ld"]:
            linker_path = shutil.which(linker)
            if linker_path:
                return linker_path
        raise RuntimeError("No linker found (mold, lld, gold, or ld required)")

    elif system == "Darwin":  # macOS
        # macOS linker priority
        for linker in ["ld64.lld", "ld"]:
            linker_path = shutil.which(linker)
            if linker_path:
                return linker_path
        raise RuntimeError("No linker found (ld64.lld or ld required)")

    else:
        # Default fallback for other platforms
        linker_path = shutil.which("ld")
        if linker_path:
            return linker_path
        raise RuntimeError(f"No linker found for platform {system}")


def create_archive_sync(
    object_files: list[Path],
    output_archive: Path,
    options: LibarchiveOptions = LibarchiveOptions(),
    archiver: str | None = None,
) -> Result:
    """
    Create a static library archive (.a) from compiled object files.

    Args:
        object_files: List of .o files to include in archive
        output_archive: Output .a file path
        options: Archive generation options
        archiver: Path to archiver tool (auto-detected if None)

    Returns:
        Result: Success status and command output
    """
    if not object_files:
        return Result(
            ok=False,
            stdout="",
            stderr="No object files provided for archive creation",
            return_code=1,
        )

    # Validate all object files exist
    for obj_file in object_files:
        if not obj_file.exists():
            return Result(
                ok=False,
                stdout="",
                stderr=f"Object file not found: {obj_file}",
                return_code=1,
            )

    # Auto-detect archiver if not provided
    if archiver is None:
        try:
            archiver = detect_archiver()
        except RuntimeError as e:
            return Result(ok=False, stdout="", stderr=str(e), return_code=1)

    # Build archive command
    cmd = [archiver]

    # Archive flags: r=insert, c=create if needed, s=write symbol table
    flags = "rcs"
    if options.use_thin:
        flags += "T"  # Add thin archive flag

    cmd.append(flags)
    cmd.append(str(output_archive))
    cmd.extend(str(obj) for obj in object_files)

    # Ensure output directory exists
    output_archive.parent.mkdir(parents=True, exist_ok=True)

    # Execute archive command
    try:
        process = subprocess.run(cmd, capture_output=True, text=True, cwd=None)

        return Result(
            ok=process.returncode == 0,
            stdout=process.stdout,
            stderr=process.stderr,
            return_code=process.returncode,
        )
    except Exception as e:
        return Result(
            ok=False, stdout="", stderr=f"Archive command failed: {e}", return_code=-1
        )


if __name__ == "__main__":
    import sys

    success = main()
    sys.exit(0 if success else 1)
