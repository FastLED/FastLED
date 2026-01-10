#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Any

from ci.util.bin_2_elf import bin_to_elf
from ci.util.elf import dump_symbol_sizes
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.map_dump import map_dump
from ci.util.symbol_analysis import SymbolInfo, analyze_symbols


def cpp_filt(cpp_filt_path: Path, input_text: str) -> str:
    """
    Demangle C++ symbols using c++filt.

    Args:
        cpp_filt_path (Path): Path to c++filt executable.
        input_text (str): Text to demangle.

    Returns:
        str: Demangled text.
    """
    if not cpp_filt_path.exists():
        raise FileNotFoundError(f"cppfilt not found at '{cpp_filt_path}'")
    command = [str(cpp_filt_path), "-t", "-n"]
    process = subprocess.Popen(
        command,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    stdout, stderr = process.communicate(input=input_text)
    if process.returncode != 0:
        raise RuntimeError(f"Error running c++filt: {stderr}")
    return stdout


def demangle_gnu_linkonce_symbols(cpp_filt_path: Path, map_text: str) -> str:
    """
    Demangle .gnu.linkonce.t symbols in the map file.

    Args:
        cpp_filt_path (Path): Path to c++filt executable.
        map_text (str): Content of the map file.

    Returns:
        str: Map file content with demangled symbols.
    """
    # Extract all .gnu.linkonce.t symbols
    pattern = r"\.gnu\\.linkonce\\.t\\.(.+?)\\s"
    matches = re.findall(pattern, map_text)

    if not matches:
        return map_text

    # Create a block of text with the extracted symbols
    symbols_block = "\n".join(matches)

    # Demangle the symbols
    demangled_block = cpp_filt(cpp_filt_path, symbols_block)

    # Create a dictionary of mangled to demangled symbols
    demangled_dict = dict(zip(matches, demangled_block.strip().split("\n")))

    # Replace the mangled symbols with demangled ones in the original text
    for mangled, demangled in demangled_dict.items():
        map_text = map_text.replace(
            f".gnu.linkonce.t.{mangled}", f".gnu.linkonce.t.{demangled}"
        )

    return map_text


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert a binary file to ELF using map file."
    )
    parser.add_argument("--first", action="store_true", help="Inspect the first board")
    parser.add_argument("--cwd", type=Path, help="Custom working directory")

    return parser.parse_args()


def load_build_info(build_info_path: Path) -> dict[str, Any]:
    """
    Load build information from a JSON file.

    Args:
        build_info_path (Path): Path to the build_info.json file.

    Returns:
        dict: Parsed JSON data.
    """
    if not build_info_path.exists():
        raise FileNotFoundError(f"Build info JSON not found at '{build_info_path}'")
    return json.loads(build_info_path.read_text())


def display_function_sizes(
    symbols: list[SymbolInfo], board_name: str, top_n: int = 50
) -> None:
    """
    Display per-function code sizes from the final binary (after tree-shaking).

    Args:
        symbols: List of symbol information from the ELF analysis
        board_name: Name of the board being analyzed
        top_n: Number of top functions to display
    """
    # Filter for symbols with size > 0 and sort by size descending
    sized_symbols = [s for s in symbols if s.size > 0]
    sized_symbols.sort(key=lambda x: x.size, reverse=True)

    # Calculate statistics
    total_size = sum(s.size for s in sized_symbols)
    total_symbols = len(sized_symbols)

    # Type breakdown
    type_counts: dict[str, int] = {}
    type_sizes: dict[str, int] = {}
    for sym in sized_symbols:
        sym_type = sym.type
        type_counts[sym_type] = type_counts.get(sym_type, 0) + 1
        type_sizes[sym_type] = type_sizes.get(sym_type, 0) + sym.size

    print("\n" + "=" * 80)
    print(f"PER-FUNCTION CODE SIZE ANALYSIS - {board_name.upper()}")
    print("(Functions from final binary after tree-shaking)")
    print("=" * 80)
    print(f"\nTotal symbols with size: {total_symbols}")
    print(f"Total code size: {total_size:,} bytes ({total_size / 1024:.1f} KB)")

    print("\nSymbol Type Breakdown:")
    for sym_type in sorted(
        type_sizes.keys(), key=lambda t: type_sizes[t], reverse=True
    ):
        count = type_counts[sym_type]
        size = type_sizes[sym_type]
        percentage = (size / total_size * 100) if total_size > 0 else 0
        print(
            f"  {sym_type}: {count:4d} symbols, {size:8,} bytes ({size / 1024:6.1f} KB, {percentage:5.1f}%)"
        )

    print(f"\nTop {min(top_n, len(sized_symbols))} Largest Functions/Symbols:")
    print("-" * 80)

    for i, sym in enumerate(sized_symbols[:top_n], 1):
        # Calculate percentage of total
        percentage = (sym.size / total_size * 100) if total_size > 0 else 0

        # Display demangled name (truncate if too long)
        display_name = sym.demangled_name
        if len(display_name) > 70:
            display_name = display_name[:67] + "..."

        print(f"  {i:2d}. {sym.size:6,} bytes ({percentage:4.1f}%) - {display_name}")

        # Show type indicator
        type_indicator = {
            "T": "text (code)",
            "t": "text (code)",
            "D": "data (initialized)",
            "d": "data (initialized)",
            "B": "bss (uninitialized)",
            "b": "bss (uninitialized)",
            "R": "read-only data",
            "r": "read-only data",
        }.get(sym.type, sym.type)
        print(f"      Type: {type_indicator}")

    print("=" * 80)


def main() -> int:
    args = parse_args()
    if args.cwd:
        root_build_dir = args.cwd / ".build"
    else:
        root_build_dir = Path(".build")

    # Support nested PlatformIO structure: .build/pio/<board>
    nested_pio_dir = root_build_dir / "pio"
    if nested_pio_dir.is_dir():
        root_build_dir = nested_pio_dir

    board_dirs = [d for d in root_build_dir.iterdir() if d.is_dir()]
    if not board_dirs:
        print(f"No board directories found in {root_build_dir.absolute()}")
        return 1

    print("Available boards:")
    for i, board_dir in enumerate(board_dirs):
        print(f"[{i}]: {board_dir.name}")

    which = (
        0
        if args.first
        else int(input("Enter the number of the board you want to inspect: "))
    )
    board_dir = board_dirs[which]

    # Find build_info.json (try example-specific files first, then generic)
    build_info_files = list(board_dir.glob("build_info_*.json"))
    if build_info_files:
        # Prefer example-specific files
        build_info_json = build_info_files[0]
    else:
        # Fall back to generic build_info.json
        build_info_json = board_dir / "build_info.json"

    if not build_info_json.exists():
        print(f"Error: No build_info*.json found in {board_dir}")
        return 1

    build_info = load_build_info(build_info_json)
    board = board_dir.name
    board_info = build_info.get(board) or build_info[next(iter(build_info))]

    # Validate paths from build_info.json
    elf_path = Path(board_info.get("prog_path", ""))
    if not elf_path.exists():
        print(
            f"Error: ELF path '{elf_path}' does not exist. Check the 'prog_path' in build_info.json."
        )
        return 1

    bin_file = elf_path.with_suffix(".bin")
    if not bin_file.exists():
        # use .hex or .uf2 if .bin doesn't exist
        bin_file = elf_path.with_suffix(".hex")
        if not bin_file.exists():
            bin_file = elf_path.with_suffix(".uf2")
            if not bin_file.exists():
                print(f"Error: Binary file not found for '{elf_path}'")
                return 1
    cpp_filt_path = Path(board_info["aliases"]["c++filt"])
    ld_path = Path(board_info["aliases"]["ld"])
    as_path = Path(board_info["aliases"]["as"])
    nm_path = Path(board_info["aliases"]["nm"])
    objcopy_path = Path(board_info["aliases"]["objcopy"])

    # Get readelf path (derive from nm path if not in aliases)
    if "readelf" in board_info["aliases"]:
        readelf_path = board_info["aliases"]["readelf"]
    else:
        # Derive readelf path from nm path (replace 'nm' with 'readelf')
        readelf_path = str(nm_path).replace("-nm", "-readelf")

    map_file = board_dir / "firmware.map"
    if not map_file.exists():
        # Search for the map file
        map_file = bin_file.with_suffix(".map")
        if not map_file.exists():
            possible_map_files = list(board_dir.glob("**/firmware.map"))
            if possible_map_files:
                map_file = possible_map_files[0]
            else:
                print("Error: firmware.map file not found")
                return 1

    # ========================================================================
    # NEW: Per-function analysis from final ELF (after tree-shaking)
    # ========================================================================
    print("\n" + "=" * 80)
    print("ANALYZING FINAL BINARY (after tree-shaking)")
    print("=" * 80)

    try:
        print(f"\nAnalyzing ELF file: {elf_path}")
        print(f"Using nm: {nm_path}")
        print(f"Using c++filt: {cpp_filt_path}")
        print(f"Using readelf: {readelf_path}")

        # Analyze symbols from the final ELF file
        symbols = analyze_symbols(
            str(elf_path), str(nm_path), str(cpp_filt_path), str(readelf_path)
        )

        # Display per-function sizes prominently
        display_function_sizes(symbols, board, top_n=50)

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"\nWarning: Per-function analysis failed: {e}")
        print("Continuing with archive-level analysis...\n")

    # ========================================================================
    # Legacy analysis: bin-to-elf conversion (for compatibility)
    # ========================================================================
    try:
        with TemporaryDirectory() as temp_dir:
            temp_dir_path = Path(temp_dir)
            output_elf = bin_to_elf(
                bin_file,
                map_file,
                as_path,
                ld_path,
                objcopy_path,
                temp_dir_path / "output.elf",
            )
            out = dump_symbol_sizes(nm_path, cpp_filt_path, output_elf)
            print("\n" + "=" * 80)
            print("LEGACY ANALYSIS (bin-to-elf conversion)")
            print("=" * 80)
            print(out)
        handle_keyboard_interrupt_properly()
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(
            f"Error while converting binary to ELF, binary analysis will not work on this build: {e}"
        )

    # ========================================================================
    # Archive-level analysis from map file (using fpvgcc)
    # ========================================================================
    print("\n" + "=" * 80)
    print("ARCHIVE-LEVEL SIZE BREAKDOWN (from map file)")
    print("=" * 80)
    map_dump(map_file)

    # Demangle .gnu.linkonce.t symbols and print map file
    print("\n##################################################")
    print("# Map file dump:")
    print("##################################################\n")
    map_text = map_file.read_text()
    demangled_map_text = demangle_gnu_linkonce_symbols(cpp_filt_path, map_text)
    print(demangled_map_text)

    return 0


if __name__ == "__main__":
    sys.exit(main())
