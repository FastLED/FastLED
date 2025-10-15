#!/usr/bin/env python3
"""Meson build system integration for FastLED unit tests."""

import os
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Tuple

from running_process import RunningProcess

from ci.util.build_lock import libfastled_build_lock


def check_meson_installed() -> bool:
    """Check if Meson is installed and accessible."""
    try:
        result = subprocess.run(
            ["meson", "--version"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=5,
        )
        return result.returncode == 0
    except (subprocess.SubprocessError, FileNotFoundError):
        return False


def detect_system_llvm_tools() -> Tuple[bool, bool]:
    """
    Detect if system has LLD and LLVM-AR that support thin archives.

    Returns:
        Tuple of (has_lld, has_llvm_ar)
    """
    has_lld = False
    has_llvm_ar = False

    # Check for system lld (not zig's bundled lld)
    # Try 'lld' first (created by workflow symlink or available by default)
    # Then try 'ld.lld' (Linux naming) as fallback
    for lld_cmd in ["lld", "ld.lld"]:
        try:
            result = subprocess.run(
                [lld_cmd, "--version"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=5,
            )
            if result.returncode == 0:
                has_lld = True
                break
        except (subprocess.SubprocessError, FileNotFoundError):
            continue

    # Check for llvm-ar with thin archive support
    # Try unversioned first, then common versioned variants (llvm-ar-20, llvm-ar-19, etc.)
    for ar_cmd in ["llvm-ar", "llvm-ar-20", "llvm-ar-19", "llvm-ar-18"]:
        try:
            result = subprocess.run(
                [ar_cmd, "--help"],
                capture_output=True,
                encoding="utf-8",
                errors="replace",
                timeout=5,
            )
            if result.returncode == 0 and "thin" in result.stdout.lower():
                has_llvm_ar = True
                break
        except (subprocess.SubprocessError, FileNotFoundError):
            continue

    return has_lld, has_llvm_ar


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

    # Detect system LLVM tools
    has_lld, has_llvm_ar = detect_system_llvm_tools()

    # Enable thin archives if:
    # 1. System has LLVM tools that support thin archives (both lld and llvm-ar)
    # 2. Not explicitly disabled via FASTLED_DISABLE_THIN_ARCHIVES=1
    disable_thin_archives = os.environ.get("FASTLED_DISABLE_THIN_ARCHIVES", "0") == "1"
    use_thin_archives = (has_lld and has_llvm_ar) and not disable_thin_archives

    # Set compiler environment variables to use zig
    # This matches the compiler used by the regular build system (ci/compiler/clang_compiler.py:89)
    # Determine if we need to run meson setup/reconfigure
    # We skip meson setup only if already configured and not reconfiguring
    skip_meson_setup = already_configured and not reconfigure

    # Create wrapper scripts for meson since it expects single executables
    # IMPORTANT: Always recreate wrappers to ensure thin archive flags match current detection
    # This happens before the skip check so wrappers are always up-to-date
    wrapper_dir = build_dir / "compiler_wrappers"
    wrapper_dir.mkdir(parents=True, exist_ok=True)

    cmd: Optional[List[str]] = None
    if skip_meson_setup:
        # Build already configured, wrappers will be recreated below
        print(f"[MESON] Build directory already configured: {build_dir}")
        print(
            f"[MESON] Recreating compiler wrappers with current thin archive settings"
        )
    elif already_configured and reconfigure:
        # Reconfigure existing build
        print(f"[MESON] Reconfiguring build directory: {build_dir}")
        cmd = ["meson", "setup", "--reconfigure", str(build_dir)]
    else:
        # Initial setup
        print(f"[MESON] Setting up build directory: {build_dir}")
        cmd = ["meson", "setup", str(build_dir)]

    is_windows = sys.platform.startswith("win") or os.name == "nt"

    # Thin archives configuration (faster builds, smaller disk usage when supported)
    thin_flag = " --thin" if use_thin_archives else ""

    if use_thin_archives:
        print("[MESON] ✅ Thin archives enabled (using system LLVM tools)")
    else:
        if not (has_lld and has_llvm_ar):
            # Show prominent yellow warning banner when optimization cannot be enabled
            print("=" * 80)
            print("⚠️  WARNING: Build optimization unavailable - thin archives disabled")
            print("    Reason: System LLVM tools (lld and llvm-ar) not found")
            print("    Impact: Slower archive creation, larger disk usage")
            missing_tools: List[str] = []
            if not has_lld:
                missing_tools.append("lld/ld.lld")
            if not has_llvm_ar:
                missing_tools.append("llvm-ar")
            print(f"    Missing: {', '.join(missing_tools)}")
            print("=" * 80)
        else:
            print(
                "[MESON] ℹ️  Thin archives disabled (explicitly disabled via FASTLED_DISABLE_THIN_ARCHIVES=1)"
            )

    # Use system tools when available for thin archives, otherwise use zig
    use_system_ar = use_thin_archives and has_llvm_ar

    # When using thin archives, we must use system lld for linking
    # Add -fuse-ld=lld flag to force use of system lld instead of zig's bundled lld
    linker_flag = " -fuse-ld=lld" if use_thin_archives and has_lld else ""

    if is_windows:
        # Windows: Create .cmd wrappers
        cc_wrapper = wrapper_dir / "zig-cc.cmd"
        cxx_wrapper = wrapper_dir / "zig-cxx.cmd"
        ar_wrapper = wrapper_dir / "zig-ar.cmd"
        cc_wrapper.write_text(
            f"@echo off\npython -m ziglang cc{linker_flag} %*\n", encoding="utf-8"
        )
        cxx_wrapper.write_text(
            f"@echo off\npython -m ziglang c++{linker_flag} %*\n", encoding="utf-8"
        )
        if use_system_ar:
            # Use system llvm-ar with thin archives support
            ar_wrapper.write_text(
                f"@echo off\nllvm-ar{thin_flag} %*\n", encoding="utf-8"
            )
        else:
            # Use zig's ar
            # Filter out thin archive flag 'T' from arguments when not using thin archives
            # because Zig's linker doesn't support thin archives
            if use_thin_archives:
                ar_wrapper.write_text(
                    f"@echo off\npython -m ziglang ar{thin_flag} %*\n", encoding="utf-8"
                )
            else:
                # When not using thin archives, filter out 'T' flag from ar arguments
                ar_wrapper.write_text(
                    "@echo off\n"
                    "setlocal enabledelayedexpansion\n"
                    'set "filtered_args="\n'
                    "for %%a in (%*) do (\n"
                    '  set "arg=%%a"\n'
                    '  echo !arg! | findstr /r "^cr.*T.*$ ^rc.*T.*$" >nul\n'
                    "  if !errorlevel! equ 0 (\n"
                    "    REM Remove T flag from ar operation flags\n"
                    '    set "arg=!arg:T=!"\n'
                    "  )\n"
                    '  set "filtered_args=!filtered_args! !arg!"\n'
                    ")\n"
                    "python -m ziglang ar %filtered_args%\n",
                    encoding="utf-8",
                )
    else:
        # Unix/Linux/macOS: Create shell script wrappers
        cc_wrapper = wrapper_dir / "zig-cc"
        cxx_wrapper = wrapper_dir / "zig-cxx"
        ar_wrapper = wrapper_dir / "zig-ar"
        cc_wrapper.write_text(
            f'#!/bin/sh\nexec python -m ziglang cc{linker_flag} "$@"\n',
            encoding="utf-8",
        )
        cxx_wrapper.write_text(
            f'#!/bin/sh\nexec python -m ziglang c++{linker_flag} "$@"\n',
            encoding="utf-8",
        )
        if use_system_ar:
            # Use system llvm-ar with thin archives support
            ar_wrapper.write_text(
                f'#!/bin/sh\nexec llvm-ar{thin_flag} "$@"\n',
                encoding="utf-8",
            )
        else:
            # Use zig's ar
            # Filter out thin archive flag 'T' from arguments when not using thin archives
            # because Zig's linker doesn't support thin archives
            if use_thin_archives:
                ar_wrapper.write_text(
                    f'#!/bin/sh\nexec python -m ziglang ar{thin_flag} "$@"\n',
                    encoding="utf-8",
                )
            else:
                # When not using thin archives, filter out 'T' flag from ar arguments
                ar_wrapper.write_text(
                    "#!/bin/sh\n"
                    "# Filter out thin archive flag T - Zig linker does not support thin archives\n"
                    'filtered_args=""\n'
                    'for arg in "$@"; do\n'
                    '  case "$arg" in\n'
                    "    cr*T* | rc*T*)\n"
                    "      # Remove T flag from ar operation flags\n"
                    '      arg=$(echo "$arg" | sed "s/T//g")\n'
                    "      ;;\n"
                    "  esac\n"
                    '  filtered_args="$filtered_args $arg"\n'
                    "done\n"
                    "exec python -m ziglang ar $filtered_args\n",
                    encoding="utf-8",
                )
        # Make executable on Unix-like systems
        cc_wrapper.chmod(0o755)
        cxx_wrapper.chmod(0o755)
        ar_wrapper.chmod(0o755)

    env = os.environ.copy()
    env["CC"] = str(cc_wrapper)
    env["CXX"] = str(cxx_wrapper)
    env["AR"] = str(ar_wrapper)

    ar_tool = "llvm-ar" if use_system_ar else "zig ar"
    linker_tool = "system lld" if (use_thin_archives and has_lld) else "zig lld"
    print(f"[MESON] Using compilers: CC=zig cc, CXX=zig c++, AR={ar_tool}")
    print(f"[MESON] Using linker: {linker_tool}")

    # If we're skipping meson setup (already configured), return early now that wrappers are recreated
    if skip_meson_setup:
        return True

    # Run meson setup using RunningProcess for proper streaming output
    assert cmd is not None, "cmd should be set when not skipping meson setup"
    try:
        proc = RunningProcess(
            cmd,
            cwd=source_dir,
            timeout=600,
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=env,
        )

        returncode = proc.wait(echo=True)

        if returncode != 0:
            print(
                f"[MESON] Setup failed with return code {returncode}", file=sys.stderr
            )
            return False

        print(f"[MESON] Setup successful")
        return True

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
        # Use RunningProcess for streaming output
        # Inherit environment to ensure compiler wrappers are available
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout for compilation
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=os.environ.copy(),  # Pass current environment with wrapper paths
        )

        returncode = proc.wait(echo=True)

        if returncode != 0:
            print(
                f"[MESON] Compilation failed with return code {returncode}",
                file=sys.stderr,
            )
            return False

        print(f"[MESON] Compilation successful")
        return True

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
    cmd = ["meson", "test", "-C", str(build_dir), "--print-errorlogs"]

    if verbose:
        cmd.append("-v")

    if test_name:
        cmd.append(test_name)
        print(f"[MESON] Running test: {test_name}")
    else:
        print(f"[MESON] Running all tests...")

    try:
        # Use RunningProcess for streaming output
        # Inherit environment to ensure compiler wrappers are available
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout for tests
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=os.environ.copy(),  # Pass current environment with wrapper paths
        )

        returncode = proc.wait(echo=True)

        if returncode != 0:
            print(
                f"[MESON] Tests failed with return code {returncode}", file=sys.stderr
            )
            return False

        print(f"[MESON] All tests passed")
        return True

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

    # Convert test name to executable name (add test_ prefix if needed, convert to lowercase)
    meson_test_name = None
    if test_name:
        # Convert to lowercase to match Meson target naming convention
        test_name_lower = test_name.lower()
        if not test_name_lower.startswith("test_"):
            meson_test_name = f"test_{test_name_lower}"
        else:
            meson_test_name = test_name_lower

    # Compile with build lock to prevent conflicts with example builds
    try:
        with libfastled_build_lock(timeout=600):  # 10 minute timeout
            if not compile_meson(
                build_dir, target=meson_test_name if meson_test_name else None
            ):
                return False
    except TimeoutError as e:
        print(f"[MESON] {e}", file=sys.stderr)
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
