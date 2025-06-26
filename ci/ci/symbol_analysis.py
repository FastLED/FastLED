#!/usr/bin/env python3
"""
Enhanced Symbol Analysis Tool with Function Call Graph Analysis
Analyzes ELF files to identify symbols and function call relationships.
Shows which functions call other functions (call graph analysis).
Works with any platform (ESP32S3, UNO, etc.) that has build_info.json.
"""
import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# Import board mapping system
from ci.boards import get_board


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


def analyze_function_calls(
    elf_file: str, objdump_path: str, cppfilt_path: str
) -> Dict[str, List[str]]:
    """Analyze function calls using objdump to build call graph"""
    print("Analyzing function calls using objdump...")

    # Use objdump to disassemble the binary
    cmd = f'"{objdump_path}" -t "{elf_file}"'
    print(f"Running: {cmd}")
    symbol_output = run_command(cmd)

    # Build symbol address map for function symbols
    symbol_map = {}  # address -> symbol_name
    function_symbols = set()  # set of function names

    for line in symbol_output.strip().split("\n"):
        if not line.strip():
            continue

        # Parse objdump symbol table output
        # Format: address flags section size name
        parts = line.split()
        if len(parts) >= 5 and ("F" in parts[1] or "f" in parts[1]):  # Function symbol
            try:
                address = parts[0]
                symbol_name = " ".join(parts[4:])

                # Demangle the symbol name
                demangled_name = demangle_symbol(symbol_name, cppfilt_path)

                symbol_map[address] = {"name": symbol_name, "demangled": demangled_name}
                function_symbols.add(demangled_name)
            except (ValueError, IndexError):
                continue

    print(f"Found {len(function_symbols)} function symbols")

    # Now disassemble text sections to find function calls
    cmd = f'"{objdump_path}" -d "{elf_file}"'
    print(f"Running: {cmd}")
    disasm_output = run_command(cmd)

    if not disasm_output:
        print("Warning: No disassembly output received")
        return {}

    # Parse disassembly to find function calls
    call_graph = defaultdict(set)  # caller -> set of callees
    current_function = None

    # Common call instruction patterns for different architectures
    call_patterns = [
        r"call\s+(\w+)",  # x86/x64 call
        r"bl\s+(\w+)",  # ARM branch with link
        r"jal\s+(\w+)",  # RISC-V jump and link
        r"callx?\d*\s+(\w+)",  # Xtensa call variations
    ]

    call_regex = re.compile(
        "|".join(f"(?:{pattern})" for pattern in call_patterns), re.IGNORECASE
    )

    function_start_pattern = re.compile(r"^([0-9a-f]+)\s+<([^>]+)>:")

    lines = disasm_output.split("\n")
    for i, line in enumerate(lines):
        line = line.strip()

        # Check for function start
        func_match = function_start_pattern.match(line)
        if func_match:
            func_name = func_match.group(2)

            # Demangle function name
            current_function = demangle_symbol(func_name, cppfilt_path)
            continue

        # Look for call instructions
        if current_function and (
            "call" in line.lower() or "bl " in line.lower() or "jal" in line.lower()
        ):
            call_match = call_regex.search(line)
            if call_match:
                # Extract the target function name
                for group in call_match.groups():
                    if group:
                        target_func = demangle_symbol(group, cppfilt_path)
                        call_graph[current_function].add(target_func)
                        break

    print(f"Built call graph with {len(call_graph)} calling functions")

    # Convert sets to lists for JSON serialization
    return {caller: list(callees) for caller, callees in call_graph.items()}


def build_reverse_call_graph(call_graph: Dict[str, List[str]]) -> Dict[str, List[str]]:
    """Build reverse call graph: function -> list of functions that call it"""
    reverse_graph = defaultdict(list)

    for caller, callees in call_graph.items():
        for callee in callees:
            reverse_graph[callee].append(caller)

    return dict(reverse_graph)


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
    board_name: str,
    symbols: List[Dict],
    dependencies: Dict[str, List[str]],
    call_graph: Optional[Dict[str, List[str]]] = None,
    reverse_call_graph: Optional[Dict[str, List[str]]] = None,
    enhanced_mode: bool = False,
) -> Dict:
    """Generate a comprehensive report with optional call graph analysis"""
    print("\n" + "=" * 80)
    if enhanced_mode and call_graph and reverse_call_graph:
        print(f"{board_name.upper()} ENHANCED SYMBOL ANALYSIS REPORT")
    else:
        print(f"{board_name.upper()} SYMBOL ANALYSIS REPORT")
    print("=" * 80)

    # Summary statistics
    total_symbols = len(symbols)
    total_size = sum(s["size"] for s in symbols)

    print("\nSUMMARY:")
    print(f"  Total symbols: {total_symbols}")
    print(f"  Total symbol size: {total_size} bytes ({total_size/1024:.1f} KB)")

    if enhanced_mode and call_graph and reverse_call_graph:
        print(f"  Functions with calls: {len(call_graph)}")
        print(f"  Functions called by others: {len(reverse_call_graph)}")

    # Largest symbols overall
    print("\nLARGEST SYMBOLS (all symbols, sorted by size):")
    symbols_sorted = sorted(symbols, key=lambda x: x["size"], reverse=True)

    display_count = 20 if enhanced_mode else 30
    for i, sym in enumerate(symbols_sorted[:display_count]):
        display_name = sym.get("demangled_name", sym["name"])
        print(f"  {i+1:2d}. {sym['size']:6d} bytes - {display_name}")

        # Show what functions call this symbol (if enhanced mode and it's a function)
        if enhanced_mode and reverse_call_graph and display_name in reverse_call_graph:
            callers = reverse_call_graph[display_name]
            if callers:
                caller_names = [
                    name[:40] + "..." if len(name) > 40 else name
                    for name in callers[:3]
                ]
                print(f"      Called by: {', '.join(caller_names)}")
                if len(callers) > 3:
                    print(f"      ... and {len(callers) - 3} more")

        # Show mangled name if different (non-enhanced mode)
        elif (
            not enhanced_mode
            and "demangled_name" in sym
            and sym["demangled_name"] != sym["name"]
        ):
            print(
                f"      (mangled: {sym['name'][:80]}{'...' if len(sym['name']) > 80 else ''})"
            )

    # Initialize variables for enhanced mode data
    most_called = []
    most_calling = []

    # Enhanced function call analysis
    if enhanced_mode and call_graph and reverse_call_graph:
        print("\n" + "=" * 80)
        print("FUNCTION CALL ANALYSIS")
        print("=" * 80)

        # Most called functions
        most_called = sorted(
            reverse_call_graph.items(), key=lambda x: len(x[1]), reverse=True
        )
        print("\nMOST CALLED FUNCTIONS (functions called by many others):")
        for i, (func_name, callers) in enumerate(most_called[:15]):
            short_name = func_name[:60] + "..." if len(func_name) > 60 else func_name
            print(f"  {i+1:2d}. {short_name}")
            print(f"      Called by {len(callers)} functions")
            if len(callers) <= 5:
                for caller in callers:
                    caller_short = caller[:50] + "..." if len(caller) > 50 else caller
                    print(f"        - {caller_short}")
            else:
                for caller in callers[:3]:
                    caller_short = caller[:50] + "..." if len(caller) > 50 else caller
                    print(f"        - {caller_short}")
                print(f"        ... and {len(callers) - 3} more")
            print()

        # Functions that call many others
        most_calling = sorted(call_graph.items(), key=lambda x: len(x[1]), reverse=True)
        print("\nFUNCTIONS THAT CALL MANY OTHERS:")
        for i, (func_name, callees) in enumerate(most_calling[:10]):
            short_name = func_name[:60] + "..." if len(func_name) > 60 else func_name
            print(f"  {i+1:2d}. {short_name}")
            print(f"      Calls {len(callees)} functions")
            if len(callees) <= 5:
                for callee in callees:
                    callee_short = callee[:50] + "..." if len(callee) > 50 else callee
                    print(f"        -> {callee_short}")
            else:
                for callee in callees[:3]:
                    callee_short = callee[:50] + "..." if len(callee) > 50 else callee
                    print(f"        -> {callee_short}")
                print(f"        ... and {len(callees) - 3} more")
            print()

    # Symbol type breakdown
    section_title = "\n" + "=" * 80 + "\n" if enhanced_mode else "\n"
    print(section_title + "SYMBOL TYPE BREAKDOWN:")
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

    # Build return data
    report_data = {
        "board": board_name,
        "total_symbols": total_symbols,
        "total_size": total_size,
        "largest_symbols": symbols_sorted[:20],
        "type_breakdown": type_stats,
        "dependencies": dependencies,
    }

    # Add enhanced data if available
    if enhanced_mode and call_graph and reverse_call_graph:
        report_data.update(
            {
                "call_graph": call_graph,
                "reverse_call_graph": reverse_call_graph,
                "call_stats": {
                    "functions_with_calls": len(call_graph),
                    "functions_called_by_others": len(reverse_call_graph),
                    "most_called": [
                        (func, len(callers)) for func, callers in most_called[:10]
                    ],
                    "most_calling": [
                        (func, len(callees)) for func, callees in most_calling[:10]
                    ],
                },
            }
        )

    return report_data


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
        description="Enhanced symbol analysis with optional function call analysis for any platform"
    )
    parser.add_argument(
        "--board", help="Board name to analyze (e.g., uno, esp32dev, esp32s3)"
    )
    parser.add_argument(
        "--enhanced",
        action="store_true",
        help="Enable enhanced analysis with function call graph",
    )
    parser.add_argument(
        "--show-calls-to",
        help="Show what functions call a specific function (enables enhanced mode)",
    )
    args = parser.parse_args()

    # Enable enhanced mode if show-calls-to is specified
    enhanced_mode = args.enhanced or args.show_calls_to

    # Find build info
    build_info_path, board_name = find_board_build_info(args.board)
    print(f"Found build info for {board_name}: {build_info_path}")

    with open(build_info_path) as f:
        build_info = json.load(f)

    # Get board info using proper board mapping
    board = get_board(board_name)
    real_board_name = board.get_real_board_name()

    # Try the real board name first, then fall back to directory name
    if real_board_name in build_info:
        board_info = build_info[real_board_name]
        actual_board_key = real_board_name
        if real_board_name != board_name:
            print(
                f"Note: Using board key '{real_board_name}' from board mapping (directory was '{board_name}')"
            )
    elif board_name in build_info:
        board_info = build_info[board_name]
        actual_board_key = board_name
    else:
        # Try to find the actual board key in the JSON as fallback
        board_keys = list(build_info.keys())
        if len(board_keys) == 1:
            actual_board_key = board_keys[0]
            board_info = build_info[actual_board_key]
            print(
                f"Note: Using only available board key '{actual_board_key}' from build_info.json (expected '{real_board_name}' or '{board_name}')"
            )
        else:
            print(
                f"Error: Could not find board '{real_board_name}' or '{board_name}' in build_info.json"
            )
            print(f"Available board keys: {board_keys}")
            sys.exit(1)

    nm_path = board_info["aliases"]["nm"]
    cppfilt_path = board_info["aliases"]["c++filt"]
    elf_file = board_info["prog_path"]

    # Find map file
    map_file = Path(elf_file).with_suffix(".map")

    print(f"Analyzing ELF file: {elf_file}")
    print(f"Using nm tool: {nm_path}")
    print(f"Using c++filt tool: {cppfilt_path}")
    if enhanced_mode:
        objdump_path = board_info["aliases"]["objdump"]
        print(f"Using objdump tool: {objdump_path}")
    print(f"Map file: {map_file}")

    # Analyze symbols
    symbols = analyze_symbols(elf_file, nm_path, cppfilt_path)

    # Analyze function calls if enhanced mode
    call_graph = {}
    reverse_call_graph = {}
    if enhanced_mode:
        objdump_path = board_info["aliases"]["objdump"]
        call_graph = analyze_function_calls(elf_file, objdump_path, cppfilt_path)
        reverse_call_graph = build_reverse_call_graph(call_graph)

    # Analyze dependencies
    dependencies = analyze_map_file(map_file)

    # Handle specific function query
    if args.show_calls_to:
        target_function = args.show_calls_to
        print("\n" + "=" * 80)
        print(f"FUNCTIONS THAT CALL: {target_function}")
        print("=" * 80)

        # Find functions that call the target (exact match first)
        exact_callers = reverse_call_graph.get(target_function, [])

        # Also search for partial matches
        partial_matches = {}
        for func_name, callers in reverse_call_graph.items():
            if (
                target_function.lower() in func_name.lower()
                and func_name != target_function
            ):
                partial_matches[func_name] = callers

        if exact_callers:
            print(f"\nExact match - Functions calling '{target_function}':")
            for i, caller in enumerate(exact_callers, 1):
                print(f"  {i}. {caller}")

        if partial_matches:
            print(f"\nPartial matches - Functions containing '{target_function}':")
            for func_name, callers in partial_matches.items():
                print(f"\n  Function: {func_name}")
                print(f"  Called by {len(callers)} functions:")
                for caller in callers[:5]:
                    print(f"    - {caller}")
                if len(callers) > 5:
                    print(f"    ... and {len(callers) - 5} more")

        if not exact_callers and not partial_matches:
            print(f"No functions found that call '{target_function}'")
            print("Available functions (first 20):")
            available_funcs = sorted(reverse_call_graph.keys())[:20]
            for func in available_funcs:
                print(f"  - {func}")

        return  # Exit early for specific query

    # Generate report using user-friendly board name
    report = generate_report(
        board_name.upper(),
        symbols,
        dependencies,
        call_graph,
        reverse_call_graph,
        enhanced_mode,
    )

    # Save detailed data to JSON (sorted by size, largest first)
    # Find the build directory (go up from wherever we are to find .build)
    current = Path.cwd()
    while current != current.parent:
        build_dir = current / ".build"
        if build_dir.exists():
            filename_suffix = (
                "_enhanced_symbol_analysis.json"
                if enhanced_mode
                else "_symbol_analysis.json"
            )
            output_file = build_dir / f"{board_name}{filename_suffix}"
            break
        current = current.parent
    else:
        # Fallback to current directory if .build not found
        filename_suffix = (
            "_enhanced_symbol_analysis.json"
            if enhanced_mode
            else "_symbol_analysis.json"
        )
        output_file = Path(f"{board_name}{filename_suffix}")

    detailed_data = {
        "summary": report,
        "all_symbols_sorted_by_size": sorted(
            symbols, key=lambda x: x["size"], reverse=True
        ),
        "dependencies": dependencies,
    }

    # Add enhanced data if available
    if enhanced_mode and call_graph and reverse_call_graph:
        detailed_data.update(
            {"call_graph": call_graph, "reverse_call_graph": reverse_call_graph}
        )

    with open(output_file, "w") as f:
        json.dump(detailed_data, f, indent=2)

    description = (
        "enhanced analysis with complete call graph"
        if enhanced_mode
        else "basic symbol analysis"
    )
    print(f"\nDetailed {description} saved to: {output_file}")

    if not enhanced_mode:
        print("This file contains ALL symbols without any filtering or classification.")
        print("Use --enhanced flag for function call graph analysis.")
    else:
        print("This file contains ALL symbols and complete call graph analysis.")


if __name__ == "__main__":
    main()
