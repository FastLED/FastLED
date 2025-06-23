#!/usr/bin/env -S uv run --script

# /// script
# dependencies = [
#   "mcp>=1.0.0",
# ]
# ///

"""
FastLED MCP Server - Provides tools for working with the FastLED project

To use this server, make sure you have the MCP library installed:
pip install mcp

Or use with uv:
uv add mcp
uv run mcp_server.py
"""

import asyncio
import json
import subprocess
import sys
import re
from pathlib import Path
from typing import Any, Dict, List, Optional

try:
    from mcp.server import Server
    from mcp.server.stdio import stdio_server 
    from mcp.types import (
        CallToolResult,
        TextContent,
        Tool,
    )
    MCP_AVAILABLE = True
except ImportError:
    print("Error: MCP library not found.")
    print("Please install it with: pip install mcp")
    print("Or with uv: uv add mcp")
    sys.exit(1)

# Initialize the MCP server
server = Server("fastled-mcp-server")

@server.list_tools()
async def list_tools() -> List[Tool]:
    """List available tools for the FastLED project."""
    return [
        Tool(
            name="run_tests",
            description="Run FastLED tests with various options",
            inputSchema={
                "type": "object",
                "properties": {
                    "test_type": {
                        "type": "string",
                        "enum": ["all", "cpp", "specific"],
                        "description": "Type of tests to run",
                        "default": "all"
                    },
                    "specific_test": {
                        "type": "string",
                        "description": "Name of specific C++ test to run (without 'test_' prefix, e.g. 'algorithm' for test_algorithm.cpp)"
                    },
                    "test_case": {
                        "type": "string", 
                        "description": "Specific TEST_CASE name to run within a test file (requires doctest filtering)"
                    },
                    "use_clang": {
                        "type": "boolean",
                        "description": "Use Clang compiler instead of default",
                        "default": False
                    },
                    "clean": {
                        "type": "boolean",
                        "description": "Clean build before compiling",
                        "default": False
                    },
                    "verbose": {
                        "type": "boolean",
                        "description": "Enable verbose output showing all test details",
                        "default": False
                    }
                },
                "required": ["test_type"]
            }
        ),
        Tool(
            name="list_test_cases",
            description="List TEST_CASEs available in FastLED test files",
            inputSchema={
                "type": "object",
                "properties": {
                    "test_file": {
                        "type": "string",
                        "description": "Specific test file to analyze (without 'test_' prefix and '.cpp' extension, e.g. 'algorithm')"
                    },
                    "search_pattern": {
                        "type": "string",
                        "description": "Search pattern to filter TEST_CASE names"
                    }
                }
            }
        ),
        Tool(
            name="compile_examples",
            description="Compile FastLED examples for different platforms",
            inputSchema={
                "type": "object",
                "properties": {
                    "platform": {
                        "type": "string",
                        "description": "Target platform (e.g., 'uno', 'esp32', 'teensy')",
                        "default": "uno"
                    },
                    "examples": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "List of example names to compile",
                        "default": ["Blink"]
                    },
                    "interactive": {
                        "type": "boolean",
                        "description": "Run in interactive mode",
                        "default": False
                    }
                }
            }
        ),
        Tool(
            name="code_fingerprint",
            description="Calculate a fingerprint of the codebase to detect changes",
            inputSchema={
                "type": "object",
                "properties": {
                    "directory": {
                        "type": "string",
                        "description": "Directory to fingerprint (relative to project root)",
                        "default": "src"
                    },
                    "patterns": {
                        "type": "string",
                        "description": "Glob patterns for files to include",
                        "default": "**/*.h,**/*.cpp,**/*.hpp"
                    }
                }
            }
        ),
        Tool(
            name="lint_code",
            description="Run code formatting and linting tools",
            inputSchema={
                "type": "object",
                "properties": {
                    "tool": {
                        "type": "string",
                        "enum": ["ruff", "clang-format", "all"],
                        "description": "Linting tool to run",
                        "default": "all"
                    },
                    "fix": {
                        "type": "boolean",
                        "description": "Automatically fix issues where possible",
                        "default": False
                    }
                }
            }
        ),
        Tool(
            name="list_examples",
            description="List available FastLED examples",
            inputSchema={
                "type": "object",
                "properties": {
                    "category": {
                        "type": "string",
                        "description": "Filter by category (optional)"
                    }
                }
            }
        ),
        Tool(
            name="project_info",
            description="Get information about the FastLED project structure and status",
            inputSchema={
                "type": "object",
                "properties": {
                    "include_git_status": {
                        "type": "boolean",
                        "description": "Include git status information",
                        "default": True
                    }
                }
            }
        ),
        Tool(
            name="run_specific_command",
            description="Run a specific command in the project context",
            inputSchema={
                "type": "object",
                "properties": {
                    "command": {
                        "type": "string",
                        "description": "Command to run"
                    },
                    "working_directory": {
                        "type": "string",
                        "description": "Working directory (relative to project root)",
                        "default": "."
                    }
                },
                "required": ["command"]
            }
        ),
        Tool(
            name="test_instructions",
            description="Get detailed instructions on how to run TEST_CASEs in FastLED",
            inputSchema={
                "type": "object",
                "properties": {}
            }
        )
    ]

@server.call_tool()
async def call_tool(name: str, arguments: Dict[str, Any]) -> CallToolResult:
    """Handle tool calls."""
    
    project_root = Path(__file__).parent
    
    try:
        if name == "run_tests":
            return await run_tests(arguments, project_root)
        elif name == "list_test_cases":
            return await list_test_cases(arguments, project_root)
        elif name == "compile_examples":
            return await compile_examples(arguments, project_root)
        elif name == "code_fingerprint":
            return await code_fingerprint(arguments, project_root)
        elif name == "lint_code":
            return await lint_code(arguments, project_root)
        elif name == "list_examples":
            return await list_examples(arguments, project_root)
        elif name == "project_info":
            return await project_info(arguments, project_root)
        elif name == "run_specific_command":
            return await run_specific_command(arguments, project_root)
        elif name == "test_instructions":
            return await test_instructions(arguments, project_root)
        else:
            return CallToolResult(
                content=[TextContent(type="text", text=f"Unknown tool: {name}")],
                isError=True
            )
    except Exception as e:
        return CallToolResult(
            content=[TextContent(type="text", text=f"Error executing {name}: {str(e)}")],
            isError=True
        )

async def run_tests(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Run FastLED tests."""
    test_type = arguments.get("test_type", "all")
    specific_test = arguments.get("specific_test")
    test_case = arguments.get("test_case")
    use_clang = arguments.get("use_clang", False)
    clean = arguments.get("clean", False)
    verbose = arguments.get("verbose", False)
    
    cmd = ["uv", "run", "test.py"]
    
    if test_type == "cpp":
        cmd.append("--cpp")
    
    if specific_test and test_type == "specific":
        cmd.extend(["--cpp", specific_test])
    
    if use_clang:
        cmd.append("--clang")
    
    if clean:
        cmd.append("--clean")
        
    if verbose:
        cmd.append("--verbose")
    
    # For individual TEST_CASE execution, we need to use a different approach
    if test_case and specific_test:
        # Run the specific test executable with doctest filtering
        test_executable = f"tests/.build/bin/test_{specific_test}"
        cmd = [test_executable, f"--test-case={test_case}"]
        if verbose:
            cmd.append("--verbose")
    
    result = await run_command(cmd, project_root)
    
    # Add helpful context about what was run
    context = f"Command executed: {' '.join(cmd)}\n"
    if test_case:
        context += f"Running specific TEST_CASE: {test_case} from test_{specific_test}\n"
    context += "\n"
    
    return CallToolResult(
        content=[TextContent(type="text", text=context + result)]
    )

async def list_test_cases(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """List TEST_CASEs in FastLED test files."""
    test_file = arguments.get("test_file")
    search_pattern = arguments.get("search_pattern", "")
    
    tests_dir = project_root / "tests"
    
    if not tests_dir.exists():
        return CallToolResult(
            content=[TextContent(type="text", text="Tests directory not found")],
            isError=True
        )
    
    test_cases = {}
    
    if test_file:
        # Analyze specific test file
        test_path = tests_dir / f"test_{test_file}.cpp"
        if not test_path.exists():
            return CallToolResult(
                content=[TextContent(type="text", text=f"Test file not found: test_{test_file}.cpp")],
                isError=True
            )
        test_cases[test_file] = extract_test_cases(test_path, search_pattern)
    else:
        # Analyze all test files
        for test_path in tests_dir.glob("test_*.cpp"):
            test_name = test_path.stem[5:]  # Remove "test_" prefix
            cases = extract_test_cases(test_path, search_pattern)
            if cases:  # Only include files with test cases
                test_cases[test_name] = cases
    
    # Format output
    if not test_cases:
        return CallToolResult(
            content=[TextContent(type="text", text="No TEST_CASEs found matching criteria")]
        )
    
    result_text = "FastLED TEST_CASEs:\n"
    result_text += "=" * 50 + "\n\n"
    
    total_cases = 0
    for test_name, cases in sorted(test_cases.items()):
        result_text += f"📁 {test_name} ({len(cases)} TEST_CASEs):\n"
        for i, case in enumerate(cases, 1):
            result_text += f"   {i:2d}. {case}\n"
        result_text += "\n"
        total_cases += len(cases)
    
    result_text += f"Total: {total_cases} TEST_CASEs across {len(test_cases)} files\n\n"
    
    # Add usage instructions
    result_text += "Usage Examples:\n"
    result_text += "• Run specific test file: uv run test.py --cpp algorithm\n"
    result_text += "• Run with verbose output: uv run test.py --cpp algorithm --verbose\n"
    if test_cases:
        first_test = list(test_cases.keys())[0]
        if test_cases[first_test]:
            first_case = test_cases[first_test][0]
            result_text += f"• Run specific TEST_CASE: ./tests/.build/bin/test_{first_test} --test-case='{first_case}'\n"
    
    return CallToolResult(
        content=[TextContent(type="text", text=result_text)]
    )

def extract_test_cases(file_path: Path, search_pattern: str = "") -> List[str]:
    """Extract TEST_CASE names from a test file."""
    test_cases = []
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Find TEST_CASE macros using regex
        pattern = r'TEST_CASE\s*\(\s*"([^"]+)"'
        matches = re.findall(pattern, content)
        
        for match in matches:
            if not search_pattern or search_pattern.lower() in match.lower():
                test_cases.append(match)
    
    except Exception as e:
        # Silently skip files that can't be read
        pass
    
    return test_cases

async def test_instructions(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Provide detailed instructions on running TEST_CASEs."""
    
    instructions = """
# FastLED TEST_CASE Execution Guide

## Overview
FastLED uses the **doctest** framework for C++ unit testing. Tests are organized in files named `test_*.cpp` in the `tests/` directory. Each file can contain multiple `TEST_CASE` macros.

## Test Structure
- **Test Files**: Located in `tests/test_*.cpp` (e.g., `test_algorithm.cpp`, `test_easing.cpp`)
- **TEST_CASEs**: Individual test functions defined with `TEST_CASE("name")` macro
- **SUBCASEs**: Nested test sections within TEST_CASEs using `SUBCASE("name")`

## Running Tests

### 1. Run All Tests
```bash
uv run test.py
# or using the user rule:
bash test
```

### 2. Run Only C++ Tests
```bash
uv run test.py --cpp
```

### 3. Run Specific Test File
```bash
uv run test.py --cpp <test_name>
```
Example: `uv run test.py --cpp algorithm` (runs `test_algorithm.cpp`)

### 4. Run with Verbose Output
```bash
uv run test.py --cpp <test_name> --verbose
```

### 5. Run Specific TEST_CASE
First compile the tests, then run the executable with doctest filters:
```bash
# Compile tests first
uv run test.py --cpp <test_name>

# Run specific TEST_CASE
./tests/.build/bin/test_<test_name> --test-case="TEST_CASE_NAME"
```

### 6. Using MCP Server Tools
```bash
# Start MCP server
uv run mcp_server.py

# Available MCP tools:
# - run_tests: Run tests with various options
# - list_test_cases: List available TEST_CASEs
# - test_instructions: Show this guide
```

## Example Workflows

### Debug a Specific Algorithm Test
```bash
# List available TEST_CASEs in algorithm tests
# (use MCP list_test_cases tool with test_file: "algorithm")

# Run specific TEST_CASE with verbose output
uv run test.py --cpp algorithm --verbose

```

### Test Development Workflow
```bash
# Clean build and run specific test
uv run test.py --cpp easing --clean --verbose

# Check for specific TEST_CASE patterns
# (use MCP list_test_cases tool with search_pattern)

# Run failed tests only
cd tests/.build && ctest --rerun-failed
```

## Doctest Command Line Options
When running test executables directly, you can use doctest options:
- `--test-case=<name>`: Run specific TEST_CASE
- `--test-case-exclude=<name>`: Exclude specific TEST_CASE
- `--subcase=<name>`: Run specific SUBCASE
- `--list-test-cases`: List all TEST_CASEs in the executable
- `--verbose`: Show detailed output
- `--success`: Show successful assertions too

## Common Test File Names
- `test_algorithm.cpp`: Algorithm utilities
- `test_easing.cpp`: Easing functions
- `test_hsv16.cpp`: HSV color space tests
- `test_math.cpp`: Mathematical functions
- `test_vector.cpp`: Vector container tests
- `test_fx.cpp`: Effects framework tests

## Tips
1. **Use `--verbose`** to see detailed test output and assertions
2. **Use `--clean`** when testing after code changes
3. **List TEST_CASEs first** to see what's available before running
4. **Individual TEST_CASE execution** is useful for debugging specific functionality
5. **Check test output carefully** - doctest provides detailed failure information

## Environment Setup
- Have UV in your environment.
- uv run tests.py, don't try and run the test manually, go through this entry point.
"""
    
    return CallToolResult(
        content=[TextContent(type="text", text=instructions.strip())]
    )

async def compile_examples(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Compile FastLED examples."""
    platform = arguments.get("platform", "uno")
    examples = arguments.get("examples", ["Blink"])
    interactive = arguments.get("interactive", False)
    
    cmd = ["uv", "run", "ci/ci-compile.py", platform]
    
    if examples:
        cmd.extend(["--examples"] + examples)
    
    if not interactive:
        cmd.append("--no-interactive")
    
    result = await run_command(cmd, project_root)
    return CallToolResult(
        content=[TextContent(type="text", text=result)]
    )

async def code_fingerprint(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Calculate code fingerprint."""
    # Import the fingerprint function from test.py
    sys.path.insert(0, str(project_root))
    from test import calculate_fingerprint
    
    directory = arguments.get("directory", "src")
    target_dir = project_root / directory
    
    fingerprint_data = calculate_fingerprint(target_dir)
    
    result_text = f"Code fingerprint for {directory}:\n"
    result_text += f"Hash: {fingerprint_data.get('hash', 'N/A')}\n"
    result_text += f"Elapsed time: {fingerprint_data.get('elapsed_seconds', 'N/A')}s\n"
    
    if "status" in fingerprint_data:
        result_text += f"Status: {fingerprint_data['status']}\n"
    
    return CallToolResult(
        content=[TextContent(type="text", text=result_text)]
    )

async def lint_code(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Run code linting tools."""
    tool = arguments.get("tool", "all")
    fix = arguments.get("fix", False)
    
    results = []
    
    if tool in ["ruff", "all"]:
        cmd = ["uv", "run", "ruff", "check"]
        if fix:
            cmd.append("--fix")
        result = await run_command(cmd, project_root)
        results.append(f"Ruff results:\n{result}")
    
    if tool in ["clang-format", "all"]:
        # Check if there's a lint script
        lint_script = project_root / "lint"
        if lint_script.exists():
            result = await run_command(["./lint"], project_root)
            results.append(f"Lint script results:\n{result}")
    
    return CallToolResult(
        content=[TextContent(type="text", text="\n\n".join(results))]
    )

async def list_examples(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """List available FastLED examples."""
    examples_dir = project_root / "examples"
    
    if not examples_dir.exists():
        return CallToolResult(
            content=[TextContent(type="text", text="Examples directory not found")],
            isError=True
        )
    
    examples = []
    for item in examples_dir.iterdir():
        if item.is_dir() and not item.name.startswith('.'):
            # Check if it has a .ino file
            ino_files = list(item.glob("*.ino"))
            if ino_files:
                examples.append(item.name)
    
    examples.sort()
    
    result_text = f"Available FastLED examples ({len(examples)} total):\n\n"
    result_text += "\n".join(f"- {example}" for example in examples)
    
    return CallToolResult(
        content=[TextContent(type="text", text=result_text)]
    )

async def project_info(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Get project information."""
    include_git = arguments.get("include_git_status", True)
    
    info = []
    
    # Basic project info
    info.append("FastLED Project Information")
    info.append("=" * 30)
    
    # Check if key files exist
    key_files = ["library.json", "library.properties", "platformio.ini", "pyproject.toml"]
    for file in key_files:
        file_path = project_root / file
        status = "✓" if file_path.exists() else "✗"
        info.append(f"{status} {file}")
    
    # Count examples
    examples_dir = project_root / "examples"
    if examples_dir.exists():
        example_count = len([d for d in examples_dir.iterdir() if d.is_dir() and not d.name.startswith('.')])
        info.append(f"📁 {example_count} examples available")
    
    # Git status
    if include_git:
        try:
            git_result = await run_command(["git", "status", "--porcelain"], project_root)
            if git_result.strip():
                info.append(f"\n🔄 Git status: {len(git_result.strip().split())} files modified")
            else:
                info.append("\n✅ Git status: Working tree clean")
        except Exception:
            info.append("\n❓ Git status: Unable to determine")
    
    return CallToolResult(
        content=[TextContent(type="text", text="\n".join(info))]
    )

async def run_specific_command(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Run a specific command."""
    command = arguments["command"]
    working_directory = arguments.get("working_directory", ".")
    
    work_dir = project_root / working_directory
    
    result = await run_command(command.split(), work_dir)
    return CallToolResult(
        content=[TextContent(type="text", text=f"Command: {command}\nOutput:\n{result}")]
    )

async def run_command(cmd: List[str], cwd: Path) -> str:
    """Run a shell command and return its output."""
    try:
        process = await asyncio.create_subprocess_exec(
            *cmd,
            cwd=cwd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            text=True
        )
        
        stdout, _ = await process.communicate()
        
        if process.returncode != 0:
            return f"Command failed with exit code {process.returncode}:\n{stdout}"
        
        return stdout
    except Exception as e:
        return f"Error running command: {str(e)}"

async def main():
    """Main entry point for the MCP server."""
    try:
        async with stdio_server() as (read_stream, write_stream):
            await server.run(
                read_stream,
                write_stream,
                server.create_initialization_options()
            )
    except Exception as e:
        print(f"Error running MCP server: {e}")
        print("Make sure the MCP library is properly installed.")
        print("Try: pip install mcp")
        sys.exit(1)

if __name__ == "__main__":
    if not MCP_AVAILABLE:
        print("MCP library is required but not installed.")
        print("Install with: pip install mcp")
        sys.exit(1)
    
    asyncio.run(main()) 
