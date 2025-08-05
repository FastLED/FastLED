#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false, reportMissingParameterType=false, reportUnknownLambdaType=false, reportArgumentType=false
"""
ESP32 Symbol Analysis Tool
Analyzes the ESP32 ELF file to identify symbols that can be eliminated for binary size reduction.
"""

import json
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Tuple


@dataclass
class SymbolInfo:
    """Represents a symbol in the ESP32 binary"""

    address: str
    size: int
    type: str
    name: str
    demangled_name: str
    source: str  # STRICT: NO defaults - all callers must provide explicit source


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


def analyze_symbols(
    elf_file: str, nm_path: str, cppfilt_path: str
) -> Tuple[List[SymbolInfo], List[SymbolInfo], List[SymbolInfo]]:
    """Analyze symbols in ELF file using nm with C++ demangling"""
    print("Analyzing symbols...")

    # Get all symbols with sizes
    cmd = f'"{nm_path}" --print-size --size-sort --radix=d "{elf_file}"'
    output = run_command(cmd)

    symbols: List[SymbolInfo] = []
    fastled_symbols: List[SymbolInfo] = []
    large_symbols: List[SymbolInfo] = []

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

            symbol_info = SymbolInfo(
                address=addr,
                size=size,
                type=symbol_type,
                name=mangled_name,
                demangled_name=demangled_name,
                source="esp32_nm",
            )

            symbols.append(symbol_info)

            # Identify FastLED-related symbols using demangled names
            search_text = demangled_name.lower()
            if any(
                keyword in search_text
                for keyword in [
                    "fastled",
                    "cfastled",
                    "crgb",
                    "hsv",
                    "pixel",
                    "controller",
                    "led",
                    "rmt",
                    "strip",
                    "neopixel",
                    "ws2812",
                    "apa102",
                ]
            ):
                fastled_symbols.append(symbol_info)

            # Identify large symbols (>100 bytes)
            if size > 100:
                large_symbols.append(symbol_info)

    return symbols, fastled_symbols, large_symbols


def analyze_map_file(map_file: str) -> Dict[str, List[str]]:
    """Analyze the map file to understand module dependencies"""
    print("Analyzing map file...")

    dependencies: Dict[str, List[str]] = {}
    current_archive = None

    try:
        with open(map_file, "r") as f:
            for line in f:
                line = line.strip()

                # Look for archive member includes
                if line.startswith(".pio/build/esp32dev/liba4c/libsrc.a("):
                    # Extract module name
                    start = line.find("(") + 1
                    end = line.find(")")
                    if start > 0 and end > start:
                        current_archive = line[start:end]
                        dependencies[current_archive] = []

                elif current_archive and line and not line.startswith(".pio"):
                    # This line shows what pulled in the module
                    if "(" in line and ")" in line:
                        # Extract the symbol that caused the inclusion
                        symbol_start = line.find("(") + 1
                        symbol_end = line.find(")")
                        if symbol_start > 0 and symbol_end > symbol_start:
                            symbol = line[symbol_start:symbol_end]
                            dependencies[current_archive].append(symbol)
                    current_archive = None
    except FileNotFoundError:
        print(f"Map file not found: {map_file}")
        return {}

    return dependencies


def generate_report(
    symbols: List[SymbolInfo],
    fastled_symbols: List[SymbolInfo],
    large_symbols: List[SymbolInfo],
    dependencies: Dict[str, List[str]],
) -> Dict[str, Any]:
    """Generate a comprehensive report"""
    print("\n" + "=" * 80)
    print("ESP32 FASTLED SYMBOL ANALYSIS REPORT")
    print("=" * 80)

    # Summary statistics
    total_symbols = len(symbols)
    total_fastled = len(fastled_symbols)
    fastled_size = sum(s.size for s in fastled_symbols)

    print("\nSUMMARY:")
    print(f"  Total symbols: {total_symbols}")
    print(f"  FastLED symbols: {total_fastled}")
    print(f"  Total FastLED size: {fastled_size} bytes ({fastled_size / 1024:.1f} KB)")

    # Largest FastLED symbols
    print("\nLARGEST FASTLED SYMBOLS (potential elimination targets):")
    fastled_sorted: List[SymbolInfo] = sorted(
        fastled_symbols, key=lambda x: x.size, reverse=True
    )
    for i, sym in enumerate(fastled_sorted[:20]):
        display_name = sym.demangled_name
        print(f"  {i + 1:2d}. {sym.size:6d} bytes - {display_name}")
        if sym.demangled_name != sym.name:
            print(
                f"      (mangled: {sym.name[:80]}{'...' if len(sym.name) > 80 else ''})"
            )

    # FastLED modules analysis
    print("\nFASTLED MODULES PULLED IN:")
    fastled_modules: List[str] = [
        mod
        for mod in dependencies.keys()
        if any(kw in mod.lower() for kw in ["fastled", "crgb", "led", "rmt", "strip"])
    ]

    for module in sorted(fastled_modules):
        print(f"  {module}:")
        for symbol in dependencies[module][:5]:  # Show first 5 symbols
            print(f"    - {symbol}")
        if len(dependencies[module]) > 5:
            print(f"    ... and {len(dependencies[module]) - 5} more")

    # Largest overall symbols (non-FastLED)
    print("\nLARGEST NON-FASTLED SYMBOLS:")
    non_fastled: List[SymbolInfo] = [
        s
        for s in large_symbols
        if not any(
            keyword in s.demangled_name.lower()
            for keyword in ["fastled", "cfastled", "crgb", "hsv"]
        )
    ]
    non_fastled_sorted: List[SymbolInfo] = sorted(
        non_fastled, key=lambda x: x.size, reverse=True
    )

    for i, sym in enumerate(non_fastled_sorted[:15]):
        display_name = sym.demangled_name
        print(f"  {i + 1:2d}. {sym.size:6d} bytes - {display_name}")

    # Recommendations
    print("\nRECOMMENDATIONS FOR SIZE REDUCTION:")

    # Identify unused features
    # unused_features = []
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
        feature_symbols: List[SymbolInfo] = [
            s for s in fastled_symbols if any(p in s.demangled_name for p in patterns)
        ]
        if feature_symbols:
            total_size = sum(s.size for s in feature_symbols)
            print(f"  - {feature}: {len(feature_symbols)} symbols, {total_size} bytes")
            if total_size > 1000:  # Only show features with >1KB
                print(
                    f"    Consider removing if not needed (could save ~{total_size / 1024:.1f} KB)"
                )
                # Show a few example symbols
                for sym in feature_symbols[:3]:
                    display_name = sym.demangled_name[:60]
                    print(f"      * {sym.size} bytes: {display_name}")
                if len(feature_symbols) > 3:
                    print(f"      ... and {len(feature_symbols) - 3} more")

    return {
        "total_symbols": total_symbols,
        "fastled_symbols": total_fastled,
        "fastled_size": fastled_size,
        "largest_fastled": fastled_sorted[:10],
        "dependencies": fastled_modules,
    }


def main():
    # Detect build directory and board - try multiple possible locations
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

    # Find ESP32 board directory
    esp32_boards = [
        "esp32dev",
        "esp32",
        "esp32s2",
        "esp32s3",
        "esp32c3",
        "esp32c6",
        "esp32h2",
        "esp32p4",
        "esp32c2",
    ]
    board_dir = None
    board_name = None

    for board in esp32_boards:
        candidate_dir = build_dir / board
        if candidate_dir.exists():
            build_info_file = candidate_dir / "build_info.json"
            if build_info_file.exists():
                board_dir = candidate_dir
                board_name = board
                break

    if not board_dir:
        print("Error: No ESP32 board with build_info.json found in build directory")
        print(f"Searched in: {build_dir}")
        print(f"Looking for boards: {esp32_boards}")
        sys.exit(1)

    build_info_path = board_dir / "build_info.json"
    print(f"Found ESP32 build info for {board_name}: {build_info_path}")

    with open(build_info_path) as f:
        build_info = json.load(f)

    # Use the detected board name instead of hardcoded "esp32dev"
    esp32_info = build_info[board_name]
    nm_path = esp32_info["aliases"]["nm"]
    elf_file = esp32_info["prog_path"]

    # Find map file
    map_file = Path(elf_file).with_suffix(".map")

    print(f"Analyzing ELF file: {elf_file}")
    print(f"Using nm tool: {nm_path}")
    print(f"Map file: {map_file}")

    # Analyze symbols
    cppfilt_path = esp32_info["aliases"]["c++filt"]
    symbols, fastled_symbols, large_symbols = analyze_symbols(
        elf_file, nm_path, cppfilt_path
    )

    # Analyze dependencies
    dependencies = analyze_map_file(map_file)

    # Generate report
    report = generate_report(symbols, fastled_symbols, large_symbols, dependencies)

    # Save detailed data to JSON (sorted by size, largest first)
    # Use board-specific filename and place it relative to build directory
    output_file = build_dir / f"{board_name}_symbol_analysis.json"
    detailed_data = {
        "summary": report,
        "all_fastled_symbols": sorted(
            fastled_symbols, key=lambda x: x.size, reverse=True
        ),
        "all_symbols_sorted_by_size": sorted(
            symbols, key=lambda x: x.size, reverse=True
        )[:100],  # Top 100 largest symbols
        "dependencies": dependencies,
        "large_symbols": sorted(large_symbols, key=lambda x: x.size, reverse=True)[
            :50
        ],  # Top 50 largest symbols
    }

    with open(output_file, "w") as f:
        json.dump(detailed_data, f, indent=2)

    print(f"\nDetailed analysis saved to: {output_file}")
    print("You can examine this file to identify specific symbols to eliminate.")


if __name__ == "__main__":
    main()
