import json
import sys
from dataclasses import dataclass
from typing import Any, Dict, List
from urllib.request import urlopen

import emoji


def _can_use_emoji() -> bool:
    """Check if emoji can be used in the current environment"""
    # Check if stdout is a TTY (not redirected to file)
    return hasattr(sys.stdout, "isatty") and sys.stdout.isatty()


@dataclass
class Help:
    """Help information with online documentation links"""

    online: str


@dataclass
class Board:
    """Supported board information"""

    name: str


@dataclass
class ToolDependency:
    """Tool dependency specification"""

    packager: str
    name: str
    version: str


@dataclass
class System:
    """System-specific tool information"""

    host: str
    url: str
    archiveFileName: str
    checksum: str
    size: str


@dataclass
class Tool:
    """Tool specification with version and systems"""

    name: str
    version: str
    systems: List[System]


@dataclass
class Platform:
    """Platform specification with boards and dependencies"""

    name: str
    architecture: str
    version: str
    category: str
    url: str
    archiveFileName: str
    checksum: str
    size: str
    help: Help
    boards: List[Board]
    toolsDependencies: List[ToolDependency]


@dataclass
class Package:
    """ESP32 package containing platforms and tools"""

    name: str
    maintainer: str
    websiteURL: str
    email: str
    help: Help
    platforms: List[Platform]
    tools: List[Tool]


@dataclass
class PackageIndex:
    """Root package index structure"""

    packages: List[Package]


def parse_package_index(json_str: str) -> PackageIndex:
    """Parse JSON string into typed PackageIndex structure"""
    data = json.loads(json_str)

    packages: List[Package] = []
    for pkg_data in data["packages"]:
        # Parse help
        help_data = pkg_data["help"]
        help_obj = Help(online=help_data["online"])

        # Parse platforms
        platforms: List[Platform] = []
        for platform_data in pkg_data["platforms"]:
            # Parse platform help
            platform_help = Help(online=platform_data["help"]["online"])

            # Parse boards
            boards: List[Board] = []
            for board_data in platform_data["boards"]:
                boards.append(Board(name=board_data["name"]))

            # Parse tool dependencies
            tool_deps: List[ToolDependency] = []
            for dep_data in platform_data["toolsDependencies"]:
                tool_deps.append(
                    ToolDependency(
                        packager=dep_data["packager"],
                        name=dep_data["name"],
                        version=dep_data["version"],
                    )
                )

            platforms.append(
                Platform(
                    name=platform_data["name"],
                    architecture=platform_data["architecture"],
                    version=platform_data["version"],
                    category=platform_data["category"],
                    url=platform_data["url"],
                    archiveFileName=platform_data["archiveFileName"],
                    checksum=platform_data["checksum"],
                    size=platform_data["size"],
                    help=platform_help,
                    boards=boards,
                    toolsDependencies=tool_deps,
                )
            )

        # Parse tools
        tools: List[Tool] = []
        for tool_data in pkg_data["tools"]:
            # Parse systems
            systems: List[System] = []
            for system_data in tool_data["systems"]:
                systems.append(
                    System(
                        host=system_data["host"],
                        url=system_data["url"],
                        archiveFileName=system_data["archiveFileName"],
                        checksum=system_data["checksum"],
                        size=system_data["size"],
                    )
                )

            tools.append(
                Tool(
                    name=tool_data["name"],
                    version=tool_data["version"],
                    systems=systems,
                )
            )

        packages.append(
            Package(
                name=pkg_data["name"],
                maintainer=pkg_data["maintainer"],
                websiteURL=pkg_data["websiteURL"],
                email=pkg_data["email"],
                help=help_obj,
                platforms=platforms,
                tools=tools,
            )
        )

    return PackageIndex(packages=packages)


def serialize_package_index(package_index: PackageIndex) -> str:
    """Serialize PackageIndex structure back to JSON string"""

    def help_to_dict(help_obj: Help) -> Dict[str, str]:
        return {"online": help_obj.online}

    def board_to_dict(board: Board) -> Dict[str, str]:
        return {"name": board.name}

    def tool_dep_to_dict(tool_dep: ToolDependency) -> Dict[str, str]:
        return {
            "packager": tool_dep.packager,
            "name": tool_dep.name,
            "version": tool_dep.version,
        }

    def system_to_dict(system: System) -> Dict[str, str]:
        return {
            "host": system.host,
            "url": system.url,
            "archiveFileName": system.archiveFileName,
            "checksum": system.checksum,
            "size": system.size,
        }

    def tool_to_dict(tool: Tool) -> Dict[str, Any]:
        return {
            "name": tool.name,
            "version": tool.version,
            "systems": [system_to_dict(system) for system in tool.systems],
        }

    def platform_to_dict(platform: Platform) -> Dict[str, Any]:
        return {
            "name": platform.name,
            "architecture": platform.architecture,
            "version": platform.version,
            "category": platform.category,
            "url": platform.url,
            "archiveFileName": platform.archiveFileName,
            "checksum": platform.checksum,
            "size": platform.size,
            "help": help_to_dict(platform.help),
            "boards": [board_to_dict(board) for board in platform.boards],
            "toolsDependencies": [
                tool_dep_to_dict(dep) for dep in platform.toolsDependencies
            ],
        }

    def package_to_dict(package: Package) -> Dict[str, Any]:
        return {
            "name": package.name,
            "maintainer": package.maintainer,
            "websiteURL": package.websiteURL,
            "email": package.email,
            "help": help_to_dict(package.help),
            "platforms": [platform_to_dict(platform) for platform in package.platforms],
            "tools": [tool_to_dict(tool) for tool in package.tools],
        }

    result = {
        "packages": [package_to_dict(package) for package in package_index.packages]
    }

    return json.dumps(result, indent=2)


def print_tree_header(title: str) -> None:
    """Print a beautiful tree-style header"""
    border = "=" * (len(title) + 4)
    print(f"\n+{border}+")
    print(f"|  {title}  |")
    print(f"+{border}+")


def _calculate_indent(level: int) -> str:
    """Calculate consistent indentation for tree structure"""
    # Base indentation: 2 spaces per level
    # When using emojis at level 0, add 2 extra spaces to align with emoji width
    if _can_use_emoji() and level > 0:
        return "  " * (level + 1)  # Add 1 extra level for emoji alignment
    else:
        return "  " * level


def print_tree_section(title: str, level: int = 0) -> None:
    """Print a tree-style section header"""
    indent = "  " * level
    if _can_use_emoji() and emoji is not None:
        if level == 0:
            icon = emoji.emojize(":package:")
        else:
            icon = emoji.emojize(":file_folder:")
        print(f"\n{indent}{icon} {title}")
    else:
        print(f"\n{indent}+-- {title}")


def print_tree_item(
    label: str, value: str, level: int = 1, is_last: bool = False
) -> None:
    """Print a tree-style item"""
    indent = _calculate_indent(level)
    if _can_use_emoji():
        connector = "└── " if is_last else "├── "
    else:
        connector = "`-- " if is_last else "|-- "
    print(f"{indent}{connector}{label}: {value}")


def print_tree_list_header(title: str, count: int, level: int = 1) -> None:
    """Print a tree-style list header with count"""
    indent = _calculate_indent(level)
    if _can_use_emoji() and emoji is not None:
        icon = emoji.emojize(":clipboard:")
        connector = "├── "
        print(f"{indent}{connector}{icon} {title} ({count} items)")
    else:
        connector = "|-- "
        print(f"{indent}{connector}{title} ({count} items)")


def print_tree_list_item(item: str, level: int = 2, is_last: bool = False) -> None:
    """Print a tree-style list item"""
    indent = _calculate_indent(level)
    if _can_use_emoji():
        connector = "└── " if is_last else "├── "
    else:
        connector = "`-- " if is_last else "|-- "
    print(f"{indent}{connector}{item}")


def print_tree_close_section(level: int = 0) -> None:
    """Print a tree-style section closer"""
    indent = _calculate_indent(level)
    if _can_use_emoji() and emoji is not None:
        icon = emoji.emojize(":check_mark:")
        print(f"{indent}└── {icon} COMPLETE")
    else:
        print(f"{indent}`-- [COMPLETE]")


def analyze_esp32_packages_typed(package_index: PackageIndex) -> None:
    """Analyze ESP32 packages using beautiful tree-style output"""

    print_tree_header("ESP32 PACKAGE INDEX")

    # Root structure
    print_tree_section("Package Index")
    print_tree_item("Total Packages", str(len(package_index.packages)), level=1)

    if not package_index.packages:
        print_tree_item("Status", "No packages found!", level=1, is_last=True)
        return

    # Analyze first package (should be the ESP32 package)
    package = package_index.packages[0]

    print_tree_section("Package Details", level=1)
    print_tree_item("Name", package.name, level=2)
    print_tree_item("Maintainer", package.maintainer, level=2)
    print_tree_item("Website", package.websiteURL, level=2)
    print_tree_item("Email", package.email, level=2, is_last=True)

    # Platform analysis
    if package.platforms:
        print_tree_section("Latest Platform", level=1)
        platform = package.platforms[0]
        size_mb = int(platform.size) / 1024 / 1024

        print_tree_item("Name", platform.name, level=2)
        print_tree_item("Version", platform.version, level=2)
        print_tree_item("Architecture", platform.architecture, level=2)
        print_tree_item("Category", platform.category, level=2)
        print_tree_item("Archive Size", f"{size_mb:.1f} MB", level=2)

        # Show boards in tree format
        print_tree_list_header("Supported Boards", len(platform.boards), level=2)
        for i, board in enumerate(platform.boards):
            is_last_board = i == len(platform.boards) - 1
            print_tree_list_item(board.name, level=3, is_last=is_last_board)

        # Show tool dependencies in tree format
        print_tree_list_header(
            "Tool Dependencies", len(platform.toolsDependencies), level=2
        )
        tool_categories: Dict[str, List[str]] = {}
        for tool_dep in platform.toolsDependencies:
            if tool_dep.packager not in tool_categories:
                tool_categories[tool_dep.packager] = []
            tool_categories[tool_dep.packager].append(tool_dep.name)

        packager_list = list(tool_categories.items())
        for pkg_idx, (packager, tool_names) in enumerate(packager_list):
            is_last_packager = pkg_idx == len(packager_list) - 1
            print_tree_list_item(
                f"{packager} ({len(tool_names)} tools)",
                level=3,
                is_last=is_last_packager,
            )
            for tool_idx, tool_name in enumerate(tool_names):
                is_last_tool = tool_idx == len(tool_names) - 1
                print_tree_list_item(tool_name, level=4, is_last=is_last_tool)

    # Tools analysis
    if package.tools:
        print_tree_section("Tool Categories", level=1)
        tool_types: Dict[str, int] = {}
        for tool in package.tools:
            if "gcc" in tool.name.lower():
                category = "Compilers (GCC)"
            elif "gdb" in tool.name.lower():
                category = "Debuggers (GDB)"
            elif any(x in tool.name.lower() for x in ["esp", "tool", "util"]):
                category = "ESP Tools"
            else:
                category = "Other Tools"

            tool_types[category] = tool_types.get(category, 0) + 1

        categories = list(tool_types.items())
        for cat_idx, (category, count) in enumerate(categories):
            is_last_category = cat_idx == len(categories) - 1
            print_tree_item(category, str(count), level=2, is_last=is_last_category)

    # Show version history for platforms
    if len(package.platforms) > 1:
        print_tree_section("Version History", level=1)
        version_count = min(len(package.platforms), 5)  # Show up to 5 recent versions
        for i in range(version_count):
            platform = package.platforms[i]
            size_mb = int(platform.size) / 1024 / 1024
            is_last_version = i == version_count - 1
            print_tree_item(
                f"v{platform.version}",
                f"{size_mb:.1f} MB",
                level=2,
                is_last=is_last_version,
            )

    # Close the main tree
    print_tree_close_section()


def main() -> None:
    try:
        url: str = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"

        print("Fetching ESP32 package index...")
        with urlopen(url) as response:
            content: bytes = response.read()
            json_str = content.decode("utf-8")

        # Parse into typed structure
        package_index = parse_package_index(json_str)

        # Display using typed structure
        analyze_esp32_packages_typed(package_index)

        # Demonstrate serialization back to JSON
        print_tree_header("SERIALIZATION DEMO")

        first_package = package_index.packages[0]
        first_platform = first_package.platforms[0]

        print_tree_section("Data Sample")
        if _can_use_emoji() and emoji is not None:
            gear_icon = emoji.emojize(":gear:")
            hash_icon = emoji.emojize(":hash:")
            chart_icon = emoji.emojize(":bar_chart:")
            wrench_icon = emoji.emojize(":wrench:")
            print_tree_item(f"{gear_icon} Platform Name", first_platform.name, level=1)
            print_tree_item(
                f"{hash_icon} Platform Version", first_platform.version, level=1
            )
            print_tree_item(
                f"{chart_icon} Board Count", str(len(first_platform.boards)), level=1
            )
            print_tree_item(
                f"{wrench_icon} First Board",
                first_platform.boards[0].name,
                level=1,
                is_last=True,
            )
        else:
            print_tree_item("Platform Name", first_platform.name, level=1)
            print_tree_item("Platform Version", first_platform.version, level=1)
            print_tree_item("Board Count", str(len(first_platform.boards)), level=1)
            print_tree_item(
                "First Board", first_platform.boards[0].name, level=1, is_last=True
            )

        # Show the dataclass types in a beautiful format
        print_tree_section("Type Information")
        if _can_use_emoji() and emoji is not None:
            package_icon = emoji.emojize(":package:")
            building_icon = emoji.emojize(":office_building:")
            gear_icon = emoji.emojize(":gear:")
            wrench_icon = emoji.emojize(":wrench:")
            print_tree_item(
                f"{package_icon} PackageIndex", "Root container dataclass", level=1
            )
            print_tree_item(
                f"{building_icon} Package", "ESP32 package dataclass", level=1
            )
            print_tree_item(
                f"{gear_icon} Platform", "Platform version dataclass", level=1
            )
            print_tree_item(
                f"{wrench_icon} Board",
                "Hardware board dataclass",
                level=1,
                is_last=True,
            )
        else:
            print_tree_item("PackageIndex", "Root container dataclass", level=1)
            print_tree_item("Package", "ESP32 package dataclass", level=1)
            print_tree_item("Platform", "Platform version dataclass", level=1)
            print_tree_item("Board", "Hardware board dataclass", level=1, is_last=True)

        # Demonstrate round-trip serialization
        print_tree_section("Round-Trip Test")
        serialized_json = serialize_package_index(package_index)
        reparsed_index = parse_package_index(serialized_json)

        # Verify the data survived round-trip
        original_board_count = len(package_index.packages[0].platforms[0].boards)
        reparsed_board_count = len(reparsed_index.packages[0].platforms[0].boards)

        if _can_use_emoji():
            outbox_icon = emoji.emojize(":outbox_tray:")
            inbox_icon = emoji.emojize(":inbox_tray:")
            target_icon = emoji.emojize(":direct_hit:")
            page_icon = emoji.emojize(":page_facing_up:")
            check_icon = emoji.emojize(":check_mark:")
            cross_icon = emoji.emojize(":cross_mark:")

            print_tree_item(
                f"{outbox_icon} Original Boards", str(original_board_count), level=1
            )
            print_tree_item(
                f"{inbox_icon} Reparsed Boards", str(reparsed_board_count), level=1
            )
            success_status = (
                f"{check_icon} SUCCESS"
                if original_board_count == reparsed_board_count
                else f"{cross_icon} FAILED"
            )
            print_tree_item(f"{target_icon} Status", success_status, level=1)

            # Show a compact snippet of the serialized JSON
            json_lines = serialized_json.split("\n")[:4]  # First 4 lines
            json_compact = " ".join(line.strip() for line in json_lines)
            if len(json_compact) > 80:
                json_compact = json_compact[:77] + "..."
            print_tree_item(
                f"{page_icon} JSON Sample", json_compact, level=1, is_last=True
            )
        else:
            print_tree_item("Original Boards", str(original_board_count), level=1)
            print_tree_item("Reparsed Boards", str(reparsed_board_count), level=1)
            success_status = (
                "SUCCESS" if original_board_count == reparsed_board_count else "FAILED"
            )
            print_tree_item("Status", success_status, level=1)

            # Show a compact snippet of the serialized JSON
            json_lines = serialized_json.split("\n")[:4]  # First 4 lines
            json_compact = " ".join(line.strip() for line in json_lines)
            if len(json_compact) > 80:
                json_compact = json_compact[:77] + "..."
            print_tree_item("JSON Sample", json_compact, level=1, is_last=True)

    except KeyboardInterrupt:
        print("Interrupted by user", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error fetching or parsing JSON: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
