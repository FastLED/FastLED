import json
import sys
from dataclasses import dataclass
from typing import Any, Dict, List
from urllib.request import urlopen


@dataclass
class Board:
    """Board information"""

    name: str
    properties: Dict[str, Any]


@dataclass
class ToolDependency:
    """Tool dependency specification"""

    packager: str
    name: str
    version: str


@dataclass
class Platform:
    """Platform specification with boards and dependencies"""

    name: str
    version: str
    architecture: str
    boards: List[Board]
    tool_dependencies: List[ToolDependency]
    size_mb: float


@dataclass
class Package:
    """Package containing platforms and tools"""

    name: str
    maintainer: str
    platforms: List[Platform]


def parse_board(board_data: Dict[str, Any]) -> Board:
    """Parse board data into Board object"""
    name: str = board_data["name"]

    # Flatten all properties into a single dict
    properties: Dict[str, Any] = {}
    for key, value in board_data.items():
        if key != "name":
            properties[key] = value

    return Board(name=name, properties=properties)


def parse_package_index(json_str: str) -> Package:
    """Parse JSON string into Package structure"""
    data: Dict[str, Any] = json.loads(json_str)

    # Get first package (ESP32)
    pkg_data: Dict[str, Any] = data["packages"][0]

    platforms: List[Platform] = []
    for platform_data in pkg_data["platforms"]:
        # Parse boards
        boards: List[Board] = []
        for board_data in platform_data["boards"]:
            boards.append(parse_board(board_data))

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

        # Convert size to MB
        size_bytes: int = int(platform_data["size"])
        size_mb: float = size_bytes / (1024 * 1024)

        platforms.append(
            Platform(
                name=platform_data["name"],
                version=platform_data["version"],
                architecture=platform_data["architecture"],
                boards=boards,
                tool_dependencies=tool_deps,
                size_mb=size_mb,
            )
        )

    return Package(
        name=pkg_data["name"], maintainer=pkg_data["maintainer"], platforms=platforms
    )


def display_package_info(package: Package) -> None:
    """Display package information in a simple format"""
    print(f"\nPackage: {package.name}")
    print(f"Maintainer: {package.maintainer}")
    print(f"Platforms: {len(package.platforms)}")

    for platform in package.platforms[:3]:  # Show first 3 platforms
        print(f"\n  Platform: {platform.name} v{platform.version}")
        print(f"  Architecture: {platform.architecture}")
        print(f"  Size: {platform.size_mb:.1f} MB")
        print(f"  Boards: {len(platform.boards)}")

        # Show first 5 boards
        for board in platform.boards[:5]:
            print(f"    - {board.name}")
        if len(platform.boards) > 5:
            print(f"    ... and {len(platform.boards) - 5} more")

        print(f"  Tool Dependencies: {len(platform.tool_dependencies)}")


def fetch_esp32_packages() -> Package:
    """Fetch and parse ESP32 package index"""
    url: str = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"

    try:
        print("Fetching ESP32 package index...")
        with urlopen(url) as response:
            content: bytes = response.read()
            json_str: str = content.decode("utf-8")

        return parse_package_index(json_str)

    except Exception as e:
        print(f"Error fetching package data: {e}")
        sys.exit(1)


def main() -> None:
    """Main function to fetch and display ESP32 package information"""
    try:
        package: Package = fetch_esp32_packages()
        display_package_info(package)

    except KeyboardInterrupt:
        print("\nInterrupted by user")
        sys.exit(1)


if __name__ == "__main__":
    main()
