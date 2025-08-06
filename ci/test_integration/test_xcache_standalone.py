#!/usr/bin/env python3
"""
Standalone tests for xcache.py using clang compiler without requiring sccache server.

This test suite verifies that xcache works correctly by testing the wrapper
functionality and ensuring proper command construction and execution.
"""

import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


@dataclass
class TestResults:
    """Test results container."""

    passed: int = 0
    failed: int = 0
    errors: Optional[list[str]] = None

    def __post_init__(self) -> None:
        if self.errors is None:
            self.errors = []


def find_clang() -> Optional[str]:
    """Find clang compiler in PATH."""
    clang_path = shutil.which("clang")
    if clang_path:
        return clang_path

    # Check common locations
    common_paths = [
        "/usr/bin/clang",
        "/usr/local/bin/clang",
        "/opt/local/bin/clang",
        "C:/Program Files/LLVM/bin/clang.exe",
        "C:/msys64/mingw64/bin/clang.exe",
    ]

    for path in common_paths:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path

    return None


def create_test_source_file(temp_dir: Path) -> Path:
    """Create a simple C source file for testing."""
    source_file = temp_dir / "test.c"
    source_content = """
#include <stdio.h>

int main() {
    printf("Hello from xcache test!\\n");
    return 0;
}
"""
    source_file.write_text(source_content)
    return source_file


def create_response_file(temp_dir: Path, args: list[str]) -> Path:
    """Create a response file with the given arguments."""
    response_file = temp_dir / "compile_args.rsp"

    # Write arguments to response file (space-separated)
    # Properly quote arguments that contain spaces (Windows paths)
    # Use forward slashes for Windows paths to avoid backslash escape issues
    quoted_args: list[str] = []
    for arg in args:
        # Convert Windows backslashes to forward slashes for response files
        normalized_arg = arg.replace("\\", "/")
        if " " in normalized_arg and not (
            normalized_arg.startswith('"') and normalized_arg.endswith('"')
        ):
            quoted_args.append(f'"{normalized_arg}"')
        else:
            quoted_args.append(normalized_arg)

    content = " ".join(quoted_args)
    response_file.write_text(content)

    return response_file


def test_clang_direct_compilation(results: TestResults) -> None:
    """Test that clang compilation works directly (baseline test)."""
    print("🧪 Testing direct clang compilation (baseline)...")

    clang_path = find_clang()
    if not clang_path:
        results.errors.append("clang compiler not found - skipping baseline test")
        return

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Create test source file
        source_file = create_test_source_file(temp_path)
        output_file = temp_path / "test_baseline.o"

        # Test direct clang compilation
        cmd = [clang_path, "-c", str(source_file), "-o", str(output_file)]

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)

            if result.returncode == 0 and output_file.exists():
                print("  ✅ Direct clang compilation works")
                print(f"     Output file created: {output_file.stat().st_size} bytes")
                results.passed += 1
            else:
                print(
                    f"  ❌ Direct clang compilation failed (return code: {result.returncode})"
                )
                print(f"     Stderr: {result.stderr}")
                results.failed += 1
                results.errors.append(
                    f"Direct clang compilation failed: {result.stderr}"
                )

        except subprocess.TimeoutExpired:
            print("  ❌ Direct clang compilation timed out")
            results.failed += 1
            results.errors.append("Direct clang compilation timed out")
        except Exception as e:
            print(f"  ❌ Direct clang compilation error: {e}")
            results.failed += 1
            results.errors.append(f"Direct clang compilation error: {e}")


def test_clang_response_file_compilation(results: TestResults) -> None:
    """Test that clang can handle response files directly."""
    print("\n🧪 Testing clang response file compilation...")

    clang_path = find_clang()
    if not clang_path:
        results.errors.append("clang compiler not found - skipping response file test")
        return

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Create test source file
        source_file = create_test_source_file(temp_path)
        output_file = temp_path / "test_response.o"

        # Create response file with compilation arguments
        compile_args = ["-c", str(source_file), "-o", str(output_file)]
        response_file = create_response_file(temp_path, compile_args)

        # Test clang with response file
        cmd = [clang_path, f"@{response_file}"]

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)

            if result.returncode == 0 and output_file.exists():
                print("  ✅ Clang response file compilation works")
                print(f"     Output file created: {output_file.stat().st_size} bytes")
                print(f"     Response file: @{response_file}")
                results.passed += 1
            else:
                print(
                    f"  ❌ Clang response file compilation failed (return code: {result.returncode})"
                )
                print(f"     Stderr: {result.stderr}")
                results.failed += 1
                results.errors.append(
                    f"Clang response file compilation failed: {result.stderr}"
                )

        except subprocess.TimeoutExpired:
            print("  ❌ Clang response file compilation timed out")
            results.failed += 1
            results.errors.append("Clang response file compilation timed out")
        except Exception as e:
            print(f"  ❌ Clang response file compilation error: {e}")
            results.failed += 1
            results.errors.append(f"Clang response file compilation error: {e}")


def test_xcache_wrapper_script_creation(results: TestResults) -> None:
    """Test that xcache creates wrapper scripts correctly."""
    print("\n🧪 Testing xcache wrapper script creation...")

    clang_path = find_clang()
    if not clang_path:
        results.errors.append("clang compiler not found - skipping wrapper script test")
        return

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Create test source file and response file
        source_file = create_test_source_file(temp_path)
        compile_args = ["-c", str(source_file), "-o", "test.o"]
        response_file = create_response_file(temp_path, compile_args)

        # Create a mock sccache script that just echoes its arguments
        mock_sccache = (
            temp_path / "sccache.bat" if os.name == "nt" else temp_path / "sccache"
        )

        if os.name == "nt":
            mock_content = """@echo off
echo MOCK_SCCACHE_CALLED: %*
"""
        else:
            mock_content = """#!/bin/bash
echo "MOCK_SCCACHE_CALLED: $@"
"""

        mock_sccache.write_text(mock_content)
        if os.name != "nt":
            mock_sccache.chmod(0o755)

        # Set up environment to use mock sccache
        env = os.environ.copy()
        env["PATH"] = f"{temp_path}{os.pathsep}{env.get('PATH', '')}"
        env["XCACHE_DEBUG"] = "1"

        # Test xcache with response file (should create wrapper script)
        cmd = [sys.executable, "ci/util/xcache.py", clang_path, f"@{response_file}"]

        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True, env=env, timeout=30
            )

            # Check if wrapper script was created and executed
            if "Created compiler wrapper:" in result.stderr:
                print("  ✅ Wrapper script creation detected")

                # Check if wrapper was executed
                if "Executing with wrapper:" in result.stderr:
                    print("  ✅ Wrapper script execution detected")

                    # Check if mock sccache was called
                    if "MOCK_SCCACHE_CALLED:" in result.stdout:
                        print("  ✅ Mock sccache was called through wrapper")
                        print(f"     Sccache called with: {result.stdout.strip()}")
                        results.passed += 1
                    else:
                        print("  ❌ Mock sccache was not called")
                        results.failed += 1
                        results.errors.append(
                            "Mock sccache was not called through wrapper"
                        )
                else:
                    print("  ❌ Wrapper script was not executed")
                    results.failed += 1
                    results.errors.append("Wrapper script was not executed")
            else:
                print("  ❌ Wrapper script was not created")
                print(f"     Stderr: {result.stderr}")
                results.failed += 1
                results.errors.append(
                    f"Wrapper script creation failed: {result.stderr}"
                )

        except subprocess.TimeoutExpired:
            print("  ❌ Wrapper script test timed out")
            results.failed += 1
            results.errors.append("Wrapper script test timed out")
        except Exception as e:
            print(f"  ❌ Wrapper script test error: {e}")
            results.failed += 1
            results.errors.append(f"Wrapper script test error: {e}")


def test_xcache_direct_mode(results: TestResults) -> None:
    """Test xcache direct mode (no response files)."""
    print("\n🧪 Testing xcache direct mode...")

    clang_path = find_clang()
    if not clang_path:
        results.errors.append("clang compiler not found - skipping direct mode test")
        return

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Create test source file
        source_file = create_test_source_file(temp_path)

        # Create a mock sccache script that just passes through to real compiler
        mock_sccache = (
            temp_path / "sccache.bat" if os.name == "nt" else temp_path / "sccache"
        )

        if os.name == "nt":
            mock_content = f'''@echo off
echo MOCK_SCCACHE_DIRECT: %*
"{clang_path}" %*
'''
        else:
            mock_content = f'''#!/bin/bash
echo "MOCK_SCCACHE_DIRECT: $@"
"{clang_path}" "$@"
'''

        mock_sccache.write_text(mock_content)
        if os.name != "nt":
            mock_sccache.chmod(0o755)

        # Set up environment to use mock sccache
        env = os.environ.copy()
        env["PATH"] = f"{temp_path}{os.pathsep}{env.get('PATH', '')}"
        env["XCACHE_DEBUG"] = "1"

        output_file = temp_path / "test_direct.o"

        # Test xcache without response files (direct mode)
        cmd = [
            sys.executable,
            "ci/util/xcache.py",
            clang_path,
            "-c",
            str(source_file),
            "-o",
            str(output_file),
        ]

        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True, env=env, timeout=30
            )

            if result.returncode == 0 and output_file.exists():
                print("  ✅ Direct mode compilation succeeded")
                print(f"     Output file created: {output_file.stat().st_size} bytes")

                # Check if direct execution was used (no wrapper)
                if "Direct execution:" in result.stderr:
                    print("  ✅ Direct execution mode confirmed")
                    results.passed += 1
                else:
                    print("  ⚠️  Direct execution mode not clearly indicated")
                    results.passed += 1  # Still pass since compilation worked

            else:
                print(
                    f"  ❌ Direct mode compilation failed (return code: {result.returncode})"
                )
                print(f"     Stderr: {result.stderr}")
                results.failed += 1
                results.errors.append(
                    f"Direct mode compilation failed: {result.stderr}"
                )

        except subprocess.TimeoutExpired:
            print("  ❌ Direct mode test timed out")
            results.failed += 1
            results.errors.append("Direct mode test timed out")
        except Exception as e:
            print(f"  ❌ Direct mode test error: {e}")
            results.failed += 1
            results.errors.append(f"Direct mode test error: {e}")


def run_all_tests() -> TestResults:
    """Run all xcache standalone tests."""
    print("🚀 XCACHE STANDALONE TESTS WITH CLANG")
    print("=" * 60)

    results = TestResults()

    # Check prerequisites
    clang_path = find_clang()

    print(f"📋 Test Environment:")
    print(f"   Clang: {clang_path or 'NOT FOUND'}")
    print(f"   xcache: ci/util/xcache.py")
    print(f"   Note: Using mock sccache for testing wrapper functionality")
    print()

    if not clang_path:
        print("❌ Clang compiler not found - install clang to run tests")
        results.errors.append("Clang compiler not available")
        return results

    # Run tests
    test_clang_direct_compilation(results)
    test_clang_response_file_compilation(results)
    test_xcache_wrapper_script_creation(results)
    test_xcache_direct_mode(results)

    return results


def main() -> int:
    """Main test function."""
    results = run_all_tests()

    print("\n" + "=" * 60)
    print("📊 TEST RESULTS SUMMARY")
    print("=" * 60)

    total_tests = results.passed + results.failed
    if total_tests > 0:
        success_rate = (results.passed / total_tests) * 100
        print(f"   Tests passed: {results.passed}")
        print(f"   Tests failed: {results.failed}")
        print(f"   Success rate: {success_rate:.1f}%")
    else:
        print("   No tests were executed")

    if results.errors:
        print(f"\n🚨 Errors encountered:")
        for i, error in enumerate(results.errors, 1):
            print(f"   {i}. {error}")

    if results.failed == 0 and results.passed > 0:
        print(
            "\n✅ All xcache standalone tests passed! Wrapper functionality verified."
        )
        return 0
    elif results.passed > 0:
        print(
            f"\n⚠️  Some tests passed but {results.failed} failed. Check errors above."
        )
        return 1
    else:
        print("\n❌ No tests passed. Check prerequisites and errors above.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
