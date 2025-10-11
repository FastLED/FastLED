#!/usr/bin/env python3
"""Meson build system integration for FastLED unit tests."""

import subprocess
import sys
from pathlib import Path
from typing import List, Optional


def check_meson_installed() -> bool:
    """Check if Meson is installed and accessible."""
    try:
        result = subprocess.run(
            ["meson", "--version"], capture_output=True, text=True, timeout=5
        )
        return result.returncode == 0
    except (subprocess.SubprocessError, FileNotFoundError):
        return False


def setup_meson_build(
    source_dir: Path, build_dir: Path, reconfigure: bool = False
) -> bool:
    """
    Set up Meson build directory.

    Args:
        source_dir: Project root directory containing meson.build
        build_dir: Build output directory
        reconfigure: Force reconfiguration of existing build

    Returns:
        True if setup successful, False otherwise
    """
    # Create build directory if it doesn't exist
    build_dir.mkdir(parents=True, exist_ok=True)

    # Check if already configured
    meson_info = build_dir / "meson-info"
    already_configured = meson_info.exists()

    if already_configured and not reconfigure:
        print(f"[MESON] Build directory already configured: {build_dir}")
        return True

    cmd: List[str] = []
    if already_configured and reconfigure:
        # Reconfigure existing build
        print(f"[MESON] Reconfiguring build directory: {build_dir}")
        cmd = ["meson", "setup", "--reconfigure", str(build_dir)]
    else:
        # Initial setup
        print(f"[MESON] Setting up build directory: {build_dir}")
        cmd = ["meson", "setup", str(build_dir)]

    try:
        result = subprocess.run(
            cmd, cwd=source_dir, capture_output=True, text=True, timeout=60
        )

        if result.returncode != 0:
            print(f"[MESON] Setup failed:")
            print(result.stdout)
            print(result.stderr, file=sys.stderr)
            return False

        print(f"[MESON] Setup successful")
        if result.stdout:
            print(result.stdout)

        return True

    except subprocess.TimeoutExpired:
        print("[MESON] Setup timed out after 60 seconds", file=sys.stderr)
        return False
    except Exception as e:
        print(f"[MESON] Setup failed with exception: {e}", file=sys.stderr)
        return False


def compile_meson(build_dir: Path, target: Optional[str] = None) -> bool:
    """
    Compile using Meson.

    Args:
        build_dir: Meson build directory
        target: Specific target to build (None = all)

    Returns:
        True if compilation successful, False otherwise
    """
    cmd = ["meson", "compile", "-C", str(build_dir)]

    if target:
        cmd.append(target)
        print(f"[MESON] Compiling target: {target}")
    else:
        print(f"[MESON] Compiling all targets...")

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=600,  # 10 minute timeout for compilation
        )

        if result.returncode != 0:
            print(f"[MESON] Compilation failed:")
            print(result.stdout)
            print(result.stderr, file=sys.stderr)
            return False

        print(f"[MESON] Compilation successful")
        if result.stdout:
            print(result.stdout)

        return True

    except subprocess.TimeoutExpired:
        print("[MESON] Compilation timed out after 600 seconds", file=sys.stderr)
        return False
    except Exception as e:
        print(f"[MESON] Compilation failed with exception: {e}", file=sys.stderr)
        return False


def run_meson_test(
    build_dir: Path, test_name: Optional[str] = None, verbose: bool = False
) -> bool:
    """
    Run tests using Meson.

    Args:
        build_dir: Meson build directory
        test_name: Specific test to run (None = all)
        verbose: Enable verbose test output

    Returns:
        True if all tests passed, False otherwise
    """
    cmd = ["meson", "test", "-C", str(build_dir)]

    if verbose:
        cmd.append("-v")

    if test_name:
        cmd.append(test_name)
        print(f"[MESON] Running test: {test_name}")
    else:
        print(f"[MESON] Running all tests...")

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=600,  # 10 minute timeout for tests
        )

        # Always print output for tests
        if result.stdout:
            print(result.stdout)

        if result.returncode != 0:
            print(f"[MESON] Tests failed:")
            if result.stderr:
                print(result.stderr, file=sys.stderr)
            return False

        print(f"[MESON] All tests passed")
        return True

    except subprocess.TimeoutExpired:
        print("[MESON] Tests timed out after 600 seconds", file=sys.stderr)
        return False
    except Exception as e:
        print(f"[MESON] Test execution failed with exception: {e}", file=sys.stderr)
        return False


def run_meson_build_and_test(
    source_dir: Path,
    build_dir: Path,
    test_name: Optional[str] = None,
    clean: bool = False,
    verbose: bool = False,
) -> bool:
    """
    Complete Meson build and test workflow.

    Args:
        source_dir: Project root directory
        build_dir: Build output directory
        test_name: Specific test to run (without test_ prefix, e.g., "json")
        clean: Clean build directory before setup
        verbose: Enable verbose output

    Returns:
        True if build and tests successful, False otherwise
    """
    # Check if Meson is installed
    if not check_meson_installed():
        print("[MESON] Error: Meson build system is not installed", file=sys.stderr)
        print("[MESON] Install with: pip install meson ninja", file=sys.stderr)
        return False

    # Clean if requested
    if clean and build_dir.exists():
        print(f"[MESON] Cleaning build directory: {build_dir}")
        import shutil

        shutil.rmtree(build_dir)

    # Setup build
    if not setup_meson_build(source_dir, build_dir, reconfigure=False):
        return False

    # Convert test name to executable name (add test_ prefix if needed)
    meson_test_name = None
    if test_name:
        if not test_name.startswith("test_"):
            meson_test_name = f"test_{test_name}"
        else:
            meson_test_name = test_name

    # Compile
    if not compile_meson(
        build_dir, target=meson_test_name if meson_test_name else None
    ):
        return False

    # Run tests
    if not run_meson_test(build_dir, test_name=meson_test_name, verbose=verbose):
        return False

    return True


if __name__ == "__main__":
    # Simple CLI for testing
    import argparse

    parser = argparse.ArgumentParser(
        description="Meson build system runner for FastLED"
    )
    parser.add_argument("--test", help="Specific test to build and run")
    parser.add_argument("--clean", action="store_true", help="Clean build directory")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--build-dir", default=".build/meson", help="Build directory")

    args = parser.parse_args()

    source_dir = Path.cwd()
    build_dir = Path(args.build_dir)

    success = run_meson_build_and_test(
        source_dir=source_dir,
        build_dir=build_dir,
        test_name=args.test,
        clean=args.clean,
        verbose=args.verbose,
    )

    sys.exit(0 if success else 1)
