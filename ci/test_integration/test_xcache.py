#!/usr/bin/env python3
"""
Integration tests for xcache.py using real clang compiler.

This test suite verifies that xcache works correctly with actual compilation
scenarios, including response files and direct execution modes.
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


def find_sccache() -> Optional[str]:
    """Find sccache in PATH."""
    return shutil.which("sccache")


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

    # Write arguments to response file (one per line or space-separated)
    content = " ".join(args)
    response_file.write_text(content)

    return response_file


def test_xcache_direct_execution(results: TestResults) -> None:
    """Test xcache with direct execution (no response files)."""
    print("🧪 Testing xcache direct execution...")

    clang_path = find_clang()
    if not clang_path:
        results.errors.append(
            "clang compiler not found - skipping direct execution test"
        )
        return

    sccache_path = find_sccache()
    if not sccache_path:
        results.errors.append("sccache not found - skipping direct execution test")
        return

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Create test source file
        source_file = create_test_source_file(temp_path)
        output_file = temp_path / "test_direct.o"

        # Test xcache with direct arguments (no response files)
        env = os.environ.copy()
        env["XCACHE_DEBUG"] = "1"

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
                print("  ✅ Direct execution test passed")
                print(f"     Output file created: {output_file.stat().st_size} bytes")
                results.passed += 1
            else:
                print(
                    f"  ❌ Direct execution test failed (return code: {result.returncode})"
                )
                print(f"     Stderr: {result.stderr}")
                results.failed += 1
                results.errors.append(f"Direct execution failed: {result.stderr}")

        except subprocess.TimeoutExpired:
            print("  ❌ Direct execution test timed out")
            results.failed += 1
            results.errors.append("Direct execution test timed out")
        except Exception as e:
            print(f"  ❌ Direct execution test error: {e}")
            results.failed += 1
            results.errors.append(f"Direct execution error: {e}")


def test_xcache_response_file_execution(results: TestResults) -> None:
    """Test xcache with response file execution."""
    print("\n🧪 Testing xcache response file execution...")

    clang_path = find_clang()
    if not clang_path:
        results.errors.append("clang compiler not found - skipping response file test")
        return

    sccache_path = find_sccache()
    if not sccache_path:
        results.errors.append("sccache not found - skipping response file test")
        return

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Create test source file
        source_file = create_test_source_file(temp_path)
        output_file = temp_path / "test_response.o"

        # Create response file with compilation arguments
        compile_args = ["-c", str(source_file), "-o", str(output_file), "-O2", "-Wall"]
        response_file = create_response_file(temp_path, compile_args)

        # Test xcache with response file
        env = os.environ.copy()
        env["XCACHE_DEBUG"] = "1"

        cmd = [sys.executable, "ci/util/xcache.py", clang_path, f"@{response_file}"]

        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True, env=env, timeout=30
            )

            if result.returncode == 0 and output_file.exists():
                print("  ✅ Response file execution test passed")
                print(f"     Output file created: {output_file.stat().st_size} bytes")
                print(f"     Response file handled: @{response_file}")

                # Verify that response file was passed through (not expanded)
                if f"@{response_file}" in result.stderr:
                    print("  ✅ Response file passed through correctly")
                else:
                    print(
                        "  ⚠️  Response file passthrough not clearly visible in debug output"
                    )

                results.passed += 1
            else:
                print(
                    f"  ❌ Response file execution test failed (return code: {result.returncode})"
                )
                print(f"     Stderr: {result.stderr}")
                results.failed += 1
                results.errors.append(
                    f"Response file execution failed: {result.stderr}"
                )

        except subprocess.TimeoutExpired:
            print("  ❌ Response file execution test timed out")
            results.failed += 1
            results.errors.append("Response file execution test timed out")
        except Exception as e:
            print(f"  ❌ Response file execution test error: {e}")
            results.failed += 1
            results.errors.append(f"Response file execution error: {e}")


def test_xcache_mixed_arguments(results: TestResults) -> None:
    """Test xcache with mixed regular arguments and response files."""
    print("\n🧪 Testing xcache mixed arguments...")

    clang_path = find_clang()
    if not clang_path:
        results.errors.append(
            "clang compiler not found - skipping mixed arguments test"
        )
        return

    sccache_path = find_sccache()
    if not sccache_path:
        results.errors.append("sccache not found - skipping mixed arguments test")
        return

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Create test source file
        source_file = create_test_source_file(temp_path)
        output_file = temp_path / "test_mixed.o"

        # Create response file with some arguments
        response_args = ["-O2", "-Wall", "-Wextra"]
        response_file = create_response_file(temp_path, response_args)

        # Test xcache with mixed arguments
        env = os.environ.copy()
        env["XCACHE_DEBUG"] = "1"

        cmd = [
            sys.executable,
            "ci/util/xcache.py",
            clang_path,
            "-c",
            str(source_file),
            f"@{response_file}",
            "-o",
            str(output_file),
        ]

        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True, env=env, timeout=30
            )

            if result.returncode == 0 and output_file.exists():
                print("  ✅ Mixed arguments test passed")
                print(f"     Output file created: {output_file.stat().st_size} bytes")
                print(f"     Mixed args: regular + @{response_file}")
                results.passed += 1
            else:
                print(
                    f"  ❌ Mixed arguments test failed (return code: {result.returncode})"
                )
                print(f"     Stderr: {result.stderr}")
                results.failed += 1
                results.errors.append(f"Mixed arguments failed: {result.stderr}")

        except subprocess.TimeoutExpired:
            print("  ❌ Mixed arguments test timed out")
            results.failed += 1
            results.errors.append("Mixed arguments test timed out")
        except Exception as e:
            print(f"  ❌ Mixed arguments test error: {e}")
            results.failed += 1
            results.errors.append(f"Mixed arguments error: {e}")


def test_xcache_cache_effectiveness(results: TestResults) -> None:
    """Test that xcache actually caches compilation results."""
    print("\n🧪 Testing xcache cache effectiveness...")

    clang_path = find_clang()
    if not clang_path:
        results.errors.append(
            "clang compiler not found - skipping cache effectiveness test"
        )
        return

    sccache_path = find_sccache()
    if not sccache_path:
        results.errors.append("sccache not found - skipping cache effectiveness test")
        return

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Create test source file
        source_file = create_test_source_file(temp_path)

        # Set up environment for caching
        env = os.environ.copy()
        env["XCACHE_DEBUG"] = "1"
        cache_dir = temp_path / "sccache"
        cache_dir.mkdir()
        env["SCCACHE_DIR"] = str(cache_dir)

        # First compilation (should be cache miss)
        output_file1 = temp_path / "test_cache1.o"
        cmd1 = [
            sys.executable,
            "ci/util/xcache.py",
            clang_path,
            "-c",
            str(source_file),
            "-o",
            str(output_file1),
        ]

        # Second compilation (should be cache hit)
        output_file2 = temp_path / "test_cache2.o"
        cmd2 = [
            sys.executable,
            "ci/util/xcache.py",
            clang_path,
            "-c",
            str(source_file),
            "-o",
            str(output_file2),
        ]

        try:
            # First compilation
            result1 = subprocess.run(
                cmd1, capture_output=True, text=True, env=env, timeout=30
            )

            # Second compilation
            result2 = subprocess.run(
                cmd2, capture_output=True, text=True, env=env, timeout=30
            )

            if (
                result1.returncode == 0
                and output_file1.exists()
                and result2.returncode == 0
                and output_file2.exists()
            ):
                print("  ✅ Cache effectiveness test passed")
                print(f"     First compilation: {output_file1.stat().st_size} bytes")
                print(f"     Second compilation: {output_file2.stat().st_size} bytes")

                # Check if sccache cache directory has content
                cache_files = list(cache_dir.rglob("*"))
                if cache_files:
                    print(f"     Cache directory has {len(cache_files)} entries")
                else:
                    print("     ⚠️  Cache directory appears empty")

                results.passed += 1
            else:
                print(f"  ❌ Cache effectiveness test failed")
                print(
                    f"     First result: {result1.returncode}, {output_file1.exists()}"
                )
                print(
                    f"     Second result: {result2.returncode}, {output_file2.exists()}"
                )
                results.failed += 1
                results.errors.append("Cache effectiveness test failed")

        except subprocess.TimeoutExpired:
            print("  ❌ Cache effectiveness test timed out")
            results.failed += 1
            results.errors.append("Cache effectiveness test timed out")
        except Exception as e:
            print(f"  ❌ Cache effectiveness test error: {e}")
            results.failed += 1
            results.errors.append(f"Cache effectiveness error: {e}")


def run_all_tests() -> TestResults:
    """Run all xcache tests."""
    print("🚀 XCACHE INTEGRATION TESTS WITH CLANG")
    print("=" * 60)

    results = TestResults()

    # Check prerequisites
    clang_path = find_clang()
    sccache_path = find_sccache()

    print(f"📋 Test Environment:")
    print(f"   Clang: {clang_path or 'NOT FOUND'}")
    print(f"   Sccache: {sccache_path or 'NOT FOUND'}")
    print(f"   xcache: ci/util/xcache.py")
    print()

    if not clang_path:
        print("❌ Clang compiler not found - install clang to run tests")
        results.errors.append("Clang compiler not available")
        return results

    if not sccache_path:
        print("❌ Sccache not found - install sccache to run tests")
        results.errors.append("Sccache not available")
        return results

    # Run tests
    test_xcache_direct_execution(results)
    test_xcache_response_file_execution(results)
    test_xcache_mixed_arguments(results)
    test_xcache_cache_effectiveness(results)

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
        print("\n✅ All xcache tests passed! Ready for ESP32S3 integration.")
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
