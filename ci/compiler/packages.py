import json
import sys
from typing import Any, Dict, List
from urllib.request import urlopen


def print_separator(title: str) -> None:
    """Print a formatted section separator"""
    print(f"\n{'=' * 60}")
    print(f" {title}")
    print(f"{'=' * 60}")


def print_subsection(title: str) -> None:
    """Print a formatted subsection separator"""
    print(f"\n{'-' * 40}")
    print(f" {title}")
    print(f"{'-' * 40}")


def analyze_esp32_packages(data: Dict[str, Any]) -> None:
    """Analyze and display the ESP32 package structure in a readable format"""

    print_separator("ESP32 PACKAGE INDEX ANALYSIS")

    # Top level structure
    print("\nROOT STRUCTURE:")
    packages = data.get("packages", [])
    print(f"  Contains {len(packages)} package(s)")

    if not packages:
        print("  No packages found!")
        return

    # Analyze first package (should be the ESP32 package)
    package = packages[0]
    print_subsection("PACKAGE INFORMATION")

    # Basic package info
    basic_info = ["name", "maintainer", "websiteURL", "email"]
    for key in basic_info:
        if key in package:
            print(f"  {key}: {package[key]}")

    # Platform analysis
    platforms = package.get("platforms", [])
    print_subsection(f"PLATFORMS ({len(platforms)} available)")

    if platforms:
        # Show first platform details
        platform = platforms[0]
        print(
            f"  Latest Platform: {platform.get('name', 'Unknown')} v{platform.get('version', 'Unknown')}"
        )
        print(f"  Architecture: {platform.get('architecture', 'Unknown')}")
        print(f"  Archive Size: {int(platform.get('size', 0)) / 1024 / 1024:.1f} MB")

        # Show boards
        boards = platform.get("boards", [])
        print(f"\n  SUPPORTED BOARDS ({len(boards)} total):")
        for board in boards:
            board_name = board.get("name", "Unknown Board")
            print(f"    - {board_name}")

        # Show tool dependencies
        tools = platform.get("toolsDependencies", [])
        print(f"\n  TOOL DEPENDENCIES ({len(tools)} total):")
        tool_categories: Dict[str, List[str]] = {}
        for tool in tools:
            packager = tool.get("packager", "unknown")
            tool_name = tool.get("name", "unknown")
            if packager not in tool_categories:
                tool_categories[packager] = []
            tool_categories[packager].append(tool_name)

        for packager, tool_names in tool_categories.items():
            print(f"    {packager} tools:")
            for tool_name in tool_names:
                print(f"      - {tool_name}")

    # Tools analysis
    tools = package.get("tools", [])
    print_subsection(f"AVAILABLE TOOLS ({len(tools)} total)")

    if tools:
        tool_types: Dict[str, int] = {}
        for tool in tools:
            tool_name = tool.get("name", "unknown")
            if "gcc" in tool_name.lower():
                category = "Compilers (GCC)"
            elif "gdb" in tool_name.lower():
                category = "Debuggers (GDB)"
            elif any(x in tool_name.lower() for x in ["esp", "tool", "util"]):
                category = "ESP Tools"
            else:
                category = "Other Tools"

            tool_types[category] = tool_types.get(category, 0) + 1

        for category, count in tool_types.items():
            print(f"  {category}: {count}")

    # Show version history for platforms
    print_subsection("VERSION HISTORY")
    version_count = min(len(platforms), 5)  # Show up to 5 recent versions
    for i in range(version_count):
        platform = platforms[i]
        version = platform.get("version", "Unknown")
        size_mb = int(platform.get("size", 0)) / 1024 / 1024
        print(f"  v{version}: {size_mb:.1f} MB")


def main() -> None:
    try:
        url: str = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"

        print("Fetching ESP32 package index...")
        with urlopen(url) as response:
            content: bytes = response.read()
            data: Dict[str, Any] = json.loads(content.decode("utf-8"))

        analyze_esp32_packages(data)

    except KeyboardInterrupt:
        print("Interrupted by user", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error fetching or parsing JSON: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
