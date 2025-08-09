#!/usr/bin/env python3
"""
FastLED Unified Build API

This module provides a unified build system for both unit tests and examples,
centralizing PCH and library building while providing a simple, elegant API.

Key Features:
- Unified API for both unit tests and examples
- Automatic PCH header building and reuse
- Automatic libfastled.a building and reuse
- Organized build directories under .build/
- Efficient multiple target building
- Simple, elegant interface

Usage:
    # Unit test building
    unit_builder = BuildAPI(
        config_file="ci/build_unit.toml",
        build_dir=".build/unit",
        build_type=BuildType.UNIT_TEST
    )
    unit_builder.build_targets(["test_json.cpp", "test_color.cpp"])

    # Example building
    example_builder = BuildAPI(
        config_file="ci/build_example.toml",
        build_dir=".build/examples",
        build_type=BuildType.EXAMPLE
    )
    example_builder.build_targets(["Blink.ino", "DemoReel100.ino"])
"""

import os
import shutil
import subprocess
import tempfile
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Set, Tuple

from ci.ci.fingerprint_cache import FingerprintCache
from ci.compiler.clang_compiler import (
    BuildFlags,
    Compiler,
    CompilerOptions,
    LibarchiveOptions,
)
from ci.compiler.test_example_compilation import get_fastled_core_sources
from ci.util.paths import PROJECT_ROOT
from ci.util.running_process import subprocess_run


class BuildType(Enum):
    """Build type enumeration"""

    UNIT_TEST = "unit_test"
    EXAMPLE = "example"


@dataclass
class BuildTarget:
    """Represents a single build target (test file or example)"""

    source_file: Path
    name: str
    output_dir: Path

    def get_output_executable(self) -> Path:
        """Get the expected output executable path"""
        if self.source_file.suffix == ".cpp":
            # Unit test: test_name -> test_name.exe
            base_name = self.source_file.stem
            return self.output_dir / f"{base_name}.exe"
        else:
            # Example: Blink.ino -> Blink.exe
            base_name = self.source_file.stem
            return self.output_dir / f"{base_name}.exe"


@dataclass
class BuildResult:
    """Result of a build operation"""

    target: BuildTarget
    success: bool
    message: str
    build_time: float


class BuildAPI:
    """
    Unified build API for both unit tests and examples.

    This class centralizes all build operations including PCH building,
    library creation, and target compilation. It provides a simple,
    elegant interface while handling the complexity internally.
    """

    def __init__(
        self,
        build_flags_toml: str | Path,
        build_dir: str | Path,
        build_type: BuildType,
        use_pch: bool = True,
        parallel: bool = True,
        clean: bool = False,
    ):
        """
        Initialize the build API.

        Args:
            build_flags_toml: Path to build_flags.toml file (build_unit.toml or build_example.toml)
            build_dir: Directory for build outputs (e.g., .build/unit or .build/examples)
            build_type: Type of build (UNIT_TEST or EXAMPLE)
            use_pch: Whether to use precompiled headers
            parallel: Whether to enable parallel compilation
            clean: Whether to force a clean build
        """
        self.build_flags_toml = Path(build_flags_toml)
        self.build_dir = Path(build_dir)
        self.build_type = build_type
        self.use_pch = use_pch
        self.parallel = parallel
        self.clean = clean

        # Ensure build directory exists
        self.build_dir.mkdir(parents=True, exist_ok=True)

        # Set up subdirectories for organization
        self.artifacts_dir = self.build_dir / "artifacts"  # PCH, lib files
        self.targets_dir = self.build_dir / "targets"  # Individual target builds
        self.cache_dir = self.build_dir / "cache"  # Fingerprint caches

        for directory in [self.artifacts_dir, self.targets_dir, self.cache_dir]:
            directory.mkdir(parents=True, exist_ok=True)

        # Initialize internal state
        self._compiler: Optional[Compiler] = None
        self._pch_file: Optional[Path] = None
        self._library_file: Optional[Path] = None
        self._fingerprint_cache: Optional[FingerprintCache] = None

        # Parse build flags from TOML file
        self._parse_build_flags()

        # Create compiler from TOML configuration
        self._create_compiler()

    def _parse_build_flags(self) -> None:
        """Parse build flags from TOML file"""
        if not self.build_flags_toml.exists():
            raise RuntimeError(
                f"Build flags TOML file not found: {self.build_flags_toml}"
            )

        print(f"[BUILD API] Parsing build flags from {self.build_flags_toml}")

        # Parse build flags from TOML
        self.build_flags = BuildFlags.from_toml_file(
            self.build_flags_toml,
            quick_build=True,  # Use quick build mode for faster iteration
            strict_mode=False,
        )

        # Initialize fingerprint cache
        cache_file = self.cache_dir / "fingerprint_cache.json"
        self._fingerprint_cache = FingerprintCache(cache_file)

    def _create_compiler(self) -> None:
        """Create compiler from TOML configuration ONLY - no inline settings allowed"""
        from ci.compiler.clang_compiler import create_compiler_options_from_toml

        # ALL settings must come from the TOML file - NO hardcoded/inline settings allowed
        print(f"[BUILD API] Creating compiler from TOML configuration")

        # Set up PCH output path
        pch_output_path: Optional[str] = None
        if self.use_pch:
            pch_output_path = str(
                self.artifacts_dir / f"{self.build_type.value}_pch.hpp.pch"
            )

        # Use the proper function to create CompilerOptions from TOML - NO inline settings
        self.compiler_settings = create_compiler_options_from_toml(
            toml_path=self.build_flags_toml,
            include_path=str(PROJECT_ROOT / "src"),  # Base include path only
            quick_build=True,  # Use quick build mode for faster iteration
            strict_mode=False,
            # Pass PCH and parallel settings from constructor
            use_pch=self.use_pch,
            pch_output_path=pch_output_path,
            parallel=self.parallel,
        )

        # Create compiler instance with TOML-loaded settings
        self._compiler = Compiler(self.compiler_settings, self.build_flags)

    def _ensure_pch_built(self) -> bool:
        """
        Ensure PCH header is built and ready.

        Returns:
            bool: True if PCH is ready, False otherwise
        """
        if not self.use_pch or not self._compiler:
            return True

        print(f"[BUILD API] Building PCH header for {self.build_type.value}")

        # Check if we need to rebuild PCH
        if not self.clean and self._pch_file and self._pch_file.exists():
            # TODO: Add proper dependency checking here
            print(f"[BUILD API] Reusing existing PCH: {self._pch_file}")
            return True

        # Build PCH
        success = self._compiler.create_pch_file()
        if success and self.compiler_settings.pch_output_path:
            self._pch_file = Path(self.compiler_settings.pch_output_path)
            print(f"[BUILD API] PCH built successfully: {self._pch_file}")
        else:
            print(f"[BUILD API] PCH build failed")

        return success

    def _ensure_library_built(self) -> bool:
        """
        Ensure libfastled.a is built and ready.

        Returns:
            bool: True if library is ready, False otherwise
        """
        if not self._compiler:
            return False

        lib_name = f"libfastled_{self.build_type.value}.a"
        self._library_file = self.artifacts_dir / lib_name

        print(f"[BUILD API] Building FastLED library for {self.build_type.value}")

        # Check if we need to rebuild library
        if not self.clean and self._library_file.exists():
            # TODO: Add proper dependency checking here
            print(f"[BUILD API] Reusing existing library: {self._library_file}")
            return True

        # Prefer UNITY chunk build for faster library creation
        try:
            unity_dir = self.artifacts_dir / "unity"
            unity_dir.mkdir(parents=True, exist_ok=True)

            # Collect FastLED core sources (consistent with example compilation)
            all_sources = [
                p for p in get_fastled_core_sources() if p.name != "stub_main.cpp"
            ]
            if not all_sources:
                print("[BUILD API] No FastLED sources found for unity build")
                return False

            # Deterministic sort relative to project root
            project_root = PROJECT_ROOT

            def sort_key(p: Path) -> str:
                try:
                    rel = p.relative_to(project_root)
                except Exception:
                    rel = p
                return rel.as_posix()

            all_sources = sorted(all_sources, key=sort_key)

            # Partition into chunks (1..4 typical)
            total = len(all_sources)
            max_chunks = min(4, max(1, (os.cpu_count() or 2) // 2))
            chunks = max_chunks if total >= max_chunks else max(1, total)

            # Prepare compile options (PCH off for unity)
            compile_opts = CompilerOptions(
                include_path=self._compiler.settings.include_path,
                compiler=self._compiler.settings.compiler,
                defines=self._compiler.settings.defines,
                std_version=self._compiler.settings.std_version,
                compiler_args=self._compiler.settings.compiler_args,
                use_pch=False,
                additional_flags=["-c"],
                parallel=self.parallel,
            )

            # Compute chunk sizes
            base = total // chunks
            rem = total % chunks
            start = 0
            unity_objects: list[Path] = []

            for i in range(chunks):
                size = base + (1 if i < rem else 0)
                end = start + size
                group = all_sources[start:end]
                start = end

                if not group:
                    continue

                unity_cpp = unity_dir / f"unity{i + 1}.cpp"
                # Use synchronous path to simplify error handling here
                result = self._compiler._compile_unity_sync(
                    compile_opts,
                    group,
                    unity_output_path=unity_cpp,
                )
                if not result.ok:
                    print(f"[BUILD API] Unity chunk {i + 1} failed: {result.stderr}")
                    return False
                unity_objects.append(unity_cpp.with_suffix(".o"))

            if not unity_objects:
                print("[BUILD API] No unity objects produced")
                return False

            # Archive unity objects into the final library
            archive_result = self._compiler.create_archive(
                unity_objects, self._library_file, LibarchiveOptions()
            ).result()
            if not archive_result.ok:
                print(f"[BUILD API] Archive creation failed: {archive_result.stderr}")
                return False

            print(
                f"[BUILD API] Library built successfully (UNITY): {self._library_file}"
            )
            return True
        except Exception as e:
            print(f"[BUILD API] UNITY library build failed with exception: {e}")
            return False

    def _create_static_library(
        self, object_files: List[Path], output_lib: Path
    ) -> bool:
        """
        Create static library from object files.

        Args:
            object_files: List of object files to archive
            output_lib: Output library file path

        Returns:
            bool: True if successful, False otherwise
        """
        if not object_files:
            return False

        # Use archiver from build flags
        archiver_cmd = self.build_flags.tools.archiver
        archive_flags = self.build_flags.archive.flags

        # Build archiver command
        cmd = (
            archiver_cmd
            + [archive_flags]
            + [str(output_lib)]
            + [str(f) for f in object_files]
        )

        print(f"[BUILD API] Creating static library: {' '.join(cmd)}")

        # Execute archiver command using stdout-pumping wrapper
        try:
            _completed = subprocess_run(
                command=cmd,
                cwd=None,
                check=True,
                timeout=900,
                enable_stack_trace=False,
            )
            return True
        except subprocess.CalledProcessError as e:
            print(f"[BUILD API] Archiver failed: {e}")
            print(f"[BUILD API] Archiver stdout: {e.stdout}")
            print(f"[BUILD API] Archiver stderr: {e.stderr}")
            return False

    def build_targets(self, target_files: Sequence[str | Path]) -> List[BuildResult]:
        """
        Build multiple targets efficiently.

        Args:
            target_files: List of source files to build

        Returns:
            List[BuildResult]: Results for each target
        """
        print(
            f"[BUILD API] Building {len(target_files)} targets for {self.build_type.value}"
        )

        # Ensure shared artifacts are built first
        if not self._ensure_pch_built():
            return [
                BuildResult(
                    target=BuildTarget(Path(f), Path(f).stem, self.targets_dir),
                    success=False,
                    message="PCH build failed",
                    build_time=0.0,
                )
                for f in target_files
            ]

        if not self._ensure_library_built():
            return [
                BuildResult(
                    target=BuildTarget(Path(f), Path(f).stem, self.targets_dir),
                    success=False,
                    message="Library build failed",
                    build_time=0.0,
                )
                for f in target_files
            ]

        # Build individual targets
        results: List[BuildResult] = []
        for target_file in target_files:
            result = self._build_single_target(Path(target_file))
            results.append(result)

        return results

    def _build_single_target(self, target_file: Path) -> BuildResult:
        """
        Build a single target.

        Args:
            target_file: Source file to build

        Returns:
            BuildResult: Result of the build
        """
        import time

        start_time = time.time()

        # Create build target
        target_dir = self.targets_dir / target_file.stem
        target_dir.mkdir(parents=True, exist_ok=True)

        target = BuildTarget(
            source_file=target_file, name=target_file.stem, output_dir=target_dir
        )

        print(f"[BUILD API] Building target: {target.name}")

        try:
            # Compile and link the target
            success = self._compile_and_link_target(target)

            build_time = time.time() - start_time
            message = "Build successful" if success else "Build failed"

            return BuildResult(
                target=target, success=success, message=message, build_time=build_time
            )

        except Exception as e:
            build_time = time.time() - start_time
            return BuildResult(
                target=target,
                success=False,
                message=f"Build error: {e}",
                build_time=build_time,
            )

    def _compile_and_link_target(self, target: BuildTarget) -> bool:
        """
        Compile and link a single target.

        Args:
            target: Target to build

        Returns:
            bool: True if successful, False otherwise
        """
        if not self._compiler:
            return False

        # Compile target source file
        obj_file = target.output_dir / f"{target.name}.o"
        if target.source_file.suffix == ".cpp":
            success = self._compiler.compile_cpp_file(target.source_file, obj_file)
        else:
            # For .ino files
            success = self._compiler.compile_ino_file(target.source_file, obj_file)

        if not success:
            print(f"[BUILD API] Failed to compile {target.source_file}")
            return False

        # Link with library to create executable
        exe_file = target.get_output_executable()
        success = self._link_executable([obj_file], exe_file)

        if not success:
            print(f"[BUILD API] Failed to link {exe_file}")
            return False

        print(f"[BUILD API] Successfully built {exe_file}")
        return True

    def _link_executable(self, object_files: List[Path], output_exe: Path) -> bool:
        """
        Link object files into an executable.

        Args:
            object_files: Object files to link
            output_exe: Output executable path

        Returns:
            bool: True if successful, False otherwise
        """
        if not self._library_file or not self._library_file.exists():
            print(f"[BUILD API] Library not available for linking")
            return False

        # Build linker command
        linker_cmd = self.build_flags.tools.linker
        link_flags = self.build_flags.link_flags

        cmd = (
            linker_cmd
            + link_flags
            + [str(f) for f in object_files]
            + [str(self._library_file)]
            + ["-o", str(output_exe)]
        )
        cmd_str = subprocess.list2cmdline(cmd)

        print(f"[BUILD API] Linking: {cmd_str}")

        # Execute linker using stdout-pumping wrapper
        try:
            _completed = subprocess_run(
                command=cmd,
                cwd=None,
                check=True,
                timeout=900,
                enable_stack_trace=False,
            )
            return True
        except subprocess.CalledProcessError as e:
            print(f"[BUILD API] Linker failed: {e}")
            print(f"[BUILD API] Linker stdout: {e.stdout}")
            print(f"[BUILD API] Linker stderr: {e.stderr}")
            return False

    def get_build_info(self) -> Dict[str, Any]:
        """Get information about the current build configuration"""
        return {
            "build_flags_toml": str(self.build_flags_toml),
            "build_dir": str(self.build_dir),
            "build_type": self.build_type.value,
            "use_pch": self.use_pch,
            "parallel": self.parallel,
            "pch_file": str(self._pch_file) if self._pch_file else None,
            "library_file": str(self._library_file) if self._library_file else None,
        }

    def clean_build_artifacts(self) -> None:
        """Clean all build artifacts"""
        print(f"[BUILD API] Cleaning build artifacts in {self.build_dir}")

        if self.build_dir.exists():
            shutil.rmtree(self.build_dir)
            self.build_dir.mkdir(parents=True, exist_ok=True)

        # Recreate subdirectories
        for directory in [self.artifacts_dir, self.targets_dir, self.cache_dir]:
            directory.mkdir(parents=True, exist_ok=True)

        # Reset internal state
        self._pch_file = None
        self._library_file = None
        if self._fingerprint_cache:
            # Clear the cache by resetting it
            cache_file = self.cache_dir / "fingerprint_cache.json"
            self._fingerprint_cache = FingerprintCache(cache_file)


def create_unit_test_builder(
    build_dir: str | Path = ".build/unit",
    use_pch: bool = True,
    parallel: bool = True,
    clean: bool = False,
) -> BuildAPI:
    """
    Create a BuildAPI instance for unit tests.

    Args:
        build_dir: Build directory for unit test outputs
        use_pch: Whether to use precompiled headers
        parallel: Whether to enable parallel compilation
        clean: Whether to force a clean build

    Returns:
        BuildAPI: Configured build API for unit tests
    """
    build_flags_toml = PROJECT_ROOT / "ci" / "build_unit.toml"
    return BuildAPI(
        build_flags_toml=build_flags_toml,
        build_dir=build_dir,
        build_type=BuildType.UNIT_TEST,
        use_pch=use_pch,
        parallel=parallel,
        clean=clean,
    )


def create_example_builder(
    build_dir: str | Path = ".build/examples",
    use_pch: bool = True,
    parallel: bool = True,
    clean: bool = False,
) -> BuildAPI:
    """
    Create a BuildAPI instance for examples.

    Args:
        build_dir: Build directory for example outputs
        use_pch: Whether to use precompiled headers
        parallel: Whether to enable parallel compilation
        clean: Whether to force a clean build

    Returns:
        BuildAPI: Configured build API for examples
    """
    build_flags_toml = PROJECT_ROOT / "ci" / "build_example.toml"
    return BuildAPI(
        build_flags_toml=build_flags_toml,
        build_dir=build_dir,
        build_type=BuildType.EXAMPLE,
        use_pch=use_pch,
        parallel=parallel,
        clean=clean,
    )
