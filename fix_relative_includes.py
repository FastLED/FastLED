#!/usr/bin/env python3
"""
fix_relative_includes.py - Automatically fix relative includes in C++ files

This script replaces relative includes (paths containing "..") with absolute
includes based on the file's location in the project structure.
"""

import re
import sys
from pathlib import Path
from typing import List, Tuple


def fix_include_path(file_path: Path, include_line: str) -> str:
    """
    Convert a relative include to an absolute include from src/.

    Args:
        file_path: Path to the file containing the include
        include_line: The include line to fix

    Returns:
        Fixed include line
    """
    # Extract the include path and quote/bracket type
    match = re.match(r'^(\s*#\s*include\s+)([<"])([^>"]+)([>"])', include_line)
    if not match:
        return include_line

    prefix, open_delim, include_path, close_delim = match.groups()

    # Skip if no ".." in path
    if ".." not in include_path:
        return include_line

    # Calculate the absolute path from src/
    file_dir = file_path.parent

    # Resolve the include path relative to the file's directory
    try:
        # Get relative path from src/
        src_dir = Path("C:/Users/niteris/dev/fastled8/src")
        file_rel_to_src = file_dir.relative_to(src_dir)

        # Resolve the include path
        resolved_path = (file_rel_to_src / include_path).resolve()

        # Get path relative to src/
        try:
            final_path = resolved_path.relative_to(src_dir.resolve())
            fixed_path = str(final_path).replace("\\", "/")
            return f"{prefix}{open_delim}{fixed_path}{close_delim}"
        except ValueError:
            # Path is outside src/, keep as is
            return include_line
    except Exception:
        return include_line


def fix_file(file_path: Path) -> Tuple[bool, int]:
    """
    Fix all relative includes in a file.

    Returns:
        (was_modified, num_fixes)
    """
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"Error reading {file_path}: {e}", file=sys.stderr)
        return False, 0

    modified = False
    num_fixes = 0
    new_lines: List[str] = []

    for line in lines:
        # Check if this is an include line with ".."
        if re.match(r'^\s*#\s*include\s+[<"][^>"]*\.\.[^>"]*[>"]', line):
            fixed_line = fix_include_path(file_path, line.rstrip('\n'))
            if fixed_line != line.rstrip('\n'):
                new_lines.append(fixed_line + '\n')
                modified = True
                num_fixes += 1
                print(f"  {file_path.name}:{len(new_lines)} Fixed: {line.rstrip()} -> {fixed_line}")
            else:
                new_lines.append(line)
        else:
            new_lines.append(line)

    if modified:
        try:
            with open(file_path, 'w', encoding='utf-8', newline='') as f:
                f.writelines(new_lines)
            return True, num_fixes
        except Exception as e:
            print(f"Error writing {file_path}: {e}", file=sys.stderr)
            return False, 0

    return False, 0


def main() -> int:
    """Main entry point."""
    # List of all files that need fixing from cpp_lint output
    files_to_fix = [
        "src/fl/crgb_hsv8.cpp",
        "src/fl/fastled.h",
        "src/fl/fastled_internal.cpp",
        "src/fl/fastpin.h",
        "src/fl/pixel_iterator.h",
        "src/fl/rgb8.h",
        "src/platforms/apollo3/clockless_apollo3.h",
        "src/platforms/arm/d21/clockless_arm_d21.h",
        "src/platforms/arm/d51/fastled_arm_d51.h",
        "src/platforms/arm/giga/fastled_arm_giga.h",
        "src/platforms/arm/k20/octows2811_controller.h",
        "src/platforms/arm/kl26/clockless_arm_kl26.h",
        "src/platforms/arm/nrf51/clockless_arm_nrf51.h",
        "src/platforms/arm/renesas/fastled_arm_renesas.h",
        "src/platforms/arm/rp/rp2040/clockless_arm_rp2040.h",
        "src/platforms/arm/rp/rp2040/led_sysdefs_arm_rp2040.h",
        "src/platforms/arm/rp/rp2350/clockless_arm_rp2350.h",
        "src/platforms/arm/rp/rp2350/led_sysdefs_arm_rp2350.h",
        "src/platforms/arm/rp/rpcommon/clockless_rp_pio.h",
        "src/platforms/arm/sam/fastspi_arm_sam.h",
        "src/platforms/arm/teensy/teensy31_32/octows2811_controller.h",
        "src/platforms/arm/teensy/teensy36/fastled_arm_k66.h",
        "src/platforms/arm/teensy/teensy3_common/spi_output_template.h",
        "src/platforms/arm/teensy/teensy4_common/fastled_arm_mxrt1062.h",
        "src/platforms/arm/teensy/teensy_lc/clockless_arm_kl26.h",
        "src/platforms/arm/teensy/teensy_lc/fastled_arm_kl26.h",
        "src/platforms/arm/teensy/teensy_lc/fastspi_arm_kl26.h",
        "src/platforms/avr/clockless_trinket.h",
        "src/platforms/esp/int.h",
        "src/platforms/intmap.h",
        "src/third_party/libhelix_mp3/real/coder.h",
        "src/third_party/libnsgif/src/gif.cpp",
    ]

    total_files_modified = 0
    total_fixes = 0

    for file_str in files_to_fix:
        file_path = Path(file_str)
        if not file_path.exists():
            print(f"Warning: File not found: {file_path}", file=sys.stderr)
            continue

        was_modified, num_fixes = fix_file(file_path)
        if was_modified:
            total_files_modified += 1
            total_fixes += num_fixes

    print(f"\nFixed {total_fixes} includes in {total_files_modified} files")
    return 0


if __name__ == '__main__':
    sys.exit(main())
