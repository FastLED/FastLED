#!/usr/bin/env python3
"""
Visualize FastLED dependencies - shows which operations monitor which files.

Provides multiple views:
1. By operation - what files does each operation monitor?
2. By file - which operations monitor this file?
3. Dependency graph - which operations depend on which?
"""

import json
from pathlib import Path
from typing import Any, Dict, List, Set, Tuple


def load_manifest() -> dict[str, Any]:
    """Load the dependencies manifest."""
    # Try multiple locations in order of preference
    candidates = [
        Path("ci") / "dependencies.json",  # Primary location
        Path(".") / "dependencies.json",  # Fallback for backward compatibility
    ]

    manifest_path = None
    for candidate in candidates:
        if candidate.exists():
            manifest_path = candidate
            break

    if manifest_path is None:
        raise FileNotFoundError(
            f"dependencies.json not found in any of: {', '.join(str(c) for c in candidates)}"
        )

    with open(manifest_path, "r") as f:
        return json.load(f)


def print_by_operation(manifest: dict[str, Any]) -> None:
    """Print all operations and their monitored files."""
    print("\n" + "=" * 80)
    print("DEPENDENCIES BY OPERATION - What does each operation monitor?")
    print("=" * 80)

    for op_name, op_config in manifest["operations"].items():
        print(f"\n📋 {op_name.upper()}")
        print(f"   {op_config.get('description', 'No description')}")
        print(f"   Cache: {op_config.get('cache_file', 'N/A')}")

        globs = op_config.get("globs", [])
        excludes = op_config.get("excludes", [])
        tools = op_config.get("tools", [])

        print(f"   Patterns ({len(globs)}):")
        for glob in globs:
            print(f"     ✓ {glob}")

        if excludes:
            print(f"   Excludes ({len(excludes)}):")
            for exclude in excludes:
                print(f"     ✗ {exclude}")

        if tools:
            print(f"   Tools ({len(tools)}):")
            for tool in tools[:5]:
                print(f"     • {tool}")
            if len(tools) > 5:
                print(f"     ... and {len(tools) - 5} more")


def print_by_file(manifest: dict[str, Any]) -> None:
    """Print which operations monitor which directories."""
    print("\n" + "=" * 80)
    print("DEPENDENCIES BY FILE - Which operations monitor each directory?")
    print("=" * 80)

    file_coverage = manifest.get("file_coverage", {})

    for directory, coverage in file_coverage.items():
        # Skip metadata fields
        if directory == "description" or isinstance(coverage, str):
            continue

        operations = coverage.get("monitored_by", [])
        print(f"\n📁 {directory}")
        if operations:
            for op in operations:
                print(f"   ✓ {op}")
        else:
            print(f"   (No operations monitor this)")


def print_dependency_graph(manifest: dict[str, Any]) -> None:
    """Print the dependency graph."""
    print("\n" + "=" * 80)
    print("DEPENDENCY GRAPH - Which operations depend on which?")
    print("=" * 80)

    dep_graph: dict[str, Any] = manifest.get("dependency_graph", {})

    for op_name, deps in dep_graph.items():
        depends_on: list[str] = deps.get("depends_on", [])
        print(f"\n🔹 {op_name}")
        if depends_on:
            for dep in depends_on:
                print(f"   → depends on: {dep}")
        else:
            print(f"   (independent operation)")


def print_quick_reference(manifest: dict[str, Any]) -> None:
    """Print a quick reference table."""
    print("\n" + "=" * 80)
    print("QUICK REFERENCE TABLE")
    print("=" * 80)

    print("\n{:<20} {:<40} {:<15}".format("Operation", "Description", "Files"))
    print("-" * 75)

    for op_name, op_config in manifest["operations"].items():
        desc = op_config.get("description", "")[:38]
        globs_count = len(op_config.get("globs", []))
        print("{:<20} {:<40} {:<15}".format(op_name, desc, f"{globs_count} globs"))


def print_statistics(manifest: dict[str, Any]) -> None:
    """Print statistics about the manifest."""
    print("\n" + "=" * 80)
    print("MANIFEST STATISTICS")
    print("=" * 80)

    ops: dict[str, Any] = manifest.get("operations", {})
    total_globs = sum(len(op.get("globs", [])) for op in ops.values())
    total_excludes = sum(len(op.get("excludes", [])) for op in ops.values())
    total_tools = sum(len(op.get("tools", [])) for op in ops.values())

    print(f"\n📊 Overview:")
    print(f"   Total operations: {len(ops)}")
    print(f"   Total glob patterns: {total_globs}")
    print(f"   Total excludes: {total_excludes}")
    print(f"   Total tools: {total_tools}")

    # Find largest operation
    largest_op: tuple[str, Any] = max(
        ops.items(), key=lambda x: len(x[1].get("globs", []))
    )
    print(f"\n📈 Largest operation:")
    print(f"   {largest_op[0]}: {len(largest_op[1].get('globs', []))} globs")

    # Find operations with most tools
    tools_count: dict[str, int] = {
        op: len(config.get("tools", [])) for op, config in ops.items()
    }
    most_tools: tuple[str, int] = max(tools_count.items(), key=lambda x: x[1])
    if most_tools[1] > 0:
        print(f"\n🔧 Most tools:")
        print(f"   {most_tools[0]}: {most_tools[1]} tools")


def main() -> int:
    """Run the visualization."""
    try:
        manifest = load_manifest()
        print("\n" + "🔍 " * 30)
        print("FASTLED CENTRALIZED DEPENDENCY MANIFEST VISUALIZATION")
        print("🔍 " * 30)

        # Print all views
        print_statistics(manifest)
        print_quick_reference(manifest)
        print_by_operation(manifest)
        print_by_file(manifest)
        print_dependency_graph(manifest)

        print("\n" + "=" * 80)
        print("✅ Manifest visualization complete")
        print("=" * 80 + "\n")

    except FileNotFoundError as e:
        print(f"❌ Error: {e}")
        return 1
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        import traceback

        traceback.print_exc()
        return 1

    return 0


if __name__ == "__main__":
    exit(main())
