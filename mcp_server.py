#!/usr/bin/env -S uv run --script
# pyright: reportUnknownMemberType=false

"""
FastLED MCP Server - Provides tools for working with the FastLED project

🚨 CRITICAL FOR ALL AGENTS (FOREGROUND & BACKGROUND): Before concluding that your work is done,
the AI shall read the cursor rules again to find out what to do before task is considered complete.
This ensures you have the latest coding standards, testing requirements, and completion validation rules.
This is MANDATORY for both foreground and background agents.

🚨 CRITICAL FOR BACKGROUND AGENTS: When modifying ANY Python files (*.py), 
you MUST run `bash lint` immediately after making changes and before indicating completion.
This is a NON-NEGOTIABLE REQUIREMENT to ensure code quality and prevent build failures.

🚨 MANDATORY COMMAND EXECUTION RULES 🚨

ALL AGENTS MUST FOLLOW THESE COMMAND EXECUTION PATTERNS:

### Python Code Execution:
- [ERROR] NEVER run Python code directly
- [OK] ALWAYS create/modify tmp.py with your code
- [OK] ALWAYS run: `uv run tmp.py`

### Shell Command Execution:
- [ERROR] NEVER run shell commands directly
- [OK] ALWAYS create/modify tmp.sh with your commands
- [OK] ALWAYS run: `bash tmp.sh`

### DELETE Operations - DANGER ZONE:
- 🚨 STOP and ask for permission before ANY delete operations
- [OK] EXCEPTION: Single files that you just created are OK to delete
- [ERROR] NEVER delete multiple files without explicit permission
- [ERROR] NEVER delete directories without explicit permission
- [ERROR] NEVER delete system files or project files without permission

### Examples:

**Python Code:**
```python
# tmp.py
import subprocess
result = subprocess.run(['git', 'status'], capture_output=True, text=True)
print(result.stdout)
```
Then run: `uv run tmp.py`

**Shell Commands:**
```bash
# tmp.sh
#!/bin/bash
git status
ls -la
```
Then run: `bash tmp.sh`

**Why These Rules:**
- Ensures all operations are reviewable and traceable
- Prevents accidental destructive operations
- Allows for better debugging and error handling
- Maintains consistency across all agent operations

To use this server, make sure you have the MCP library installed:
pip install mcp

Or use with uv:
uv add mcp
uv run mcp_server.py
"""

import asyncio
import sys
import re
import subprocess
import shutil
import tempfile
import time
from pathlib import Path
from typing import Any, Dict, List, Union

try:
    from mcp.server import Server  # type: ignore
    from mcp.server.stdio import stdio_server  # type: ignore
    from mcp.types import (  # type: ignore
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

from ci.util.build_info_analyzer import BuildInfoAnalyzer  # type: ignore

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
            description="Compile FastLED examples for different platforms using 'bash compile <platform> --examples <example>'",
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
            description="Run comprehensive code formatting and linting. For FOREGROUND agents, this runs `bash lint` for complete coverage. For BACKGROUND agents, can run specific tools for fine-grained control.",
            inputSchema={
                "type": "object",
                "properties": {
                    "tool": {
                        "type": "string",
                        "enum": ["bash_lint", "ruff", "javascript", "all"],
                        "description": "Linting approach: 'bash_lint' (recommended for foreground), 'ruff' (Python only), 'javascript' (JS only), 'all' (comprehensive via bash lint)",
                        "default": "bash_lint"
                    },
                    "agent_type": {
                        "type": "string",
                        "enum": ["foreground", "background"],
                        "description": "Agent type - foreground agents should use bash_lint, background agents can use specific tools",
                        "default": "foreground"
                    },
                    "fix": {
                        "type": "boolean",
                        "description": "Automatically fix issues where possible (only applies to specific tools, not bash_lint)",
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
        ),
        Tool(
            name="setup_stack_traces",
            description="Install and configure stack trace debugging libraries (libunwind/execinfo)",
            inputSchema={
                "type": "object",
                "properties": {
                    "method": {
                        "type": "string",
                        "enum": ["libunwind", "execinfo", "auto"],
                        "description": "Stack trace method to install and configure",
                        "default": "auto"
                    },
                    "test_installation": {
                        "type": "boolean",
                        "description": "Test the installation with crash tests",
                        "default": True
                    },
                    "install_only": {
                        "type": "boolean", 
                        "description": "Only install packages, don't run tests",
                        "default": False
                    }
                }
            }
        ),
        Tool(
            name="coding_standards",
            description="Get FastLED coding standards and best practices for C++ development",
            inputSchema={
                "type": "object",
                "properties": {
                    "topic": {
                        "type": "string",
                        "enum": ["all", "exceptions", "std_namespace", "naming", "member_naming", "containers", "debug", "bindings", "arduino_includes", "variable_naming", "python_linting"],
                        "description": "Specific topic to get standards for, or 'all' for complete guide",
                        "default": "all"
                    }
                }
            }
        ),
        Tool(
            name="validate_completion",
            description="🚨 MANDATORY FOR ALL BACKGROUND AGENTS: Validate that all tests pass before indicating completion. This tool MUST be run as the final step before any background agent indicates they are done with their task.",
            inputSchema={
                "type": "object",
                "properties": {
                    "task_description": {
                        "type": "string",
                        "description": "Brief description of the task being completed",
                        "default": "Code changes"
                    },
                    "run_full_test_suite": {
                        "type": "boolean",
                        "description": "Run the complete test suite including unit tests and compilation checks",
                        "default": True
                    }
                }
            }
        ),
        Tool(
            name="esp32_symbol_analysis",
            description="Run ESP32 symbol analysis to identify optimization opportunities for binary size reduction. Analyzes ELF files to find large symbols and provides recommendations for eliminating unused code.",
            inputSchema={
                "type": "object",
                "properties": {
                    "board": {
                        "type": "string",
                        "description": "ESP32 board name (e.g., 'esp32dev', 'esp32s3', 'esp32c3'). If not specified, auto-detects from .build directory",
                        "default": "auto"
                    },
                    "example": {
                        "type": "string",
                        "description": "Example name that was compiled (for context in reports)",
                        "default": "Blink"
                    },
                    "output_json": {
                        "type": "boolean",
                        "description": "Include detailed JSON output in results",
                        "default": False
                    },
                }
            }
        ),
        Tool(
            name="build_info_analysis",
            description="Analyze platform build information from build_info.json files. Extract platform-specific preprocessor defines, compiler flags, toolchain paths, and other build configuration data. Works with any platform that has been compiled (uno, esp32dev, teensy31, etc.).",
            inputSchema={
                "type": "object",
                "properties": {
                    "board": {
                        "type": "string",
                        "description": "Platform/board name (e.g., 'uno', 'esp32dev', 'teensy31'). Use 'list' to see available boards",
                        "default": "list"
                    },
                    "show_defines": {
                        "type": "boolean",
                        "description": "Show platform preprocessor defines (C/C++ #define values)",
                        "default": True
                    },
                    "show_compiler": {
                        "type": "boolean",
                        "description": "Show compiler information (paths, flags, types)",
                        "default": False
                    },
                    "show_toolchain": {
                        "type": "boolean",
                        "description": "Show toolchain tool aliases (gcc, g++, ar, etc.)",
                        "default": False
                    },
                    "show_all": {
                        "type": "boolean",
                        "description": "Show all available build information",
                        "default": False
                    },
                    "compare_with": {
                        "type": "string",
                        "description": "Compare platform defines with another board (e.g., 'esp32dev')"
                    },
                    "output_json": {
                        "type": "boolean",
                        "description": "Output results in JSON format for programmatic use",
                        "default": False
                    }
                }
            }
        ),
        Tool(
            name="symbol_analysis",
            description="Run generic symbol analysis for ANY platform (UNO, ESP32, Teensy, STM32, etc.) to identify optimization opportunities. Analyzes ELF files to show all symbols and their sizes without filtering. Works with any platform that has build_info.json.",
            inputSchema={
                "type": "object",
                "properties": {
                    "board": {
                        "type": "string",
                        "description": "Platform/board name (e.g., 'uno', 'esp32dev', 'teensy31', 'digix'). If 'auto', detects all available platforms from .build directory",
                        "default": "auto"
                    },
                    "example": {
                        "type": "string", 
                        "description": "Example name that was compiled (for context in reports)",
                        "default": "Blink"
                    },
                    "output_json": {
                        "type": "boolean",
                        "description": "Save detailed JSON output to .build/{board}_symbol_analysis.json",
                        "default": False
                    },
                    "run_all_platforms": {
                        "type": "boolean", 
                        "description": "If true, runs analysis on all detected platforms",
                        "default": False
                    }
                }
            }
        ),
        Tool(
            name="run_fastled_web_compiler",
            description="🌐 FOR FOREGROUND AGENTS ONLY: Run FastLED web compiler with playwright console.log capture. Compiles Arduino sketch to WASM and opens browser with automated testing. BACKGROUND AGENTS MUST NOT USE THIS TOOL.",
            inputSchema={
                "type": "object",
                "properties": {
                    "example_path": {
                        "type": "string",
                        "description": "Path to example directory (e.g., 'examples/Audio', 'examples/Blink')",
                        "default": "examples/Audio"
                    },
                    "capture_duration": {
                        "type": "integer",
                        "description": "Duration in seconds to capture console.log output",
                        "default": 30
                    },
                    "headless": {
                        "type": "boolean",
                        "description": "Run browser in headless mode",
                        "default": False
                    },
                    "port": {
                        "type": "integer",
                        "description": "Port for web server (0 for auto-detection)",
                        "default": 0
                    },
                    "docker_check": {
                        "type": "boolean",
                        "description": "Check if Docker is available for faster compilation",
                        "default": True
                    },
                    "save_screenshot": {
                        "type": "boolean",
                        "description": "Save screenshot of the running visualization",
                        "default": True
                    }
                }
            }
        ),
        Tool(
            name="validate_arduino_includes",
            description="🚨 CRITICAL: Validate that no new Arduino.h includes have been added to the codebase. This tool scans for #include \"Arduino.h\" and #include <Arduino.h> statements and reports any that are not pre-approved.",
            inputSchema={
                "type": "object",
                "properties": {
                    "directory": {
                        "type": "string",
                        "description": "Directory to scan for Arduino.h includes (relative to project root)",
                        "default": "src"
                    },
                    "include_examples": {
                        "type": "boolean",
                        "description": "Also scan examples directory for Arduino.h includes",
                        "default": False
                    },
                    "check_dev": {
                        "type": "boolean", 
                        "description": "Also scan dev directory for Arduino.h includes",
                        "default": False
                    },
                    "show_approved": {
                        "type": "boolean",
                        "description": "Show approved Arduino.h includes marked with '// ok include'",
                        "default": True
                    }
                }
            }
        )
    ]

@server.call_tool()
async def call_tool(name: str, arguments: Dict[str, Any]) -> Any:
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
        elif name == "setup_stack_traces":
            return await setup_stack_traces(arguments, project_root)
        elif name == "coding_standards":
            return await coding_standards(arguments, project_root)
        elif name == "validate_completion":
            return await validate_completion(arguments, project_root)
        elif name == "build_info_analysis":
            return await build_info_analysis(arguments, project_root)
        elif name == "esp32_symbol_analysis":
            return await esp32_symbol_analysis(arguments, project_root)
        elif name == "symbol_analysis":
            return await symbol_analysis(arguments, project_root)
        elif name == "run_fastled_web_compiler":
            return await run_fastled_web_compiler(arguments, project_root)
        elif name == "validate_arduino_includes":
            return await validate_arduino_includes(arguments, project_root)
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
    
    # Use bash test format as per user directive
    if test_case and specific_test:
        # For individual TEST_CASE, we still need to use bash test with the test name
        # The bash test script handles the details
        cmd = ["bash", "test", specific_test]
        context = f"Running test: bash test {specific_test}\n"
        if test_case:
            context += f"Note: To run specific TEST_CASE '{test_case}', the test framework will need to support filtering\n"
    elif specific_test and test_type == "specific":
        # Use bash test format for specific tests
        cmd = ["bash", "test", specific_test]
        context = f"Running specific test: bash test {specific_test}\n"
    else:
        # For all tests or cpp tests, use the original format
        cmd = ["uv", "run", "test.py"]
        
        if test_type == "cpp":
            cmd.append("--cpp")
        
        if use_clang:
            cmd.append("--clang")
        
        if clean:
            cmd.append("--clean")
            
        if verbose:
            cmd.append("--verbose")
            
        context = f"Command executed: {' '.join(cmd)}\n"
    
    result = await run_command(cmd, project_root)
    
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
    
    test_cases: Dict[str, List[str]] = {}
    
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
    result_text += "• Run specific test file: bash test algorithm\n"
    result_text += "• Run with verbose output: uv run test.py --cpp algorithm --verbose\n"
    if test_cases:
        first_test = list(test_cases.keys())[0]
        if test_cases[first_test]:
            first_case = test_cases[first_test][0]
            result_text += f"• Run specific test: bash test {first_test}\n"
            result_text += f"  (Note: Individual TEST_CASE execution requires test framework support)\n"
    
    return CallToolResult(
        content=[TextContent(type="text", text=result_text)]
    )

def extract_test_cases(file_path: Path, search_pattern: str = "") -> List[str]:
    """Extract TEST_CASE names from a test file."""
    test_cases: List[str] = []
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Find TEST_CASE macros using regex
        pattern = r'TEST_CASE\s*\(\s*"([^"]+)"'
        matches = re.findall(pattern, content)
        
        for match in matches:
            if not search_pattern or search_pattern.lower() in match.lower():
                test_cases.append(match)
    
    except Exception:
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

### 🚨 CRITICAL: Always Use `bash test` Format

**[OK] CORRECT Format:**
```bash
bash test                      # Run all tests
bash test <test_name>         # Run specific test
```

**[ERROR] INCORRECT Format:**
```bash
./.build/bin/test_<name>.exe  # DO NOT run executables directly
./tests/.build/bin/test_*     # DO NOT use this format
```

### 1. Run All Tests
```bash
bash test
```

### 2. Run Specific Test File
```bash
bash test <test_name>
```
Example: `bash test algorithm` (runs `test_algorithm.cpp`)
Example: `bash test audio_json_parsing` (runs `test_audio_json_parsing.cpp`)

### 3. Alternative: Using test.py directly
For advanced options, you can also use:
```bash
uv run test.py              # Run all tests
uv run test.py --cpp        # Run only C++ tests
uv run test.py --cpp <name> # Run specific test file
```

### 4. Run with Verbose Output
```bash
uv run test.py --cpp <test_name> --verbose
```

### 5. Using MCP Server Tools
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
# Run algorithm tests
bash test algorithm

# Or with verbose output
uv run test.py --cpp algorithm --verbose

# List available TEST_CASEs in algorithm tests
# (use MCP list_test_cases tool with test_file: "algorithm")
```

### Test Development Workflow
```bash
# Clean build and run specific test
uv run test.py --cpp easing --clean --verbose

# Check for specific TEST_CASE patterns
# (use MCP list_test_cases tool with search_pattern)
```

## Common Test File Names
- `test_algorithm.cpp`: Algorithm utilities
- `test_easing.cpp`: Easing functions
- `test_hsv16.cpp`: HSV color space tests
- `test_math.cpp`: Mathematical functions
- `test_vector.cpp`: Vector container tests
- `test_fx.cpp`: Effects framework tests
- `test_audio_json_parsing.cpp`: Audio JSON parsing tests

## Tips
1. **Always use `bash test <name>`** format for running specific tests
2. **Use `--verbose`** to see detailed test output and assertions
3. **Use `--clean`** when testing after code changes
4. **List TEST_CASEs first** to see what's available before running
5. **Check test output carefully** - doctest provides detailed failure information

## Environment Setup
- Have UV in your environment
- Use `bash test` or `uv run test.py` - don't try to run test executables manually
- The test infrastructure handles platform differences and proper execution

## Why Use `bash test`?
The `bash test` wrapper:
- Handles platform differences automatically
- Sets up the proper environment
- Manages test execution across different systems
- Provides consistent behavior regardless of OS
"""
    
    return CallToolResult(
        content=[TextContent(type="text", text=instructions.strip())]
    )

async def coding_standards(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Provide FastLED coding standards and best practices."""
    topic = arguments.get("topic", "all")
    
    standards = {
        "exceptions": """
# Exception Handling Standards

⚠️ **CRITICAL: DO NOT use try-catch blocks or C++ exception handling in FastLED code**

## Why No Exceptions?
FastLED is designed for embedded systems like Arduino where:
- Exception handling may not be available
- Exceptions consume significant memory and performance
- Reliable operation across all platforms is required

## What to Avoid:
[ERROR] `try { ... } catch (const std::exception& e) { ... }`
[ERROR] `throw std::runtime_error("error message")`
[ERROR] `#include <exception>` or `#include <stdexcept>`

## Use Instead:
[OK] **Return error codes:** `bool function() { return false; }`
[OK] **Optional types:** `fl::optional<T>`
[OK] **Assertions:** `FL_ASSERT(condition)`
[OK] **Early returns:** `if (!valid) return false;`
[OK] **Status objects:** Custom result types

## Examples:
```cpp
// Good: Using return codes
bool initializeHardware() {
    if (!setupPins()) {
        FL_WARN("Failed to setup pins");
        return false;
    }
    return true;
}

// Good: Using fl::optional
fl::optional<float> calculateValue(int input) {
    if (input < 0) {
        return fl::nullopt;
    }
    return fl::make_optional(sqrt(input));
}
```
""",
        
        "std_namespace": """
# Standard Library Namespace Standards

⚠️ **DO NOT use std:: prefixed functions or headers**

FastLED provides its own STL-equivalent implementations under the `fl::` namespace.

## Common Replacements:
- [ERROR] `#include <vector>` → [OK] `#include "fl/vector.h"`
- [ERROR] `#include <algorithm>` → [OK] `#include "fl/algorithm.h"`
- [ERROR] `std::move()` → [OK] `fl::move()`
- [ERROR] `std::vector` → [OK] `fl::vector`

**Always check if there's a `fl::` equivalent in `src/fl/` first!**
""",
        
        "naming": """
# Naming Conventions

## Class/Object Names:
**Simple Objects:** lowercase (e.g., `fl::vec2f`, `fl::point`, `fl::rect`)
**Complex Objects:** CamelCase (e.g., `Raster`, `Controller`, `Canvas`)
**Pixel Types:** ALL CAPS (e.g., `CRGB`, `CHSV`, `RGB24`)

## Member Variables and Functions:

### Complex Classes/Objects:
[OK] **Member variables:** Use `mVariableName` format (e.g., `mPixelCount`, `mBufferSize`, `mCurrentIndex`)
[OK] **Member functions:** Use camelCase (e.g., `getValue()`, `setPixelColor()`, `updateBuffer()`)
[ERROR] Avoid: `m_variable_name`, `variableName`, `GetValue()`, `set_pixel_color()`

### Simple Classes/Structs:
[OK] **Member variables:** Use lowercase snake_case (e.g., `x`, `y`, `width`, `height`, `pixel_count`)
[OK] **Member functions:** Use camelCase (e.g., `getValue()`, `setPosition()`, `normalize()`)
[ERROR] Avoid: `mX`, `mY`, `get_value()`, `set_position()`

## Examples:

```cpp
// Complex class - use mVariableName for members
class Controller {
private:
    int mPixelCount;           // [OK] Complex class member variable
    uint8_t* mBuffer;          // [OK] Complex class member variable
    bool mIsInitialized;       // [OK] Complex class member variable
    
public:
    void setPixelCount(int count);  // [OK] Complex class member function
    int getPixelCount() const;      // [OK] Complex class member function
    void updateBuffer();            // [OK] Complex class member function
};

// Simple struct - use snake_case for members
struct vec2 {
    int x;                     // [OK] Simple struct member variable
    int y;                     // [OK] Simple struct member variable
    
    float magnitude() const;   // [OK] Simple struct member function
    void normalize();          // [OK] Simple struct member function
};

struct point {
    float x;                   // [OK] Simple struct member variable
    float y;                   // [OK] Simple struct member variable
    float z;                   // [OK] Simple struct member variable
    
    void setPosition(float x, float y, float z);  // [OK] Simple struct member function
    float distanceTo(const point& other) const;   // [OK] Simple struct member function
};
```

**Why:** Complex classes use Hungarian notation for member variables to clearly distinguish them from local variables, while simple structs use concise snake_case for lightweight data containers.
""",
        
        "member_naming": """
# Member Variable and Function Naming Standards

## Complex Classes/Objects:
[OK] **Member variables:** Use `mVariableName` format (e.g., `mPixelCount`, `mBufferSize`, `mCurrentIndex`)
[OK] **Member functions:** Use camelCase (e.g., `getValue()`, `setPixelColor()`, `updateBuffer()`)
[ERROR] Avoid: `m_variable_name`, `variableName`, `GetValue()`, `set_pixel_color()`

## Simple Classes/Structs:
[OK] **Member variables:** Use lowercase snake_case (e.g., `x`, `y`, `width`, `height`, `pixel_count`)
[OK] **Member functions:** Use camelCase (e.g., `getValue()`, `setPosition()`, `normalize()`)
[ERROR] Avoid: `mX`, `mY`, `get_value()`, `set_position()`

## Examples:

```cpp
// Complex class - use mVariableName for members
class Controller {
private:
    int mPixelCount;           // [OK] Complex class member variable
    uint8_t* mBuffer;          // [OK] Complex class member variable
    bool mIsInitialized;       // [OK] Complex class member variable
    
public:
    void setPixelCount(int count);  // [OK] Complex class member function
    int getPixelCount() const;      // [OK] Complex class member function
    void updateBuffer();            // [OK] Complex class member function
};

// Simple struct - use snake_case for members
struct vec2 {
    int x;                     // [OK] Simple struct member variable
    int y;                     // [OK] Simple struct member variable
    
    float magnitude() const;   // [OK] Simple struct member function
    void normalize();          // [OK] Simple struct member function
};

struct point {
    float x;                   // [OK] Simple struct member variable
    float y;                   // [OK] Simple struct member variable
    float z;                   // [OK] Simple struct member variable
    
    void setPosition(float x, float y, float z);  // [OK] Simple struct member function
    float distanceTo(const point& other) const;   // [OK] Simple struct member function
};
```

**Why:** Complex classes use Hungarian notation for member variables to clearly distinguish them from local variables, while simple structs use concise snake_case for lightweight data containers.
""",
        
        "containers": """
# Container Parameter Standards

**Prefer `fl::span<T>` over `fl::vector<T>` for function parameters**

[OK] `void processData(fl::span<const uint8_t> data)`
[ERROR] `void processData(fl::vector<uint8_t>& data)`

Benefits: automatic conversion, type safety, zero-cost abstraction
""",
        
        "debug": """
# Debug Printing Standards

**Use `FL_WARN` for debug printing throughout the codebase**

[OK] `FL_WARN("Debug message: " << message);`
[ERROR] `FL_WARN("Value: %d", value);`

Provides unified logging across all platforms and testing environments.
""",
        
        "bindings": """
# WebAssembly Bindings Warning

🚨 **EXTREMELY CRITICAL: DO NOT modify function signatures in WASM bindings!**

High-risk files:
- `src/platforms/wasm/js_bindings.cpp`
- `src/platforms/wasm/ui.cpp`
- `src/platforms/wasm/active_strip_data.cpp`

Changing signatures causes runtime errors that are extremely difficult to debug.
""",

        "arduino_includes": r"""
# Arduino.h Include Standards

🚨 **CRITICAL: DO NOT add new #include "Arduino.h" or #include <Arduino.h> statements**

## 🚨 DEBUGGING LINKER ERRORS
**If you encounter weird linker errors after making changes, FIRST check if Arduino.h has been added anywhere:**

### Common Symptoms of Arduino.h-Related Linker Errors:
- [ERROR] **Undefined reference errors** for standard functions
- [ERROR] **Multiple definition errors** for common symbols
- [ERROR] **Conflicting symbol errors** between Arduino and FastLED
- [ERROR] **Missing library errors** that were not missing before
- [ERROR] **Platform-specific compilation failures** that worked before

### Immediate Debugging Steps:
1. **🔍 Search for new Arduino.h includes:** `grep -r "Arduino\.h" src/` and check if any new includes were added
2. **📝 Check recent file changes** for any #include "Arduino.h" or #include <Arduino.h> statements  
3. **🔄 Remove any new Arduino.h includes** and replace with FastLED platform abstractions
4. **[OK] Rebuild and test** to confirm the linker errors are resolved

### Why Arduino.h Causes Linker Issues:
- **Path Conflicts:** Arduino.h includes can interfere with FastLED's custom path resolution
- **Symbol Conflicts:** Arduino.h defines symbols that conflict with FastLED's implementations
- **Build Issues:** Incorrect Arduino.h includes cause compilation failures across platforms
- **Dependency Management:** FastLED manages Arduino compatibility through its own abstraction layer

## What to Avoid:
[ERROR] `#include "Arduino.h"`
[ERROR] `#include <Arduino.h"`
[ERROR] Adding new Arduino.h includes to existing files
[ERROR] Copying Arduino.h includes from other examples

## Approved Alternatives:
[OK] Use existing FastLED platform abstractions in `src/platforms/`
[OK] Include specific FastLED headers that provide Arduino functionality
[OK] Use FastLED's `fl::` namespace equivalents for Arduino functions
[OK] Reference existing Arduino.h includes that are already marked as approved

## Existing Approved Includes:
Some files already have Arduino.h includes marked with `// ok include` comments.
These are pre-approved and should NOT be changed or removed.

## If You Need Arduino Functionality:
1. **Check FastLED abstractions first** - Look in `src/platforms/` for platform-specific implementations
2. **Use existing patterns** - Find similar functionality in existing FastLED code
3. **Ask for guidance** - Before adding Arduino.h, consider if there's a FastLED-native approach

## Background Agent Requirements:
🚨 **NEVER add Arduino.h includes when creating or modifying files**
🚨 **Always use FastLED's platform abstraction layer instead**
🚨 **Check existing code patterns before adding platform-specific includes**
🚨 **When troubleshooting linker errors, ALWAYS check for Arduino.h additions first**

This rule prevents path conflicts and ensures consistent cross-platform compatibility.
""",

        "variable_naming": """
# Variable Naming Standards

⚠️ **CRITICAL: DO NOT change variable names unless absolutely necessary**

## Why Avoid Variable Name Changes?
- **Code Consistency:** Existing variable names maintain consistency across the codebase
- **Review Complexity:** Unnecessary name changes make code reviews harder to follow
- **Merge Conflicts:** Gratuitous renaming increases the likelihood of merge conflicts
- **API Stability:** Public variable names may be part of the API contract
- **Documentation:** Variable names may be referenced in documentation and examples

## When Variable Name Changes Are Acceptable:
[OK] **Fixing misleading names** that don't match their actual purpose
[OK] **Following established naming conventions** when code is being substantially refactored
[OK] **Resolving naming conflicts** during integration or merging
[OK] **Compliance with coding standards** when specifically required
[OK] **Improving clarity** for genuinely confusing or cryptic names

## What to Avoid:
[ERROR] Changing names just for personal preference
[ERROR] "Improving" names that are already clear and consistent
[ERROR] Mass renaming operations without clear justification
[ERROR] Style-only changes to established, working variable names
[ERROR] Renaming in stable, public-facing APIs

## Best Practice:
**If the existing variable name is clear and follows project conventions, leave it unchanged.**
Focus your efforts on functional improvements rather than cosmetic naming changes.

## Examples:
```cpp
// DON'T change these if they're working fine:
int ledCount;     // Clear and consistent
CRGB* ledData;    // Follows FastLED conventions
uint8_t brightness; // Standard and descriptive

// DO change these if absolutely necessary:
int x;            // Too generic in complex context
bool flag;        // Meaningless name
char* ptr123;     // Confusing and non-descriptive
```
""",

        "python_linting": """
# Python File Modification Standards

🚨 **MANDATORY: Run `bash lint` after modifying any Python files (*.py)**

## Critical Rule for Background Agents:
**WHENEVER you modify any Python file (*.py), you MUST run `bash lint` before indicating completion.**

## Why This is Required:
- **Code Quality:** Ensures consistent Python code formatting and style
- **CI/CD Compatibility:** Prevents linting failures in continuous integration
- **Team Standards:** Maintains consistent code quality across all Python files
- **Error Prevention:** Catches syntax errors and style issues early

## What Files This Applies To:
[OK] **All *.py files** in the project
[OK] **test.py** - Main test runner
[OK] **mcp_server.py** - MCP server implementation  
[OK] **ci/util-compile.py** - CI compilation scripts
[OK] **ci/util/*.py** - All CI utility scripts
[OK] **Any new Python files** you create

## Required Process:
1. **Make your Python file changes**
2. **IMMEDIATELY run:** `bash lint`
3. **Fix any linting issues** that are reported
4. **Re-run:** `bash lint` until it passes clean
5. **Only then indicate completion**

## Example Workflow:
```bash
# 1. After modifying any *.py file
# 2. Run linting (MANDATORY)
bash lint

# 3. If linting reports issues, fix them and run again
bash lint

# 4. Only proceed when linting passes with no errors
```

## What `bash lint` Does:
- **📝 Python Linting** - ruff, black, isort, pyright
- **🔧 C++ Linting** - clang-format (when enabled)
- **🌐 JavaScript Linting** - Fast Node.js + ESLint linting (critical issues only)
- **🔍 JavaScript Enhancement** - Analysis and recommendations
- **💡 AI Agent Guidance** - Clear instructions for proper usage

## AI Agent Linting Rules:
- **FOREGROUND AGENTS:** Always use `bash lint` - never run individual linting scripts
- **BACKGROUND AGENTS:** Prefer `bash lint` but can use individual scripts for specific needs
- **MCP Server:** Use `lint_code` tool with `agent_type` parameter for guidance

## Background Agent Requirements:
🚨 **FAILURE TO RUN `bash lint` AFTER PYTHON MODIFICATIONS WILL RESULT IN BROKEN BUILDS**

This is a **NON-NEGOTIABLE REQUIREMENT** for all background agents working with Python files.

## Integration with Completion Validation:
The `validate_completion` tool will run `bash test`, but you should run `bash lint` 
IMMEDIATELY after Python modifications, not wait until final validation.
"""
    }
    
    if topic == "all":
        result = "# FastLED C++ Coding Standards\n\n"
        result += "## 🚨 MOST CRITICAL RULES 🚨\n\n"
        result += "1. **NO ARDUINO.H INCLUDES** - Never add new #include \"Arduino.h\" or #include <Arduino.h>\n"
        result += "2. **LINKER ERROR DEBUGGING** - If weird linker errors occur, check for Arduino.h additions FIRST\n"
        result += "3. **PYTHON LINTING MANDATORY** - Run `bash lint` after modifying any *.py files\n"
        result += "4. **NO TRY-CATCH BLOCKS** - Use return codes, fl::optional, or early returns\n"
        result += "5. **NO std:: NAMESPACE** - Use fl:: equivalents instead\n"
        result += "6. **NO WASM BINDING CHANGES** - Extremely dangerous for runtime stability\n"
        result += "7. **NO UNNECESSARY VARIABLE RENAMING** - Don't change names unless absolutely necessary\n"
        result += "8. **MEMBER NAMING CONVENTIONS** - Complex classes: mVariableName; Simple structs: snake_case\n\n"
        
        for section_name, content in standards.items():
            result += content + "\n" + ("="*50) + "\n\n"
    
    elif topic in standards:
        result = standards[topic]
    else:
        result = f"Unknown topic: {topic}. Available topics: {', '.join(standards.keys())}"
    
    return CallToolResult(
        content=[TextContent(type="text", text=result)]
    )

async def setup_stack_traces(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Install and configure stack trace debugging libraries."""
    method = arguments.get("method", "auto")
    test_installation = arguments.get("test_installation", True)
    install_only = arguments.get("install_only", False)
    
    result_text = "# FastLED Stack Trace Setup\n\n"
    
    try:
        # Detect OS and package manager
        import platform
        system = platform.system().lower()
        
        if system == "linux":
            # Try to detect package manager
            distro_info = ""
            try:
                with open("/etc/os-release", "r") as f:
                    distro_info = f.read().lower()
            except (OSError, IOError):
                pass
            
            install_commands: List[str] = []
            
            if method in ["libunwind", "auto"]:
                result_text += "## Installing LibUnwind (Enhanced Stack Traces)\n\n"
                
                if "ubuntu" in distro_info or "debian" in distro_info:
                    install_commands.append("sudo apt-get update")
                    install_commands.append("sudo apt-get install -y libunwind-dev build-essential cmake")
                elif "centos" in distro_info or "rhel" in distro_info:
                    install_commands.append("sudo yum install -y libunwind-devel gcc-c++ cmake")
                elif "fedora" in distro_info:
                    install_commands.append("sudo dnf install -y libunwind-devel gcc-c++ cmake")
                else:
                    result_text += "⚠️  Unknown Linux distribution. Manual installation required.\n"
                    result_text += "Common package names: libunwind-dev, libunwind-devel\n\n"
                
            elif method == "execinfo":
                result_text += "## Using Execinfo (Standard GCC Stack Traces)\n\n"
                result_text += "[OK] Execinfo is part of glibc - no additional packages needed!\n\n"
                
                # Still need build tools
                if "ubuntu" in distro_info or "debian" in distro_info:
                    install_commands.append("sudo apt-get update")
                    install_commands.append("sudo apt-get install -y build-essential cmake")
                elif "centos" in distro_info or "rhel" in distro_info:
                    install_commands.append("sudo yum install -y gcc-c++ cmake")
                elif "fedora" in distro_info:
                    install_commands.append("sudo dnf install -y gcc-c++ cmake")
            
            # Run installation commands
            for cmd in install_commands:
                result_text += f"Running: `{cmd}`\n"
                try:
                    _ = await run_command(cmd.split(), project_root)
                    result_text += "[OK] Success\n\n"
                except Exception as e:
                    result_text += f"[ERROR] Error: {e}\n\n"
                    return CallToolResult(
                        content=[TextContent(type="text", text=result_text)],
                        isError=True
                    )
                    
        elif system == "darwin":  # macOS
            result_text += "## Installing LibUnwind on macOS\n\n"
            try:
                _ = await run_command(["brew", "install", "libunwind"], project_root)
                result_text += "[OK] LibUnwind installed via Homebrew\n\n"
            except Exception as e:
                result_text += f"[ERROR] Error installing libunwind: {e}\n"
                result_text += "Please install Homebrew first: https://brew.sh/\n\n"
                
        else:
            result_text += f"⚠️  Unsupported OS: {system}\n"
            result_text += "Manual installation required.\n\n"
        
        if not install_only and test_installation:
            result_text += "## Testing Stack Trace Installation\n\n"
            
            # Build the crash test programs
            tests_dir = project_root / "tests"
            result_text += "Building crash test programs...\n"
            
            try:
                # Clean and rebuild
                _ = await run_command(["rm", "-f", "CMakeCache.txt"], tests_dir)
                _ = await run_command(["cmake", "."], tests_dir)
                _ = await run_command(["make", "-j4"], tests_dir)
                result_text += "[OK] FastLED test framework built successfully\n\n"
                
                # Test by running a simple unit test to verify stack traces work
                result_text += "### Testing Stack Trace Integration\n"
                try:
                    # Look for any existing test executable to verify crash handling works
                    test_executables = await run_command(["find", ".build/bin", "-name", "test_*", "-type", "f"], tests_dir)
                    if test_executables.strip():
                        first_test = test_executables.strip().split('\n')[0]
                        test_name = first_test.split('/')[-1]
                        result_text += f"Testing with {test_name}...\n"
                        # Just run help to verify the executable works and crash handler is linked
                        _ = await run_command([first_test, "--help"], tests_dir)
                        result_text += "[OK] Stack trace system is properly integrated with test framework\n\n"
                    else:
                        result_text += "⚠️ No test executables found to verify integration\n\n"
                except Exception as e:
                    result_text += f"⚠️ Could not verify integration: {e}\n\n"
                        
            except Exception as e:
                result_text += f"[ERROR] Error building test framework: {e}\n\n"
        
        # Add usage instructions
        result_text += "## Usage Instructions\n\n"
        result_text += "The FastLED project automatically detects and uses the best available stack trace method:\n\n"
        result_text += "1. **LibUnwind** (if available) - Enhanced stack traces with symbol resolution\n"
        result_text += "2. **Execinfo** (fallback) - Basic stack traces using glibc\n"
        result_text += "3. **Windows** (on Windows) - Windows-specific debugging APIs\n"
        result_text += "4. **No-op** (last resort) - Minimal crash handling\n\n"
        
        result_text += "### Testing Stack Trace Integration\n"
        result_text += "```bash\n"
        result_text += "cd tests\n"
        result_text += "cmake . && make\n"
        result_text += "# Stack traces are automatically enabled in test executables\n"
        result_text += "# Run any test to see crash handling in action if a test fails\n"
        result_text += "```\n\n"
        
        result_text += "### Enabling in Your Code\n"
        result_text += "```cpp\n"
        result_text += '#include "tests/crash_handler.h"\n'
        result_text += "\n"
        result_text += "int main() {\n"
        result_text += "    setup_crash_handler();  // Enable crash handling\n"
        result_text += "    // Your code here...\n"
        result_text += "    return 0;\n"
        result_text += "}\n"
        result_text += "```\n\n"
        
        return CallToolResult(
            content=[TextContent(type="text", text=result_text)]
        )
        
    except Exception as e:
        return CallToolResult(
            content=[TextContent(type="text", text=f"Error setting up stack traces: {e}")],
            isError=True
        )

async def compile_examples(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Compile FastLED examples."""
    platform = arguments.get("platform", "uno")
    examples = arguments.get("examples", ["Blink"])
    interactive = arguments.get("interactive", False)
    
    cmd = ["uv", "run", "-m", "ci.ci-compile", platform]
    
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
    from test import calculate_fingerprint
    
    directory = arguments.get("directory", "src")
    target_dir = project_root / directory
    
    fingerprint_data = calculate_fingerprint(target_dir)
    
    result_text = f"Code fingerprint for {directory}:\n"
    result_text += f"Hash: {fingerprint_data.hash}\n"
    result_text += f"Elapsed time: {fingerprint_data.elapsed_seconds or 'N/A'}s\n"
    
    if fingerprint_data.status:
        result_text += f"Status: {fingerprint_data.status}\n"
    
    return CallToolResult(
        content=[TextContent(type="text", text=result_text)]
    )

async def lint_code(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Run code linting tools with agent-appropriate guidance."""
    tool = arguments.get("tool", "bash_lint")
    agent_type = arguments.get("agent_type", "foreground")
    fix = arguments.get("fix", False)
    
    results: List[str] = []
    
    # Provide guidance based on agent type
    if agent_type == "foreground" and tool != "bash_lint":
        guidance = "⚠️  FOREGROUND AGENT GUIDANCE:\n"
        guidance += "For comprehensive linting, foreground agents should use 'bash_lint' tool option.\n"
        guidance += "This ensures all linting (Python, C++, JavaScript) runs in proper sequence.\n\n"
        results.append(guidance)
    
    # Handle different tool options
    if tool == "bash_lint" or tool == "all":
        # Use the comprehensive bash lint script
        lint_script = project_root / "lint"
        if lint_script.exists():
            result = await run_command(["bash", "lint"], project_root)
            results.append(f"🚀 Comprehensive Linting Results (bash lint):\n{result}")
        else:
            return CallToolResult(
                content=[TextContent(type="text", text="[ERROR] bash lint script not found")],
                isError=True
            )
    
    elif tool == "ruff":
        # Python-only linting for background agents
        cmd = ["uv", "run", "ruff", "check"]
        if fix:
            cmd.append("--fix")
        result = await run_command(cmd, project_root)
        results.append(f"📝 Python Linting (ruff):\n{result}")
        
        if agent_type == "background":
            results.append("\n💡 Background Agent: Consider running 'bash_lint' for comprehensive coverage.")
    
    elif tool == "javascript":
        # JavaScript-only linting for background agents
        lint_js_script = project_root / "ci" / "js" / "lint-js"
        check_js_script = project_root / "ci" / "js" / "check-js"
        
        if lint_js_script.exists():
            result = await run_command(["ci/js/lint-js"], project_root)
            results.append(f"🌐 JavaScript Linting:\n{result}")
            
            if check_js_script.exists():
                result = await run_command(["ci/js/check-js"], project_root)
                results.append(f"🔍 JavaScript Type Checking:\n{result}")
        else:
            results.append("[ERROR] JavaScript linting tools not found. Run: uv run ci/setup-js-linting.py")
        
        if agent_type == "background":
            results.append("\n💡 Background Agent: Consider running 'bash_lint' for comprehensive coverage.")
    
    # Add final guidance
    if agent_type == "foreground":
        results.append("\n💡 FOREGROUND AGENT: Always prefer 'bash_lint' for complete linting coverage.")
    else:
        results.append("\n💡 BACKGROUND AGENT: Fine-grained linting available, but 'bash_lint' recommended for comprehensive checking.")
    
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
    
    examples: List[str] = []
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
    
    info: List[str] = []
    
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
                info.append("\n[OK] Git status: Working tree clean")
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

async def validate_completion(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """🚨 MANDATORY validation step for all background agents before indicating completion."""
    task_description = arguments.get("task_description", "Code changes")
    run_full_test_suite = arguments.get("run_full_test_suite", True)
    
    result_text = f"# 🚨 COMPLETION VALIDATION FOR: {task_description}\n\n"
    result_text += "## MANDATORY PRE-COMPLETION CHECK\n\n"
    result_text += "This tool MUST be run by all background agents before indicating completion.\n"
    result_text += "Running comprehensive test suite to ensure all changes are working correctly.\n\n"
    
    validation_failed = False
    
    # First, validate Arduino.h includes
    result_text += "### Validating Arduino.h Includes\n\n"
    try:
        arduino_validation = await validate_arduino_includes(
            {"directory": "src", "include_examples": False, "check_dev": False, "show_approved": False},
            project_root
        )
        
        if arduino_validation.isError:
            validation_failed = True
            result_text += "[ERROR] **ARDUINO.H VALIDATION FAILED**\n\n"
            result_text += "New Arduino.h includes detected! This violates FastLED coding standards.\n"
            result_text += "Please remove Arduino.h includes and use FastLED's platform abstractions instead.\n\n"
        else:
            result_text += "[OK] **Arduino.h validation passed**\n\n"
            
    except Exception as e:
        result_text += f"⚠️ **Arduino.h validation error:** {str(e)}\n\n"
    
    if run_full_test_suite:
        result_text += "### Running Full Test Suite: `bash test`\n\n"
        
        try:
            # Run the bash test command as specified in user rules
            test_result = await run_command(["bash", "test"], project_root)
            
            # Check if tests passed by looking for common failure indicators
            if "FAILED" in test_result or "ERROR" in test_result or "error:" in test_result.lower():
                validation_failed = True
                result_text += "[ERROR] **VALIDATION FAILED - TESTS CONTAIN ERRORS**\n\n"
                result_text += "```\n" + test_result + "\n```\n\n"
                result_text += "🚨 **BACKGROUND AGENT MUST NOT INDICATE COMPLETION**\n"
                result_text += "🚨 **PLEASE FIX ALL ERRORS BEFORE COMPLETING THE TASK**\n\n"
            else:
                result_text += "�� **ALL TESTS PASSED**\n\n"
                result_text += "```\n" + test_result + "\n```\n\n"
                result_text += "[OK] **VALIDATION SUCCESSFUL - SAFE TO INDICATE COMPLETION**\n\n"
            
        except Exception as e:
            validation_failed = True
            result_text += f"[ERROR] **VALIDATION FAILED - ERROR RUNNING TESTS**\n\n"
            result_text += f"Error: {str(e)}\n\n"
            result_text += "🚨 **BACKGROUND AGENT MUST NOT INDICATE COMPLETION**\n"
            result_text += "🚨 **PLEASE RESOLVE TEST EXECUTION ISSUES BEFORE COMPLETING**\n\n"
    
    # Final instructions
    result_text += "## COMPLETION REQUIREMENTS FOR BACKGROUND AGENTS\n\n"
    
    if validation_failed:
        result_text += "🚫 **DO NOT INDICATE COMPLETION**\n"
        result_text += "- Tests are failing or could not run\n"
        result_text += "- Fix all issues and run this validation again\n"
        result_text += "- Only indicate completion after ALL tests pass\n\n"
        
        return CallToolResult(
            content=[TextContent(type="text", text=result_text)],
            isError=True
        )
    else:
        result_text += "[OK] **SAFE TO INDICATE COMPLETION**\n"
        result_text += "- All tests are passing\n"
        result_text += "- Code changes have been validated\n"
        result_text += "- Background agent may now indicate task completion\n\n"
        
        return CallToolResult(
            content=[TextContent(type="text", text=result_text)]
        )

async def build_info_analysis(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Analyze platform build information from build_info.json files."""
    board = arguments.get("board", "list")
    show_defines = arguments.get("show_defines", True)
    show_compiler = arguments.get("show_compiler", False)
    show_toolchain = arguments.get("show_toolchain", False)
    show_all = arguments.get("show_all", False)
    compare_with = arguments.get("compare_with")
    output_json = arguments.get("output_json", False)
    
    # Import our build info analyzer

        
    
    analyzer = BuildInfoAnalyzer(str(project_root / ".build"))
    
    # Handle board listing
    if board == "list" or not board:
        boards = analyzer.list_available_boards()
        if not boards:
            result_text = "[ERROR] No boards with build_info.json found in .build directory\n"
            result_text += "   Try running a compilation first:\n"
            result_text += "   uv run python -m ci.ci-compile uno --examples Blink\n"
            result_text += "   uv run python -m ci.ci-compile esp32dev --examples Blink\n"
            return CallToolResult(
                content=[TextContent(type="text", text=result_text)]
            )
        
        result_text = f"📋 Available boards with build_info.json ({len(boards)}):\n"
        for board_name in boards:
            result_text += f"  [OK] {board_name}\n"
        result_text += "\nUsage: Use 'board' parameter with any of these names to analyze platform information.\n"
        result_text += "Example: board='uno', show_defines=True\n"
        
        return CallToolResult(
            content=[TextContent(type="text", text=result_text)]
        )
    
    # Handle board comparison
    if compare_with:
        success, comparison, error = analyzer.compare_defines(board, compare_with)  # type: ignore
        if not success:
            return CallToolResult(
                content=[TextContent(type="text", text=f"[ERROR] Error: {error}")],
                isError=True
            )
        
        if output_json:
            import json
            result_text = json.dumps(comparison, indent=2)
        else:
            board1 = comparison['board1']
            board2 = comparison['board2']
            
            result_text = f"🔍 Platform Defines Comparison:\n"
            result_text += "=" * 60 + "\n"
            result_text += f"📊 {board1.upper()} vs {board2.upper()}\n"  # type: ignore
            result_text += f"   {board1}: {comparison['board1_total']} defines\n"
            result_text += f"   {board2}: {comparison['board2_total']} defines\n"
            result_text += f"   Common: {comparison['common_count']} defines\n"
            
            if comparison['board1_only']:
                result_text += f"\n🔴 Only in {board1.upper()} ({len(comparison['board1_only'])}):\n"
                for define in comparison['board1_only']:
                    result_text += f"  {define}\n"
            
            if comparison['board2_only']:
                result_text += f"\n🔵 Only in {board2.upper()} ({len(comparison['board2_only'])}):\n"
                for define in comparison['board2_only']:
                    result_text += f"  {define}\n"
            
            if comparison['common']:
                result_text += f"\n🟢 Common Defines ({len(comparison['common'])}):\n"
                for define in comparison['common']:
                    result_text += f"  {define}\n"
        
        return CallToolResult(
            content=[TextContent(type="text", text=result_text)]
        )
    
    # Handle single board analysis
    result_parts: List[str] = []
    
    if show_defines or show_all:
        success, defines, error = analyzer.get_platform_defines(board)
        if not success:
            return CallToolResult(
                content=[TextContent(type="text", text=f"[ERROR] Error getting defines: {error}")],
                isError=True
            )
        
        if output_json:
            import json
            result_parts.append(json.dumps({"defines": defines}, indent=2))
        else:
            result_parts.append(f"📋 Platform Defines for {board.upper()}:")
            result_parts.append("=" * 50)
            for define in defines:
                result_parts.append(f"  {define}")
            result_parts.append(f"\nTotal: {len(defines)} defines")
    
    if show_compiler or show_all:
        success, compiler_info, error = analyzer.get_compiler_info(board)
        if not success:
            return CallToolResult(
                content=[TextContent(type="text", text=f"[ERROR] Error getting compiler info: {error}")],
                isError=True
            )
        
        if output_json:
            import json
            from dataclasses import asdict
            result_parts.append(json.dumps({"compiler": asdict(compiler_info)}, indent=2))
        else:
            result_parts.append(f"\n🔧 Compiler Information for {board.upper()}:")
            result_parts.append("=" * 50)
            result_parts.append(f"Compiler Type: {compiler_info.compiler_type or 'Unknown'}")
            result_parts.append(f"Build Type: {compiler_info.build_type or 'Unknown'}")
            result_parts.append(f"C Compiler: {compiler_info.cc_path or 'Unknown'}")
            result_parts.append(f"C++ Compiler: {compiler_info.cxx_path or 'Unknown'}")
            
            cc_flags = compiler_info.cc_flags
            if cc_flags:
                result_parts.append(f"\nC Flags ({len(cc_flags)}):")
                for flag in cc_flags:
                    result_parts.append(f"  {flag}")
            
            cxx_flags = compiler_info.cxx_flags
            if cxx_flags:
                result_parts.append(f"\nC++ Flags ({len(cxx_flags)}):")
                for flag in cxx_flags:
                    result_parts.append(f"  {flag}")
    
    if show_toolchain or show_all:
        success, aliases, error = analyzer.get_toolchain_aliases(board)
        if not success:
            return CallToolResult(
                content=[TextContent(type="text", text=f"[ERROR] Error getting toolchain aliases: {error}")],
                isError=True
            )
        
        if output_json:
            import json
            result_parts.append(json.dumps({"toolchain": aliases}, indent=2))
        else:
            result_parts.append(f"\n⚙️  Toolchain Aliases for {board.upper()}:")
            result_parts.append("=" * 50)
            for tool, path in aliases.items():
                if path:
                    # Show just the tool name from the path for readability
                    from pathlib import Path as PathLib
                    tool_name = PathLib(path).name if path else "Not available"
                    result_parts.append(f"  {tool:10}: {tool_name}")
                else:
                    result_parts.append(f"  {tool:10}: Not available")
    
    result_text = "\n".join(result_parts)
    
    return CallToolResult(
        content=[TextContent(type="text", text=result_text)]
    )


async def esp32_symbol_analysis(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Run ESP32 symbol analysis to identify optimization opportunities for binary size reduction."""
    import json
    import subprocess
    from pathlib import Path
    
    board = arguments.get("board", "auto")
    example = arguments.get("example", "Blink")
    skip_on_failure = arguments.get("skip_on_failure", True)
    output_json = arguments.get("output_json", False)
    focus_on_fastled = arguments.get("focus_on_fastled", True)
    
    result_text = "# ESP32 Symbol Analysis Report\n\n"
    
    try:
        # Define ESP32 boards
        esp32_boards = ["esp32dev", "esp32", "esp32s2", "esp32s3", "esp32c3", "esp32c6", "esp32h2", "esp32p4", "esp32c2"]
        
        # Find build directory
        build_dir = project_root / ".build"
        if not build_dir.exists():
            return CallToolResult(
                content=[TextContent(type="text", text="Build directory (.build) not found. Please compile an example first.")],
                isError=not skip_on_failure
            )
        
        # Auto-detect board if needed
        if board == "auto":
            detected_boards: List[str] = []
            for esp32_board in esp32_boards:
                candidate_dir = build_dir / esp32_board
                if candidate_dir.exists() and (candidate_dir / "build_info.json").exists():
                    detected_boards.append(esp32_board)
            
            if not detected_boards:
                return CallToolResult(
                    content=[TextContent(type="text", text="No ESP32 boards with build_info.json found in .build directory")],
                    isError=not skip_on_failure
                )
            
            board = detected_boards[0]  # Use first detected board
            if len(detected_boards) > 1:
                result_text += f"**Multiple ESP32 boards detected: {', '.join(detected_boards)}. Using: {board}**\n\n"
        
        # Validate board is ESP32-based
        if not any(esp32_board in board.lower() for esp32_board in esp32_boards):
            return CallToolResult(
                content=[TextContent(type="text", text=f"Board '{board}' is not an ESP32-based board")],
                isError=not skip_on_failure
            )
        
        # Find board directory
        board_dir = build_dir / board
        build_info_path = board_dir / "build_info.json"
        
        if not build_info_path.exists():
            return CallToolResult(
                content=[TextContent(type="text", text=f"Build info not found for board '{board}' at {build_info_path}")],
                isError=not skip_on_failure
            )
        
        # Load build info
        with open(build_info_path) as f:
            build_info = json.load(f)
        
        esp32_info = build_info[board]
        nm_path = esp32_info["aliases"]["nm"]
        cppfilt_path = esp32_info["aliases"]["c++filt"]
        elf_file = esp32_info["prog_path"]
        
        result_text += f"**Board:** {board}\n"
        result_text += f"**Example:** {example}\n"
        result_text += f"**ELF File:** {elf_file}\n"
        result_text += f"**NM Tool:** {nm_path}\n\n"
        
        # Check if ELF file exists
        if not Path(elf_file).exists():
            return CallToolResult(
                content=[TextContent(type="text", text=f"ELF file not found: {elf_file}")],
                isError=not skip_on_failure
            )
        
        result_text += "## Symbol Analysis Results\n\n"
        
        # Run nm command to get symbols
        cmd = [nm_path, "--print-size", "--size-sort", "--radix=d", elf_file]
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            nm_output = result.stdout
        except subprocess.CalledProcessError as e:
            return CallToolResult(
                content=[TextContent(type="text", text=f"Error running nm command: {e.stderr}")],
                isError=not skip_on_failure
            )
        
        # Parse symbol data
        symbols: List[Dict[str, Any]] = []
        fastled_symbols: List[Dict[str, Any]] = []
        large_symbols: List[Dict[str, Any]] = []
        
        for line in nm_output.strip().split("\n"):
            if not line.strip():
                continue
            
            parts = line.split()
            if len(parts) >= 4:
                try:
                    addr = parts[0]
                    size = int(parts[1])
                    symbol_type = parts[2]
                    mangled_name = " ".join(parts[3:])
                    
                    # Demangle symbol if possible
                    try:
                        cmd_demangle = ["echo", mangled_name, "|", cppfilt_path]
                        demangle_result = subprocess.run(
                            f'echo "{mangled_name}" | "{cppfilt_path}"',
                            shell=True, capture_output=True, text=True, check=True
                        )
                        demangled_name = demangle_result.stdout.strip()
                        if demangled_name == mangled_name:
                            demangled_name = mangled_name  # Demangling failed
                    except:
                        demangled_name = mangled_name
                    
                    symbol_info = {
                        "address": addr,
                        "size": size,
                        "type": symbol_type,
                        "name": mangled_name,
                        "demangled_name": demangled_name
                    }
                    
                    symbols.append(symbol_info)
                    
                    # Check if FastLED-related
                    search_text = demangled_name.lower()
                    if any(keyword in search_text for keyword in [
                        "fastled", "cfastled", "crgb", "hsv", "pixel", "controller",
                        "led", "rmt", "strip", "neopixel", "ws2812", "apa102"
                    ]):
                        fastled_symbols.append(symbol_info)
                    
                    # Check if large symbol
                    if size > 100:
                        large_symbols.append(symbol_info)
                        
                except (ValueError, IndexError):
                    continue  # Skip malformed lines
        
        # Generate summary
        total_symbols = len(symbols)
        total_fastled = len(fastled_symbols)
        fastled_size = sum(s["size"] for s in fastled_symbols)
        
        result_text += f"**Summary:**\n"
        result_text += f"- Total symbols: {total_symbols}\n"
        result_text += f"- FastLED symbols: {total_fastled}\n"
        result_text += f"- Total FastLED size: {fastled_size} bytes ({fastled_size/1024:.1f} KB)\n\n"
        
        # Initialize fastled_sorted for later use
        fastled_sorted: List[Dict[str, Any]] = []
        
        if focus_on_fastled and fastled_symbols:
            result_text += "## Largest FastLED Symbols (Optimization Targets)\n\n"
            fastled_sorted = sorted(fastled_symbols, key=lambda x: x["size"], reverse=True)
            
            for i, sym in enumerate(fastled_sorted[:15]):
                display_name = sym["demangled_name"][:80]
                if len(sym["demangled_name"]) > 80:
                    display_name += "..."
                result_text += f"{i+1:2d}. **{sym['size']:,} bytes** - `{display_name}`\n"
            
            if len(fastled_sorted) > 15:
                result_text += f"\n... and {len(fastled_sorted) - 15} more FastLED symbols\n"
        
        result_text += "\n## Largest Overall Symbols\n\n"
        all_large = sorted(large_symbols, key=lambda x: x["size"], reverse=True)
        
        for i, sym in enumerate(all_large[:10]):
            display_name = sym["demangled_name"][:80]
            if len(sym["demangled_name"]) > 80:
                display_name += "..."
            result_text += f"{i+1:2d}. **{sym['size']:,} bytes** - `{display_name}`\n"
        
        # Feature analysis
        result_text += "\n## Feature Analysis & Recommendations\n\n"
        feature_patterns = {
            "JSON functionality": ["json", "Json"],
            "Audio processing": ["audio", "fft", "Audio"],
            "2D effects": ["2d", "noise", "matrix"],
            "Video functionality": ["video", "Video"],
            "UI components": ["ui", "button", "slider"],
            "File system": ["file", "File", "fs_"],
            "Mathematical functions": ["sqrt", "sin", "cos", "math"],
            "String processing": ["string", "str", "String"],
        }
        
        for feature, patterns in feature_patterns.items():
            feature_symbols = [
                s for s in fastled_symbols
                if any(p in s["demangled_name"] for p in patterns)
            ]
            if feature_symbols:
                total_size = sum(s["size"] for s in feature_symbols)
                result_text += f"- **{feature}**: {len(feature_symbols)} symbols, {total_size:,} bytes"
                if total_size > 1000:
                    result_text += f" ⚠️ Could save ~{total_size/1024:.1f} KB if removed"
                result_text += "\n"
        
        # Include JSON output if requested
        if output_json:
            json_data = {
                "summary": {
                    "total_symbols": total_symbols,
                    "fastled_symbols": total_fastled,
                    "fastled_size": fastled_size,
                    "board": board,
                    "example": example
                },
                "largest_fastled": fastled_sorted[:10] if 'fastled_sorted' in locals() else [],
                "largest_overall": all_large[:10]
            }
            result_text += f"\n## JSON Output\n\n```json\n{json.dumps(json_data, indent=2)}\n```\n"
        
        result_text += "\n## Next Steps\n\n"
        result_text += "1. Identify unused features from the analysis above\n"
        result_text += "2. Use conditional compilation to exclude unused code\n"
        result_text += "3. Consider splitting large functions into smaller ones\n"
        result_text += "4. Re-run analysis after optimizations to measure impact\n"
        
        return CallToolResult(
            content=[TextContent(type="text", text=result_text)]
        )
        
    except Exception as e:
        error_msg = f"Error running ESP32 symbol analysis: {str(e)}"
        if skip_on_failure:
            return CallToolResult(
                content=[TextContent(type="text", text=f"{error_msg}\n\n(Skipped due to skip_on_failure=True)")]
            )
        else:
            return CallToolResult(
                content=[TextContent(type="text", text=error_msg)],
                isError=True
            )

async def symbol_analysis(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Run generic symbol analysis for any platform to identify optimization opportunities."""
    board = arguments.get("board", "auto")
    example = arguments.get("example", "Blink")
    output_json = arguments.get("output_json", False)
    run_all_platforms = arguments.get("run_all_platforms", False)
    
    result_text = "# Generic Symbol Analysis Report\n\n"
    
    try:
        if run_all_platforms:
            # Run demo script for all platforms
            result_text += "## Running analysis on ALL available platforms\n\n"
            cmd = ["uv", "run", "ci/demo_symbol_analysis.py"]
            
            try:
                demo_result = await run_command(cmd, project_root)
                result_text += demo_result
            except Exception as e:
                return CallToolResult(
                    content=[TextContent(type="text", text=f"Error running demo symbol analysis: {str(e)}")],
                    isError=True
                )
        else:
            # Run for specific board
            result_text += f"## Symbol Analysis for Platform: {board}\n\n"
            
            cmd = ["uv", "run", "ci/util/symbol_analysis.py", "--board", board]
            
            try:
                analysis_result = await run_command(cmd, project_root)
                result_text += analysis_result
            except Exception as e:
                return CallToolResult(
                    content=[TextContent(type="text", text=f"Error running symbol analysis for {board}: {str(e)}")],
                    isError=True
                )
        
        # Add usage instructions
        result_text += "\n## How to Use Symbol Analysis\n\n"
        result_text += "### Available Commands:\n"
        result_text += "- `uv run -m ci.util.symbol_analysis --board uno` - Analyze UNO platform\n"
        result_text += "- `uv run -m ci.util.symbol_analysis --board esp32dev` - Analyze ESP32 platform\n"
        result_text += "- `uv run -m ci.util.symbol_analysis --board teensy31` - Analyze Teensy platform\n"
        result_text += "- `uv run ci/demo_symbol_analysis.py` - Analyze all available platforms\n\n"
        
        result_text += "### Prerequisites:\n"
        result_text += "1. Compile platform first: `bash compile {board} --examples Blink`\n"
        result_text += "2. Ensure .build/{board}/build_info.json exists\n"
        result_text += "3. Run symbol analysis: `uv run -m ci.util.symbol_analysis --board {board}`\n\n"
        
        result_text += "### Supported Platforms:\n"
        result_text += "- [OK] UNO (AVR) - Small embedded platform\n"
        result_text += "- [OK] ESP32DEV (Xtensa) - WiFi-enabled microcontroller\n" 
        result_text += "- [OK] TEENSY31 (ARM Cortex-M4) - High-performance microcontroller\n"
        result_text += "- [OK] TEENSYLC (ARM Cortex-M0+) - Low-cost ARM platform\n"
        result_text += "- [OK] DIGIX (ARM Cortex-M3) - Arduino Due compatible\n"
        result_text += "- [OK] STM32 (ARM Cortex-M3) - STMicroelectronics platform\n"
        result_text += "- [OK] And many more! Works with any platform that generates build_info.json\n\n"
        
        if output_json:
            result_text += "### JSON Output\n"
            result_text += f"Detailed analysis results saved to: `.build/{board}_symbol_analysis.json`\n\n"
        
        return CallToolResult(
            content=[TextContent(type="text", text=result_text)]
        )
        
    except Exception as e:
        return CallToolResult(
            content=[TextContent(type="text", text=f"Error running symbol analysis: {str(e)}")],
            isError=True
        )

async def validate_arduino_includes(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Validate that no new Arduino.h includes have been added to the codebase."""
    directory = arguments.get("directory", "src")
    include_examples = arguments.get("include_examples", False)
    check_dev = arguments.get("check_dev", False)
    show_approved = arguments.get("show_approved", True)
    
    result_text = "# Arduino.h Include Validation\n\n"
    
    # Define directories to scan
    scan_dirs = [directory]
    if include_examples:
        scan_dirs.append("examples")
    if check_dev:
        scan_dirs.append("dev")
    
    # Known approved includes (from our grep search)
    approved_includes = {
        "src/sensors/digital_pin.hpp": "ok include",
        "src/third_party/arduinojson/json.hpp": "ok include", 
        "src/lib8tion.cpp": "ok include",
        "src/led_sysdefs.h": "ok include",
        "src/platforms/arm/rp2040/led_sysdefs_arm_rp2040.h": True,
        # WASM platform includes are specifically for the WASM platform
        "src/platforms/wasm/led_sysdefs_wasm.h": True,
        "src/platforms/wasm/clockless.h": True,
        "src/platforms/wasm/compiler/Arduino.cpp": True,
        "src/FastLED.h": True,  # References WASM Arduino.h
    }
    
    all_includes: List[Dict[str, Any]] = []
    violations: List[Dict[str, Any]] = []
    approved_count = 0
    
    try:
        for scan_dir in scan_dirs:
            scan_path = project_root / scan_dir
            if not scan_path.exists():
                result_text += f"⚠️ Directory not found: {scan_dir}\n"
                continue
                
            result_text += f"📁 Scanning directory: {scan_dir}\n"
            
            # Use ripgrep to find Arduino.h includes
            try:
                cmd = ["rg", "--type", "cpp", "--type", "c", r"#include.*Arduino\.h", str(scan_path)]
                output = await run_command(cmd, project_root)
                
                for line in output.strip().split('\n'):
                    if not line.strip():
                        continue
                        
                    # Parse ripgrep output: filename:line_number:content
                    parts = line.split(':', 2)
                    if len(parts) >= 3:
                        file_path = parts[0]
                        line_number = parts[1]
                        content = parts[2].strip()
                        
                        # Make path relative to project root
                        rel_path = str(Path(file_path).relative_to(project_root))
                        
                        include_info = {
                            'file': rel_path,
                            'line': line_number,
                            'content': content,
                            'approved': False
                        }
                        
                        # Check if this is an approved include
                        if rel_path in approved_includes:
                            include_info['approved'] = True
                            approved_count += 1
                        elif "// ok include" in content:
                            include_info['approved'] = True
                            approved_count += 1
                        elif content.startswith('//') or content.startswith('/*'):
                            # Commented out includes are not violations
                            include_info['approved'] = True  
                            approved_count += 1
                        else:
                            violations.append(include_info)
                        
                        all_includes.append(include_info)
                        
            except Exception as e:
                result_text += f"⚠️ Error scanning {scan_dir}: {e}\n"
    
    except Exception as e:
        return CallToolResult(
            content=[TextContent(type="text", text=f"Error validating Arduino includes: {e}")],
            isError=True
        )
    
    # Generate report
    result_text += f"\n## Summary\n"
    result_text += f"- **Total Arduino.h includes found:** {len(all_includes)}\n"
    result_text += f"- **Approved includes:** {approved_count}\n"
    result_text += f"- **🚨 VIOLATIONS:** {len(violations)}\n\n"
    
    if violations:
        result_text += "## 🚨 CRITICAL VIOLATIONS FOUND 🚨\n\n"
        result_text += "The following files contain PROHIBITED Arduino.h includes:\n\n"
        
        for violation in violations:
            result_text += f"[ERROR] **{violation['file']}:{violation['line']}**\n"
            result_text += f"   `{violation['content']}`\n\n"
        
        result_text += "## 🚨 IMMEDIATE ACTION REQUIRED 🚨\n\n"
        result_text += "These Arduino.h includes MUST be removed or replaced with FastLED alternatives:\n\n"
        result_text += "1. **Remove the Arduino.h include**\n"
        result_text += "2. **Use FastLED platform abstractions** from `src/platforms/`\n"
        result_text += "3. **Replace Arduino functions** with `fl::` namespace equivalents\n"
        result_text += "4. **If absolutely necessary**, mark with `// ok include` and document why\n\n"
        
        is_error = True
    else:
        result_text += "[OK] **NO VIOLATIONS FOUND**\n\n"
        result_text += "All Arduino.h includes are properly approved or commented out.\n\n"
        is_error = False
    
    if show_approved and approved_count > 0:
        result_text += "## Approved Arduino.h Includes\n\n"
        result_text += "These includes are pre-approved and should not be modified:\n\n"
        
        for include in all_includes:
            if include['approved']:
                result_text += f"[OK] {include['file']}:{include['line']}\n"
                result_text += f"   `{include['content']}`\n\n"
    
    result_text += "---\n\n"
    result_text += "**Remember:** Never add new Arduino.h includes. Use FastLED's platform abstraction layer instead!\n"
    
    return CallToolResult(
        content=[TextContent(type="text", text=result_text)],
        isError=is_error
    )

async def run_fastled_web_compiler(arguments: Dict[str, Any], project_root: Path) -> CallToolResult:
    """Run FastLED web compiler with playwright console.log capture."""
    
    # Check if this is a background agent and refuse to run
    import os
    if os.environ.get('FASTLED_CI_NO_INTERACTIVE') == 'true' or os.environ.get('CI') == 'true':
        return CallToolResult(
            content=[TextContent(type="text", text="🚫 FastLED Web Compiler is disabled for background agents. This tool is only available for foreground agents with interactive environments.")],
            isError=True
        )
    
    example_path = arguments.get("example_path", "examples/Audio")
    capture_duration = arguments.get("capture_duration", 30)
    headless = arguments.get("headless", False)
    port = arguments.get("port", 0)
    docker_check = arguments.get("docker_check", True)
    save_screenshot = arguments.get("save_screenshot", True)
    
    # Check prerequisites
    result_text = "🌐 FastLED Web Compiler with Console.log Capture\n"
    result_text += "=" * 50 + "\n\n"
    
    # Check if fastled command is available
    if not shutil.which("fastled"):
        return CallToolResult(
            content=[TextContent(type="text", text="[ERROR] FastLED command not found. Please install with: pip install fastled")],
            isError=True
        )
    
    result_text += "[OK] FastLED command found\n"
    
    # Check if Docker is available (optional)
    docker_available = shutil.which("docker") is not None
    if docker_check:
        if docker_available:
            result_text += "[OK] Docker available (faster compilation)\n"
        else:
            result_text += "⚠️  Docker not available (slower compilation)\n"
    
    # Check if playwright is available
    try:
        from playwright.async_api import async_playwright
        result_text += "[OK] Playwright available\n"
    except ImportError:
        return CallToolResult(
            content=[TextContent(type="text", text="[ERROR] Playwright not found. Please install with: pip install playwright")],
            isError=True
        )
    
    # Validate example path
    example_dir = project_root / example_path
    if not example_dir.exists():
        return CallToolResult(
            content=[TextContent(type="text", text=f"[ERROR] Example directory not found: {example_path}")],
            isError=True
        )
    
    result_text += f"[OK] Example directory found: {example_path}\n\n"
    
    # Install playwright browsers
    result_text += "📦 Installing Playwright browsers...\n"
    try:
        install_result = subprocess.run(
            [sys.executable, "-m", "playwright", "install", "chromium"],
            capture_output=True, text=True, cwd=project_root
        )
        if install_result.returncode != 0:
            result_text += f"⚠️  Playwright browser installation warning: {install_result.stderr}\n"
        else:
            result_text += "[OK] Playwright browsers installed\n"
    except Exception as e:
        result_text += f"⚠️  Playwright browser installation error: {e}\n"
    
    # Run fastled compiler
    result_text += f"\n🔧 Compiling {example_path} with FastLED...\n"
    
    # Store original directory before trying operations
    original_cwd = Path.cwd()
    
    try:
        # Change to example directory
        os.chdir(example_dir)
        
        # Run fastled command
        compile_result = subprocess.run(
            ["fastled", "--just-compile", "."],
            capture_output=True, text=True, timeout=300  # 5 minute timeout
        )
        
        if compile_result.returncode != 0:
            os.chdir(original_cwd)
            return CallToolResult(
                content=[TextContent(type="text", text=f"[ERROR] FastLED compilation failed:\n{compile_result.stderr}")],
                isError=True
            )
        
        result_text += "[OK] FastLED compilation successful\n"
        result_text += f"Compilation output:\n{compile_result.stdout}\n\n"
        
        # Check for generated files
        fastled_js_dir = example_dir / "fastled_js"
        if not fastled_js_dir.exists():
            os.chdir(original_cwd)
            return CallToolResult(
                content=[TextContent(type="text", text="[ERROR] FastLED output directory not found: fastled_js")],
                isError=True
            )
        
        required_files = ["fastled.js", "fastled.wasm", "index.html"]
        missing_files = [f for f in required_files if not (fastled_js_dir / f).exists()]
        if missing_files:
            os.chdir(original_cwd)
            return CallToolResult(
                content=[TextContent(type="text", text=f"[ERROR] Missing required files: {missing_files}")],
                isError=True
            )
        
        result_text += f"[OK] All required files generated in {fastled_js_dir}\n\n"
        
        # Start HTTP server and playwright
        result_text += "🌐 Starting web server and browser automation...\n"
        
        # Use Python's built-in HTTP server
        import http.server
        import socketserver
        from threading import Thread
        import socket
        
        # Find available port
        if port == 0:
            sock = socket.socket()
            sock.bind(('', 0))
            port = sock.getsockname()[1]
            sock.close()
        
        # Start HTTP server in thread
        os.chdir(fastled_js_dir)
        handler = http.server.SimpleHTTPRequestHandler
        httpd = socketserver.TCPServer(("", port), handler)
        server_thread = Thread(target=httpd.serve_forever)
        server_thread.daemon = True
        server_thread.start()
        
        result_text += f"[OK] HTTP server started on port {port}\n"
        
        # Run playwright automation
        console_logs: List[str] = []
        error_logs: List[str] = []
        
        async def run_playwright():
            async with async_playwright() as p:
                browser = await p.chromium.launch(headless=headless)
                page = await browser.new_page()
                
                # Setup console log capture
                def handle_console(msg: Any):
                    timestamp = time.strftime("%H:%M:%S")
                    log_entry = f"[{timestamp}] {msg.type}: {msg.text}"
                    console_logs.append(log_entry)
                    if msg.type in ["error", "warning"]:
                        error_logs.append(log_entry)
                
                page.on("console", handle_console)
                
                # Setup error handlers
                page.on("pageerror", lambda err: error_logs.append(f"Page Error: {err}"))
                
                try:
                    # Navigate to the page
                    await page.goto(f"http://localhost:{port}", timeout=30000)
                    
                    # Wait for FastLED to initialize
                    await page.evaluate("""
                        window.frameCallCount = 0;
                        window.consoleLogCount = 0;
                        globalThis.FastLED_onFrame = (jsonStr) => {
                            console.log('FastLED_onFrame called:', jsonStr);
                            window.frameCallCount++;
                        };
                    """)
                    
                    # Wait for page to load and run
                    await page.wait_for_timeout(5000)
                    
                    # Check if FastLED initialized
                    frame_count = await page.evaluate("window.frameCallCount || 0")
                    
                    # Capture for specified duration
                    await page.wait_for_timeout(capture_duration * 1000)
                    
                    # Take screenshot if requested
                    screenshot_path = None
                    if save_screenshot:
                        screenshot_path = fastled_js_dir / f"fastled_capture_{int(time.time())}.png"
                        await page.screenshot(path=str(screenshot_path))
                    
                    return frame_count, screenshot_path
                    
                finally:
                    await browser.close()
        
        # Run the playwright automation
        frame_count, screenshot_path = await run_playwright()
        
        # Stop HTTP server
        httpd.shutdown()
        
        # Generate report
        result_text += f"\n📊 Capture Results ({capture_duration}s):\n"
        result_text += f"   • FastLED_onFrame calls: {frame_count}\n"
        result_text += f"   • Console log entries: {len(console_logs)}\n"
        result_text += f"   • Error/Warning logs: {len(error_logs)}\n"
        
        if screenshot_path:
            result_text += f"   • Screenshot saved: {screenshot_path.name}\n"
        
        result_text += "\n📋 Console Logs:\n"
        result_text += "-" * 40 + "\n"
        
        if console_logs:
            for log in console_logs[-20:]:  # Show last 20 logs
                result_text += f"{log}\n"
            if len(console_logs) > 20:
                result_text += f"... ({len(console_logs) - 20} more logs)\n"
        else:
            result_text += "No console logs captured\n"
        
        if error_logs:
            result_text += "\n[ERROR] Errors/Warnings:\n"
            result_text += "-" * 40 + "\n"
            for error in error_logs:
                result_text += f"{error}\n"
        
        result_text += "\n[OK] FastLED web compiler execution completed successfully!\n"
        
        # Analysis
        if frame_count > 0:
            result_text += f"\n🎯 Analysis: FastLED is running correctly ({frame_count} frames rendered)\n"
        else:
            result_text += "\n⚠️  Analysis: FastLED may not be initializing properly (no frames detected)\n"
        
        return CallToolResult(
            content=[TextContent(type="text", text=result_text)]
        )
        
    except subprocess.TimeoutExpired:
        return CallToolResult(
            content=[TextContent(type="text", text="[ERROR] FastLED compilation timed out (5 minutes)")],
            isError=True
        )
    except Exception as e:
        return CallToolResult(
            content=[TextContent(type="text", text=f"[ERROR] Error running FastLED web compiler: {str(e)}")],
            isError=True
        )
    finally:
        # Restore original directory
        try:
            os.chdir(original_cwd)
        except:
            pass

async def run_command(cmd: List[str], cwd: Path) -> str:
    """Run a shell command and return its output."""
    try:
        process = await asyncio.create_subprocess_exec(
            *cmd,
            cwd=cwd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT
        )
        
        stdout_bytes, _ = await process.communicate()
        stdout = stdout_bytes.decode('utf-8', errors='replace') if stdout_bytes else ""
        
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
