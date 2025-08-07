#!/usr/bin/env python3
"""
Optimized compiler wrapper for FastLED build optimization.

This module extends the cached_compiler functionality to capture build metadata
during the first compilation and enable optimized subsequent builds.
"""

import json
import os
import shutil
import subprocess
import time
from pathlib import Path
from typing import Dict, List, Optional

from ci.util.boards import Board
from ci.util.build_optimizer import CompilationCommand


class OptimizedCompilerWrapper:
    """Enhanced compiler wrapper that captures build metadata during first build."""

    def __init__(
        self,
        build_phase: str,
        json_capture_dir: Path,
        board: Board,
        verbose: bool = False,
    ):
        self.build_phase = build_phase  # "capture" or "optimized"
        self.json_capture_dir = json_capture_dir
        self.board = board
        self.verbose = verbose
        self.command_counter = 0

        # Ensure JSON capture directory exists for capture phase
        if self.build_phase == "capture":
            self.json_capture_dir.mkdir(parents=True, exist_ok=True)

    def execute_compile(self, compiler: str, args: List[str], working_dir: str) -> int:
        """Execute compilation with metadata capture or optimization."""
        if self.build_phase == "capture":
            return self._capture_and_execute(compiler, args, working_dir)
        else:
            # For now, fall back to normal execution
            # TODO: Implement optimized execution in Phase 4
            return self._execute_normal(compiler, args, working_dir)

    def _capture_and_execute(
        self, compiler: str, args: List[str], working_dir: str
    ) -> int:
        """Capture compilation metadata and execute normally."""
        try:
            # Parse command arguments
            cmd_metadata = self._parse_compilation_command(compiler, args, working_dir)

            # Save to JSON file
            json_file = self.json_capture_dir / f"cmd_{self.command_counter:04d}.json"
            with open(json_file, "w") as f:
                json.dump(cmd_metadata.__dict__, f, indent=2)

            if self.verbose:
                print(f"Captured command metadata: {json_file}")

            self.command_counter += 1

            # Execute normal compilation
            return self._execute_normal(compiler, args, working_dir)

        except Exception as e:
            if self.verbose:
                print(f"Warning: Failed to capture metadata: {e}")
            # Continue with normal execution even if capture fails
            return self._execute_normal(compiler, args, working_dir)

    def _execute_normal(self, compiler: str, args: List[str], working_dir: str) -> int:
        """Execute normal compilation without optimization."""
        command = [compiler] + args

        if self.verbose:
            print(f"Executing: {' '.join(command)}")

        try:
            result = subprocess.run(
                command,
                cwd=working_dir,
                capture_output=False,  # Let output go to stdout/stderr
                text=True,
            )
            return result.returncode
        except FileNotFoundError:
            print(f"Error: Compiler not found: {compiler}")
            return 127
        except Exception as e:
            print(f"Error executing command: {e}")
            return 1

    def _parse_compilation_command(
        self, compiler: str, args: List[str], working_dir: str
    ) -> CompilationCommand:
        """Parse compilation command to extract metadata."""

        # Determine command type
        command_type = self._determine_command_type(compiler, args)

        # Extract source and output files
        source_file, output_file = self._extract_files(args, command_type)

        # Extract include paths and defines
        include_paths = self._extract_include_paths(args)
        defines = self._extract_defines(args)

        return CompilationCommand(
            command_type=command_type,
            compiler=compiler,
            source_file=source_file,
            output_file=output_file,
            args=args.copy(),
            include_paths=include_paths,
            defines=defines,
            timestamp=time.time(),
            working_dir=working_dir,
        )

    def _determine_command_type(self, compiler: str, args: List[str]) -> str:
        """Determine the type of compilation command."""

        # Check for archiver tools
        if "ar" in compiler.lower() or "ranlib" in compiler.lower():
            return "archive"

        # Check for linker (usually last step with -o and .elf/.exe output)
        if "-o" in args:
            o_index = args.index("-o")
            if o_index + 1 < len(args):
                output_file = args[o_index + 1]
                if any(ext in output_file.lower() for ext in [".elf", ".exe", ".bin"]):
                    return "link"

        # Check for compilation (has -c flag)
        if "-c" in args:
            return "compile"

        # Default to compile
        return "compile"

    def _extract_files(self, args: List[str], command_type: str) -> tuple[str, str]:
        """Extract source and output file paths."""
        source_file = ""
        output_file = ""

        # Find output file (-o flag)
        if "-o" in args:
            try:
                o_index = args.index("-o")
                if o_index + 1 < len(args):
                    output_file = args[o_index + 1]
            except (ValueError, IndexError):
                pass

        # Find source file (usually last non-flag argument or after specific patterns)
        if command_type == "compile":
            # For compilation, find source files (.c, .cpp, .S, etc.)
            for arg in reversed(args):
                if not arg.startswith("-") and any(
                    ext in arg.lower() for ext in [".c", ".cpp", ".cc", ".cxx", ".s"]
                ):
                    source_file = arg
                    break
        elif command_type == "archive":
            # For archiving, source files are object files
            source_file = "multiple_objects"  # Archives combine multiple files
        elif command_type == "link":
            # For linking, source files are object files and libraries
            source_file = "multiple_objects_and_libs"

        return source_file, output_file

    def _extract_include_paths(self, args: List[str]) -> List[str]:
        """Extract include paths from compilation arguments."""
        includes: List[str] = []

        i = 0
        while i < len(args):
            arg = args[i]

            if arg == "-I" and i + 1 < len(args):
                includes.append(f"-I{args[i + 1]}")
                i += 1
            elif arg.startswith("-I"):
                includes.append(arg)

            i += 1

        return includes

    def _extract_defines(self, args: List[str]) -> List[str]:
        """Extract preprocessor defines from compilation arguments."""
        defines: List[str] = []

        i = 0
        while i < len(args):
            arg = args[i]

            if arg == "-D" and i + 1 < len(args):
                defines.append(f"-D{args[i + 1]}")
                i += 1
            elif arg.startswith("-D"):
                defines.append(arg)

            i += 1

        return defines


def create_optimized_compiler_script(
    compiler_name: str,
    build_phase: str,
    json_capture_dir: Path,
    board: Board,
    output_dir: Path,
    verbose: bool = False,
) -> Path:
    """Create a Python script that acts as an optimized compiler wrapper."""
    
    script_name = f"optimized_{compiler_name.replace('-', '_')}.py"
    script_path = output_dir / script_name
    
    # Calculate the absolute path to the project root at generation time
    # This avoids any relative path calculation issues at runtime
    from pathlib import Path as CurrentPath
    project_root_abs = CurrentPath(__file__).parent.parent.parent.resolve()
    
    script_content = f'''#!/usr/bin/env python3
"""
Optimized compiler wrapper for {compiler_name}
Generated automatically for FastLED build optimization.

This script captures build metadata during first compilation and
enables optimized subsequent builds.
"""

import os
import sys
from pathlib import Path

# Add project root to Python path (absolute path set at generation time)
project_root = Path(r"{project_root_abs}")
if str(project_root) not in sys.path:
    sys.path.insert(0, str(project_root))

# Import optimization modules
try:
    from ci.util.optimized_compiler import OptimizedCompilerWrapper
    from ci.util.boards import Board, create_board
    from ci.util.cached_compiler import find_toolchain_compiler, get_platform_packages_paths
    
    # Configuration (set at generation time)
    BUILD_PHASE = "{build_phase}"
    JSON_CAPTURE_DIR = Path(r"{json_capture_dir}")
    BOARD_NAME = "{board.board_name}"
    VERBOSE = {verbose}
    
    def main() -> int:
        """Main entry point for optimized compiler."""
        try:
            # Create board object
            board = create_board(BOARD_NAME)
            
            # Find real compiler
            compiler_name = "{compiler_name}"
            platform_packages = get_platform_packages_paths()
            real_compiler = find_toolchain_compiler(compiler_name, platform_packages)
            
            if not real_compiler:
                print(f"ERROR: Could not find real compiler: {{compiler_name}}", file=sys.stderr)
                return 1
            
            # Create optimization wrapper
            wrapper = OptimizedCompilerWrapper(
                build_phase=BUILD_PHASE,
                json_capture_dir=JSON_CAPTURE_DIR,
                board=board,
                verbose=VERBOSE
            )
            
            # Execute with optimization
            working_dir = os.getcwd()
            return wrapper.execute_compile(real_compiler, sys.argv[1:], working_dir)
            
        except Exception as e:
            print(f"ERROR in optimized compiler {{compiler_name}}: {{e}}", file=sys.stderr)
            if VERBOSE:
                import traceback
                traceback.print_exc()
            return 1

    if __name__ == "__main__":
        sys.exit(main())
    
except ImportError as e:
    # Fallback to direct execution if optimization modules not available
    import subprocess
    
    print(f"Warning: Optimization not available, falling back to direct execution: {{e}}", file=sys.stderr)
    
    # Execute compiler directly
    command = ["{compiler_name}"] + sys.argv[1:]
    result = subprocess.run(command)
    sys.exit(result.returncode)
'''

    # Write the script
    script_path.write_text(script_content, encoding="utf-8")

    # Make executable on Unix systems
    if os.name != "nt":
        script_path.chmod(script_path.stat().st_mode | 0o755)

    return script_path


def create_optimized_toolchain(
    toolchain_info: Dict[str, str],
    build_phase: str,
    json_capture_dir: Path,
    board: Board,
    output_dir: Path,
    verbose: bool = False,
) -> Dict[str, str]:
    """Create a complete set of optimized compiler scripts for a toolchain."""

    output_dir.mkdir(parents=True, exist_ok=True)
    optimized_tools: Dict[str, str] = {}

    # Standard compiler tools that should be wrapped with optimization
    cacheable_tools = {"CC", "CXX", "gcc", "g++", "clang", "clang++"}

    for tool_name, tool_path_or_name in toolchain_info.items():
        # Only wrap cacheable tools
        tool_base = Path(tool_path_or_name).name if tool_path_or_name else tool_name
        if not any(cacheable in tool_base for cacheable in cacheable_tools):
            if verbose:
                print(f"Skipping non-cacheable tool: {tool_name} = {tool_path_or_name}")
            continue

        # Create optimized compiler script
        optimized_script = create_optimized_compiler_script(
            compiler_name=tool_path_or_name,
            build_phase=build_phase,
            json_capture_dir=json_capture_dir,
            board=board,
            output_dir=output_dir,
            verbose=verbose,
        )

        # Use Python to execute the script
        optimized_tools[tool_name] = f"python3 {optimized_script}"

        if verbose:
            print(f"Created optimized {tool_name}: {optimized_tools[tool_name]}")

    return optimized_tools
