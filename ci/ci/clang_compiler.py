#!/usr/bin/env python3

"""
Clang compiler accessibility and testing functions for FastLED.
Supports the simple build system approach outlined in FEATURE.md.
"""

import os
import subprocess
import tempfile
from concurrent.futures import Future, ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path
from typing import cast


def cpu_count() -> int:
    # get the nubmer of cpus on the machine
    import multiprocessing

    return multiprocessing.cpu_count() or 1


_EXECUTOR = ThreadPoolExecutor(max_workers=cpu_count() * 2)


@dataclass
class CompilerSettings:
    """
    Shared compiler settings for FastLED compilation.
    """

    include_path: str
    compiler: str = "clang++"
    defines: list[str] | None = None
    std_version: str = "c++17"
    compiler_args: list[str] = field(default_factory=list)


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


class Compiler:
    """
    Generic compiler wrapper for .ino files.
    Handles compilation, testing, and file discovery for FastLED examples.
    Supports any compiler including clang++, gcc, sccache, etc.
    """

    def __init__(
        self,
        settings: CompilerSettings,
    ) -> None:
        """
        Initialize the compiler wrapper.

        Args:
            settings (CompilerSettings): Compiler settings including compiler path/command.
        """
        self.settings: CompilerSettings = settings

    def get_compiler_args(self) -> list[str]:
        """
        Get a copy of the complete compiler arguments that would be used for compilation.

        Returns:
            list[str]: Copy of compiler arguments including compiler, flags, defines, and settings
        """
        cmd = [
            self.settings.compiler,
            "-x",
            "c++",  # Force C++ compilation of .ino files
            f"-std={self.settings.std_version}",
            f"-I{self.settings.include_path}",  # FastLED include path
        ]

        # Add defines if specified
        if self.settings.defines:
            for define in self.settings.defines:
                cmd.append(f"-D{define}")

        # Add additional compiler args
        cmd.extend(self.settings.compiler_args)

        # Add standard compilation flags
        cmd.extend(["-c"])

        return cmd.copy()  # Return a copy to prevent modification

    def check_clang_version(self) -> VersionCheckResult:
        """
        Check that clang++ is accessible and return version information.

        Returns:
            VersionCheckResult: Result containing success status, version string, and error message
        """
        # Test uv run access first
        try:
            result = subprocess.run(
                [self.settings.compiler, "--version"],
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
    ) -> Future[Result]:
        """
        Compile a single .ino file using clang++ with optional temporary output.

        Args:
            ino_path (str|Path): Path to the .ino file to compile
            output_path (str|Path, optional): Output object file path. If None, uses temp file.
            additional_flags (list, optional): Additional compiler flags

        Returns:
            Future[Result]: Future that will contain Result dataclass with subprocess execution details
        """
        # Submit the compilation task to a thread pool

        return _EXECUTOR.submit(
            self._compile_ino_file_sync, ino_path, output_path, additional_flags
        )

    def _compile_ino_file_sync(
        self,
        ino_path: str | Path,
        output_path: str | Path | None = None,
        additional_flags: list[str] | None = None,
    ) -> Result:
        """
        Internal synchronous implementation of compile_ino_file.

        Args:
            ino_path (str|Path): Path to the .ino file to compile
            output_path (str|Path, optional): Output object file path. If None, uses temp file.
            additional_flags (list, optional): Additional compiler flags

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

        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".cpp", delete=False
        ) as tmp_cpp:
            tmp_cpp.write(f'#include "{ino_file_path}"')
            temp_cpp_path = Path(tmp_cpp.name)

        # Build compiler command
        cmd = [
            self.settings.compiler,
            "-x",
            "c++",  # Force C++ compilation of .ino files
            f"-std={self.settings.std_version}",
            f"-I{self.settings.include_path}",  # FastLED include path
        ]

        # Add defines if specified
        if self.settings.defines:
            for define in self.settings.defines:
                cmd.append(f"-D{define}")
        cmd.extend(self.settings.compiler_args)
        cmd.extend(
            [
                "-c",
                str(temp_cpp_path),
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
        finally:
            os.unlink(temp_cpp_path)

    def find_ino_files(
        self, examples_dir: str | Path, filter_names: list[str] | None = None
    ) -> list[Path]:
        """
        Find all .ino files in the specified examples directory.

        Args:
            examples_dir (str|Path): Directory containing .ino files
            filter_names (list, optional): Only include files with these stem names

        Returns:
            list[Path]: List of .ino file paths
        """
        examples_dir = Path(examples_dir)
        ino_files = list(examples_dir.rglob("*.ino"))

        if filter_names:
            filter_set = set(filter_names)
            ino_files = [f for f in ino_files if f.stem in filter_set]

        return ino_files

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


# Convenience functions for backward compatibility

# Default settings for backward compatibility
_DEFAULT_SETTINGS = CompilerSettings(
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
    settings = CompilerSettings(
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
    settings_with_args = CompilerSettings(
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

    return result


if __name__ == "__main__":
    import sys

    success = main()
    sys.exit(0 if success else 1)
