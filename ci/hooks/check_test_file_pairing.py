#!/usr/bin/env python3
"""
PreToolUse hook for Write: enforce test-file-to-header pairing.

When creating a NEW .cpp or .hpp file under tests/ (except tests/misc/),
verify that a corresponding header exists in src/ with the same
relative path. For example:

    tests/fl/async.cpp  ->  src/fl/async.h
    tests/fl/eorder.hpp ->  src/fl/eorder.h

If no matching header is found, the hook blocks the Write and
instructs the agent to find an existing test file instead.

Exit codes:
- 0: Allow the write
- 2: Block the write (error message sent to agent)
"""

import json
import os
import sys


def main():
    try:
        input_data = json.load(sys.stdin)
    except json.JSONDecodeError:
        sys.exit(0)

    tool_name = input_data.get("tool_name", "")
    if tool_name != "Write":
        sys.exit(0)

    tool_input = input_data.get("tool_input", {})
    file_path = tool_input.get("file_path", "")
    if not file_path:
        sys.exit(0)

    # Normalize to forward slashes
    file_path = file_path.replace("\\", "/")

    # Find project root (directory containing .claude/)
    project_root = os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ).replace("\\", "/")

    # Get path relative to project root
    # Handle both absolute and relative paths
    if file_path.startswith(project_root):
        rel_path = file_path[len(project_root) :].lstrip("/")
    else:
        # Try normalizing both paths
        norm_file = os.path.normpath(file_path).replace("\\", "/")
        norm_root = os.path.normpath(project_root).replace("\\", "/")
        if norm_file.startswith(norm_root):
            rel_path = norm_file[len(norm_root) :].lstrip("/")
        else:
            # Not under project root, allow
            sys.exit(0)

    # Only check files under tests/
    if not rel_path.startswith("tests/"):
        sys.exit(0)

    # Only check .cpp and .hpp files
    if rel_path.endswith(".cpp"):
        ext = ".cpp"
    elif rel_path.endswith(".hpp"):
        ext = ".hpp"
    else:
        sys.exit(0)

    # Exempt tests/misc/
    if rel_path.startswith("tests/misc/"):
        sys.exit(0)

    # If the file already exists on disk, it's an overwrite, not a new file
    full_path = os.path.join(project_root, rel_path)
    if os.path.exists(full_path):
        sys.exit(0)

    # Extract relative path within tests/  e.g. "fl/async.cpp"
    inner_path = rel_path[len("tests/") :]
    # Strip extension  e.g. "fl/async"
    stem = inner_path[: -len(ext)]

    # Check for corresponding header in src/
    header_path = f"src/{stem}.h"
    header_full = os.path.join(project_root, header_path)

    if os.path.exists(header_full):
        # Matching header found, allow the write
        sys.exit(0)

    # No matching header - block with helpful error message
    error_lines = [
        f"BLOCKED: Creating unpaired test file '{rel_path}'.",
        f"",
        f"No matching header found at '{header_path}'.",
        f"",
        f"Test files under tests/ must correspond to a header in src/ with the same path:",
        f"  tests/fl/foo.cpp  ->  src/fl/foo.h",
        f"  tests/fl/stl/bar.cpp  ->  src/fl/stl/bar.h",
        f"",
        f"What to do instead:",
        f"  1. Find an EXISTING test file that tests the header you're working with.",
        f"     Look for a test whose path matches the source header's path.",
        f"  2. Edit that existing test file to add your new test cases.",
        f"",
        f"Escape hatch (intentional unpaired test):",
        f"  Create the file under tests/misc/ first, then move it to the final location.",
        f"  Example: Write to 'tests/misc/{os.path.basename(inner_path)}' instead.",
    ]
    print("\n".join(error_lines), file=sys.stderr)
    sys.exit(2)


if __name__ == "__main__":
    main()
