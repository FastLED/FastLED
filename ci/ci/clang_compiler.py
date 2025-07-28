#!/usr/bin/env python3

"""
Clang compiler accessibility and testing functions for FastLED.
Supports the simple build system approach outlined in FEATURE.md.
"""

import os
import subprocess
import tempfile
from pathlib import Path
from typing import cast


class FastLEDClangCompiler:
    """
    FastLED-specific clang++ compiler wrapper for .ino files.
    Handles compilation, testing, and file discovery for FastLED examples.
    """

    def __init__(
        self,
        examples_dir: str = "examples",
        include_path: str = "./src",
        platform_define: str = "STUB_PLATFORM",
        std_version: str = "c++17",
    ):
        """
        Initialize the FastLED clang compiler wrapper.

        Args:
            examples_dir (str): Directory containing .ino files
            include_path (str): Include path for FastLED headers
            platform_define (str): Platform define for compilation
            std_version (str): C++ standard version
        """
        self.examples_dir = Path(examples_dir)
        self.include_path = include_path
        self.platform_define = platform_define
        self.std_version = std_version
        self.compiler = "clang++"

    def check_clang_version(self):
        """
        Check that clang++ is accessible and return version information.

        Returns:
            tuple: (success: bool, version: str, error: str)
        """
        # Test uv run access first
        try:
            result = subprocess.run(
                ["uv", "run", self.compiler, "--version"],
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
                return True, f"uv:{version}", ""
        except Exception as e:
            pass  # Try direct access

        # Test direct access
        try:
            result = subprocess.run(
                [self.compiler, "--version"], capture_output=True, text=True, timeout=10
            )
            if result.returncode == 0 and "clang version" in result.stdout:
                version = (
                    result.stdout.split()[2]
                    if len(result.stdout.split()) > 2
                    else "unknown"
                )
                return True, f"direct:{version}", ""
            else:
                return False, "", f"clang++ version check failed: {result.stderr}"
        except Exception as e:
            return False, "", f"clang++ not accessible: {str(e)}"

    def get_compile_command(
        self,
        ino_file: str | Path,
        output_file: str | Path,
        additional_flags: list[str] | None = None,
    ) -> list[str]:
        """
        Get the compilation command for an .ino file.

        Args:
            ino_file (str|Path): Path to the .ino file
            output_file (str|Path): Path for the output object file
            additional_flags (list, optional): Additional compiler flags

        Returns:
            list: Command list suitable for subprocess.run()
        """
        cmd = [
            self.compiler,
            "-x",
            "c++",  # Force C++ compilation of .ino files
            f"-std={self.std_version}",
            f"-I{self.include_path}",  # FastLED include path
            f"-D{self.platform_define}",  # Platform define
            "-c",
            str(ino_file),
            "-o",
            str(output_file),
        ]

        if additional_flags:
            cmd.extend(additional_flags)

        return cmd

    def compile_ino_file(
        self,
        ino_path: str | Path,
        output_path: str | Path | None = None,
        additional_flags: list[str] | None = None,
    ) -> dict[str, str | bool | int | list[str]]:
        """
        Compile a single .ino file using clang++.

        Args:
            ino_path (str|Path): Path to the .ino file to compile
            output_path (str|Path, optional): Output object file path. If None, uses temp file.
            additional_flags (list, optional): Additional compiler flags

        Returns:
            dict: {
                "success": bool,
                "output_path": str,
                "stderr": str,
                "size": int (bytes, only if success=True),
                "command": list (the command that was executed)
            }
        """
        ino_path = Path(ino_path)

        # Create output path if not provided
        if output_path is None:
            temp_file = tempfile.NamedTemporaryFile(suffix=".o", delete=False)
            output_path = temp_file.name
            temp_file.close()
            cleanup_temp = True
        else:
            cleanup_temp = False

        # Get compiler command
        cmd = self.get_compile_command(ino_path, output_path, additional_flags)

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)

            if result.returncode == 0:
                # Check if output file was created and get size
                if os.path.exists(output_path):
                    size = os.path.getsize(output_path)
                    return {
                        "success": True,
                        "output_path": str(output_path),
                        "stderr": result.stderr,
                        "size": size,
                        "command": cmd,
                    }
                else:
                    if cleanup_temp and os.path.exists(output_path):
                        os.unlink(output_path)
                    return {
                        "success": False,
                        "output_path": str(output_path),
                        "stderr": "Object file was not created",
                        "size": 0,
                        "command": cmd,
                    }
            else:
                if cleanup_temp and os.path.exists(output_path):
                    os.unlink(output_path)
                return {
                    "success": False,
                    "output_path": str(output_path),
                    "stderr": result.stderr,
                    "size": 0,
                    "command": cmd,
                }

        except Exception as e:
            if cleanup_temp and os.path.exists(output_path):
                try:
                    os.unlink(output_path)
                except:
                    pass
            return {
                "success": False,
                "output_path": str(output_path),
                "stderr": str(e),
                "size": 0,
                "command": cmd,
            }

    def find_ino_files(self, filter_names: list[str] | None = None) -> list[Path]:
        """
        Find all .ino files in the examples directory.

        Args:
            filter_names (list, optional): Only include files with these stem names

        Returns:
            list[Path]: List of .ino file paths
        """
        ino_files = list(self.examples_dir.rglob("*.ino"))

        if filter_names:
            filter_set = set(filter_names)
            ino_files = [f for f in ino_files if f.stem in filter_set]

        return ino_files

    def test_clang_accessibility(self):
        """
        Comprehensive test that clang++ is accessible and working properly.
        This is the foundational test for the simple build system approach.

        Returns:
            bool: True if all tests pass, False otherwise
        """
        print("Testing clang++ accessibility...")

        # Test 1: Check clang++ version
        success, version, error = self.check_clang_version()
        if not success:
            print(f"X clang++ version check failed: {error}")
            return False
        print(f"[OK] clang++ version: {version}")

        # Test 2: Simple compilation test with Blink example
        print("Testing Blink.ino compilation...")
        result = self.compile_ino_file("examples/Blink/Blink.ino")

        if not result["success"]:
            print(f"X Blink compilation failed: {cast(str, result['stderr'])}")
            print(f"Command used: {' '.join(cast(list[str], result['command']))}")
            return False

        if cast(int, result["size"]) < 1024:
            print(f"X Blink object file too small: {cast(int, result['size'])} bytes")
            self._cleanup_file(cast(str, result["output_path"]))
            return False

        print(
            f"[OK] Blink.ino compilation: {cast(int, result['size'])} bytes object file"
        )
        self._cleanup_file(cast(str, result["output_path"]))

        # Test 3: Complex example compilation test with DemoReel100
        print("Testing DemoReel100.ino compilation...")
        result = self.compile_ino_file("examples/DemoReel100/DemoReel100.ino")

        if not result["success"]:
            print(f"X DemoReel100 compilation failed: {result['stderr']}")
            print(f"Command used: {' '.join(result['command'])}")
            return False

        if result["size"] < 1024:
            print(f"X DemoReel100 object file too small: {result['size']} bytes")
            self._cleanup_file(result["output_path"])
            return False

        print(f"[OK] DemoReel100.ino compilation: {result['size']} bytes object file")
        self._cleanup_file(result["output_path"])

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
def check_clang_version() -> tuple[bool, str, str]:
    """Backward compatibility wrapper."""
    compiler = FastLEDClangCompiler()
    return compiler.check_clang_version()


def compile_ino_file(
    ino_path: str | Path,
    output_path: str | Path | None = None,
    additional_flags: list[str] | None = None,
) -> dict:
    """Backward compatibility wrapper."""
    compiler = FastLEDClangCompiler()
    return compiler.compile_ino_file(ino_path, output_path, additional_flags)


def test_clang_accessibility() -> bool:
    """Backward compatibility wrapper."""
    compiler = FastLEDClangCompiler()
    return compiler.test_clang_accessibility()


def find_ino_files(
    examples_dir: str = "examples", filter_names: list[str] | None = None
) -> list[Path]:
    """Backward compatibility wrapper."""
    compiler = FastLEDClangCompiler(examples_dir=examples_dir)
    return compiler.find_ino_files(filter_names=filter_names)


def get_fastled_compile_command(
    ino_file: str | Path, output_file: str | Path
) -> list[str]:
    """Backward compatibility wrapper."""
    compiler = FastLEDClangCompiler()
    return compiler.get_compile_command(ino_file, output_file)


def main() -> bool:
    """Test all functions in the module using the class-based approach."""
    print("=== Testing FastLEDClangCompiler class ===")

    # Create compiler instance
    compiler = FastLEDClangCompiler()

    # Test version check
    success, version, error = compiler.check_clang_version()
    print(f"Version check: success={success}, version={version}")
    if not success:
        print(f"Error: {error}")
        return False

    # Test finding ino files
    ino_files = compiler.find_ino_files(filter_names=["Blink", "DemoReel100"])
    print(f"Found {len(ino_files)} ino files: {[f.name for f in ino_files]}")

    if ino_files:
        # Test compile function
        result = compiler.compile_ino_file(ino_files[0])
        print(
            f"Compile {ino_files[0].name}: success={result['success']}, size={result['size']}"
        )
        print(f"Command used: {' '.join(result['command'])}")

        # Test get command function
        cmd = compiler.get_compile_command(ino_files[0], "test.o")
        print(f"Get command: {' '.join(cmd)}")

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
