#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""
Demo script showing how to use the new generic symbol analysis functionality
"""

import json
import subprocess
import sys
from pathlib import Path
from typing import List


def run_symbol_analysis(board_name: str):
    """Run symbol analysis for a specific board"""
    print(f"Running symbol analysis for {board_name}...")

    try:
        result = subprocess.run(
            [sys.executable, "ci/util/symbol_analysis.py", "--board", board_name],
            cwd=".",
            capture_output=True,
            text=True,
        )

        if result.returncode == 0:
            print(f"✅ Symbol analysis completed successfully for {board_name}")
            return True
        else:
            print(f"❌ Symbol analysis failed for {board_name}")
            print(f"Error: {result.stderr}")
            return False
    except Exception as e:
        print(f"❌ Error running symbol analysis for {board_name}: {e}")
        return False


def load_and_summarize_results(board_name: str):
    """Load and display a summary of symbol analysis results"""
    json_file = Path(f".build/{board_name}_symbol_analysis.json")

    if not json_file.exists():
        print(f"❌ Results file not found: {json_file}")
        return

    try:
        with open(json_file) as f:
            data = json.load(f)

        summary = data["summary"]
        print(f"\n📊 SUMMARY FOR {board_name.upper()}:")
        print(f"   Total symbols: {summary['total_symbols']}")
        print(
            f"   Total size: {summary['total_size']} bytes ({summary['total_size'] / 1024:.1f} KB)"
        )

        print("\n🔝 TOP 5 LARGEST SYMBOLS:")
        for i, symbol in enumerate(summary["largest_symbols"][:5], 1):
            name = symbol.get("demangled_name", symbol["name"])
            print(
                f"   {i}. {symbol['size']:6d} bytes - {name[:80]}{'...' if len(name) > 80 else ''}"
            )

        print("\n📁 SYMBOL TYPES:")
        for sym_type, stats in list(summary["type_breakdown"].items())[:5]:
            print(f"   {sym_type}: {stats['count']} symbols, {stats['size']} bytes")

    except Exception as e:
        print(f"❌ Error loading results for {board_name}: {e}")


def main():
    """Demo the symbol analysis functionality"""
    print("=" * 80)
    print("FASTLED SYMBOL ANALYSIS DEMO")
    print("=" * 80)

    # List of boards to analyze (you can add more)
    boards_to_analyze: List[str] = []

    # Check which boards have build info available
    build_dir = Path(".build")
    if build_dir.exists():
        for item in build_dir.iterdir():
            if item.is_dir() and (item / "build_info.json").exists():
                boards_to_analyze.append(item.name)

    if not boards_to_analyze:
        print("❌ No boards with build_info.json found in .build directory")
        print(
            "   Please compile some examples first using: uv run -m ci.ci-compile <board> --examples Blink"
        )
        sys.exit(1)

    print(
        f"📋 Found {len(boards_to_analyze)} boards with build info: {', '.join(boards_to_analyze)}"
    )

    # Run symbol analysis for each board
    successful_boards: List[str] = []
    for board in boards_to_analyze:
        if run_symbol_analysis(board):
            successful_boards.append(board)

    print(f"\n📈 RESULTS SUMMARY FOR {len(successful_boards)} BOARDS:")
    print("=" * 80)

    # Load and summarize results
    for board in successful_boards:
        load_and_summarize_results(board)
        print()

    print("🎉 Symbol analysis demo completed!")
    print("📁 Detailed JSON files saved in .build/ directory")
    print("💡 Use these files to identify optimization opportunities")


if __name__ == "__main__":
    main()
