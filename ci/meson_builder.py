#!/usr/bin/env python3
"""Meson build system wrapper for FastLED native builds."""

import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Optional


class MesonBuilder:
    """Handles Meson build system operations for FastLED."""

    def __init__(
        self,
        source_dir: Optional[Path] = None,
        build_dir: Optional[Path] = None,
        verbose: bool = False,
    ):
        """
        Initialize Meson builder.

        Args:
            source_dir: Path to meson source directory (ci/meson)
            build_dir: Path to build output directory
            verbose: Enable verbose output
        """
        self.source_dir = source_dir or Path.cwd() / "ci" / "meson"
        self.build_dir = build_dir or Path.cwd() / ".build" / "meson"
        self.verbose = verbose

    def setup(
        self,
        reconfigure: bool = False,
        unity: bool = True,
        unity_size: int = 20,
    ) -> int:
        """
        Run meson setup to configure the build.

        Args:
            reconfigure: Force reconfiguration even if already configured
            unity: Enable unity builds
            unity_size: Number of files per unity translation unit

        Returns:
            Exit code (0 for success)
        """
        cmd: List[str] = ["meson", "setup"]

        if reconfigure and self.build_dir.exists():
            cmd.append("--reconfigure")

        # Add build options
        cmd.extend(
            [
                f"-Dfastled_unity={'enabled' if unity else 'disabled'}",
                f"-Dfastled_unity_size={unity_size}",
                str(self.build_dir),
                str(self.source_dir),
            ]
        )

        if self.verbose:
            print(f"Running: {' '.join(cmd)}")

        return subprocess.call(cmd)

    def compile(self, jobs: Optional[int] = None) -> int:
        """
        Run meson compile to build the project.

        Args:
            jobs: Number of parallel jobs (None = auto-detect)

        Returns:
            Exit code (0 for success)
        """
        if not self.build_dir.exists():
            print("Error: Build directory does not exist. Run setup first.")
            return 1

        cmd: List[str] = ["meson", "compile", "-C", str(self.build_dir)]

        if jobs:
            cmd.extend(["-j", str(jobs)])

        if self.verbose:
            cmd.append("-v")
            print(f"Running: {' '.join(cmd)}")

        return subprocess.call(cmd)

    def test(self, test_name: Optional[str] = None, verbose: bool = False) -> int:
        """
        Run meson test to execute tests.

        Args:
            test_name: Optional specific test to run (None = all tests)
            verbose: Enable verbose test output

        Returns:
            Exit code (0 for success)
        """
        if not self.build_dir.exists():
            print("Error: Build directory does not exist. Run setup and compile first.")
            return 1

        cmd: List[str] = ["meson", "test", "-C", str(self.build_dir)]

        if verbose or self.verbose:
            cmd.append("-v")

        if test_name:
            cmd.append(test_name)

        if self.verbose:
            print(f"Running: {' '.join(cmd)}")

        return subprocess.call(cmd)

    def clean(self) -> int:
        """
        Clean the build directory.

        Returns:
            Exit code (0 for success)
        """
        if self.build_dir.exists():
            if self.verbose:
                print(f"Removing build directory: {self.build_dir}")
            shutil.rmtree(self.build_dir)
        return 0

    def full_build_and_test(
        self,
        clean: bool = False,
        unity: bool = True,
        unity_size: int = 20,
        test_name: Optional[str] = None,
    ) -> int:
        """
        Perform a full build and test cycle.

        Args:
            clean: Clean before building
            unity: Enable unity builds
            unity_size: Number of files per unity translation unit
            test_name: Optional specific test to run (None = all tests)

        Returns:
            Exit code (0 for success)
        """
        if clean:
            ret = self.clean()
            if ret != 0:
                return ret

        # Setup
        ret = self.setup(reconfigure=False, unity=unity, unity_size=unity_size)
        if ret != 0:
            print("Meson setup failed")
            return ret

        # Compile
        ret = self.compile()
        if ret != 0:
            print("Meson compile failed")
            return ret

        # Test
        ret = self.test(test_name=test_name, verbose=True)
        if ret != 0:
            print("Meson test failed")
            return ret

        return 0


def main() -> int:
    """Command-line entry point for Meson builder."""
    import argparse

    parser = argparse.ArgumentParser(description="FastLED Meson build wrapper")
    parser.add_argument("--clean", action="store_true", help="Clean before building")
    parser.add_argument("--no-unity", action="store_true", help="Disable unity builds")
    parser.add_argument(
        "--unity-size", type=int, default=20, help="Files per unity TU (default: 20)"
    )
    parser.add_argument(
        "--test", type=str, help="Specific test to run (default: all tests)"
    )
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")

    args = parser.parse_args()

    builder = MesonBuilder(verbose=args.verbose)
    return builder.full_build_and_test(
        clean=args.clean,
        unity=not args.no_unity,
        unity_size=args.unity_size,
        test_name=args.test,
    )


if __name__ == "__main__":
    sys.exit(main())
