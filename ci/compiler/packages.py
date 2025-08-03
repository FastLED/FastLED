import json
import sys
from dataclasses import dataclass
from typing import Any, Dict, List, Optional
from urllib.request import urlopen


URL: str = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"

@dataclass
class Help:
    """Help information with online resources"""

    online: str


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
    category: str
    url: str
    archive_filename: str
    checksum: str
    size_mb: float
    boards: List[Board]
    tool_dependencies: List[ToolDependency]
    help: Help


@dataclass
class SystemDownload:
    """System-specific download information for tools"""

    host: str
    url: str
    archive_filename: str
    checksum: str
    size_mb: float


@dataclass
class Tool:
    """Tool with system-specific downloads"""

    name: str
    version: str
    systems: List[SystemDownload]


@dataclass
class Package:
    """Package containing platforms and tools"""

    name: str
    maintainer: str
    website_url: str
    email: str
    help: Help
    platforms: List[Platform]
    tools: List[Tool]


def parse_help(help_data: Dict[str, Any]) -> Help:
    """Parse help data into Help object"""
    return Help(online=help_data["online"])


def parse_board(board_data: Dict[str, Any]) -> Board:
    """Parse board data into Board object"""
    name: str = board_data["name"]

    # Flatten all properties into a single dict
    properties: Dict[str, Any] = {}
    for key, value in board_data.items():
        if key != "name":
            properties[key] = value

    return Board(name=name, properties=properties)


def parse_system_download(system_data: Dict[str, Any]) -> SystemDownload:
    """Parse system download data into SystemDownload object"""
    size_bytes: int = int(system_data["size"])
    size_mb: float = size_bytes / (1024 * 1024)
    
    return SystemDownload(
        host=system_data["host"],
        url=system_data["url"],
        archive_filename=system_data["archiveFileName"],
        checksum=system_data["checksum"],
        size_mb=size_mb
    )


def parse_tool(tool_data: Dict[str, Any]) -> Tool:
    """Parse tool data into Tool object"""
    systems: List[SystemDownload] = []
    for system_data in tool_data["systems"]:
        systems.append(parse_system_download(system_data))
    
    return Tool(
        name=tool_data["name"],
        version=tool_data["version"],
        systems=systems
    )


def parse_package_index(json_str: str) -> Package:
    """Parse JSON string into Package structure"""
    data: Dict[str, Any] = json.loads(json_str)

    # Get first package (ESP32)
    pkg_data: Dict[str, Any] = data["packages"][0]

    # Parse platforms
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

        # Parse platform help
        platform_help: Help = parse_help(platform_data["help"])

        platforms.append(
            Platform(
                name=platform_data["name"],
                version=platform_data["version"],
                architecture=platform_data["architecture"],
                category=platform_data["category"],
                url=platform_data["url"],
                archive_filename=platform_data["archiveFileName"],
                checksum=platform_data["checksum"],
                size_mb=size_mb,
                boards=boards,
                tool_dependencies=tool_deps,
                help=platform_help,
            )
        )

    # Parse tools
    tools: List[Tool] = []
    for tool_data in pkg_data["tools"]:
        tools.append(parse_tool(tool_data))

    # Parse package help
    package_help: Help = parse_help(pkg_data["help"])

    return Package(
        name=pkg_data["name"],
        maintainer=pkg_data["maintainer"],
        website_url=pkg_data["websiteURL"],
        email=pkg_data["email"],
        help=package_help,
        platforms=platforms,
        tools=tools,
    )


def display_package_info(package: Package) -> None:
    """Display package information in a simple format"""
    print(f"\nPackage: {package.name}")
    print(f"Maintainer: {package.maintainer}")
    print(f"Website: {package.website_url}")
    print(f"Email: {package.email}")
    print(f"Help: {package.help.online}")
    print(f"Platforms: {len(package.platforms)}")
    print(f"Tools: {len(package.tools)}")

    # Show platform information
    for platform in package.platforms[:3]:  # Show first 3 platforms
        print(f"\n  Platform: {platform.name} v{platform.version}")
        print(f"  Architecture: {platform.architecture}")
        print(f"  Category: {platform.category}")
        print(f"  Size: {platform.size_mb:.1f} MB")
        print(f"  URL: {platform.url}")
        print(f"  Archive: {platform.archive_filename}")
        print(f"  Checksum: {platform.checksum[:16]}...")
        print(f"  Help: {platform.help.online}")
        print(f"  Boards: {len(platform.boards)}")

        # Show first 5 boards
        for board in platform.boards[:5]:
            print(f"    - {board.name}")
        if len(platform.boards) > 5:
            print(f"    ... and {len(platform.boards) - 5} more")

        print(f"  Tool Dependencies: {len(platform.tool_dependencies)}")
        for dep in platform.tool_dependencies[:3]:  # Show first 3 dependencies
            print(f"    - {dep.name} v{dep.version} ({dep.packager})")
        if len(platform.tool_dependencies) > 3:
            print(f"    ... and {len(platform.tool_dependencies) - 3} more")

    if len(package.platforms) > 3:
        print(f"\n  ... and {len(package.platforms) - 3} more platforms")


def display_tools_info(package: Package) -> None:
    """Display detailed tools information"""
    print(f"\n=== TOOLS INFORMATION ===")
    print(f"Total tools: {len(package.tools)}")

    for tool in package.tools[:5]:  # Show first 5 tools
        print(f"\n  Tool: {tool.name} v{tool.version}")
        print(f"  Systems: {len(tool.systems)}")
        
        # Show first 3 systems
        for system in tool.systems[:3]:
            print(f"    - {system.host}: {system.size_mb:.1f} MB")
            print(f"      URL: {system.url}")
            print(f"      Archive: {system.archive_filename}")
            print(f"      Checksum: {system.checksum[:16]}...")
        
        if len(tool.systems) > 3:
            print(f"    ... and {len(tool.systems) - 3} more systems")
    
    if len(package.tools) > 5:
        print(f"\n  ... and {len(package.tools) - 5} more tools")


def display_full_package_info(package: Package) -> None:
    """Display complete package information including tools"""
    display_package_info(package)
    display_tools_info(package)


def fetch_esp32_packages() -> Package:
    """Fetch and parse ESP32 package index"""

    try:
        print("Fetching ESP32 package index...")
        with urlopen(URL) as response:
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
        
        # Show basic package info
        display_package_info(package)
        
        # Ask if user wants to see detailed tools information
        try:
            answer: str = input("\nShow detailed tools information? (y/N): ").strip().lower()
            if answer in ['y', 'yes']:
                display_tools_info(package)
        except KeyboardInterrupt:
            pass  # User pressed Ctrl+C during input

    except KeyboardInterrupt:
        print("\nInterrupted by user")
        sys.exit(1)


if __name__ == "__main__":
    main()
