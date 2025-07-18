#!/usr/bin/env python3
"""
Implementation Files Checker
Scans src/fl/ and src/fx/ directories for *.hpp and *.cpp.hpp files.
Provides statistics and can verify inclusion in the all-source build.
"""

import argparse
import json
import os
from pathlib import Path
from typing import Dict, List, Set


# Get project root directory
PROJECT_ROOT = Path(__file__).parent.parent.parent
SRC_ROOT = PROJECT_ROOT / "src"
FL_DIR = SRC_ROOT / "fl"
FX_DIR = SRC_ROOT / "fx"
ALL_SOURCE_BUILD_FILE = SRC_ROOT / "fastled_compile.hpp.cpp"

# Hierarchical compile files
HIERARCHICAL_FILES = [
    SRC_ROOT / "fl" / "fl_compile.hpp",
    SRC_ROOT / "fx" / "fx_compile.hpp",
    SRC_ROOT / "sensors" / "sensors_compile.hpp",
    SRC_ROOT / "platforms" / "platforms_compile.hpp",
    SRC_ROOT / "third_party" / "third_party_compile.hpp",
    SRC_ROOT / "src_compile.hpp",
]

# Detect if running in CI/test environment for ASCII-only output
USE_ASCII_ONLY = (
    os.environ.get("FASTLED_CI_NO_INTERACTIVE") == "true"
    or os.environ.get("GITHUB_ACTIONS") == "true"
    or os.environ.get("CI") == "true"
)


def collect_files_by_type(directory: Path) -> Dict[str, List[Path]]:
    """Collect files by type (.hpp vs .cpp.hpp) from a directory.

    Args:
        directory: Directory to scan

    Returns:
        Dictionary with 'hpp' and 'cpp_hpp' keys containing lists of files
    """
    files = {"hpp": [], "cpp_hpp": []}

    if not directory.exists():
        print(f"Warning: Directory {directory} does not exist")
        return files

    # Recursively find all .hpp and .cpp.hpp files
    for file_path in directory.rglob("*.hpp"):
        if file_path.name.endswith(".cpp.hpp"):
            files["cpp_hpp"].append(file_path)
        else:
            files["hpp"].append(file_path)

    return files


def get_all_source_build_includes() -> Set[str]:
    """Extract the list of #include statements from the all-source build files.

    This function handles the hierarchical structure by checking:
    1. The main all-source build file (fastled_compile.hpp.cpp)
    2. All hierarchical module compile files (fl_compile.hpp, fx_compile.hpp, etc.)

    Returns:
        Set of included file paths (relative to src/)
    """
    includes = set()

    # Check main all-source build file
    if not ALL_SOURCE_BUILD_FILE.exists():
        print(f"Warning: All-source build file {ALL_SOURCE_BUILD_FILE} does not exist")
        return includes

    # Function to extract includes from a file
    def extract_includes_from_file(file_path: Path) -> Set[str]:
        file_includes = set()
        if not file_path.exists():
            return file_includes

        try:
            with open(file_path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    # Look for #include statements
                    if line.startswith('#include "') and line.endswith('"'):
                        # Extract the include path
                        include_path = line[10:-1]  # Remove '#include "' and '"'
                        file_includes.add(include_path)
        except Exception as e:
            print(f"Error reading file {file_path}: {e}")

        return file_includes

    # Extract includes from main file
    includes.update(extract_includes_from_file(ALL_SOURCE_BUILD_FILE))

    # Extract includes from all hierarchical files
    for hierarchical_file in HIERARCHICAL_FILES:
        if hierarchical_file.exists():
            hierarchical_includes = extract_includes_from_file(hierarchical_file)
            includes.update(hierarchical_includes)

    return includes


def check_inclusion_in_all_source_build(
    files: Dict[str, List[Path]], base_dir: Path
) -> Dict[str, Dict[str, bool]]:
    """Check which implementation files are included in the all-source build.

    Args:
        files: Dictionary of files by type
        base_dir: Base directory (fl or fx) for relative path calculation

    Returns:
        Dictionary mapping file types to dictionaries of file -> included status
    """
    all_source_includes = get_all_source_build_includes()

    inclusion_status = {"hpp": {}, "cpp_hpp": {}}

    for file_type, file_list in files.items():
        if file_type == "cpp_hpp":  # Only check .cpp.hpp files for inclusion
            for file_path in file_list:
                # Calculate relative path from src/
                relative_path = file_path.relative_to(SRC_ROOT)
                relative_path_str = str(relative_path).replace(
                    "\\", "/"
                )  # Normalize path separators

                # Check if this file is included
                is_included = relative_path_str in all_source_includes
                inclusion_status[file_type][str(file_path)] = is_included

    return inclusion_status


def print_file_list(
    files: List[Path], title: str, base_dir: Path, show_relative: bool = True
):
    """Print a formatted list of files.

    Args:
        files: List of file paths
        title: Title for the section
        base_dir: Base directory for relative path calculation
        show_relative: Whether to show relative paths
    """
    print(f"\n{title} ({len(files)} files):")
    print("-" * (len(title) + 20))

    if not files:
        print("  (none)")
        return

    # Sort files for consistent output
    sorted_files = sorted(files, key=lambda p: str(p))

    for i, file_path in enumerate(sorted_files, 1):
        if show_relative:
            rel_path = file_path.relative_to(base_dir)
            print(f"  {i:2d}. {rel_path}")
        else:
            print(f"  {i:2d}. {file_path.name}")


def print_inclusion_report(
    inclusion_status: Dict[str, Dict[str, bool]], base_dir: Path
):
    """Print a report of which implementation files are included in all-source build.

    Args:
        inclusion_status: Dictionary of inclusion status by file type
        base_dir: Base directory for relative path calculation
    """
    cpp_hpp_status = inclusion_status.get("cpp_hpp", {})

    if not cpp_hpp_status:
        print("\nNo .cpp.hpp files found to check for inclusion")
        return

    included_files = [path for path, included in cpp_hpp_status.items() if included]
    missing_files = [path for path, included in cpp_hpp_status.items() if not included]

    print("\nALL-SOURCE BUILD INCLUSION STATUS:")
    print("=" * 50)

    # Use ASCII or Unicode symbols based on environment
    check_symbol = "[+]" if USE_ASCII_ONLY else "✅"
    cross_symbol = "[-]" if USE_ASCII_ONLY else "❌"

    print(f"{check_symbol} Included in all-source build: {len(included_files)}")
    if included_files:
        for file_path in sorted(included_files):
            rel_path = Path(file_path).relative_to(SRC_ROOT)
            print(f"   {check_symbol} {rel_path}")

    print(f"\n{cross_symbol} Missing from all-source build: {len(missing_files)}")
    if missing_files:
        for file_path in sorted(missing_files):
            rel_path = Path(file_path).relative_to(SRC_ROOT)
            print(f"   {cross_symbol} {rel_path}")


def generate_summary_report(
    fl_files: Dict[str, List[Path]], fx_files: Dict[str, List[Path]]
) -> Dict:
    """Generate a summary report with statistics.

    Args:
        fl_files: Files found in fl/ directory
        fx_files: Files found in fx/ directory

    Returns:
        Dictionary containing summary statistics
    """
    summary = {
        "fl_directory": {
            "hpp_files": len(fl_files["hpp"]),
            "cpp_hpp_files": len(fl_files["cpp_hpp"]),
            "total_files": len(fl_files["hpp"]) + len(fl_files["cpp_hpp"]),
        },
        "fx_directory": {
            "hpp_files": len(fx_files["hpp"]),
            "cpp_hpp_files": len(fx_files["cpp_hpp"]),
            "total_files": len(fx_files["hpp"]) + len(fx_files["cpp_hpp"]),
        },
        "totals": {
            "hpp_files": len(fl_files["hpp"]) + len(fx_files["hpp"]),
            "cpp_hpp_files": len(fl_files["cpp_hpp"]) + len(fx_files["cpp_hpp"]),
            "total_files": len(fl_files["hpp"])
            + len(fl_files["cpp_hpp"])
            + len(fx_files["hpp"])
            + len(fx_files["cpp_hpp"]),
        },
    }

    return summary


def print_summary_report(summary: Dict):
    """Print a formatted summary report.

    Args:
        summary: Summary statistics dictionary
    """
    print("\n" + "=" * 80)
    print("IMPLEMENTATION FILES SUMMARY REPORT")
    print("=" * 80)

    # Use ASCII or Unicode symbols based on environment
    folder_symbol = "[DIR]" if USE_ASCII_ONLY else "📁"
    chart_symbol = "[STATS]" if USE_ASCII_ONLY else "📊"
    ratio_symbol = "[RATIO]" if USE_ASCII_ONLY else "📈"

    print(f"\n{folder_symbol} FL DIRECTORY ({FL_DIR.relative_to(PROJECT_ROOT)}):")
    print(
        f"   Header files (.hpp):           {summary['fl_directory']['hpp_files']:3d}"
    )
    print(
        f"   Implementation files (.cpp.hpp): {summary['fl_directory']['cpp_hpp_files']:3d}"
    )
    print(
        f"   Total files:                   {summary['fl_directory']['total_files']:3d}"
    )

    print(f"\n{folder_symbol} FX DIRECTORY ({FX_DIR.relative_to(PROJECT_ROOT)}):")
    print(
        f"   Header files (.hpp):           {summary['fx_directory']['hpp_files']:3d}"
    )
    print(
        f"   Implementation files (.cpp.hpp): {summary['fx_directory']['cpp_hpp_files']:3d}"
    )
    print(
        f"   Total files:                   {summary['fx_directory']['total_files']:3d}"
    )

    print(f"\n{chart_symbol} TOTALS:")
    print(f"   Header files (.hpp):           {summary['totals']['hpp_files']:3d}")
    print(
        f"   Implementation files (.cpp.hpp): {summary['totals']['cpp_hpp_files']:3d}"
    )
    print(f"   Total files:                   {summary['totals']['total_files']:3d}")

    # Calculate ratios
    total_hpp = summary["totals"]["hpp_files"]
    total_cpp_hpp = summary["totals"]["cpp_hpp_files"]

    if total_hpp > 0:
        impl_ratio = (total_cpp_hpp / total_hpp) * 100
        print(f"\n{ratio_symbol} IMPLEMENTATION RATIO:")
        print(f"   Implementation files per header: {impl_ratio:.1f}%")
        print(f"   ({total_cpp_hpp} .cpp.hpp files for {total_hpp} .hpp files)")


def main():
    """Main function to run the implementation files checker."""
    parser = argparse.ArgumentParser(
        description="Check *.hpp and *.cpp.hpp files in fl/ and fx/ directories"
    )
    parser.add_argument("--list", action="store_true", help="List all files found")
    parser.add_argument(
        "--check-inclusion",
        action="store_true",
        help="Check which .cpp.hpp files are included in all-source build",
    )
    parser.add_argument("--json", action="store_true", help="Output summary as JSON")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument(
        "--ascii-only",
        action="store_true",
        help="Use ASCII-only output (no Unicode emoji)",
    )
    parser.add_argument(
        "--suppress-summary-on-100-percent",
        action="store_true",
        help="Suppress summary report when inclusion percentage is 100%",
    )

    args = parser.parse_args()

    # Override USE_ASCII_ONLY if command line flag is set
    global USE_ASCII_ONLY
    if args.ascii_only:
        USE_ASCII_ONLY = True

    # Collect files from both directories
    print("Scanning implementation files...")
    fl_files = collect_files_by_type(FL_DIR)
    fx_files = collect_files_by_type(FX_DIR)

    # Generate summary
    summary = generate_summary_report(fl_files, fx_files)

    # Define symbols once for all output modes
    search_symbol = "[SEARCH]" if USE_ASCII_ONLY else "🔍"
    stats_symbol = "[STATS]" if USE_ASCII_ONLY else "📊"
    config_symbol = "[CONFIG]" if USE_ASCII_ONLY else "🔧"

    # Calculate inclusion percentage to determine if we should suppress summary
    should_suppress_summary = False
    if args.suppress_summary_on_100_percent:
        all_cpp_hpp_files = fl_files["cpp_hpp"] + fx_files["cpp_hpp"]
        all_source_includes = get_all_source_build_includes()

        included_count = 0
        for file_path in all_cpp_hpp_files:
            relative_path = file_path.relative_to(SRC_ROOT)
            relative_path_str = str(relative_path).replace("\\", "/")
            if relative_path_str in all_source_includes:
                included_count += 1

        total_impl_files = len(all_cpp_hpp_files)
        if total_impl_files > 0:
            inclusion_percentage = (included_count / total_impl_files) * 100
            should_suppress_summary = inclusion_percentage >= 100.0

    # Output based on requested format
    if args.json:
        # Add file lists to summary for JSON output
        summary["fl_files"] = {
            "hpp": [str(p.relative_to(SRC_ROOT)) for p in fl_files["hpp"]],
            "cpp_hpp": [str(p.relative_to(SRC_ROOT)) for p in fl_files["cpp_hpp"]],
        }
        summary["fx_files"] = {
            "hpp": [str(p.relative_to(SRC_ROOT)) for p in fx_files["hpp"]],
            "cpp_hpp": [str(p.relative_to(SRC_ROOT)) for p in fx_files["cpp_hpp"]],
        }
        print(json.dumps(summary, indent=2))
        return

    # Print summary report only if not suppressed
    if not should_suppress_summary:
        print_summary_report(summary)
    else:
        print("Summary report suppressed: 100% inclusion coverage achieved")

    # List files if requested
    if args.list:
        print("\n" + "=" * 80)
        print("DETAILED FILE LISTINGS")
        print("=" * 80)

        # FL directory files
        print_file_list(fl_files["hpp"], "FL Header Files (.hpp)", FL_DIR)
        print_file_list(
            fl_files["cpp_hpp"], "FL Implementation Files (.cpp.hpp)", FL_DIR
        )

        # FX directory files
        print_file_list(fx_files["hpp"], "FX Header Files (.hpp)", FX_DIR)
        print_file_list(
            fx_files["cpp_hpp"], "FX Implementation Files (.cpp.hpp)", FX_DIR
        )

    # Check inclusion in all-source build if requested
    if args.check_inclusion:
        # Only show inclusion check if not suppressing or if we don't have 100% coverage
        if not should_suppress_summary:
            print("\n" + "=" * 80)
            print("ALL-SOURCE BUILD INCLUSION CHECK")
            print("=" * 80)

            fl_inclusion = check_inclusion_in_all_source_build(fl_files, FL_DIR)
            fx_inclusion = check_inclusion_in_all_source_build(fx_files, FX_DIR)

            print(f"\n{search_symbol} FL DIRECTORY INCLUSION:")
            print_inclusion_report(fl_inclusion, FL_DIR)

            print(f"\n{search_symbol} FX DIRECTORY INCLUSION:")
            print_inclusion_report(fx_inclusion, FX_DIR)

            # Overall inclusion statistics
            all_cpp_hpp_files = fl_files["cpp_hpp"] + fx_files["cpp_hpp"]
            all_source_includes = get_all_source_build_includes()

            included_count = 0
            for file_path in all_cpp_hpp_files:
                relative_path = file_path.relative_to(SRC_ROOT)
                relative_path_str = str(relative_path).replace("\\", "/")
                if relative_path_str in all_source_includes:
                    included_count += 1

            total_impl_files = len(all_cpp_hpp_files)
            if total_impl_files > 0:
                inclusion_percentage = (included_count / total_impl_files) * 100
                print(f"\n{stats_symbol} OVERALL INCLUSION STATISTICS:")
                print(f"   Total .cpp.hpp files found:     {total_impl_files}")
                print(f"   Included in all-source build:   {included_count}")
                print(f"   Inclusion percentage:           {inclusion_percentage:.1f}%")

        # Always check for missing files and exit with error if any are missing
        all_cpp_hpp_files = fl_files["cpp_hpp"] + fx_files["cpp_hpp"]
        all_source_includes = get_all_source_build_includes()

        included_count = 0
        for file_path in all_cpp_hpp_files:
            relative_path = file_path.relative_to(SRC_ROOT)
            relative_path_str = str(relative_path).replace("\\", "/")
            if relative_path_str in all_source_includes:
                included_count += 1

        total_impl_files = len(all_cpp_hpp_files)
        total_missing = total_impl_files - included_count
        if total_missing > 0:
            # Print an explicit error message before exiting
            error_symbol = "[ERROR]" if USE_ASCII_ONLY else "🚨"
            print(
                f"\n{error_symbol} {total_missing} implementation file(s) are missing from the all-source build!"
            )
            print("   Failing script due to incomplete all-source build inclusion.")
            import sys

            sys.exit(1)

    if args.verbose:
        print(f"\n{config_symbol} CONFIGURATION:")
        print(f"   Project root: {PROJECT_ROOT}")
        print(f"   FL directory: {FL_DIR}")
        print(f"   FX directory: {FX_DIR}")
        print(f"   All-source build file: {ALL_SOURCE_BUILD_FILE}")
        print("   Hierarchical compile files:")
        for hfile in HIERARCHICAL_FILES:
            status = "✓" if hfile.exists() else "✗"
            print(f"     {status} {hfile.relative_to(PROJECT_ROOT)}")


if __name__ == "__main__":
    main()
