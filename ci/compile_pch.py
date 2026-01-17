from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Wrapper script for PCH compilation that fixes dependency file generation.

Problem: When compiling a PCH with `-MD -MF`, the compiler generates a depfile
that references a temporary .obj file in the Zig cache, not the PCH output file.
This causes Ninja to ignore the depfile and always rebuild the PCH.

Solution: After compilation, rewrite the depfile to reference the actual PCH
output file instead of the temporary object file.

Usage:
    python compile_pch.py <compiler> <args...>

The script expects:
    - '-o' flag followed by the PCH output path
    - '-MF' flag followed by the depfile path
"""

import subprocess
import sys
from pathlib import Path


def fix_depfile(depfile_path: Path, pch_output_path: Path) -> None:
    r"""
    Rewrite the depfile to reference the PCH output instead of temporary obj file.

    The compiler-generated depfile has format:
        C:\path\to\temp.obj: \
          dep1.h \
          dep2.h

    We need to change it to:
        tests/output.pch: \
          dep1.h \
          dep2.h

    Args:
        depfile_path: Path to the .d dependency file
        pch_output_path: Path to the .pch output file
    """
    if not depfile_path.exists():
        # Depfile wasn't generated, nothing to fix
        return

    # Read the depfile
    content = depfile_path.read_text(encoding="utf-8")

    # Find the colon that marks the end of the target
    # On Windows, paths like "C:\..." contain colons, so we need to distinguish
    # between drive letter colons ("C:\") and the target:dependency separator ("obj: ")
    #
    # The separator is "obj: \" where the backslash is a line continuation character
    # Windows drive letters appear as ":\\" (colon-backslash-backslash in paths)

    separator_idx = -1
    idx = 0
    while True:
        # Look for ": " pattern
        space_idx = content.find(": ", idx)
        if space_idx == -1:
            # No more ": " patterns found
            break

        # Check if this is a Windows drive letter (preceded by single letter and followed by backslash)
        # Pattern: "C:\" vs "obj: \"
        is_drive_letter = False
        if space_idx > 0:
            char_before = content[space_idx - 1]
            # Check if preceded by a single letter and followed by backslash
            if char_before.isalpha() and space_idx + 2 < len(content):
                if content[space_idx + 1] == "\\" and (
                    space_idx == 1 or not content[space_idx - 2].isalnum()
                ):
                    # This looks like "C:\" at start or after non-alphanumeric
                    is_drive_letter = True

        if not is_drive_letter:
            # This is the target:dependency separator
            separator_idx = space_idx
            break

        # Keep looking after this position
        idx = space_idx + 1

    if separator_idx == -1:
        # Couldn't find separator, leave depfile as-is
        return

    # Everything after the separator (including ": ") is the dependencies
    dependencies = content[separator_idx:]

    # Create new depfile with correct target
    # Use forward slashes for Ninja compatibility on Windows
    pch_output_str = str(pch_output_path).replace("\\", "/")
    new_content = f"{pch_output_str}{dependencies}"

    # Write the fixed depfile
    depfile_path.write_text(new_content, encoding="utf-8")


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: compile_pch.py <compiler> <args...>", file=sys.stderr)
        return 1

    # Parse command line arguments to find -o and -MF flags
    args = sys.argv[1:]
    pch_output: Path | None = None
    depfile: Path | None = None

    i = 0
    while i < len(args):
        if args[i] == "-o" and i + 1 < len(args):
            pch_output = Path(args[i + 1])
            i += 2
        elif args[i] == "-MF" and i + 1 < len(args):
            depfile = Path(args[i + 1])
            i += 2
        else:
            i += 1

    # Run the compiler
    result = subprocess.run(args, check=False)

    # Fix the depfile if both paths were found and compilation succeeded
    if result.returncode == 0 and pch_output and depfile:
        try:
            fix_depfile(depfile, pch_output)
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"ERROR fixing PCH depfile: {e}", file=sys.stderr)
            import traceback

            traceback.print_exc(file=sys.stderr)
            # Continue even if depfile fix fails - the build might still work

    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
