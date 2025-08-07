#!/usr/bin/env python3
"""
Build optimization system for FastLED compilation.

This module provides classes for capturing build metadata during the first compilation
and using it to create optimized subsequent builds.
"""

import json
import os
import shutil
import subprocess
import tempfile
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from ci.util.boards import Board


@dataclass
class CompilationCommand:
    """Captured compilation command metadata."""

    command_type: str  # "compile", "archive", "link"
    compiler: str
    source_file: str
    output_file: str
    args: List[str]
    include_paths: List[str]
    defines: List[str]
    timestamp: float
    working_dir: str


@dataclass
class BuildOptimizationMetadata:
    """Aggregated build optimization metadata."""

    platform: str
    compiler: str
    archiver: str
    base_flags: List[str]
    include_paths: List[str]
    link_flags: List[str]
    archives_created: List[str]
    sketch_compilation_pattern: Dict[str, str]
    total_commands: int
    framework_commands: int
    fastled_commands: int
    sketch_commands: int
    optimization_timestamp: float


class BuildMetadataAggregator:
    """Aggregates captured JSON files into optimization metadata."""

    def __init__(self, json_capture_dir: Path, board: Board):
        self.json_capture_dir = json_capture_dir
        self.board = board

    def aggregate_metadata(self) -> BuildOptimizationMetadata:
        """Process all captured JSON files and create optimization metadata."""
        json_files = list(self.json_capture_dir.glob("cmd_*.json"))

        if not json_files:
            raise ValueError(f"No command JSON files found in {self.json_capture_dir}")

        # Load all commands
        commands: List[CompilationCommand] = []
        for json_file in sorted(json_files):
            try:
                with open(json_file) as f:
                    cmd_data = json.load(f)
                    commands.append(CompilationCommand(**cmd_data))
            except Exception as e:
                print(f"Warning: Failed to load {json_file}: {e}")
                continue

        if not commands:
            raise ValueError("No valid compilation commands found")

        # Analyze commands
        include_paths = self._extract_unique_includes(commands)
        base_flags = self._extract_base_compilation_flags(commands)
        link_flags = self._extract_link_flags(commands)
        archives = self._find_created_archives(commands)

        # Categorize commands
        framework_cmds = [c for c in commands if self._is_framework_command(c)]
        fastled_cmds = [c for c in commands if self._is_fastled_command(c)]
        sketch_cmds = [c for c in commands if self._is_sketch_command(c)]

        return BuildOptimizationMetadata(
            platform=self.board.board_name,
            compiler=self._find_primary_compiler(commands),
            archiver=self._find_archiver_tool(commands),
            base_flags=base_flags,
            include_paths=include_paths,
            link_flags=link_flags,
            archives_created=archives,
            sketch_compilation_pattern=self._extract_sketch_pattern(sketch_cmds),
            total_commands=len(commands),
            framework_commands=len(framework_cmds),
            fastled_commands=len(fastled_cmds),
            sketch_commands=len(sketch_cmds),
            optimization_timestamp=time.time(),
        )

    def _extract_unique_includes(self, commands: List[CompilationCommand]) -> List[str]:
        """Extract unique include paths in order."""
        includes: List[str] = []
        seen: set[str] = set()
        
        for cmd in commands:
            for inc_path in cmd.include_paths:
                if inc_path not in seen:
                    includes.append(inc_path)
                    seen.add(inc_path)
        
        return includes

    def _extract_base_compilation_flags(
        self, commands: List[CompilationCommand]
    ) -> List[str]:
        """Extract common compilation flags."""
        compile_cmds = [c for c in commands if c.command_type == "compile"]
        if not compile_cmds:
            return []

        # Find flags that appear in most compilation commands
        flag_counts: Dict[str, int] = {}
        for cmd in compile_cmds:
            for arg in cmd.args:
                if (
                    arg.startswith("-")
                    and not arg.startswith("-I")
                    and not arg.startswith("-D")
                ):
                    flag_counts[arg] = flag_counts.get(arg, 0) + 1

        # Include flags that appear in at least 50% of commands
        threshold = len(compile_cmds) * 0.5
        common_flags: List[str] = [
            flag for flag, count in flag_counts.items() if count >= threshold
        ]

        return sorted(common_flags)

    def _extract_link_flags(self, commands: List[CompilationCommand]) -> List[str]:
        """Extract linking flags."""
        link_cmds = [c for c in commands if c.command_type == "link"]
        if not link_cmds:
            return []

        # Get flags from the first link command (they should be consistent)
        link_cmd = link_cmds[0]
        flags: List[str] = []

        for arg in link_cmd.args:
            if (
                arg.startswith("-")
                and not arg.startswith("-L")
                and not arg.startswith("-l")
            ):
                flags.append(arg)

        return flags

    def _find_created_archives(self, commands: List[CompilationCommand]) -> List[str]:
        """Find archives that were created."""
        archive_cmds = [c for c in commands if c.command_type == "archive"]
        archives: List[str] = []

        for cmd in archive_cmds:
            if cmd.output_file.endswith(".a"):
                archives.append(os.path.basename(cmd.output_file))

        return archives

    def _find_primary_compiler(self, commands: List[CompilationCommand]) -> str:
        """Find the primary compiler used."""
        compile_cmds = [c for c in commands if c.command_type == "compile"]
        if compile_cmds:
            return compile_cmds[0].compiler
        return "gcc"  # fallback

    def _find_archiver_tool(self, commands: List[CompilationCommand]) -> str:
        """Find the archiver tool used."""
        archive_cmds = [c for c in commands if c.command_type == "archive"]
        if archive_cmds:
            return archive_cmds[0].compiler
        return "ar"  # fallback

    def _extract_sketch_pattern(
        self, sketch_cmds: List[CompilationCommand]
    ) -> Dict[str, Union[str, List[str]]]:
        """Extract sketch compilation pattern."""
        if not sketch_cmds:
            return {}

        # Get pattern from first sketch command
        cmd = sketch_cmds[0]
        return {
            "compiler": cmd.compiler,
            "base_args": [
                arg
                for arg in cmd.args
                if not arg.startswith("-I") and not arg.startswith("-D")
            ],
            "working_dir": cmd.working_dir,
        }

    def _is_framework_command(self, cmd: CompilationCommand) -> bool:
        """Check if command is for framework compilation."""
        indicators = [
            "FrameworkArduino",
            "framework-arduino",
            "Arduino.cpp",
            "wiring.c",
            "WString.cpp",
        ]

        source_path = cmd.source_file.lower()
        return any(indicator.lower() in source_path for indicator in indicators)

    def _is_fastled_command(self, cmd: CompilationCommand) -> bool:
        """Check if command is for FastLED compilation."""
        indicators = ["FastLED", "fastled", "/fl/", "/fx/", "colorutils", "hsv2rgb"]

        source_path = cmd.source_file.lower()
        return any(indicator.lower() in source_path for indicator in indicators)

    def _is_sketch_command(self, cmd: CompilationCommand) -> bool:
        """Check if command is for sketch compilation."""
        indicators = ["main.cpp", "/src/", "/sketch/", ".ino"]

        source_path = cmd.source_file.lower()
        return any(indicator.lower() in source_path for indicator in indicators)


def detect_build_phase(board: Board) -> str:
    """Determine if this is first build (capture) or optimized build."""
    from ci.compiler.pio import FastLEDPaths

    paths = FastLEDPaths(board.board_name)

    # Check for optimization artifacts
    unified_archive = paths.build_dir / "libfastled_unified.a"
    build_metadata = paths.build_dir / "optimization_metadata.json"
    json_capture_dir = paths.build_dir / "json_capture"

    if (
        unified_archive.exists()
        and build_metadata.exists()
        and json_capture_dir.exists()
        and len(list(json_capture_dir.glob("cmd_*.json"))) > 0
    ):
        return "optimized"
    else:
        return "capture"


def setup_optimization_environment(board: Board, build_phase: str) -> Dict[str, str]:
    """Set up environment variables for optimization phase."""
    from ci.compiler.pio import FastLEDPaths

    paths = FastLEDPaths(board.board_name)

    env_vars = {
        "FASTLED_BUILD_PHASE": build_phase,
        "FASTLED_JSON_CAPTURE_DIR": str(paths.build_dir / "json_capture"),
        "FASTLED_BOARD_NAME": board.board_name,
    }

    if build_phase == "optimized":
        env_vars.update(
            {
                "FASTLED_UNIFIED_ARCHIVE": str(
                    paths.build_dir / "libfastled_unified.a"
                ),
                "FASTLED_OPTIMIZATION_METADATA": str(
                    paths.build_dir / "optimization_metadata.json"
                ),
            }
        )

    return env_vars


def save_optimization_metadata(
    metadata: BuildOptimizationMetadata, output_file: Path
) -> None:
    """Save optimization metadata to JSON file."""
    with open(output_file, "w") as f:
        json.dump(asdict(metadata), f, indent=2)

    print(f"✅ Saved optimization metadata: {output_file}")
    print(f"   Total commands captured: {metadata.total_commands}")
    print(f"   Framework commands: {metadata.framework_commands}")
    print(f"   FastLED commands: {metadata.fastled_commands}")
    print(f"   Sketch commands: {metadata.sketch_commands}")


def load_optimization_metadata(build_dir: Path) -> dict:
    """Load optimization metadata from JSON file as dict."""
    metadata_file = build_dir / "optimization_metadata.json"
    
    if not metadata_file.exists():
        raise FileNotFoundError(f"Optimization metadata not found: {metadata_file}")
        
    with open(metadata_file) as f:
        return json.load(f)


def load_optimization_metadata_typed(metadata_file: Path) -> BuildOptimizationMetadata:
    """Load optimization metadata from JSON file as typed object."""
    with open(metadata_file) as f:
        data = json.load(f)

    return BuildOptimizationMetadata(**data)


def create_unified_archive(build_dir: Path, metadata: BuildOptimizationMetadata) -> Path:
    """Create unified archive from all generated library archives."""
    import shutil
    import subprocess
    import tempfile
    
    unified_archive = build_dir / "libfastled_unified.a"
    
    # Find all generated archives from the build
    archive_patterns = [
        ".pio/build/**/lib*.a",
        "lib*/lib*.a"
    ]
    
    archives = []
    for pattern in archive_patterns:
        archives.extend(build_dir.glob(pattern))
    
    # Filter out archives that are sketch-specific
    framework_archives = []
    for archive in archives:
        archive_name = archive.name.lower()
        # Include FastLED, Arduino framework, and standard libraries
        if any(keyword in archive_name for keyword in 
               ["fastled", "framework", "arduino", "spi", "wire", "eeprom"]):
            framework_archives.append(archive)
    
    if not framework_archives:
        print("WARNING: No framework archives found for unification")
        return unified_archive
    
    print(f"Creating unified archive from {len(framework_archives)} libraries...")
    
    # Create temporary directory for extraction
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        
        # Extract all archives
        for archive in framework_archives:
            print(f"  Extracting: {archive.name}")
            try:
                subprocess.run([
                    "ar", "x", str(archive)
                ], cwd=temp_path, check=True, capture_output=True)
            except subprocess.CalledProcessError as e:
                print(f"WARNING: Failed to extract {archive}: {e}")
                continue
        
        # Create unified archive from all extracted objects
        object_files = list(temp_path.glob("*.o"))
        if object_files:
            print(f"  Creating unified archive with {len(object_files)} object files...")
            try:
                ar_cmd = ["ar", "rcs", str(unified_archive)] + [str(obj) for obj in object_files]
                subprocess.run(ar_cmd, check=True, capture_output=True)
                print(f"✅ Created unified archive: {unified_archive}")
            except subprocess.CalledProcessError as e:
                print(f"ERROR: Failed to create unified archive: {e}")
                raise
        else:
            print("WARNING: No object files found to create unified archive")
    
    return unified_archive
