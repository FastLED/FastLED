#!/usr/bin/env python3
"""JSON-RPC handler for FL::Remote communication.

This module provides utilities for sending JSON-RPC commands to devices
and parsing JSON responses prefixed with "REMOTE: ".
"""

import json
import re
from pathlib import Path
from typing import Any, cast

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


class JsonRpcHandler:
    """Handler for JSON-RPC communication with FL::Remote devices.

    Parses and stores JSON responses that are prefixed with "REMOTE: ".
    """

    REMOTE_PREFIX = "REMOTE: "

    def __init__(self):
        """Initialize the JSON-RPC handler."""
        self.responses: list[dict[str, Any]] = []
        self.raw_lines: list[str] = []

    def process_line(self, line: str) -> dict[str, Any] | None:
        """Process a line of output, extracting JSON-RPC responses.

        Args:
            line: Line of text from serial output

        Returns:
            Parsed JSON object if line contains valid RPC response, None otherwise
        """
        # Check if line starts with REMOTE: prefix
        if not line.startswith(self.REMOTE_PREFIX):
            return None

        # Extract JSON portion (everything after prefix)
        json_str = line[len(self.REMOTE_PREFIX) :].strip()
        self.raw_lines.append(json_str)

        # Parse JSON
        try:
            response = json.loads(json_str)
            self.responses.append(response)
            return response
        except json.JSONDecodeError as e:
            print(f"⚠️  Warning: Failed to parse REMOTE JSON: {e}")
            print(f"   Raw line: {json_str}")
            return None

    def get_responses(self) -> list[dict[str, Any]]:
        """Get all parsed JSON-RPC responses.

        Returns:
            List of parsed JSON response objects
        """
        return self.responses

    def get_response_by_function(self, function_name: str) -> dict[str, Any] | None:
        """Get the first response for a specific function.

        Args:
            function_name: Name of the RPC function

        Returns:
            First matching response object, or None if not found
        """
        for response in self.responses:
            if response.get("function") == function_name:
                return response
        return None

    def get_all_responses_by_function(self, function_name: str) -> list[dict[str, Any]]:
        """Get all responses for a specific function.

        Args:
            function_name: Name of the RPC function

        Returns:
            List of matching response objects
        """
        return [r for r in self.responses if r.get("function") == function_name]

    def clear(self) -> None:
        """Clear all stored responses."""
        self.responses.clear()
        self.raw_lines.clear()


def parse_json_rpc_commands(json_rpc_arg: str | None) -> list[dict[str, Any]]:
    """Parse JSON-RPC commands from CLI argument.

    Supports three formats:
    1. JSON string: '{"function":"ping","args":[]}'
    2. JSON array: '[{"function":"ping"},{"function":"getConfig"}]'
    3. File path: '@commands.json' or '@/path/to/commands.json'

    Args:
        json_rpc_arg: JSON-RPC argument from CLI (None if not provided)

    Returns:
        List of JSON-RPC command objects

    Raises:
        ValueError: If JSON is invalid or file not found
    """
    if not json_rpc_arg:
        return []

    # Check if argument is a file path (starts with @)
    if json_rpc_arg.startswith("@"):
        file_path = Path(json_rpc_arg[1:])
        if not file_path.exists():
            raise ValueError(f"JSON-RPC file not found: {file_path}")

        try:
            with open(file_path, "r", encoding="utf-8") as f:
                content = f.read()
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise  # Always re-raise KeyboardInterrupt
        except Exception as e:
            raise ValueError(f"Failed to read JSON-RPC file {file_path}: {e}")

        json_str = content
    else:
        json_str = json_rpc_arg

    # Parse JSON
    try:
        parsed = json.loads(json_str)
    except json.JSONDecodeError as e:
        raise ValueError(f"Invalid JSON in RPC commands: {e}")

    # Normalize to list format
    commands: list[dict[str, Any]]
    if isinstance(parsed, dict):
        # Single command object
        commands = [cast(dict[str, Any], parsed)]
    elif isinstance(parsed, list):
        # Array of commands - cast to proper type since JSON parsing returns Any
        commands = cast(list[dict[str, Any]], parsed)
    else:
        raise ValueError(
            f"JSON-RPC commands must be object or array, got {type(parsed).__name__}"
        )

    # Validate each command has required fields
    for i, cmd in enumerate(commands):
        if not isinstance(cmd, dict):
            raise ValueError(f"Command {i} must be an object, got {type(cmd).__name__}")
        if "function" not in cmd:
            raise ValueError(f"Command {i} missing required field 'function'")

        # Add default empty args if not provided
        if "args" not in cmd:
            cmd["args"] = []

    return commands


def format_json_rpc_command(command: dict[str, Any]) -> str:
    """Format a JSON-RPC command for transmission.

    Args:
        command: JSON-RPC command object

    Returns:
        JSON string representation (single line, compact)
    """
    return json.dumps(command, separators=(",", ":"))
