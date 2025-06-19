#!/usr/bin/env -S uv run --script

# /// script
# dependencies = [
#   "mcp",
# ]
# ///

"""
FastLED MCP Server - Provides tools for working with the FastLED project
"""

import asyncio
import json
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence

from mcp import ClientSession, StdioServerTransport
from mcp.server import Server
from mcp.types import (
    CallToolRequest,
    CallToolResult,
    ListToolsRequest,
    TextContent,
    Tool,
)

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
                        "description": "Name of specific C++ test to run (when test_type is 'specific')"
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
                    }
                },
                "required": ["test_type"]
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
        )
    ]

@server.call_tool()
async def call_tool(name: str, arguments: Dict[str, Any]) -> CallToolResult:
    """Handle tool calls."""
    
    project_root = Path(__file__).parent
    
    try:
        if name == "run_tests":
            return await run_tests(arguments, project_root)
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
    use_clang = arguments.get("use_clang", False)
    clean = arguments.get("clean", False)
    
    cmd = ["uv", "run", "test.py"]
    
    if test_type == "cpp":
        cmd.append("--cpp")
    
    if specific_test and test_type == "specific":
        cmd.extend(["--cpp", specific_test])
    
    if use_clang:
        cmd.append("--clang")
    
    if clean:
        cmd.append("--clean")
    
    result = await run_command(cmd, project_root)
    return CallToolResult(
        content=[TextContent(type="text", text=result)]
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
        except:
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
    transport = StdioServerTransport()
    
    async with transport.connect_sse(server.create_session, server.list_capabilities) as streams:
        await server.run(streams[0], streams[1], server.create_session)

if __name__ == "__main__":
    asyncio.run(main()) 