#!/usr/bin/env python3

"""
Clang compiler accessibility and testing functions for FastLED.
Supports the simple build system approach outlined in FEATURE.md.
"""

import _thread
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
import tomllib
import warnings
from concurrent.futures import Future, ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Set, Tuple, Union

from ..ci.fingerprint_cache import FingerprintCache


_HERE = Path(__file__).parent


def cpu_count() -> int:
    # get the nubmer of cpus on the machine
    import multiprocessing

    return multiprocessing.cpu_count() or 1


def get_max_workers() -> int:
    # Check for NO_PARALLEL environment variable
    import os

    if os.environ.get("NO_PARALLEL"):
        print("NO_PARALLEL environment variable set - forcing sequential compilation")
        return 1
    return cpu_count() * 2


_EXECUTOR = ThreadPoolExecutor(max_workers=get_max_workers())


def optimize_python_command(cmd: list[str]) -> list[str]:
    """
    Optimize command list for subprocess execution in uv environment.

    For python commands, we need to use 'uv run python' to ensure access to
    installed packages like ziglang. Direct sys.executable bypasses uv environment.

    Args:
        cmd: Command list that may contain 'python' as first element

    Returns:
        list[str]: Optimized command with 'python' prefixed by 'uv run'
    """
    if cmd and (cmd[0] == "python" or cmd[0] == "python3"):
        # Use uv run python to ensure access to uv-managed packages
        optimized_cmd = ["uv", "run", "python"] + cmd[1:]
        return optimized_cmd
    return cmd


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
    compiler: str = "python -m ziglang c++"
    defines: list[str] | None = None
    std_version: str = "c++17"
    compiler_args: list[str] = field(default_factory=list[str])

    # PCH (Precompiled Headers) settings
    use_pch: bool = False
    pch_header_content: str | None = None
    pch_output_path: str | None = None

    # Archive generation settings
    archiver: str = "ar"  # Default archiver tool
    archiver_args: list[str] = field(default_factory=list[str])

    # Compilation operation settings (formerly CompileOptions, unity options removed)
    additional_flags: list[str] = field(
        default_factory=list[str]
    )  # Extra compiler flags
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
class UnityChunkResult:
    """Result for a single unity chunk compilation."""

    ok: bool
    unity_cpp: Path
    object_path: Path
    stderr: str
    return_code: int


@dataclass
class UnityChunksResult:
    """Aggregate result for a chunked unity build."""

    success: bool
    chunks: list[UnityChunkResult]


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
    output_executable: Union[str, Path]  # Output executable path

    # Input files
    object_files: List[Union[str, Path]] = field(
        default_factory=lambda: []
    )  # .o files to link
    static_libraries: List[Union[str, Path]] = field(
        default_factory=lambda: []
    )  # .a files to link

    # Linker configuration
    linker: Optional[str] = None  # Custom linker path (auto-detected if None)
    linker_args: List[str] = field(
        default_factory=lambda: []
    )  # All linker command line arguments

    # Platform-specific defaults can be added via helper functions
    temp_dir: Optional[Union[str, Path]] = (
        None  # Custom temporary directory for intermediate files
    )


@dataclass
class ArchiveOptions:
    """Archive creation options loaded from build configuration TOML"""

    flags: str  # Archive flags (e.g., "rcsD" for deterministic builds)
    linux: Optional["ArchivePlatformOptions"] = None  # Linux-specific flags
    windows: Optional["ArchivePlatformOptions"] = None  # Windows-specific flags
    darwin: Optional["ArchivePlatformOptions"] = None  # macOS-specific flags


@dataclass
class ArchivePlatformOptions:
    """Platform-specific archive options"""

    flags: str  # Platform-specific archive flags


@dataclass
class BuildTools:
    """Build tool paths and configurations"""

    cpp_compiler: list[str]  # C++ compiler (maps to CompilerOptions.compiler)
    archiver: list[str]  # Archive tool (maps to CompilerOptions.archiver)
    linker: list[str]  # Linker tool (maps to LinkOptions.linker)
    c_compiler: list[str]  # C compiler (maps to CompilerOptions.c_compiler)
    objcopy: list[str]  # Object copy utility (for firmware/embedded)
    nm: list[str]  # Symbol table utility (for analysis)
    strip: list[str]  # Strip utility (for release builds)
    ranlib: list[str]  # Archive indexer (for static libraries)


@dataclass
class BuildFlags:
    """
    Build flags loaded from TOML configuration with parsing and serialization capabilities.

    This class represents the build flags structure and can parse from TOML files
    and serialize back to TOML format.
    """

    defines: List[str]
    compiler_flags: List[str]
    include_flags: List[str]
    link_flags: List[str]
    strict_mode_flags: List[str]
    tools: BuildTools
    archive: ArchiveOptions

    @classmethod
    def parse(
        cls, toml_path: Path, quick_build: bool = False, strict_mode: bool = False
    ) -> "BuildFlags":
        """
        Parse build flags from TOML configuration file.

        Args:
            toml_path: Path to the build configuration TOML file (build_unit.toml or build_example.toml)
            quick_build: Use quick build mode flags (default: debug mode)
            strict_mode: Enable strict mode warning flags

        Returns:
            BuildFlags: Parsed build flags from TOML file
        """
        try:
            with open(toml_path, "rb") as f:
                config = tomllib.load(f)
        except FileNotFoundError:
            raise FileNotFoundError(
                f"Required build_flags.toml not found at {toml_path}"
            )
        except KeyboardInterrupt:
            _thread.interrupt_main()
            raise
        except Exception as e:
            raise RuntimeError(f"Failed to parse build_flags.toml: {e}")

        # Extract tools configuration with validation
        if "tools" not in config:
            raise RuntimeError(
                f"FATAL ERROR: [tools] section missing from build_flags.toml at {toml_path}\n"
                f"The [tools] section is required and must define all compiler tools.\n"
                f"Example:\n"
                f"[tools]\n"
                f'compiler_command = ["uv", "run", "python", "-m", "ziglang", "c++"]\n'
                f'c_compiler = "clang"'
            )

        tools_config = config["tools"]

        # # Extract linker_command and archiver_command with validation
        # linker_command = tools_config.get("linker_command", [])
        # archiver_command = tools_config.get("archiver_command", [])

        # # Validate command fields if present
        # if linker_command and not isinstance(linker_command, list):
        #     raise RuntimeError(
        #         f"FATAL ERROR: tools.linker_command must be a list in build_flags.toml at {toml_path}\n"
        #         f"Got: {linker_command} (type: {type(linker_command).__name__})\n"
        #         f'Expected: ["python", "-m", "ziglang", "c++"]'
        #     )

        # if archiver_command and not isinstance(archiver_command, list):
        #     raise RuntimeError(
        #         f"FATAL ERROR: tools.archiver_command must be a list in build_flags.toml at {toml_path}\n"
        #         f"Got: {archiver_command} (type: {type(archiver_command).__name__})\n"
        #         f'Expected: ["python", "-m", "ziglang", "ar"]'
        #     )

        cpp_compiler = tools_config.get("cpp_compiler", None)
        linker = tools_config.get("linker", None)
        c_compiler = tools_config.get("c_compiler", None)
        objcopy = tools_config.get("objcopy", None)
        nm = tools_config.get("nm", None)
        strip = tools_config.get("strip", None)
        ranlib = tools_config.get("ranlib", None)
        archiver = tools_config.get("archiver", None)

        assert cpp_compiler is not None, (
            f"FATAL ERROR: cpp_compiler is not set in build_flags.toml at {toml_path}"
        )
        assert linker is not None, (
            f"FATAL ERROR: linker is not set in build_flags.toml at {toml_path}"
        )
        assert c_compiler is not None, (
            f"FATAL ERROR: c_compiler is not set in build_flags.toml at {toml_path}"
        )
        assert objcopy is not None, (
            f"FATAL ERROR: objcopy is not set in build_flags.toml at {toml_path}"
        )
        assert nm is not None, (
            f"FATAL ERROR: nm is not set in build_flags.toml at {toml_path}"
        )
        assert strip is not None, (
            f"FATAL ERROR: strip is not set in build_flags.toml at {toml_path}"
        )
        assert ranlib is not None, (
            f"FATAL ERROR: ranlib is not set in build_flags.toml at {toml_path}"
        )
        assert archiver is not None, (
            f"FATAL ERROR: archiver is not set in build_flags.toml at {toml_path}"
        )

        # tools = BuildTools(
        #     compiler=cpp_compiler,  # Use last element as fallback
        #     archiver=archiver,  # Use fallback for deprecated field
        #     linker=linker,
        #     c_compiler=c_compiler,
        #     objcopy=objcopy,
        #     nm=nm,
        #     strip=strip,
        #     ranlib=ranlib,
        #     compiler_command=compiler_command,  # Add the full command
        #     linker_command=linker,  # Add the linker command
        #     archiver_command=archiver,  # Add the archiver command
        # )

        tools = BuildTools(
            cpp_compiler=cpp_compiler,
            archiver=archiver,
            linker=linker,
            c_compiler=c_compiler,
            objcopy=objcopy,
            nm=nm,
            strip=strip,
            ranlib=ranlib,
        )

        # Extract archive configuration - NO DEFAULTS, must be in build_flags.toml
        if "archive" not in config:
            raise RuntimeError(
                f"CRITICAL: [archive] section missing from build_flags.toml at {toml_path}\n"
                f"The [archive] section is required for deterministic builds.\n"
                f"Example:\n"
                f"[archive]\n"
                f'flags = "rcsD"  # r=insert, c=create, s=symbol table, D=deterministic'
            )

        archive_config = config["archive"]
        if "flags" not in archive_config:
            raise RuntimeError(
                f"CRITICAL: archive.flags missing from build configuration TOML at {toml_path}\n"
                f"Archive flags are required for proper static library creation.\n"
                f"Example:\n"
                f"[archive]\n"
                f'flags = "rcs"  # r=insert, c=create, s=symbol table'
            )

        # Parse platform-specific archive configurations
        linux_options = None
        windows_options = None
        darwin_options = None

        if "linux" in archive_config and "flags" in archive_config["linux"]:
            linux_options = ArchivePlatformOptions(
                flags=archive_config["linux"]["flags"]
            )

        if "windows" in archive_config and "flags" in archive_config["windows"]:
            windows_options = ArchivePlatformOptions(
                flags=archive_config["windows"]["flags"]
            )

        if "darwin" in archive_config and "flags" in archive_config["darwin"]:
            darwin_options = ArchivePlatformOptions(
                flags=archive_config["darwin"]["flags"]
            )

        archive_options = ArchiveOptions(
            flags=archive_config["flags"],
            linux=linux_options,
            windows=windows_options,
            darwin=darwin_options,
        )

        # Extract base flags
        base_flags = cls(
            defines=config.get("all", {}).get("defines", []),
            compiler_flags=config.get("all", {}).get("compiler_flags", []),
            include_flags=config.get("all", {}).get("include_flags", []),
            link_flags=config.get("linking", {}).get("base", {}).get("flags", []),
            strict_mode_flags=config.get("strict_mode", {}).get("flags", []),
            tools=tools,
            archive=archive_options,
        )

        # Add platform-specific flags if available
        current_platform = platform.system().lower()

        # Check for new-style platform configuration (windows, linux, etc.)
        if current_platform in config:
            platform_config = config[current_platform]

            # Use CPP flags for C++ compilation (most common case)
            if "cpp_flags" in platform_config:
                base_flags.compiler_flags = platform_config["cpp_flags"]

            # Use platform-specific link flags
            if "link_flags" in platform_config:
                if current_platform == "windows":
                    # On Windows, replace base linking flags entirely (avoid pthread conflicts)
                    base_flags.link_flags = platform_config["link_flags"]
                else:
                    # On other platforms, extend base linking flags
                    base_flags.link_flags.extend(platform_config["link_flags"])

        # Fallback to old-style platform configuration for backward compatibility
        else:
            compiler_flags_config = config.get("compiler_flags", {})
            if current_platform in compiler_flags_config:
                platform_flags = compiler_flags_config[current_platform].get(
                    "flags", []
                )
                if platform_flags:
                    base_flags.compiler_flags = platform_flags

            linking_config = config.get("linking", {})
            if current_platform in linking_config:
                platform_link_flags = linking_config[current_platform].get("flags", [])
                if platform_link_flags:
                    if current_platform == "windows":
                        base_flags.link_flags = platform_link_flags
                    else:
                        base_flags.link_flags.extend(platform_link_flags)

        # Add build mode specific flags
        build_mode = "quick" if quick_build else "debug"
        build_modes = config.get("build_modes", {})
        if build_mode in build_modes:
            mode_config = build_modes[build_mode]
            if "flags" in mode_config:
                base_flags.compiler_flags.extend(mode_config["flags"])
            if "link_flags" in mode_config:
                base_flags.link_flags.extend(mode_config["link_flags"])

        return base_flags

    def serialize(self) -> str:
        """
        Serialize build flags to TOML format string.

        Returns:
            str: TOML representation of the build flags
        """
        toml_content: List[str] = []

        # Add header
        header_lines: List[str] = [
            "# ================================================================================================",
            "# FastLED Build Flags Configuration",
            "# ================================================================================================",
            "# Generated build flags configuration",
            "",
        ]
        toml_content.extend(header_lines)

        # [all] section
        all_section_lines: List[str] = [
            "[all]",
            "# Universal compilation flags",
            "",
        ]
        toml_content.extend(all_section_lines)

        # Add defines
        if self.defines:
            toml_content.append("defines = [")
            for define in self.defines:
                toml_content.append(f'    "{define}",')
            toml_content.append("]")
            toml_content.append("")

        # Add compiler flags
        if self.compiler_flags:
            toml_content.append("compiler_flags = [")
            for flag in self.compiler_flags:
                toml_content.append(f'    "{flag}",')
            toml_content.append("]")
            toml_content.append("")

        # Add include flags
        if self.include_flags:
            toml_content.append("include_flags = [")
            for flag in self.include_flags:
                toml_content.append(f'    "{flag}",')
            toml_content.append("]")
            toml_content.append("")

        # [linking.base] section
        if self.link_flags:
            linking_section_lines: List[str] = [
                "[linking.base]",
                "# Base linking flags",
                "",
                "flags = [",
            ]
            toml_content.extend(linking_section_lines)
            for flag in self.link_flags:
                toml_content.append(f'    "{flag}",')
            toml_content.append("]")
            toml_content.append("")

        # [strict_mode] section
        if self.strict_mode_flags:
            strict_section_lines: List[str] = [
                "[strict_mode]",
                "# Strict mode compilation flags",
                "",
                "flags = [",
            ]
            toml_content.extend(strict_section_lines)
            for flag in self.strict_mode_flags:
                toml_content.append(f'    "{flag}",')
            toml_content.append("]")
            toml_content.append("")

        # [tools] section
        tools_section_lines: List[str] = [
            "[tools]",
            "# Build tool configuration",
            "",
        ]
        toml_content.extend(tools_section_lines)

        # Add tools - using new field names

        # Add cpp_compiler if it exists
        if self.tools.cpp_compiler:
            toml_content.append(f"cpp_compiler = {self.tools.cpp_compiler!r}")

        # Add linker if it exists
        if self.tools.linker:
            toml_content.append(f"linker = {self.tools.linker!r}")

        # Add archiver if it exists
        if self.tools.archiver:
            toml_content.append(f"archiver = {self.tools.archiver!r}")

        # Add individual tool commands as strings (for backward compatibility)
        if self.tools.c_compiler:
            toml_content.append(f"c_compiler = {self.tools.c_compiler!r}")
        if self.tools.objcopy:
            toml_content.append(f"objcopy = {self.tools.objcopy!r}")
        if self.tools.nm:
            toml_content.append(f"nm = {self.tools.nm!r}")
        if self.tools.strip:
            toml_content.append(f"strip = {self.tools.strip!r}")
        if self.tools.ranlib:
            toml_content.append(f"ranlib = {self.tools.ranlib!r}")
        toml_content.append("")

        # [archive] section
        archive_section_lines = [
            "[archive]",
            "# Archive creation flags for deterministic builds",
            "",
            f'flags = "{self.archive.flags}"',
            "",
        ]
        toml_content.extend(archive_section_lines)

        return "\n".join(toml_content)

    def to_toml_file(self, toml_path: Path) -> None:
        """
        Write build flags to TOML file.

        Args:
            toml_path: Path where to write the TOML file
        """
        toml_content = self.serialize()

        # Ensure directory exists
        toml_path.parent.mkdir(parents=True, exist_ok=True)

        # Write to file
        with open(toml_path, "w", encoding="utf-8") as f:
            f.write(toml_content)

    @classmethod
    def from_toml_file(
        cls, toml_path: Path, quick_build: bool = False, strict_mode: bool = False
    ) -> "BuildFlags":
        """
        Convenience method to create BuildFlags from TOML file.

        This is an alias for parse() method for better readability.

        Args:
            toml_path: Path to the build configuration TOML file (build_unit.toml or build_example.toml)
            quick_build: Use quick build mode flags (default: debug mode)
            strict_mode: Enable strict mode warning flags

        Returns:
            BuildFlags: Parsed build flags from TOML file
        """
        return cls.parse(toml_path, quick_build, strict_mode)


class Compiler:
    """
    Compiler wrapper for FastLED .ino file compilation.
    """

    def __init__(self, settings: CompilerOptions, build_flags: BuildFlags):
        self.settings = settings
        self.build_flags = build_flags
        self._pch_file_path: Path | None = None
        self._pch_header_path: Path | None = None
        self._pch_ready: bool = False

        # Initialize fingerprint cache for PCH optimization
        cache_dir = Path(".build") / "cache"
        cache_dir.mkdir(parents=True, exist_ok=True)
        # For PCH validity, toolchains like Clang require strict modtime checks.
        # Use modtime_only=True to avoid content-hash-based reuse that would
        # conflict with Clang's mtime validation.
        self._pch_cache = FingerprintCache(
            cache_dir / "pch_fingerprint_cache.json", modtime_only=True
        )

    def get_compiler_args(self) -> list[str]:
        """
        Get a copy of the complete compiler arguments that would be used for compilation.

        Uses compiler_args from build configuration TOML without any hardcoded modifications.

        Returns:
            list[str]: Copy of compiler arguments from TOML configuration
        """
        # Use compiler args exactly as specified in build configuration TOML
        cmd = self.settings.compiler_args.copy()

        # Add standard compilation requirements (don't override std version - use what's in TOML)
        cmd.extend(
            [
                "-x",
                "c++",  # Force C++ compilation of .ino files
                f"-I{self.settings.include_path}",  # FastLED include path
            ]
        )

        # Add defines if specified
        if self.settings.defines:
            for define in self.settings.defines:
                cmd.append(f"-D{define}")

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

    def _get_pch_dependencies(self) -> List[Path]:
        """
        Get list of files that PCH depends on for change detection.

        Returns:
            List of Path objects representing files that affect PCH validity
        """
        dependencies: List[Path] = []

        # FastLED.h is the main dependency
        fastled_h = Path(self.settings.include_path) / "FastLED.h"
        if fastled_h.exists():
            dependencies.append(fastled_h)

        # Core FastLED headers that are commonly included
        core_headers = [
            "platforms.h",
            "led_sysdefs.h",
            "hsv2rgb.h",
            "colorutils.h",
            "colorpalettes.h",
            "fastled_config.h",
        ]

        for header in core_headers:
            header_path = Path(self.settings.include_path) / header
            if header_path.exists():
                dependencies.append(header_path)

        # CRITICAL: Add ALL source files in src/** directory
        # PCH contains namespace configuration and other build settings that
        # can be affected by any source file change
        src_dir = Path(self.settings.include_path)
        if src_dir.exists():
            # Add all header files recursively in src/
            for pattern in ["**/*.h", "**/*.hpp"]:
                for header in src_dir.glob(pattern):
                    if header.is_file():
                        dependencies.append(header)

        # Platform-specific headers
        platform_dir = Path(self.settings.include_path) / "platforms"
        if platform_dir.exists():
            # Add common platform headers
            for pattern in ["*.h", "*/*.h"]:
                for header in platform_dir.glob(pattern):
                    if header.is_file():
                        dependencies.append(header)

        return dependencies

    def _should_rebuild_pch(self) -> bool:
        """
        Check if PCH needs to be rebuilt based on dependency changes.

        Returns:
            True if PCH should be rebuilt, False if cache is valid
        """
        if not self.settings.use_pch:
            return False

        # Set PCH file path if using configured path
        if self.settings.pch_output_path:
            self._pch_file_path = Path(self.settings.pch_output_path)

        # If PCH file doesn't exist, need to build
        if not self._pch_file_path or not self._pch_file_path.exists():
            print("[PCH CACHE] PCH file doesn't exist, rebuilding required")
            return True

        # Get PCH file modification time as baseline
        pch_modtime = os.path.getmtime(self._pch_file_path)

        # Check if any PCH dependencies have changed
        dependencies = self._get_pch_dependencies()
        print(f"[PCH CACHE] Checking {len(dependencies)} PCH dependencies...")

        for dep_file in dependencies:
            try:
                if self._pch_cache.has_changed(dep_file, pch_modtime):
                    print(
                        f"[PCH CACHE] Dependency changed: {dep_file.name} - PCH rebuild required"
                    )
                    return True
            except FileNotFoundError:
                print(
                    f"[PCH CACHE] Dependency not found: {dep_file.name} - PCH rebuild required"
                )
                return True

        print("[PCH CACHE] All dependencies unchanged - using cached PCH")
        return False

    def create_pch_file(self) -> bool:
        """
        Create a precompiled header file for faster compilation.

        IMPORTANT: PCH generation uses direct clang++ compilation
        for maximum compatibility.

        Returns:
            bool: True if PCH creation was successful, False otherwise
        """
        if not self.settings.use_pch:
            return False

        # Check if existing PCH is still valid using fingerprint cache
        if not self._should_rebuild_pch():
            # PCH is still valid, reuse existing one
            if self.settings.pch_output_path:
                self._pch_file_path = Path(self.settings.pch_output_path)
                self._pch_ready = True
                print("[PCH CACHE] Reusing valid cached PCH file")
                return True

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

            # Build PCH compilation command using TOML configuration
            # Start with the compiler command, then add args from TOML
            compiler_parts = self.settings.compiler.split()
            cmd: list[str] = optimize_python_command(
                compiler_parts + self.settings.compiler_args.copy()
            )

            # Add PCH-specific flags (don't override std version - use what's in TOML)
            cmd.extend(
                [
                    "-x",
                    "c++-header",
                    f"-I{self.settings.include_path}",
                ]
            )

            # Add defines if specified
            if self.settings.defines:
                for define in self.settings.defines:
                    cmd.append(f"-D{define}")

            # Skip existing PCH flags to avoid conflicts, but keep everything else
            skip_next = False
            final_cmd: List[str] = []
            for arg in cmd:
                if skip_next:
                    skip_next = False
                    continue
                elif arg.startswith("-include-pch"):
                    skip_next = True  # Skip the PCH file path argument too
                    continue
                else:
                    final_cmd.append(arg)

            # Add the PCH file paths
            final_cmd.extend([str(pch_header_path), "-o", str(pch_output_path)])
            cmd = final_cmd

            # DEBUG: Print the complete PCH compilation command
            print("[PCH] Compilation Command:")
            for i, arg in enumerate(cmd):
                print(f"  {i}: {arg}")
            print()

            # Compile PCH with direct compiler (no sccache) - use single stream to prevent buffer overflow
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,  # Merge stderr into stdout
                text=True,
                bufsize=1,  # Line buffered
                encoding="utf-8",  # Explicitly use UTF-8
                errors="replace",  # Replace invalid chars instead of failing
            )

            stdout_lines: list[str] = []
            stderr_lines: list[str] = []  # Keep separate for API compatibility

            # Read only stdout (which contains both stdout and stderr)
            while True:
                output_line = process.stdout.readline() if process.stdout else ""

                # Process output lines (both stdout and stderr combined)
                if output_line:
                    line_stripped = output_line.rstrip()
                    # Add to both lists for API compatibility
                    stdout_lines.append(line_stripped)
                    stderr_lines.append(line_stripped)

                # Check if process has finished
                if process.poll() is not None:
                    # Read any remaining output
                    remaining_output = process.stdout.read() if process.stdout else ""

                    if remaining_output:
                        for line in remaining_output.splitlines():
                            line_stripped = line.rstrip()
                            stdout_lines.append(line_stripped)
                            stderr_lines.append(line_stripped)
                    break

            if process.returncode == 0:
                self._pch_file_path = pch_output_path
                self._pch_header_path = (
                    pch_header_path  # Keep track of header file for cleanup
                )
                self._pch_ready = True

                # Update fingerprint cache for all PCH dependencies
                print(
                    "[PCH CACHE] PCH compilation successful, updating dependency cache..."
                )
                pch_modtime = os.path.getmtime(pch_output_path)
                dependencies = self._get_pch_dependencies()

                for dep_file in dependencies:
                    try:
                        # Force cache update for each dependency
                        self._pch_cache.has_changed(dep_file, pch_modtime - 1)
                        print(f"[PCH CACHE] Cached dependency: {dep_file.name}")
                    except FileNotFoundError:
                        print(
                            f"[PCH CACHE] Warning: Could not cache dependency: {dep_file.name}"
                        )

                return True
            else:
                # PCH compilation failed, clean up and continue without PCH
                # Print debug info to help diagnose PCH issues
                stderr_output = "\n".join(stderr_lines)
                print(
                    f"[DEBUG] PCH compilation failed with command: {subprocess.list2cmdline(cmd)}"
                )
                print(f"[DEBUG] PCH error output: {stderr_output}")
                os.unlink(pch_header_path)
                return False

        except KeyboardInterrupt:
            _thread.interrupt_main()
            raise
        except Exception as e:
            # PCH creation failed, continue without PCH
            print(f"[DEBUG] PCH creation exception: {e}")
            return False

    def cleanup_pch(self):
        """Clean up PCH files."""
        if self._pch_file_path and self._pch_file_path.exists():
            try:
                os.unlink(self._pch_file_path)
            except KeyboardInterrupt:
                _thread.interrupt_main()
                raise
            except Exception:
                pass
        if self._pch_header_path and self._pch_header_path.exists():
            try:
                os.unlink(self._pch_header_path)
            except KeyboardInterrupt:
                _thread.interrupt_main()
                raise
            except Exception:
                pass
        self._pch_file_path = None
        self._pch_header_path = None
        self._pch_ready = False

    def check_clang_version(self) -> VersionCheckResult:
        """
        Check that ziglang c++ is accessible and return version information.
        Handles ziglang c++ compiler properly.

        Returns:
            VersionCheckResult: Result containing success status, version string, and error message
        """
        # Test python access first
        try:
            # Use optimized python -m ziglang c++ (no uv run overhead)
            version_cmd = ["python", "-m", "ziglang", "c++", "--version"]

            # Optimize command to use sys.executable instead of shell 'python' resolution
            python_exe = optimize_python_command(version_cmd)

            # Use Popen with stderr redirected to stdout to prevent buffer overflow
            process = subprocess.Popen(
                python_exe,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,  # Merge stderr into stdout
                text=True,
                bufsize=1,
                encoding="utf-8",
                errors="replace",
            )

            stdout_lines: list[str] = []
            stderr_lines: list[str] = []  # Keep separate for API compatibility

            # Read only stdout (which contains both stdout and stderr)
            while True:
                output_line = process.stdout.readline() if process.stdout else ""

                if output_line:
                    line_stripped = output_line.rstrip()
                    # Add to both lists for API compatibility
                    stdout_lines.append(line_stripped)
                    stderr_lines.append(line_stripped)

                if process.poll() is not None:
                    # Read remaining output
                    remaining_output = process.stdout.read() if process.stdout else ""

                    if remaining_output:
                        for line in remaining_output.splitlines():
                            line_stripped = line.rstrip()
                            stdout_lines.append(line_stripped)
                            stderr_lines.append(line_stripped)
                    break

            stdout_result = "\n".join(stdout_lines)
            stderr_result = "\n".join(stderr_lines)
            if process.returncode == 0 and "clang version" in stdout_result:
                version = (
                    stdout_result.split()[2]
                    if len(stdout_result.split()) > 2
                    else "unknown"
                )
                return VersionCheckResult(
                    success=True, version=f"ziglang:{version}", error=""
                )
            else:
                return VersionCheckResult(
                    success=False,
                    version="",
                    error=f"ziglang c++ version check failed: {stderr_result}",
                )
        except KeyboardInterrupt:
            _thread.interrupt_main()
            raise
        except Exception as e:
            return VersionCheckResult(
                success=False, version="", error=f"ziglang c++ not accessible: {str(e)}"
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
        # Handle cache-wrapped compilers (sccache/ccache) or ziglang c++
        if len(self.settings.compiler_args) > 0 and self.settings.compiler_args[
            0:4
        ] == ["python", "-m", "ziglang", "c++"]:
            # This is optimized ziglang c++ in compiler_args, use it directly
            cmd = self.settings.compiler_args[
                0:4
            ]  # Use optimized ziglang c++ command from compiler_args
            remaining_cache_args = self.settings.compiler_args[
                4:
            ]  # Skip the ziglang c++ part
        elif len(self.settings.compiler_args) > 0 and self.settings.compiler_args[
            0:6
        ] == ["uv", "run", "python", "-m", "ziglang", "c++"]:
            # This is legacy ziglang c++ in compiler_args, use it directly
            cmd = self.settings.compiler_args[
                0:6
            ]  # Use ziglang c++ command from compiler_args
            remaining_cache_args = self.settings.compiler_args[
                6:
            ]  # Skip the ziglang c++ part
        elif (
            len(self.settings.compiler_args) > 0
            and self.settings.compiler_args[0] == "clang++"
        ):
            # This is a clang++, replace with optimized ziglang c++
            cmd = ["python", "-m", "ziglang", "c++"]
            remaining_cache_args = self.settings.compiler_args[1:]
        else:
            # Use ziglang c++ directly
            cmd = ["python", "-m", "ziglang", "c++"]
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
            # Optimize command to use sys.executable instead of shell 'python' resolution
            python_exe = optimize_python_command(cmd)

            # Use Popen with stderr redirected to stdout for single stream pumping
            process = subprocess.Popen(
                python_exe,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,  # Merge stderr into stdout
                text=True,
                bufsize=1,  # Line buffered
                encoding="utf-8",  # Explicitly use UTF-8
                errors="replace",  # Replace invalid chars instead of failing
            )

            stdout_lines: list[str] = []
            stderr_lines: list[str] = []  # Keep separate for API compatibility

            # Read only stdout (which contains both stdout and stderr)
            while True:
                output_line = process.stdout.readline() if process.stdout else ""

                # Process output lines (both stdout and stderr combined)
                if output_line:
                    line_stripped = output_line.rstrip()
                    # Add to both lists for API compatibility
                    stdout_lines.append(line_stripped)
                    stderr_lines.append(line_stripped)

                # Check if process has finished
                if process.poll() is not None:
                    # Read any remaining output
                    remaining_output = process.stdout.read() if process.stdout else ""

                    if remaining_output:
                        for line in remaining_output.splitlines():
                            line_stripped = line.rstrip()
                            stdout_lines.append(line_stripped)
                            stderr_lines.append(line_stripped)
                    break

            # Clean up temp file on failure if we created it
            if cleanup_temp and process.returncode != 0 and os.path.exists(output_path):
                try:
                    os.unlink(output_path)
                except:
                    pass

            return Result(
                ok=(process.returncode == 0),
                stdout="\n".join(stdout_lines),
                stderr="\n".join(stderr_lines),
                return_code=process.returncode,
            )

        except KeyboardInterrupt:
            _thread.interrupt_main()
            raise
        except Exception as e:
            if cleanup_temp and os.path.exists(output_path):
                try:
                    os.unlink(output_path)
                except Exception:
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
        # Handle cache-wrapped compilers (sccache/ccache) or ziglang c++
        if len(self.settings.compiler_args) > 0 and self.settings.compiler_args[
            0:4
        ] == ["python", "-m", "ziglang", "c++"]:
            # This is optimized ziglang c++ in compiler_args, use it directly
            cmd = self.settings.compiler_args[
                0:4
            ]  # Use optimized ziglang c++ command from compiler_args
            remaining_cache_args = self.settings.compiler_args[
                4:
            ]  # Skip the ziglang c++ part
        elif len(self.settings.compiler_args) > 0 and self.settings.compiler_args[
            0:6
        ] == ["uv", "run", "python", "-m", "ziglang", "c++"]:
            # This is legacy ziglang c++ in compiler_args, use it directly
            cmd = self.settings.compiler_args[
                0:6
            ]  # Use ziglang c++ command from compiler_args
            remaining_cache_args = self.settings.compiler_args[
                6:
            ]  # Skip the ziglang c++ part
        elif (
            len(self.settings.compiler_args) > 0
            and self.settings.compiler_args[0] == "clang++"
        ):
            # This is a clang++, replace with optimized ziglang c++
            cmd = ["python", "-m", "ziglang", "c++"]
            remaining_cache_args = self.settings.compiler_args[1:]
        else:
            # Use ziglang c++ directly
            cmd = ["python", "-m", "ziglang", "c++"]
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
            # Optimize command to use sys.executable instead of shell 'python' resolution
            python_exe = optimize_python_command(cmd)

            # Use Popen with stderr redirected to stdout for single stream pumping
            process = subprocess.Popen(
                python_exe,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,  # Merge stderr into stdout
                text=True,
                bufsize=1,  # Line buffered
                encoding="utf-8",  # Explicitly use UTF-8
                errors="replace",  # Replace invalid chars instead of failing
            )

            stdout_lines: list[str] = []
            stderr_lines: list[str] = []  # Keep separate for API compatibility

            # Read only stdout (which contains both stdout and stderr)
            while True:
                output_line = process.stdout.readline() if process.stdout else ""

                # Process output lines (both stdout and stderr combined)
                if output_line:
                    line_stripped = output_line.rstrip()
                    print(output_line, end="", flush=True)  # Stream to console
                    # Add to both lists for API compatibility
                    stdout_lines.append(line_stripped)
                    stderr_lines.append(line_stripped)

                # Check if process has finished
                if process.poll() is not None:
                    # Read any remaining output
                    remaining_output = process.stdout.read() if process.stdout else ""

                    if remaining_output:
                        print(remaining_output, end="", flush=True)
                        for line in remaining_output.splitlines():
                            line_stripped = line.rstrip()
                            stdout_lines.append(line_stripped)
                            stderr_lines.append(line_stripped)
                    break

            # Clean up temp file on failure if we created it
            if cleanup_temp and process.returncode != 0 and os.path.exists(output_path):
                try:
                    os.unlink(output_path)
                except:
                    pass

            return Result(
                ok=(process.returncode == 0),
                stdout="".join(stdout_lines),
                stderr="".join(stderr_lines),
                return_code=process.returncode,
            )

        except KeyboardInterrupt:
            _thread.interrupt_main()
            raise
        except Exception as e:
            if cleanup_temp and os.path.exists(output_path):
                try:
                    os.unlink(output_path)
                except Exception:
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
        Comprehensive test that ziglang c++ is accessible and working properly.
        This is the foundational test for the simple build system approach.

        Returns:
            bool: True if all tests pass, False otherwise
        """
        print("Testing ziglang c++ accessibility...")

        # Test 1: Check ziglang c++ version
        version_result = self.check_clang_version()
        if not version_result.success:
            print(f"X ziglang c++ version check failed: {version_result.error}")
            return False
        print(f"[OK] ziglang c++ version: {version_result.version}")

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

        print("[INFO] All ziglang c++ accessibility tests passed!")
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

            for line in lines:
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
                    return True

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

        except KeyboardInterrupt:
            _thread.interrupt_main()
            raise
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

    def compile_unity_chunks(
        self,
        compile_options: CompilerOptions,
        list_of_cpp_files: list[str | Path],
        chunks: int,
        unity_dir: str | Path | None = None,
        no_parallel: bool = False,
    ) -> Future[UnityChunksResult]:
        """Compile a chunked UNITY build producing N unity{n}.o objects.

        Args:
            compile_options: Compilation options (PCH is forced off for UNITY)
            list_of_cpp_files: Input .cpp files (will be sorted deterministically)
            chunks: Number of chunks (>=1). Capped to number of files.
            unity_dir: Directory to write unity{n}.cpp/.o (temp dir if None)
            no_parallel: If True, compile sequentially

        Returns:
            Future[UnityChunksResult]
        """
        return _EXECUTOR.submit(
            self._compile_unity_chunks_sync,
            compile_options,
            list_of_cpp_files,
            chunks,
            unity_dir,
            no_parallel,
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
        except KeyboardInterrupt:
            _thread.interrupt_main()
            raise
        except Exception as e:
            if cleanup_unity and unity_path.exists():
                try:
                    unity_path.unlink()
                except Exception:
                    warnings.warn(f"Failed to unlink unity.cpp: {e}")
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

        except KeyboardInterrupt:
            _thread.interrupt_main()
            raise
        except Exception as e:
            # Clean up temporary files on exception
            if cleanup_unity and unity_path.exists():
                try:
                    unity_path.unlink()
                except Exception:
                    pass
            if cleanup_obj and output_path and os.path.exists(output_path):
                try:
                    os.unlink(output_path)
                except Exception:
                    pass

            return Result(
                ok=False,
                stdout="",
                stderr=f"Unity compilation failed: {e}",
                return_code=-1,
            )

    def _compile_unity_chunks_sync(
        self,
        compile_options: CompilerOptions,
        list_of_cpp_files: Sequence[str | Path],
        chunks: int,
        unity_dir: str | Path | None = None,
        no_parallel: bool = False,
    ) -> UnityChunksResult:
        if not list_of_cpp_files:
            return UnityChunksResult(success=False, chunks=[])

        # Normalize and validate inputs
        cpp_paths: list[Path] = []
        for cpp_file in list_of_cpp_files:
            p = Path(cpp_file).resolve()
            if not p.exists() or p.suffix != ".cpp":
                return UnityChunksResult(success=False, chunks=[])
            cpp_paths.append(p)

        # Deterministic ordering
        project_root = Path.cwd()

        def sort_key(p: Path) -> str:
            try:
                rel = p.relative_to(project_root)
            except Exception:
                rel = p
            return rel.as_posix()

        cpp_paths = sorted(cpp_paths, key=sort_key)

        total = len(cpp_paths)
        chunks = max(1, min(chunks, total))
        base = total // chunks
        rem = total % chunks

        # Resolve unity dir
        cleanup_dir = False
        if unity_dir is None:
            temp_dir = compile_options.temp_dir or tempfile.gettempdir()
            unity_root = Path(temp_dir) / f"fastled_unity_{os.getpid()}"
            unity_root.mkdir(parents=True, exist_ok=True)
            cleanup_dir = True
        else:
            unity_root = Path(unity_dir)
            unity_root.mkdir(parents=True, exist_ok=True)

        # Build per-chunk unity files
        chunk_results: list[UnityChunkResult] = []
        compile_futures: list[tuple[int, Future[Result], Path, Path]] = []

        start = 0
        for i in range(chunks):
            size = base + (1 if i < rem else 0)
            end = start + size
            group = cpp_paths[start:end]
            start = end

            unity_cpp = unity_root / f"unity{i + 1}.cpp"

            # Generate content (idempotent write if content unchanged)
            unity_content = self._generate_unity_content(group)
            try:
                should_write = True
                if unity_cpp.exists():
                    try:
                        if unity_cpp.read_text(encoding="utf-8") == unity_content:
                            should_write = False
                    except KeyboardInterrupt:
                        _thread.interrupt_main()
                        raise
                    except Exception:
                        # If we cannot read for comparison, fall back to writing
                        should_write = True
                if should_write:
                    unity_cpp.write_text(unity_content, encoding="utf-8")
            except KeyboardInterrupt:
                _thread.interrupt_main()
                raise
            except Exception as e:
                if cleanup_dir:
                    try:
                        unity_cpp.unlink(missing_ok=True)
                    except KeyboardInterrupt:
                        _thread.interrupt_main()
                        raise
                    except Exception:
                        pass
                return UnityChunksResult(success=False, chunks=[])

            obj_path = unity_cpp.with_suffix(".o")

            # Force unity builds to not use PCH
            additional_flags = list(compile_options.additional_flags or [])
            if "-c" not in additional_flags:
                additional_flags.insert(0, "-c")
            per_file_options = CompilerOptions(
                include_path=compile_options.include_path,
                compiler=compile_options.compiler,
                defines=compile_options.defines,
                std_version=compile_options.std_version,
                compiler_args=compile_options.compiler_args,
                use_pch=False,
                additional_flags=additional_flags,
                parallel=compile_options.parallel,
                temp_dir=compile_options.temp_dir,
                archiver=compile_options.archiver,
                archiver_args=compile_options.archiver_args,
                pch_header_content=compile_options.pch_header_content,
                pch_output_path=compile_options.pch_output_path,
            )

            if no_parallel:
                res = self._compile_cpp_file_sync(
                    unity_cpp, obj_path, per_file_options.additional_flags, False
                )
                chunk_results.append(
                    UnityChunkResult(
                        ok=res.ok,
                        unity_cpp=unity_cpp,
                        object_path=obj_path,
                        stderr=res.stderr,
                        return_code=res.return_code,
                    )
                )
                if not res.ok and cleanup_dir:
                    try:
                        unity_cpp.unlink(missing_ok=True)
                        obj_path.unlink(missing_ok=True)
                    except KeyboardInterrupt:
                        _thread.interrupt_main()
                        raise
                    except Exception:
                        pass
                    return UnityChunksResult(success=False, chunks=chunk_results)
            else:
                fut = _EXECUTOR.submit(
                    self._compile_cpp_file_sync,
                    unity_cpp,
                    obj_path,
                    per_file_options.additional_flags,
                    False,
                )
                compile_futures.append((i, fut, unity_cpp, obj_path))

        if not no_parallel:
            for i, fut, unity_cpp, obj_path in compile_futures:
                res = fut.result()
                chunk_results.append(
                    UnityChunkResult(
                        ok=res.ok,
                        unity_cpp=unity_cpp,
                        object_path=obj_path,
                        stderr=res.stderr,
                        return_code=res.return_code,
                    )
                )
                if not res.ok and cleanup_dir:
                    try:
                        unity_cpp.unlink(missing_ok=True)
                        obj_path.unlink(missing_ok=True)
                    except Exception:
                        pass

        success = all(c.ok for c in chunk_results) and len(chunk_results) == chunks
        # Note: we do not remove files on success; caller may need them for archiving
        return UnityChunksResult(success=success, chunks=chunk_results)

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
            object_files,
            output_archive,
            options,
            self.settings.archiver,
            self.build_flags,
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
        return link_program_sync(link_options, self.build_flags)

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


def get_configured_linker_command(build_flags_config: BuildFlags) -> list[str]:
    """Get linker command from build configuration TOML."""
    if build_flags_config.tools.linker:
        return build_flags_config.tools.linker
    assert False, "FATAL ERROR: linker is not set in build_flags.toml"


def link_program_sync(
    link_options: LinkOptions, build_flags_config: BuildFlags
) -> Result:
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
    system = platform.system()

    # Check if we're using GNU-style arguments (indicates Zig toolchain)
    using_gnu_style = any(
        arg.startswith("-") and not arg.startswith("-Wl,")
        for arg in link_options.linker_args
    )

    # Build linker command - prefer configuration over detection
    cmd: list[str] = []

    if linker is None:
        if build_flags_config:
            # Use configured toolchain for linking when available
            configured_cmd = get_configured_linker_command(build_flags_config)
            if configured_cmd:
                cmd = configured_cmd[:]  # Copy the configured command
            else:
                # Fallback to detection if no configuration available
                try:
                    linker = detect_linker()
                    cmd = [linker]
                except RuntimeError as e:
                    return Result(ok=False, stdout="", stderr=str(e), return_code=1)
        elif system == "Windows" and using_gnu_style:
            # Legacy fallback: Use Zig toolchain for linking when GNU-style args are provided
            cmd = ["python", "-m", "ziglang", "c++"]
        else:
            try:
                linker = detect_linker()
                cmd = [linker]
            except RuntimeError as e:
                return Result(ok=False, stdout="", stderr=str(e), return_code=1)
    else:
        # linker was provided explicitly
        if isinstance(linker, str):
            # For Windows paths, don't use shlex.split() as it mangles backslashes
            # The linker variable should be a single executable path, not a command with args
            cmd = [linker]
        else:
            # linker should be str at this point
            cmd = [str(linker)]

    # Add platform-specific output flag first
    output_path = Path(link_options.output_executable)

    if system == "Windows" and using_gnu_style:
        # Zig toolchain or GNU-style linker on Windows
        cmd.extend(["-o", str(output_path)])
    elif system == "Windows" and linker and ("lld-link" in linker or "link" in linker):
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
        # Optimize command to use sys.executable instead of shell 'python' resolution
        python_exe = optimize_python_command(cmd)

        # Use Popen with stderr redirected to stdout for single stream pumping
        process = subprocess.Popen(
            python_exe,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # Merge stderr into stdout
            text=True,
            bufsize=1,  # Line buffered
            encoding="utf-8",  # Explicitly use UTF-8
            errors="replace",  # Replace invalid chars instead of failing
        )

        stdout_lines: list[str] = []
        stderr_lines: list[str] = []  # Keep separate for API compatibility

        # Read only stdout (which contains both stdout and stderr)
        while True:
            output_line = process.stdout.readline() if process.stdout else ""

            # Process output lines (both stdout and stderr combined)
            if output_line:
                line_stripped = output_line.rstrip()
                print(output_line, end="", flush=True)  # Stream to console
                # Add to both lists for API compatibility
                stdout_lines.append(line_stripped)
                stderr_lines.append(line_stripped)

            # Check if process has finished
            if process.poll() is not None:
                # Read any remaining output
                remaining_output = process.stdout.read() if process.stdout else ""

                if remaining_output:
                    print(remaining_output, end="", flush=True)
                    for line in remaining_output.splitlines():
                        line_stripped = line.rstrip()
                        stdout_lines.append(line_stripped)
                        stderr_lines.append(line_stripped)
                break

        return Result(
            ok=process.returncode == 0,
            stdout="".join(stdout_lines),
            stderr="".join(stderr_lines),
            return_code=process.returncode,
        )

    except KeyboardInterrupt:
        _thread.interrupt_main()
        raise
    except Exception as e:
        # Check if output executable was created successfully despite the exception
        output_path = Path(link_options.output_executable)
        if output_path.exists() and output_path.stat().st_size > 0:
            # Linking succeeded - executable was created successfully
            # The exception was likely during output reading or cleanup, not linking
            return Result(
                ok=True,
                stdout="",
                stderr=f"Linking succeeded with minor exception during cleanup: {type(e).__name__}: {e}",
                return_code=0,
            )

        # Create verbose error message with full command and exception details
        cmd_str = subprocess.list2cmdline(str(arg) for arg in cmd)
        verbose_error = (
            f"Linker command failed with exception: {type(e).__name__}: {e}\n"
            f"Failed command: {cmd_str}\n"
            f"Working directory: {Path.cwd()}\n"
            f"Output executable: {link_options.output_executable}\n"
            f"Object files: {link_options.object_files}\n"
            f"Static libraries: {link_options.static_libraries}\n"
            f"Linker args: {link_options.linker_args}"
        )
        return Result(ok=False, stdout="", stderr=verbose_error, return_code=-1)


# Helper functions for common linker argument patterns


# NOTE: Linker flag functions removed - all linker configuration now comes from build configuration TOML
# Use BuildFlags.parse() to load linker flags from TOML configuration instead


# Convenience functions for backward compatibility


# Default settings for backward compatibility - NO FALLBACKS POLICY
def _get_default_settings() -> CompilerOptions:
    """Get default compiler settings with MANDATORY configuration-based stub platform defines."""

    from ci.compiler.test_example_compilation import (
        extract_stub_platform_defines_from_toml,
        extract_stub_platform_include_paths_from_toml,
        load_build_flags_toml,
    )

    # Add the ci/compiler directory to path for imports
    ci_dir = _HERE.parent

    # MANDATORY: Load build_unit.toml configuration - NO fallbacks allowed
    toml_path = ci_dir / "build_unit.toml"
    if not toml_path.exists():
        raise RuntimeError(
            f"CRITICAL: build_unit.toml not found at {toml_path}. "
            f"This file is MANDATORY for all compiler operations."
        )

    # Load configuration with strict error handling
    build_config = load_build_flags_toml(str(toml_path))
    stub_defines = extract_stub_platform_defines_from_toml(build_config)
    stub_include_paths = extract_stub_platform_include_paths_from_toml(build_config)

    # Convert relative include paths to absolute and create compiler args
    compiler_args: list[str] = []
    for include_path in stub_include_paths:
        if os.path.isabs(include_path):
            compiler_args.append(f"-I{include_path}")
        else:
            # Convert relative path to absolute from project root
            abs_path = str(ci_dir.parent / include_path)
            compiler_args.append(f"-I{abs_path}")

    return CompilerOptions(
        include_path="./src",
        defines=stub_defines,
        std_version="c++17",
        compiler_args=compiler_args,  # Include paths from configuration
    )


def _get_default_build_flags() -> BuildFlags:
    """Get default BuildFlags with MANDATORY configuration loading - NO FALLBACKS!"""
    from pathlib import Path

    # MANDATORY: Load build_unit.toml configuration - NO fallbacks allowed
    current_dir = Path(__file__).parent
    ci_dir = current_dir.parent
    toml_path = ci_dir / "build_unit.toml"

    if not toml_path.exists():
        raise RuntimeError(
            f"CRITICAL: build_unit.toml not found at {toml_path}. "
            f"This file is MANDATORY for all compiler operations."
        )

    # Use BuildFlags.parse() method for proper type construction
    return BuildFlags.parse(toml_path)


# Shared compiler instance for backward compatibility functions
_default_compiler = None


def _get_default_compiler() -> Compiler:
    """Get or create a shared compiler instance with default settings."""
    global _default_compiler
    if _default_compiler is None:
        # STRICT: Both CompilerOptions AND BuildFlags are REQUIRED - NO DEFAULTS!
        build_flags = _get_default_build_flags()
        default_settings = _get_default_settings()
        _default_compiler = Compiler(default_settings, build_flags)
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
    # STRICT: Load BuildFlags explicitly - NO defaults allowed
    build_flags = _get_default_build_flags()
    compiler = Compiler(settings, build_flags)

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
    # STRICT: Load BuildFlags explicitly - NO defaults allowed
    build_flags_with_args = _get_default_build_flags()
    compiler_with_args = Compiler(settings_with_args, build_flags_with_args)
    if ino_files:
        # Test that compiler_args are included in the command
        with tempfile.NamedTemporaryFile(suffix=".o", delete=False) as temp_file:
            temp_output = temp_file.name

        # Get the command that would be executed
        cmd = get_fastled_compile_command(ino_files[0], temp_output)
        print(f"Command with compiler_args: {subprocess.list2cmdline(cmd)}")

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
    success_compat, version_compat, _ = check_clang_version()
    print(
        f"Backward compatibility version check: success={success_compat}, version={version_compat}"
    )

    return result and success_compat


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


def get_configured_archiver_command(build_flags_config: BuildFlags) -> list[str] | None:
    """Get archiver command from build configuration TOML."""
    if build_flags_config.tools.archiver:
        return build_flags_config.tools.archiver
    return None


# detect_archiver function REMOVED - auto-detection from environment is FORBIDDEN
# All tools must be explicitly configured in TOML build configuration


def create_archive_sync(
    object_files: list[Path],
    output_archive: Path,
    options: LibarchiveOptions = LibarchiveOptions(),
    archiver: str | None = None,
    build_flags_config: BuildFlags | None = None,
) -> Result:
    """
    Create a static library archive (.a) from compiled object files.

    Args:
        object_files: List of .o files to include in archive
        output_archive: Output .a file path
        options: Archive generation options
        archiver: Archiver command from TOML configuration (REQUIRED - no auto-detection)
        build_flags_config: Build configuration with archive flags (REQUIRED)

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

    # Archiver MUST be provided from TOML configuration - NO AUTO-DETECTION
    if archiver is None:
        return Result(
            ok=False,
            stdout="",
            stderr="CRITICAL: No archiver provided to create_archive_sync(). "
            "Archiver must be configured in [tools] section of build TOML file. "
            "Auto-detection from environment is NOT ALLOWED.",
            return_code=1,
        )

    # Build archive command - STRICT configuration enforcement
    cmd: list[str] = []
    if build_flags_config:
        configured_cmd = get_configured_archiver_command(build_flags_config)
        if configured_cmd:
            # Use configured command - verify it matches what was passed
            configured_str = subprocess.list2cmdline(configured_cmd)
            if archiver and archiver != configured_str:
                print(f"[ARCHIVE WARNING] Archiver mismatch detected:")
                print(f"  Passed archiver: '{archiver}'")
                print(f"  Configured archiver: '{configured_str}'")
                print(f"  Using configured archiver (takes precedence)")

            cmd = configured_cmd[:]
            print(
                f"[ARCHIVE] Using configured archiver command: {subprocess.list2cmdline(cmd)}"
            )
        else:
            raise RuntimeError(
                f"CRITICAL: No archiver configured in build_flags_config.tools.archiver. "
                f"Archiver fallbacks are NOT ALLOWED. "
                f"Please configure archiver in [tools] section of your build TOML file."
            )
    else:
        raise RuntimeError(
            f"CRITICAL: No build_flags_config provided to create_archive_sync(). "
            f"Archive creation requires build configuration. "
            f"System tool fallbacks are NOT ALLOWED to prevent silent configuration errors."
        )

    # Archive flags: MUST come from build configuration TOML - NO DEFAULTS
    if (
        not build_flags_config
        or not hasattr(build_flags_config, "archive")
        or not hasattr(build_flags_config.archive, "flags")
    ):
        raise RuntimeError(
            "CRITICAL: Archive flags not found in build configuration TOML. "
            "Please ensure [archive] section with 'flags' is configured."
        )

    # Get platform-specific archive flags if available
    import platform

    system = platform.system()
    flags = build_flags_config.archive.flags  # Default flags

    # Check for platform-specific archive flags
    if system == "Darwin":  # macOS
        if (
            build_flags_config.archive.darwin
            and build_flags_config.archive.darwin.flags
        ):
            flags = build_flags_config.archive.darwin.flags
            print(f"[ARCHIVE] Using macOS-specific flags: {flags}")
    elif system == "Linux":
        if build_flags_config.archive.linux and build_flags_config.archive.linux.flags:
            flags = build_flags_config.archive.linux.flags
            print(f"[ARCHIVE] Using Linux-specific flags: {flags}")
    elif system == "Windows":
        if (
            build_flags_config.archive.windows
            and build_flags_config.archive.windows.flags
        ):
            flags = build_flags_config.archive.windows.flags
            print(f"[ARCHIVE] Using Windows-specific flags: {flags}")

    print(f"[ARCHIVE] Final flags selected: {flags}")

    if options.use_thin:
        flags += "T"  # Add thin archive flag

    cmd.append(flags)
    cmd.append(str(output_archive))
    cmd.extend(str(obj) for obj in object_files)

    # Ensure output directory exists
    output_archive.parent.mkdir(parents=True, exist_ok=True)

    # Debug output for archiver command (helpful for diagnosing fallback issues)
    print(f"[ARCHIVE] Using archiver command: {subprocess.list2cmdline(cmd)}")
    print(f"[ARCHIVE] Archive flags: {flags}")
    print(f"[ARCHIVE] Platform: {system}")

    # Execute archive command with single stream to prevent buffer overflow
    try:
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # Merge stderr into stdout
            text=True,
            bufsize=1,
            encoding="utf-8",
            errors="replace",
            cwd=None,
        )

        stdout_lines: list[str] = []
        stderr_lines: list[str] = []  # Keep separate for API compatibility

        # Read only stdout (which contains both stdout and stderr)
        while True:
            output_line = process.stdout.readline() if process.stdout else ""

            if output_line:
                line_stripped = output_line.rstrip()
                # Add to both lists for API compatibility
                stdout_lines.append(line_stripped)
                stderr_lines.append(line_stripped)

            if process.poll() is not None:
                # Read remaining output
                remaining_output = process.stdout.read() if process.stdout else ""

                if remaining_output:
                    for line in remaining_output.splitlines():
                        line_stripped = line.rstrip()
                        stdout_lines.append(line_stripped)
                        stderr_lines.append(line_stripped)
                break

        return Result(
            ok=process.returncode == 0,
            stdout="\n".join(stdout_lines),
            stderr="\n".join(stderr_lines),
            return_code=process.returncode,
        )
    except KeyboardInterrupt:
        _thread.interrupt_main()
        raise
    except Exception as e:
        error_msg = f"Archive command failed: {e}\nCommand was: {subprocess.list2cmdline(cmd)}\nThis may indicate that the configured archiver is not available."
        print(f"[ARCHIVE ERROR] {error_msg}")
        return Result(ok=False, stdout="", stderr=error_msg, return_code=-1)


def load_build_flags_from_toml(
    toml_path: Path, quick_build: bool = False, strict_mode: bool = False
) -> BuildFlags:
    """
    Load build flags from TOML configuration file.

    This is a convenience function that delegates to BuildFlags.parse().

    Args:
        toml_path: Path to the build configuration TOML file (build_unit.toml or build_example.toml)
        quick_build: Use quick build mode flags (default: debug mode)
        strict_mode: Enable strict mode warning flags

    Returns:
        BuildFlags: Parsed build flags from TOML file
    """
    return BuildFlags.parse(toml_path, quick_build, strict_mode)


def create_compiler_options_from_toml(
    toml_path: Path,
    include_path: str,
    quick_build: bool = False,
    strict_mode: bool = False,
    additional_defines: List[str] | None = None,
    additional_compiler_args: List[str] | None = None,
    **kwargs: Any,
) -> CompilerOptions:
    """
    Create CompilerOptions from TOML build flags configuration.

    Args:
        toml_path: Path to the build configuration TOML file (build_unit.toml or build_example.toml)
        include_path: Base include path for compilation
        quick_build: Use quick build mode flags (default: debug mode)
        strict_mode: Enable strict mode warning flags
        additional_defines: Extra defines to add beyond TOML
        additional_compiler_args: Extra compiler args to add beyond TOML
        **kwargs: Additional CompilerOptions parameters

    Returns:
        CompilerOptions: Configured compiler options with TOML flags
    """
    # Load build flags from TOML
    build_flags = BuildFlags.parse(toml_path, quick_build, strict_mode)

    # Create compiler args from TOML flags
    compiler_args: List[str] = []
    compiler_args.extend(build_flags.compiler_flags)
    compiler_args.extend(build_flags.include_flags)

    # Add strict mode flags if enabled
    if strict_mode:
        compiler_args.extend(build_flags.strict_mode_flags)

    # Add additional compiler args if provided
    if additional_compiler_args:
        compiler_args.extend(additional_compiler_args)

    # Extract defines without the "-D" prefix for CompilerOptions
    defines: List[str] = []
    for define in build_flags.defines:
        if define.startswith("-D"):
            defines.append(define[2:])  # Remove "-D" prefix
        else:
            defines.append(define)

    # Add additional defines if provided
    if additional_defines:
        defines.extend(additional_defines)

    # Remove conflicting tool parameters from kwargs (TOML tools take precedence)
    filtered_kwargs = {
        k: v for k, v in kwargs.items() if k not in ["compiler", "archiver"]
    }

    # STRICT: Compiler must be configured in TOML - NO DEFAULTS
    if not build_flags.tools.cpp_compiler:
        raise RuntimeError(
            f"CRITICAL: No compiler configured in [tools] section of {toml_path}. "
            f'Please add: cpp_compiler = ["uv", "run", "python", "-m", "ziglang", "c++"] '
            f"or similar compiler configuration. "
            f"Default tool fallbacks are NOT ALLOWED."
        )
    compiler_tool = subprocess.list2cmdline(build_flags.tools.cpp_compiler)
    # STRICT: Archiver must be configured in TOML - NO DEFAULTS
    if not build_flags.tools.archiver:
        raise RuntimeError(
            f"CRITICAL: No archiver configured in [tools] section of {toml_path}. "
            f'Please add: archiver = ["uv", "run", "python", "-m", "ziglang", "ar"] '
            f"or similar archiver configuration. "
            f"Default tool fallbacks are NOT ALLOWED."
        )
    archiver_tool = subprocess.list2cmdline(build_flags.tools.archiver)

    # Create CompilerOptions with TOML-loaded flags and tools
    return CompilerOptions(
        include_path=include_path,
        defines=defines,
        compiler_args=compiler_args,
        compiler=compiler_tool,  # Use command-based tool or fallback
        archiver=archiver_tool,  # Use command-based tool or fallback
        **filtered_kwargs,
    )


if __name__ == "__main__":
    import sys

    success = main()
    sys.exit(0 if success else 1)

# --------------------------------------------------------------------------------------
# Compile Commands JSON generation
# --------------------------------------------------------------------------------------


def _normalize_define_to_token(define: str) -> str:
    """Normalize a define into a single "-DNAME[=VALUE]" token.

    Args:
        define: Define that may or may not start with "-D" and may include "=VALUE"

    Returns:
        A single token in the form "-DNAME" or "-DNAME=VALUE".
    """
    # Strip any existing leading -D
    value: str = define[2:] if define.startswith("-D") else define
    return f"-D{value}"


def _build_arguments_for_tu(
    tool_tokens: List[str],
    include_root: Path,
    settings: "CompilerOptions",
    defines_from_build: List[str],
    source_file: Path,
    object_output: Path,
) -> List[str]:
    """Compose the full arguments list for a single translation unit.

    This mirrors the real build by using the tool tokens from BuildFlags, the
    include root, normalized defines, and compiler args from CompilerOptions.
    """
    args: List[str] = []

    # Program tokens (e.g. ["uv","run","python","-m","ziglang","c++"]) as-is
    args.extend(tool_tokens)

    # Language specification and standard version where available
    args.extend(["-x", "c++"])
    if settings.std_version:
        args.append(f"-std={settings.std_version}")

    # Base include for project source root
    args.append(f"-I{str(include_root)}")

    # Normalized defines (single-token -DNAME[=VALUE])
    for d in defines_from_build:
        args.append(_normalize_define_to_token(d))

    # Additional compiler args from settings (already include flags from TOML)
    for a in settings.compiler_args:
        args.append(a)

    # Compile only
    args.extend(["-c", str(source_file)])
    args.extend(["-o", str(object_output)])

    return args


def commands_json(
    config_path: Path,
    include_root: Path,
    sources: List[Path],
    output_json: Path,
    quick_build: bool = True,
    strict_mode: bool = False,
) -> None:
    """Emit compile_commands.json compatible entries using the BuildFlags toolchain.

    - Loads BuildFlags via BuildFlags.parse(config_path, quick_build, strict_mode)
    - Creates CompilerOptions via create_compiler_options_from_toml(...)
    - For each source, composes the arguments array using tool tokens and flags
    - Writes JSON array to output_json with both "arguments" and "command" fields
    """
    try:
        # Fail fast on missing configuration
        if not config_path.exists():
            raise RuntimeError(
                f"CRITICAL: Build flags TOML not found at {config_path}. "
                f"This file is required."
            )

        # Single-source-of-truth configuration via BuildFlags.parse
        build_flags = BuildFlags.parse(config_path, quick_build, strict_mode)
        settings = create_compiler_options_from_toml(
            toml_path=config_path,
            include_path=str(include_root),
            quick_build=quick_build,
            strict_mode=strict_mode,
        )

        # Validate tools are present (no defaults allowed)
        if not build_flags.tools.cpp_compiler:
            raise RuntimeError(
                f"CRITICAL: [tools].cpp_compiler missing in {config_path}. "
                f"Please configure compiler tokens."
            )

        # Prepare output directory
        output_json.parent.mkdir(parents=True, exist_ok=True)

        # Object directory for per-file outputs (mirrors prior generator)
        objects_dir: Path = include_root.parent / ".build" / "compile_commands"
        objects_dir.mkdir(parents=True, exist_ok=True)

        entries: List[Dict[str, Any]] = []
        for src in sources:
            obj_path: Path = (objects_dir / src.with_suffix(".o").name).resolve()
            program_tokens: List[str] = list(build_flags.tools.cpp_compiler)
            defines_tokens: List[str] = list(build_flags.defines)

            args_list = _build_arguments_for_tu(
                tool_tokens=program_tokens,
                include_root=include_root,
                settings=settings,
                defines_from_build=defines_tokens,
                source_file=src,
                object_output=obj_path,
            )

            entry: Dict[str, Any] = {
                "directory": str(include_root.parent.resolve()),
                "file": str(src.resolve()),
                "arguments": args_list,
                "command": subprocess.list2cmdline(args_list),
            }
            entries.append(entry)

        # Write JSON (common to both paths)
        import json

        with output_json.open("w", encoding="utf-8") as f:
            f.write(json.dumps(entries, indent=2))
            f.write("\n")

    except KeyboardInterrupt:
        _thread.interrupt_main()
        raise
    except FileNotFoundError as e:
        raise RuntimeError(f"CRITICAL: Required file not found: {e}")
    except Exception as e:
        # Re-raise with context
        raise RuntimeError(f"CRITICAL: Failed to generate compile_commands.json: {e}")
