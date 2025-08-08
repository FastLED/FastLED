# pyright: reportUndefinedVariable=false, reportUnknownVariableType=false, reportUnknownMemberType=false, reportUnknownArgumentType=false, reportUnknownParameterType=false, reportMissingParameterType=false, reportMissingTypeArgument=false, reportAttributeAccessIssue=false, reportArgumentType=false, reportGeneralTypeIssues=false
"""
PlatformIO Advanced Script for intelligent LDF caching.

from https://github.com/Jason2866/Tasmota/blob/ldf_cache/pio-tools/ldf_cache.py 

This script implements a two-phase caching system:
1. First run: Performs verbose build, collects dependencies, creates cache
2. Second run: Applies cached dependencies with lib_ldf_mode=off for faster builds

Features:
- Intelligent cache invalidation based on file hashes
- Build order preservation for correct symbol resolution

Copyright: Jason2866
"""

Import("env")

import datetime
import hashlib
import json
import os
import pprint
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from platformio.builder.tools.piobuild import (
    SRC_ASM_EXT,
    SRC_BUILD_EXT,
    SRC_C_EXT,
    SRC_CXX_EXT,
    SRC_HEADER_EXT,
)
from platformio.builder.tools.piolib import LibBuilderBase


# INFO PlatformIO Core constants
# SRC_HEADER_EXT = ["h", "hpp", "hxx", "h++", "hh", "inc", "tpp", "tcc"]
# SRC_ASM_EXT = ["S", "spp", "SPP", "sx", "s", "asm", "ASM"]
# SRC_C_EXT = ["c"]
# SRC_CXX_EXT = ["cc", "cpp", "cxx", "c++"]
# SRC_BUILD_EXT = SRC_C_EXT + SRC_CXX_EXT + SRC_ASM_EXT

github_actions = os.getenv("GITHUB_ACTIONS")
project_dir = env.subst("$PROJECT_DIR")
env_name = env.subst("$PIOENV")
compiledb_path = (
    Path(project_dir) / ".pio" / "compiledb" / f"compile_commands_{env_name}.json"
)
logfile_path = (
    Path(project_dir) / ".pio" / "compiledb" / f"compile_commands_{env_name}.log"
)
cache_base = Path(project_dir) / ".pio" / "ldf_cache"
cache_file = cache_base / f"ldf_cache_{env_name}.py"
build_dir = Path(env.subst("$BUILD_DIR"))
src_dir = Path(env.subst("$PROJECT_SRC_DIR"))
config = env.GetProjectConfig()
flag_custom_sdkconfig = False
if config.has_option(
    "env:" + env["PIOENV"], "custom_sdkconfig"
) or env.BoardConfig().get("espidf.custom_sdkconfig", ""):
    flag_custom_sdkconfig = True

# Ensure log directory exists
logfile_path.parent.mkdir(parents=True, exist_ok=True)


def set_lib_ldf_mode_off():
    """
    Set lib_ldf_mode = off using PlatformIO's project configuration API.

    This function directly modifies the project configuration without
    touching the platformio.ini file.
    """
    projectconfig = env.GetProjectConfig()
    env_section = "env:" + env["PIOENV"]
    if not projectconfig.has_section(env_section):
        projectconfig.add_section(env_section)
    projectconfig.set(env_section, "lib_ldf_mode", "off")


def is_first_run_needed():
    """
    Determines if the first run (full verbose build) is needed based on file dependencies.

    Checks for essential build artifacts and compile database existence.

    Returns:
        bool: True if first run is needed, False if cache can be used
    """
    if not compiledb_path.exists() or compiledb_path.stat().st_size == 0:
        return True

    lib_dirs = list(build_dir.glob("lib*"))
    if not lib_dirs:
        return True

    return False


def is_build_environment_ready():
    """
    Checks if the build environment is complete and ready for cache application.

    Validates that all necessary build artifacts exist for second run.

    Returns:
        bool: True if build environment is ready for cache application
    """
    # Compile database must exist
    if not compiledb_path.exists():
        return False

    # At least one library directory must exist
    lib_dirs = list(build_dir.glob("lib*"))
    if not lib_dirs:
        return False

    return True


def should_trigger_verbose_build():
    """
    Determines if a verbose build should be triggered for first run.

    Considers environment variables, cache existence, and build targets.

    Returns:
        bool: True if verbose build should be triggered
    """
    # Prevent recursive calls
    if os.environ.get("_PIO_RECURSIVE_CALL") == "true":
        return False
    if os.environ.get("PLATFORMIO_SETTING_FORCE_VERBOSE") == "true":
        return False

    # Check for return code from previous recursive call
    if os.environ.get("_PIO_REC_CALL_RETURN_CODE") is not None:
        return False

    # If cache exists, no need for verbose build
    if cache_file.exists():
        return False

    # Debug: Print all sys.argv values
    #    print(f"Debug - sys.argv values: {sys.argv}")
    #    for i, arg in enumerate(sys.argv):
    #        print(f"  sys.argv[{i}]: {arg}")

    # Check sys.argv for "clean" target
    if any("clean" in str(arg).lower() for arg in sys.argv):
        return False

    if any("nobuild" in str(arg).lower() for arg in sys.argv):
        return False

    if any("erase" in str(arg).lower() for arg in sys.argv):
        return False

    return is_first_run_needed()


# Integrated log2compdb components for compile_commands.json generation
DIRCHANGE_PATTERN = re.compile(r"(?P<action>\w+) directory '(?P<path>.+)'")
INFILE_PATTERN = re.compile(r"(?P<path>.+\.(cpp|cxx|cc|c|hpp|hxx|h))", re.IGNORECASE)


@dataclass
class CompileCommand:
    """
    Represents a single compile command extracted from build logs.

    Attributes:
        file: Source file path
        output: Output object file path
        directory: Working directory for compilation
        arguments: Complete compiler command line arguments
    """

    file: str
    output: str
    directory: str
    arguments: list

    @classmethod
    def from_cmdline(
        cls, cc_cmd: Path, cmd_args: list[str], directory=None
    ) -> Optional["CompileCommand"]:
        """
        Create a CompileCommand from a command line.

        Parses compiler command line to extract source file, output file, and arguments.

        Args:
            cc_cmd: Path to the compiler executable
            cmd_args: List of command line arguments
            directory: Optional working directory

        Returns:
            CompileCommand or None if no valid input file found
        """
        if cc_cmd.name not in cmd_args[0]:
            return None

        cmd_args = cmd_args[:]
        cmd_args[0] = str(cc_cmd)

        if directory is None:
            directory = Path.cwd()
        else:
            directory = Path(directory)

        input_path = None

        # Try to find output file (-o flag)
        try:
            output_index = cmd_args.index("-o")
            output_arg = cmd_args[output_index + 1]
            if output_arg == "/dev/null":
                output_path = None
            else:
                output_path = directory / Path(output_arg)
        except (ValueError, IndexError):
            output_index = None
            output_path = None

        # Find input file based on output file stem or pattern matching
        if output_index is not None and output_path is not None:
            stem_matches = [
                item for item in cmd_args if Path(item).stem == output_path.stem
            ]
            for item in stem_matches:
                if input_file_match := INFILE_PATTERN.search(item):
                    input_path = input_file_match.group("path")
                    break
            if not input_path and stem_matches:
                input_path = stem_matches[0]
            if not input_path:
                return None
            input_path = directory / Path(input_path)
        else:
            # Fallback: search for source file patterns
            match = None
            for item in cmd_args:
                match = INFILE_PATTERN.search(item)
                if match:
                    break
            if not match:
                return None
            input_path = Path(match.group("path"))
            output_path = None

        return cls(
            file=str(input_path),
            arguments=cmd_args,
            directory=str(directory),
            output=str(output_path) if output_path else "",
        )


@dataclass
class Compiler:
    """
    Represents a compiler toolchain.

    Attributes:
        name: Compiler name (e.g., 'gcc', 'g++')
        path: Path to compiler executable
    """

    name: str
    path: Path

    @classmethod
    def from_name(cls, compiler_name: str) -> "Compiler":
        """
        Create Compiler from name string.

        Args:
            compiler_name: Name of the compiler

        Returns:
            Compiler instance
        """
        path = Path(compiler_name)
        return cls(name=compiler_name, path=path)

    def find_invocation_start(self, cmd_args: list[str]) -> int:
        """
        Find compiler invocation in argument list.

        Args:
            cmd_args: List of command line arguments

        Returns:
            int: Index of compiler invocation

        Raises:
            ValueError: If compiler invocation not found
        """
        for index, arg in enumerate(cmd_args):
            if self.name in arg or Path(arg).stem == self.name:
                return index
        raise ValueError(f"compiler invocation for {self.name} not found")


def parse_build_log_to_compile_commands(
    logfile_path: Path, compiler_names: list[str]
) -> list[CompileCommand]:
    """
    Parse build log to extract compile commands for compile_commands.json generation.

    Processes verbose build log to extract compiler invocations and create
    compile database entries for IDE integration.

    Args:
        logfile_path: Path to build log file
        compiler_names: List of compiler names to look for

    Returns:
        List of CompileCommand objects
    """
    if not logfile_path.exists():
        return []

    compilers = [Compiler.from_name(name) for name in compiler_names]
    entries = []
    file_entries = {}
    dirstack = [os.getcwd()]

    try:
        with logfile_path.open("r", encoding="utf-8", errors="ignore") as logfile:
            for line in logfile:
                line = line.strip()
                if not line:
                    continue

                # Handle directory changes (make-style output)
                if dirchange_match := DIRCHANGE_PATTERN.search(line):
                    action = dirchange_match.group("action")
                    path = dirchange_match.group("path")
                    if action == "Leaving":
                        if len(dirstack) > 1:
                            dirstack.pop()
                    elif action == "Entering":
                        dirstack.append(path)
                    continue

                # Parse command line
                try:
                    cmd_args = shlex.split(line)
                except ValueError:
                    continue

                if not cmd_args:
                    continue

                # Try to match against known compilers
                for compiler in compilers:
                    try:
                        compiler_invocation_start = compiler.find_invocation_start(
                            cmd_args
                        )
                        entry = CompileCommand.from_cmdline(
                            compiler.path,
                            cmd_args[compiler_invocation_start:],
                            dirstack[-1],
                        )

                        # Avoid duplicate entries for the same file
                        if entry is not None and entry.file not in file_entries:
                            entries.append(entry)
                            file_entries[entry.file] = entry
                            break
                    except ValueError:
                        continue

    except Exception as e:
        print(f"⚠ Error parsing build log: {e}")

    return entries


class LDFCacheOptimizer:
    """
    PlatformIO LDF cache optimizer to avoid unnecessary LDF runs.

    This class implements intelligent caching of Library Dependency Finder (LDF)
    results to significantly speed up subsequent builds.

    The optimizer works in two phases:
    1. First run: Collects dependencies, creates cache
    2. Second run: Applies cached dependencies with lib_ldf_mode=off

    Attributes:
        env: PlatformIO SCons environment
        env_name: Current environment name
        project_dir: Project root directory
        src_dir: Source directory path
        build_dir: Build output directory
        cache_file: Path to cache file
        _cache_applied_successfully: Flag indicating successful cache application
    """

    # File extensions relevant for LDF processing
    HEADER_EXTENSIONS = set(SRC_HEADER_EXT)
    SOURCE_EXTENSIONS = set(SRC_BUILD_EXT + SRC_C_EXT + SRC_CXX_EXT + SRC_ASM_EXT)
    CONFIG_EXTENSIONS = {".json", ".properties", ".txt", ".ini"}
    ALL_RELEVANT_EXTENSIONS = HEADER_EXTENSIONS | SOURCE_EXTENSIONS | CONFIG_EXTENSIONS

    # Directories to ignore during file scanning (optimized for ESP/Tasmota projects)
    IGNORE_DIRS = frozenset(
        [
            ".git",
            ".github",
            ".cache",
            ".vscode",
            ".pio",
            "api",
            "boards",
            "info",
            "data",
            "build",
            "build_output",
            "pio-tools",
            "tools",
            "__pycache__",
            "variants",
            "partitions",
            "berry",
            "berry_tasmota",
            "berry_matter",
            "berry_custom",
            "zigbee",
            "berry_animate",
            "berry_mapping",
            "berry_int64",
            "displaydesc",
            "language",
            "html_compressed",
            "html_uncompressed",
            "language",
            "energy_modbus_configs",
        ]
    )

    def __init__(self, environment):
        """
        Initialize the LDF cache optimizer with lazy initialization.

        Sets up paths, initializes PlatformIO integration, and determines
        whether to execute second run logic based on build environment state.

        Args:
            environment: PlatformIO SCons environment
        """
        self.env = environment
        self.env_name = self.env.get("PIOENV")
        self.project_dir = Path(self.env.subst("$PROJECT_DIR")).resolve()
        self.src_dir = Path(self.env.subst("$PROJECT_SRC_DIR")).resolve()
        self.build_dir = Path(self.env.subst("$BUILD_DIR")).resolve()

        # Setup cache and backup file paths
        cache_base = Path(self.project_dir) / ".pio" / "ldf_cache"
        self.cache_file = cache_base / f"ldf_cache_{self.env_name}.py"

        # Setup compile database paths
        compiledb_base = Path(self.project_dir) / ".pio" / "compiledb"
        self.compiledb_dir = compiledb_base
        self.compile_commands_file = (
            compiledb_base / f"compile_commands_{self.env_name}.json"
        )
        self.compile_commands_log_file = (
            compiledb_base / f"compile_commands_{self.env_name}.log"
        )

        self.lib_build_dir = Path(self.project_dir) / ".pio" / "build" / self.env_name
        self.ALL_RELEVANT_EXTENSIONS = (
            self.HEADER_EXTENSIONS | self.SOURCE_EXTENSIONS | self.CONFIG_EXTENSIONS
        )

        # Cache application status tracking
        self._cache_applied_successfully = False

        # Determine if second run should be executed (lazy initialization)
        if is_build_environment_ready() and not is_first_run_needed():
            print("🔄 Second run: Cache application mode")
            self.execute_second_run()

    def execute_second_run(self):
        """
        Execute second run logic: Apply cached dependencies with LDF disabled.

        Loads and validates cache, applies cached dependencies to SCons environment,
        and handles fallback to normal build if cache application fails.
        """
        self._cache_applied_successfully = False

        try:
            # Load and validate cache data
            cache_data = self.load_cache()
            if cache_data and self.validate_cache(cache_data):
                # Apply cached dependencies to build environment
                success = self.apply_ldf_cache_with_build_order(cache_data)
                if success:
                    # Set lib_ldf_mode = off using the new function
                    set_lib_ldf_mode_off()
                    self._cache_applied_successfully = True
                    print("✅ Cache applied successfully - lib_ldf_mode=off")
                else:
                    print("❌ Cache application failed")
            else:
                print("⚠ No valid cache found, falling back to normal build")

        except Exception as e:
            print(f"❌ Error in second run: {e}")
            self._cache_applied_successfully = False

    def apply_ldf_cache_with_build_order(self, cache_data):
        """
        Apply cached dependencies with correct build order preservation.

        Coordinates application of build order and SCons variables to ensure
        correct dependency resolution.

        Args:
            cache_data: Dictionary containing cached build data

        Returns:
            bool: True if cache application succeeded
        """
        try:
            self._current_cache_data = cache_data

            artifacts = cache_data.get("artifacts", {})
            build_order = artifacts

            if not build_order:
                print("❌ No build order data in cache")
                return False
            # Apply build order (OBJECTS)
            build_order_success = self.apply_build_order_to_environment(build_order)
            # Apply SCons variables (include paths, libraries)
            scons_vars_success = self.apply_cache_to_scons_vars(cache_data)
            if build_order_success and scons_vars_success:
                return True
            else:
                print("❌ Partial cache application failure")
                return False

        except Exception as e:
            print(f"✗ Error applying LDF cache: {e}")
            import traceback

            traceback.print_exc()
            return False

    def validate_ldf_mode_compatibility(self):
        """
        Validate that the current LDF mode is compatible with caching.

        Returns:
            bool: True if LDF mode is compatible with caching
        """
        try:
            current_mode = self.env.GetProjectOption("lib_ldf_mode", "chain")
            validated_mode = LibBuilderBase.validate_ldf_mode(current_mode)

            # Check against supported modes
            compatible_modes = ["chain", "off"]
            if validated_mode.lower() in compatible_modes:
                print(f"✅ LDF mode '{validated_mode}' is compatible with caching")
                return True
            else:
                print(f"⚠ LDF mode '{validated_mode}' not optimal for caching")
                print(f"💡 Recommended modes: {', '.join(compatible_modes)}")
                return False
        except Exception as e:
            print(f"⚠ Could not determine LDF mode: {e}")
            return True

    def create_compiledb_integrated(self):
        """
        Create compile_commands.json using integrated log parsing functionality.

        Generates compile database from verbose build log.

        Returns:
            bool: True if compile_commands.json was created successfully
        """
        # Check if compile_commands.json already exists
        if self.compile_commands_file.exists():
            print(f"✅ {self.compile_commands_file} exists")
            return True

        # Search for build log files
        build_log = None
        possible_logs = [
            self.compile_commands_log_file,
            Path(self.project_dir) / f"build_{self.env_name}.log",
            Path(self.build_dir) / f"build_{self.env_name}.log",
            Path(self.build_dir) / "build.log",
        ]

        for log_path in possible_logs:
            if log_path.exists() and log_path.stat().st_size > 0:
                build_log = log_path
                break

        if not build_log:
            print("⚠ No build log found for compile_commands.json generation")
            return False

        # Define supported compiler toolchains
        compiler_names = [
            "xtensa-esp32-elf-gcc",
            "xtensa-esp32-elf-g++",
            "xtensa-esp32s2-elf-gcc",
            "xtensa-esp32s2-elf-g++",
            "xtensa-esp32s3-elf-gcc",
            "xtensa-esp32s3-elf-g++",
            "riscv32-esp-elf-gcc",
            "riscv32-esp-elf-g++",
            "xtensa-lx106-elf-gcc",
            "xtensa-lx106-elf-g++",
            "arm-none-eabi-gcc",
            "arm-none-eabi-g++",
        ]

        try:
            # Parse build log to extract compile commands
            compile_commands = parse_build_log_to_compile_commands(
                build_log, compiler_names
            )
            if not compile_commands:
                print("❌ No compiler commands found in build log")
                return False

            # Create output directory
            self.compiledb_dir.mkdir(parents=True, exist_ok=True)

            # Convert to JSON format
            json_entries = []
            for cmd in compile_commands:
                json_entries.append(
                    {
                        "file": cmd.file,
                        "output": cmd.output,
                        "directory": cmd.directory,
                        "arguments": cmd.arguments,
                    }
                )

            # Write compile_commands.json
            with self.compile_commands_file.open("w") as f:
                json.dump(json_entries, f, indent=2)

            file_size = self.compile_commands_file.stat().st_size
            return True

        except Exception as e:
            print(f"❌ Error creating compile_commands.json: {e}")
            return False

    def get_correct_build_order(self):
        """
        Extract build order from compile_commands.json with build artifacts.

        Combines compile_commands.json (which preserves compilation order)
        with build artifact paths to create comprehensive build order data.

        Returns:
            dict: Build order data with sources, objects, and include paths
            None: If compile_commands.json doesn't exist or is invalid
        """
        if not self.compile_commands_file.exists():
            print(f"⚠ compile_commands_{self.env_name}.json not found")
            return None

        try:
            # Load compile database
            with self.compile_commands_file.open("r", encoding="utf-8") as f:
                compile_db = json.load(f)
        except Exception as e:
            print(f"✗ Error reading compile_commands.json: {e}")
            return None

        # Initialize data structures for build order extraction
        object_paths = []
        include_paths = set()

        # Process each compile command entry
        for i, entry in enumerate(compile_db, 1):
            source_file = entry.get("file", "")

            if source_file.endswith((".elf", ".bin", ".hex", ".map")):
                # not a source file, skip
                continue

            if not source_file.endswith(tuple(SRC_BUILD_EXT + SRC_HEADER_EXT)):
                # not a recognized source file, skip
                continue

            # Handle both 'arguments' and 'command' formats
            if "arguments" in entry:
                command = " ".join(entry["arguments"])
            elif "command" in entry:
                command = entry["command"]
            else:
                print(
                    f"⚠ Unsupported entry format in compile_commands.json (index {i})"
                )
                continue

            # Extract object file from -o flag
            obj_match = re.search(r"-o\s+(\S+\.o)", command)
            if obj_match:
                obj_file = obj_match.group(1)
                object_paths.append(
                    {
                        "order": i,
                        "source": source_file,
                        "object": obj_file,
                    }
                )

            # Extract include paths from -I flags
            include_matches = re.findall(r"-I\s*([^\s]+)", command)
            for inc_path in include_matches:
                inc_path = inc_path.strip("\"'")
                if Path(inc_path).exists():
                    include_paths.add(str(Path(inc_path)))

        return {"object_paths": object_paths, "include_paths": sorted(include_paths)}

    def collect_build_artifacts_paths(self):
        """
        Collect paths to build artifacts without copying them.

        Scans build directory for library (.a) and object (.o) files,
        collecting their paths for cache storage and later reuse.

        Returns:
            dict: Artifact paths organized by type with metadata
        """
        if not self.lib_build_dir.exists():
            print(f"⚠ Build directory not found: {self.lib_build_dir}")
            return {}

        library_paths = []
        object_paths = []

        # Walk through build directory to find artifacts
        for root, dirs, files in os.walk(self.lib_build_dir):
            root_path = Path(root)
            for file in files:
                if file.endswith(".a"):
                    file_path = root_path / file
                    library_paths.append(str(file_path))
                elif file.endswith(".o"):
                    file_path = root_path / file
                    object_paths.append(str(file_path))

        total_count = len(library_paths) + len(object_paths)

        return {
            "library_paths": library_paths,
            "object_paths": object_paths,
            "total_count": total_count,
        }

    def get_project_hash_with_details(self):
        """
        Calculate comprehensive project hash for cache invalidation.

        Computes hash based on all LDF-relevant files to detect changes
        that would require cache invalidation. Only includes files that
        can affect dependency resolution.

        Returns:
            dict: Hash details including file hashes and final combined hash
        """
        file_hashes = {}
        src_path = Path(self.src_dir)

        # Process all files in source directory
        for file_path in src_path.rglob("*"):
            # Skip directories and ignored directories
            if file_path.is_dir() or self._is_ignored_directory(file_path.parent):
                continue

            # Only process LDF-relevant file extensions
            if file_path.suffix in self.ALL_RELEVANT_EXTENSIONS:
                try:
                    rel_path = self._get_relative_path_from_project(file_path)

                    # Hash source files based on their include dependencies
                    if file_path.suffix in self.SOURCE_EXTENSIONS:
                        includes = self._extract_includes(file_path)
                        include_hash = hashlib.md5(
                            str(sorted(includes)).encode()
                        ).hexdigest()
                        file_hashes[rel_path] = include_hash
                    # Hash header files based on content
                    elif file_path.suffix in self.HEADER_EXTENSIONS:
                        file_content = file_path.read_bytes()
                        file_hash = hashlib.md5(file_content).hexdigest()
                        file_hashes[rel_path] = file_hash
                    # Hash config files based on content
                    elif file_path.suffix in self.CONFIG_EXTENSIONS:
                        file_content = file_path.read_bytes()
                        file_hash = hashlib.md5(file_content).hexdigest()
                        file_hashes[rel_path] = file_hash
                except (IOError, OSError) as e:
                    print(f"⚠ Could not hash {file_path}: {e}")
                    continue

        # Process platformio.ini files
        project_path = Path(self.project_dir)
        for ini_path in project_path.glob("platformio*.ini"):
            if ini_path.exists() and ini_path.is_file():
                try:
                    platformio_hash = self._hash_platformio_ini(ini_path)
                    if platformio_hash:
                        rel_ini_path = self._get_relative_path_from_project(ini_path)
                        file_hashes[rel_ini_path] = platformio_hash
                except (IOError, OSError) as e:
                    print(f"⚠ Could not hash {ini_path}: {e}")

        # Compute final combined hash
        combined_content = json.dumps(file_hashes, sort_keys=True)
        final_hash = hashlib.sha256(combined_content.encode()).hexdigest()

        return {
            "file_hashes": file_hashes,
            "final_hash": final_hash,
            "file_count": len(file_hashes),
        }

    def create_comprehensive_cache(self):
        """
        Create comprehensive cache data including all build information.

        Combines project hash, build order, and artifact information into
        a complete cache that can be used for subsequent builds.

        Returns:
            dict: Complete cache data with signature
            None: If cache creation failed
        """
        try:
            print("🔧 Creating comprehensive cache...")

            # Collect all cache components
            project_hash = self.get_project_hash_with_details()
            build_order = self.get_correct_build_order()
            artifacts = self.collect_build_artifacts_paths()

            if not build_order:
                print("⚠ No build order data available")
                return None

            # Create cache data structure
            cache_data = {
                "version": "1.0",
                "env_name": self.env_name,
                "timestamp": datetime.datetime.now().isoformat(),
                "project_hash": project_hash["final_hash"],
                "file_hashes": project_hash["file_hashes"],
                "build_order": build_order,
                "artifacts": artifacts,
            }

            # Add signature for integrity verification
            cache_data["signature"] = self.compute_signature(cache_data)
            return cache_data

        except Exception as e:
            print(f"❌ Error creating cache: {e}")
            return None

    def save_cache(self, cache_data):
        """
        Save cache data to file in Python format.

        Saves cache as executable Python code for easy loading and
        human-readable format for debugging.

        Args:
            cache_data: Dictionary containing cache data

        Returns:
            bool: True if cache was saved successfully
        """
        try:
            # Ensure cache directory exists
            self.cache_file.parent.mkdir(parents=True, exist_ok=True)

            # Write cache as Python code
            with self.cache_file.open("w") as f:
                f.write("# LDF Cache Data - Auto-generated\n")
                f.write("# Do not edit manually, this will break the Hash checksum\n\n")
                f.write("cache_data = ")
                f.write(pprint.pformat(cache_data, width=120, depth=None))
                f.write("\n")

            return True

        except Exception as e:
            print(f"❌ Error saving cache: {e}")
            return False

    def compute_signature(self, data):
        """
        Compute SHA256 signature for cache data validation.

        Creates tamper-evident signature to ensure cache integrity
        and detect corruption or manual modifications.

        Args:
            data: Dictionary to sign

        Returns:
            str: SHA256 signature hex string
        """
        # Create copy without signature field to avoid circular reference
        data_copy = data.copy()
        data_copy.pop("signature", None)

        # Create deterministic JSON representation
        data_str = json.dumps(data_copy, sort_keys=True)
        return hashlib.sha256(data_str.encode()).hexdigest()

    def load_cache(self):
        """
        Load cache data from file.

        Executes Python cache file to load cache data into memory
        for validation and application.

        Returns:
            dict: Cache data if loaded successfully
            None: If cache file doesn't exist or loading failed
        """
        if not self.cache_file.exists():
            return None

        try:
            # Execute Python cache file in isolated namespace
            cache_globals = {}
            with self.cache_file.open("r") as f:
                exec(f.read(), cache_globals)

            # Extract cache data from executed namespace
            cache_data = cache_globals.get("cache_data")
            if cache_data:
                return cache_data
            else:
                print("⚠ No cache_data found in cache file")
                return None

        except Exception as e:
            print(f"❌ Error loading cache: {e}")
            return None

    def validate_cache(self, cache_data):
        """
        Validate cache integrity and freshness.

        Performs comprehensive validation including signature verification
        and project hash comparison to ensure cache is valid and current.

        Args:
            cache_data: Cache data to validate

        Returns:
            bool: True if cache is valid and current
        """
        if not cache_data:
            return False

        try:
            # Verify signature integrity
            stored_signature = cache_data.get("signature")
            if not stored_signature:
                print("⚠ Cache missing signature")
                return False

            computed_signature = self.compute_signature(cache_data)
            if stored_signature != computed_signature:
                print("⚠ Cache signature mismatch")
                return False

            # Verify project hasn't changed
            current_hash = self.get_project_hash_with_details()
            if cache_data.get("project_hash") != current_hash["final_hash"]:
                print("⚠ Project files changed, cache invalid")
                return False

            print("✅ Cache validation successful")
            return True

        except Exception as e:
            print(f"⚠ Cache validation error: {e}")
            return False

    def apply_build_order_to_environment(self, build_order_data):
        """
        Apply correct build order to SCons environment.

        Set OBJECTS variable in correct order.

        Args:
            build_order_data: Dictionary containing build order information

        Returns:
            bool: True if build order was applied successfully
        """
        if not build_order_data:
            return False

        try:
            # Apply object file order
            object_paths = build_order_data.get("object_paths", [])
            if object_paths:
                # Use ALL cached object files
                object_file_paths = []
                for obj_entry in object_paths:
                    if isinstance(obj_entry, dict):
                        obj_path = obj_entry.get("object", obj_entry.get("path", ""))
                    else:
                        obj_path = str(obj_entry)

                    if obj_path and Path(obj_path).exists():
                        object_file_paths.append(obj_path)

                if object_file_paths:
                    self.env.Replace(OBJECTS=object_file_paths)
                else:
                    print("⚠ No valid object files found")

            return True

        except Exception as e:
            print(f"✗ Error applying build order: {e}")
            return False

    def apply_cache_to_scons_vars(self, cache_data):
        """
        Apply cache data to SCons variables.

        Uses PlatformIO's ParseFlagsExtended for robust flag processing
        and applies cached include paths and library paths to build environment.

        Args:
            cache_ Dictionary containing cached build data

        Returns:
            bool: True if cache variables were applied successfully
        """
        try:
            build_order = cache_data.get("build_order", {})

            # Apply include paths using PlatformIO's native flag processing
            if "include_paths" in build_order:
                include_flags = [f"-I{path}" for path in build_order["include_paths"]]
                parsed_flags = self.env.ParseFlagsExtended(include_flags)
                self.env.Append(**parsed_flags)

            # Apply library paths and convert to proper LIBS/LIBPATH format
            artifacts = cache_data.get("artifacts", {})
            if "library_paths" in artifacts:
                library_paths = artifacts["library_paths"]
                if library_paths:
                    lib_flags = []
                    lib_directories = set()

                    for lib_path in library_paths:
                        lib_path_obj = Path(lib_path)
                        filename = lib_path_obj.name

                        # Extrahiere Verzeichnis für LIBPATH
                        directory = str(lib_path_obj.parent)
                        if directory and directory != ".":
                            lib_directories.add(directory)

                        # Convert to -l Flag for LIBS
                        if filename.startswith("lib") and filename.endswith(".a"):
                            lib_name = filename[3:-2]  # Remove 'lib' und '.a'
                            lib_flags.append(f"-l{lib_name}")
                        elif filename.endswith(".a"):
                            lib_name = filename[:-2]  # Remove only '.a'
                            lib_flags.append(f"-l{lib_name}")
                        else:
                            lib_flags.append(f"-l{filename}")

                    if lib_directories:
                        current_libpath = self.env.get("LIBPATH", [])
                        if isinstance(current_libpath, str):
                            current_libpath = [current_libpath]

                        updated_libpath = list(current_libpath)
                        for path in lib_directories:
                            if path not in updated_libpath:
                                updated_libpath.append(path)

                        self.env["LIBPATH"] = updated_libpath

                    if lib_flags:
                        current_libs = self.env.get("LIBS", [])
                        if isinstance(current_libs, str):
                            current_libs = [current_libs]

                        updated_libs = list(current_libs) + lib_flags
                        self.env["LIBS"] = updated_libs

            return True

        except Exception as e:
            print(f"❌ Error applying cache to SCons vars: {e}")
            import traceback

            traceback.print_exc()
            return False

    def _get_relative_path_from_project(self, file_path):
        """
        Calculate relative path from project root with consistent path handling.

        Normalizes paths to use forward slashes for cross-platform compatibility.

        Args:
            file_path: File path to make relative

        Returns:
            str: Relative path from project root with forward slashes
        """
        try:
            file_path = Path(file_path).resolve()
            project_dir = Path(self.project_dir).resolve()
            rel_path = file_path.relative_to(project_dir)
            return str(rel_path).replace(os.sep, "/")
        except (ValueError, OSError):
            return str(Path(file_path)).replace(os.sep, "/")

    def _extract_includes(self, file_path):
        """
        Extract #include directives from source files.

        Parses source files to extract include dependencies for
        hash calculation and dependency tracking.

        Args:
            file_path: Source file to analyze

        Returns:
            set: Set of normalized include paths
        """
        includes = set()
        try:
            file_path = Path(file_path)
            with file_path.open("r", encoding="utf-8", errors="ignore") as f:
                for line_num, line in enumerate(f, 1):
                    line = line.strip()
                    if line.startswith("#include"):
                        include_match = re.search(r'#include\s*[<"]([^>"]+)[>"]', line)
                        if include_match:
                            include_path = include_match.group(1)
                            normalized_include = str(Path(include_path)).replace(
                                os.sep, "/"
                            )
                            includes.add(normalized_include)
        except (IOError, OSError, UnicodeDecodeError) as e:
            print(f"⚠ Could not read {file_path}: {e}")
        return includes

    def _hash_platformio_ini(self, ini_path=None):
        """
        Hash the complete platformio.ini file

        Creates hash of the entire platformio.ini content to detect any changes
        that might affect the build configuration.

        Args:
        ini_path: Path to ini file (defaults to self.platformio_ini)

        Returns:
            str: MD5 hash of the entire platformio.ini content
        """
        if ini_path is None:
            ini_path = self.platformio_ini
        if not ini_path.exists():
            return ""

        try:
            with ini_path.open("r", encoding="utf-8") as f:
                content = f.read()

            # Hash entire content as is
            return hashlib.md5(content.encode()).hexdigest()
        except (IOError, OSError) as e:
            print(f"⚠ Could not read {ini_path}: {e}")
            return ""

    def _is_ignored_directory(self, dir_path):
        """
        Check if a directory should be ignored during file scanning.

        Uses predefined ignore patterns optimized for ESP/Tasmota projects
        to skip irrelevant directories during cache creation.

        Args:
            dir_path: Directory path to check

        Returns:
            bool: True if directory should be ignored
        """
        if not dir_path:
            return False

        path_obj = Path(dir_path)

        # Check if directory name is in ignore list
        if path_obj.name in self.IGNORE_DIRS:
            return True

        # Check if any parent directory is in ignore list
        for part in path_obj.parts:
            if part in self.IGNORE_DIRS:
                return True
        return False


def execute_first_run_post_actions():
    """
    Execute post-build actions after successful first run.

    Creates cache data, generates compile_commands.json, validates LDF mode,
    for subsequent cached builds.

    Returns:
        bool: True if all post-actions completed successfully
    """
    print("🎯 First run completed successfully - executing post-build actions...")

    try:
        # Initialize optimizer for first run tasks
        optimizer = LDFCacheOptimizer(env)

        # Create compile_commands.json if needed
        if not compiledb_path.exists() or compiledb_path.stat().st_size == 0:
            success_compiledb = optimizer.create_compiledb_integrated()
            if not success_compiledb:
                print("❌ Failed to create compile_commands.json")
                return False
        else:
            print(f"✅ compile_commands.json already exists: {compiledb_path}")

        # Create cache if it doesn't exist
        if not cache_file.exists():
            cache_data = optimizer.create_comprehensive_cache()

            if not cache_data:
                print("❌ Failed to create cache data")
                return False

            # Save cache to file
            success_save = optimizer.save_cache(cache_data)
            if not success_save:
                print("❌ Failed to save cache")
                return False

            print(f"✅ LDF cache created: {cache_file}")
        else:
            print(f"✅ LDF cache already exists: {cache_file}")

        # Check current LDF mode
        current_ldf_mode = optimizer.env.GetProjectOption("lib_ldf_mode", "chain")

        if optimizer.validate_ldf_mode_compatibility():
            print("🎉 First run post-build actions completed successfully!")
            print("🚀 Next build will be using cached dependencies")
        else:
            print(f"⚠ lib_ldf_mode '{current_ldf_mode}' not supported for caching")
            print("💡 Supported modes: chain, off")
            return False

        return True

    except Exception as e:
        print(f"❌ Error in first run post-build actions: {e}")
        import traceback

        traceback.print_exc()
        return False


def terminal_size():
    """
    Get terminal width and lines from stty size command.
    VSC does not provide an easy way to get terminal width.
    Returns:
        tuple: (columns, lines) if available, otherwise (80, 16)
    """
    try:
        result = subprocess.run(
            ["stty", "size"], capture_output=True, text=True, timeout=2
        )
        if result.returncode == 0:
            parts = result.stdout.strip().split()
            if len(parts) >= 2 and parts[0].isdigit() and parts[1].isdigit():
                lines = int(parts[0])
                columns = int(parts[1])
                return columns, lines
    except (subprocess.TimeoutExpired, subprocess.SubprocessError, FileNotFoundError):
        pass
    return 80, 16


# FIRST RUN LOGIC - Execute verbose build and create cache
if should_trigger_verbose_build() and not github_actions and not flag_custom_sdkconfig:
    print("🔄 Starting LDF Cache Optimizer...")
    print(f"🔄 First run needed - starting verbose build for {env_name}...")
    print("📋 Reasons:")

    # Report reasons for first run
    if not compiledb_path.exists():
        print("  - compile_commands.json missing")
    elif compiledb_path.stat().st_size == 0:
        print("  - compile_commands.json is empty")

    if not is_build_environment_ready():
        print("  - Build environment incomplete")

    # Setup environment for verbose build
    env_vars = os.environ.copy()
    env_vars["PLATFORMIO_SETTING_FORCE_VERBOSE"] = "true"
    env_vars["_PIO_RECURSIVE_CALL"] = "true"

    terminal_columns, terminal_lines = terminal_size()
    terminal_width = terminal_columns - 3  # Adjust for emoji and padding

    # Handle recursive call return codes
    if os.environ.get("_PIO_REC_CALL_RETURN_CODE") is not None:
        sys.exit(int(os.environ.get("_PIO_REC_CALL_RETURN_CODE")))

    # Execute verbose build with output capture
    with open(logfile_path, "w") as logfile:
        process = subprocess.Popen(
            ["pio", "run", "-e", env_name, "--disable-auto-clean"],
            env=env_vars,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            bufsize=1,
        )
        print(f"🔄 Running verbose build... full log in {logfile_path}")
        for line in process.stdout:
            print("🔄 " + line[:terminal_width].splitlines()[0], end="\r")
            sys.stdout.write("\033[F")
            logfile.write(line)
            logfile.flush()
        sys.stdout.write("\x1b[2K")
        sys.stdout.write("\033[F")
        logfile.flush()
        print("\n✅ Build process completed, waiting for process to finish...")
        process.wait()

    if process.returncode == 0:
        post_actions_success = execute_first_run_post_actions()
        if post_actions_success:
            print("✅ All first run actions completed successfully")
        else:
            print("⚠ Some first run actions failed")
    else:
        print(f"❌ First run failed with return code: {process.returncode}")

    sys.exit(process.returncode)

# SECOND RUN LOGIC
try:
    if not should_trigger_verbose_build() and is_build_environment_ready():
        optimizer = LDFCacheOptimizer(env)
except Exception as e:
    print(f"❌ Error initializing LDF Cache Optimizer: {e}")
    import traceback

    traceback.print_exc()
