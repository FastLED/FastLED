#!/usr/bin/env python3
"""
Generic Symbol Analysis Tool
Analyzes ELF files to identify all symbols for binary size reduction analysis.
Works with any platform (ESP32, UNO, etc.) that has build_info.json.
"""
import json
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple


def run_command(cmd: str) -> str:
    """Run a command and return stdout"""
    try:
        result = subprocess.run(
            cmd, shell=True, capture_output=True, text=True, check=True
        )
        return result.stdout
    except subprocess.CalledProcessError as e:
        print(f"Error running command: {cmd}")
        print(f"Error: {e.stderr}")
        return ""


def demangle_symbol(mangled_name: str, cppfilt_path: str) -> str:
    """Demangle a C++ symbol using c++filt"""
    try:
        cmd = f'echo "{mangled_name}" | "{cppfilt_path}"'
        result = subprocess.run(
            cmd, shell=True, capture_output=True, text=True, check=True
        )
        demangled = result.stdout.strip()
        # If demangling failed, c++filt returns the original name
        return demangled if demangled != mangled_name else mangled_name
    except Exception as e:
        print(f"Error demangling symbol: {mangled_name}")
        print(f"Error: {e}")
        return mangled_name


def analyze_symbols(elf_file: str, nm_path: str, cppfilt_path: str) -> List[Dict]:
    """Analyze ALL symbols in ELF file using nm with C++ demangling"""
    print("Analyzing symbols...")

    # Get all symbols with sizes
    cmd = f'"{nm_path}" --print-size --size-sort --radix=d "{elf_file}"'
    output = run_command(cmd)

    symbols = []
    print("Demangling C++ symbols...")

    for line in output.strip().split("\n"):
        if not line.strip():
            continue

        parts = line.split()
        if len(parts) >= 4:
            addr = parts[0]
            size = int(parts[1])
            symbol_type = parts[2]
            mangled_name = " ".join(parts[3:])

            # Demangle the symbol name
            demangled_name = demangle_symbol(mangled_name, cppfilt_path)

            symbol_info = {
                "address": addr,
                "size": size,
                "type": symbol_type,
                "name": mangled_name,
                "demangled_name": demangled_name,
            }

            symbols.append(symbol_info)

    return symbols


def analyze_map_file(map_file: Path) -> Dict[str, List[str]]:
    """Analyze the map file to understand module dependencies"""
    print(f"Analyzing map file: {map_file}")

    dependencies = {}
    current_archive = None

    if not map_file.exists():
        print(f"Map file not found: {map_file}")
        return {}

    try:
        with open(map_file, "r") as f:
            for line in f:
                line = line.strip()

                # Look for archive member includes - handle both ESP32 and UNO formats
                if ".a(" in line and ")" in line:
                    # Extract module name
                    start = line.find("(") + 1
                    end = line.find(")")
                    if start > 0 and end > start:
                        current_archive = line[start:end]
                        dependencies[current_archive] = []

                elif current_archive and line and not line.startswith((".", "/", "*")):
                    # This line shows what pulled in the module
                    if "(" in line and ")" in line:
                        # Extract the symbol that caused the inclusion
                        symbol_start = line.find("(") + 1
                        symbol_end = line.find(")")
                        if symbol_start > 0 and symbol_end > symbol_start:
                            symbol = line[symbol_start:symbol_end]
                            dependencies[current_archive].append(symbol)
                    current_archive = None
    except Exception as e:
        print(f"Error reading map file: {e}")
        return {}

    return dependencies


def generate_report(
    board_name: str, symbols: List[Dict], dependencies: Dict[str, List[str]]
) -> Dict:
    """Generate a comprehensive report"""
    print("\n" + "=" * 80)
    print(f"{board_name.upper()} SYMBOL ANALYSIS REPORT")
    print("=" * 80)

    # Summary statistics
    total_symbols = len(symbols)
    total_size = sum(s["size"] for s in symbols)

    print("\nSUMMARY:")
    print(f"  Total symbols: {total_symbols}")
    print(f"  Total symbol size: {total_size} bytes ({total_size/1024:.1f} KB)")

    # Largest symbols overall
    print("\nLARGEST SYMBOLS (all symbols, sorted by size):")
    symbols_sorted = sorted(symbols, key=lambda x: x["size"], reverse=True)

    for i, sym in enumerate(symbols_sorted[:30]):
        display_name = sym.get("demangled_name", sym["name"])
        print(f"  {i+1:2d}. {sym['size']:6d} bytes - {display_name}")
        if "demangled_name" in sym and sym["demangled_name"] != sym["name"]:
            print(
                f"      (mangled: {sym['name'][:80]}{'...' if len(sym['name']) > 80 else ''})"
            )

    # Symbol type breakdown
    print("\nSYMBOL TYPE BREAKDOWN:")
    type_stats = {}
    for sym in symbols:
        sym_type = sym["type"]
        if sym_type not in type_stats:
            type_stats[sym_type] = {"count": 0, "size": 0}
        type_stats[sym_type]["count"] += 1
        type_stats[sym_type]["size"] += sym["size"]

    for sym_type, stats in sorted(
        type_stats.items(), key=lambda x: x[1]["size"], reverse=True
    ):
        print(
            f"  {sym_type}: {stats['count']} symbols, {stats['size']} bytes ({stats['size']/1024:.1f} KB)"
        )

    # Dependencies analysis
    if dependencies:
        print("\nMODULE DEPENDENCIES:")
        for module in sorted(dependencies.keys()):
            if dependencies[module]:  # Only show modules with dependencies
                print(f"  {module}:")
                for symbol in dependencies[module][:5]:  # Show first 5 symbols
                    print(f"    - {symbol}")
                if len(dependencies[module]) > 5:
                    print(f"    ... and {len(dependencies[module]) - 5} more")

    return {
        "board": board_name,
        "total_symbols": total_symbols,
        "total_size": total_size,
        "largest_symbols": symbols_sorted[:20],
        "type_breakdown": type_stats,
        "dependencies": dependencies,
    }


def find_board_build_info(board_name: Optional[str] = None) -> Tuple[Path, str]:
    """Find build info for a specific board or detect available boards"""
    # Detect build directory
    possible_build_dirs = [
        Path("../../.build"),
        Path("../.build"),
        Path(".build"),
        Path("../../build"),
        Path("../build"),
        Path("build"),
    ]

    build_dir = None
    for build_path in possible_build_dirs:
        if build_path.exists():
            build_dir = build_path
            break

    if not build_dir:
        print("Error: Could not find build directory (.build)")
        sys.exit(1)

    # If specific board requested, look for it
    if board_name:
        board_dir = build_dir / board_name
        if board_dir.exists() and (board_dir / "build_info.json").exists():
            return board_dir / "build_info.json", board_name
        else:
            print(f"Error: Board '{board_name}' not found or missing build_info.json")
            sys.exit(1)

    # Otherwise, find any available board
    available_boards = []
    for item in build_dir.iterdir():
        if item.is_dir():
            build_info_file = item / "build_info.json"
            if build_info_file.exists():
                available_boards.append((build_info_file, item.name))

    if not available_boards:
        print(f"Error: No boards with build_info.json found in {build_dir}")
        sys.exit(1)

    # Return the first available board
    return available_boards[0]


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="Analyze symbols in ELF files for any platform"
    )
    parser.add_argument("--board", help="Board name to analyze (e.g., uno, esp32dev)")
    args = parser.parse_args()

    # Find build info
    build_info_path, board_name = find_board_build_info(args.board)
    print(f"Found build info for {board_name}: {build_info_path}")

    with open(build_info_path) as f:
        build_info = json.load(f)

    # Get board info
    board_info = build_info[board_name]
    nm_path = board_info["aliases"]["nm"]
    cppfilt_path = board_info["aliases"]["c++filt"]
    elf_file = board_info["prog_path"]

    # Find map file
    map_file = Path(elf_file).with_suffix(".map")

    print(f"Analyzing ELF file: {elf_file}")
    print(f"Using nm tool: {nm_path}")
    print(f"Using c++filt tool: {cppfilt_path}")
    print(f"Map file: {map_file}")

    # Analyze symbols
    symbols = analyze_symbols(elf_file, nm_path, cppfilt_path)

    # Analyze dependencies
    dependencies = analyze_map_file(map_file)

    # Generate report
    report = generate_report(board_name, symbols, dependencies)

    # Save detailed data to JSON (sorted by size, largest first)
    # Find the build directory (go up from wherever we are to find .build)
    current = Path.cwd()
    while current != current.parent:
        build_dir = current / ".build"
        if build_dir.exists():
            output_file = build_dir / f"{board_name}_symbol_analysis.json"
            break
        current = current.parent
    else:
        # Fallback to current directory if .build not found
        output_file = Path(f"{board_name}_symbol_analysis.json")
    detailed_data = {
        "summary": report,
        "all_symbols_sorted_by_size": sorted(
            symbols, key=lambda x: x["size"], reverse=True
        ),
        "dependencies": dependencies,
    }

    with open(output_file, "w") as f:
        json.dump(detailed_data, f, indent=2)

    print(f"\nDetailed analysis saved to: {output_file}")
    print("This file contains ALL symbols without any filtering or classification.")


if __name__ == "__main__":
    main()
