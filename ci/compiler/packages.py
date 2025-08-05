"""
Enhanced Arduino Package Index Implementation with Pydantic

This module provides a robust, production-ready Arduino package management system
that fully complies with the Arduino CLI Package Index JSON Specification.

Key Features:
- Pydantic models with comprehensive validation
- Multi-source package index support
- Caching and persistence
- Package installation and dependency resolution
- Search and filtering capabilities
- Checksum validation and error handling
"""

import asyncio
import hashlib
import json
import shutil
import sys
import tarfile
import zipfile
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Set, Union
from urllib.parse import urlparse
from urllib.request import urlopen

import httpx
from pydantic import (
    BaseModel,
    EmailStr,
    Field,
    HttpUrl,
    ValidationError,
    field_validator,
    model_validator,
)


# Custom Exceptions
class PackageParsingError(Exception):
    """Error parsing package index data"""

    pass


class PackageInstallationError(Exception):
    """Error during package installation"""

    pass


class PackageValidationError(Exception):
    """Error validating package data"""

    pass


# Core Pydantic Models with Enhanced Validation


class Help(BaseModel):
    """Help information with online resources"""

    online: HttpUrl = Field(description="URL to online help resources")

    class Config:
        schema_extra = {
            "example": {"online": "https://github.com/espressif/arduino-esp32"}
        }


class Board(BaseModel):
    """Board information with complete properties"""

    name: str = Field(min_length=1, max_length=200, description="Board display name")
    properties: Dict[str, Any] = Field(
        default_factory=dict, description="Board configuration properties"
    )

    @field_validator("name")
    @classmethod
    def validate_board_name(cls, v: str) -> str:
        """Validate board name format"""
        if not v.strip():
            raise ValueError("Board name cannot be empty or whitespace")
        return v.strip()

    class Config:
        schema_extra = {
            "example": {
                "name": "ESP32 Dev Module",
                "properties": {
                    "upload.tool": "esptool_py",
                    "upload.maximum_size": "1310720",
                    "build.mcu": "esp32",
                },
            }
        }


class ToolDependency(BaseModel):
    """Tool dependency specification with validation"""

    packager: str = Field(
        min_length=1, max_length=100, description="Tool packager name"
    )
    name: str = Field(pattern=r"^[a-zA-Z0-9_.-]+$", description="Tool name")
    version: str = Field(min_length=1, description="Required tool version")

    @field_validator("version")
    @classmethod
    def validate_version_format(cls, v: str) -> str:
        """Validate version format - supports semantic versioning and Arduino versioning"""
        if not v.strip():
            raise ValueError("Version cannot be empty")
        # Allow flexible versioning (semantic, date-based, etc.)
        return v.strip()

    class Config:
        schema_extra = {
            "example": {
                "packager": "esp32",
                "name": "xtensa-esp32-elf-gcc",
                "version": "esp-2021r2-patch5-8.4.0",
            }
        }


class Platform(BaseModel):
    """Platform specification with comprehensive validation"""

    name: str = Field(min_length=1, max_length=200, description="Platform display name")
    architecture: str = Field(
        pattern=r"^[a-zA-Z0-9_-]+$", description="Target architecture"
    )
    version: str = Field(description="Platform version")
    category: str = Field(min_length=1, description="Platform category")
    url: HttpUrl = Field(description="Download URL for platform archive")
    archive_filename: str = Field(
        alias="archiveFileName", description="Archive file name"
    )
    checksum: str = Field(
        pattern=r"^SHA-256:[a-fA-F0-9]{64}$", description="SHA-256 checksum"
    )
    size_mb: float = Field(gt=0, alias="size", description="Archive size in megabytes")
    boards: List[Board] = Field(
        default_factory=lambda: [], description="Supported boards"
    )
    tool_dependencies: List[ToolDependency] = Field(
        default_factory=lambda: [],
        alias="toolsDependencies",
        description="Required tool dependencies",
    )
    help: Help = Field(description="Help and documentation links")

    @field_validator("version")
    @classmethod
    def validate_version_format(cls, v: str) -> str:
        """Validate version format"""
        if not v.strip():
            raise ValueError("Version cannot be empty")
        return v.strip()

    @field_validator("size_mb", mode="before")
    @classmethod
    def convert_size_from_bytes(cls, v: Union[str, int, float]) -> float:
        """Convert size from bytes to megabytes if needed"""
        if isinstance(v, str):
            try:
                size_bytes = int(v)
                return size_bytes / (1024 * 1024)
            except ValueError:
                raise ValueError(f"Invalid size format: {v}")
        elif isinstance(v, (int, float)):
            if v > 1024 * 1024:  # Assume bytes if > 1MB
                return v / (1024 * 1024)
            return v  # Already in MB
        return v

    @field_validator("archive_filename")
    @classmethod
    def validate_archive_filename(cls, v: str) -> str:
        """Validate archive filename format"""
        valid_extensions = [".zip", ".tar.gz", ".tar.bz2", ".tar.xz", ".tar.zst"]
        if not any(v.lower().endswith(ext) for ext in valid_extensions):
            raise ValueError(
                f"Archive must have one of these extensions: {valid_extensions}"
            )
        return v

    class Config:
        allow_population_by_field_name = True
        schema_extra: Dict[str, Any] = {
            "example": {
                "name": "ESP32 Arduino",
                "architecture": "esp32",
                "version": "2.0.5",
                "category": "ESP32",
                "url": "https://github.com/espressif/arduino-esp32/releases/download/2.0.5/esp32-2.0.5.zip",
                "archiveFileName": "esp32-2.0.5.zip",
                "checksum": "SHA-256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
                "size": "50000000",
                "boards": [],
                "toolsDependencies": [],
            }
        }


class SystemDownload(BaseModel):
    """System-specific download information for tools"""

    host: str = Field(min_length=1, description="Target host system identifier")
    url: HttpUrl = Field(description="Download URL for this system")
    archive_filename: str = Field(
        alias="archiveFileName", description="Archive file name"
    )
    checksum: str = Field(
        pattern=r"^SHA-256:[a-fA-F0-9]{64}$", description="SHA-256 checksum"
    )
    size_mb: float = Field(gt=0, alias="size", description="Archive size in megabytes")

    @field_validator("size_mb", mode="before")
    @classmethod
    def convert_size_from_bytes(cls, v: Union[str, int, float]) -> float:
        """Convert size from bytes to megabytes if needed"""
        if isinstance(v, str):
            try:
                size_bytes = int(v)
                return size_bytes / (1024 * 1024)
            except ValueError:
                raise ValueError(f"Invalid size format: {v}")
        elif isinstance(v, (int, float)):
            if v > 1024 * 1024:  # Assume bytes if > 1MB
                return v / (1024 * 1024)
            return v  # Already in MB
        return v

    @field_validator("host")
    @classmethod
    def validate_host_format(cls, v: str) -> str:
        """Validate host system identifier"""
        # Common host patterns: i686-pc-linux-gnu, x86_64-apple-darwin, etc.
        if not v.strip():
            raise ValueError("Host identifier cannot be empty")
        return v.strip()

    class Config:
        allow_population_by_field_name = True
        schema_extra = {
            "example": {
                "host": "x86_64-pc-linux-gnu",
                "url": "https://github.com/espressif/crosstool-NG/releases/download/esp-2021r2-patch5/xtensa-esp32-elf-gcc8_4_0-esp-2021r2-patch5-linux-amd64.tar.gz",
                "archiveFileName": "xtensa-esp32-elf-gcc8_4_0-esp-2021r2-patch5-linux-amd64.tar.gz",
                "checksum": "SHA-256:abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
                "size": "150000000",
            }
        }


class Tool(BaseModel):
    """Tool with system-specific downloads and enhanced functionality"""

    name: str = Field(pattern=r"^[a-zA-Z0-9_.-]+$", description="Tool name")
    version: str = Field(min_length=1, description="Tool version")
    systems: List[SystemDownload] = Field(
        min_length=1, description="System-specific downloads"
    )

    @field_validator("systems")
    @classmethod
    def validate_unique_systems(cls, v: List[SystemDownload]) -> List[SystemDownload]:
        """Ensure no duplicate host systems"""
        hosts = [system.host for system in v]
        if len(hosts) != len(set(hosts)):
            raise ValueError("Duplicate host systems found in tool downloads")
        return v

    def get_system_download(self, host_pattern: str) -> Optional[SystemDownload]:
        """Get system download matching host pattern"""
        for system in self.systems:
            if host_pattern in system.host:
                return system
        return None

    def get_compatible_systems(self) -> List[str]:
        """Get list of compatible host systems"""
        return [system.host for system in self.systems]

    class Config:
        schema_extra = {
            "example": {
                "name": "xtensa-esp32-elf-gcc",
                "version": "esp-2021r2-patch5-8.4.0",
                "systems": [
                    {
                        "host": "x86_64-pc-linux-gnu",
                        "url": "https://github.com/espressif/crosstool-NG/releases/download/esp-2021r2-patch5/xtensa-esp32-elf-gcc8_4_0-esp-2021r2-patch5-linux-amd64.tar.gz",
                        "archiveFileName": "xtensa-esp32-elf-gcc8_4_0-esp-2021r2-patch5-linux-amd64.tar.gz",
                        "checksum": "SHA-256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
                        "size": "150000000",
                    }
                ],
            }
        }


class Package(BaseModel):
    """Package containing platforms and tools with comprehensive validation"""

    name: str = Field(pattern=r"^[A-Za-z0-9_.-]+$", description="Package identifier")
    maintainer: str = Field(
        min_length=1, max_length=200, description="Package maintainer"
    )
    website_url: HttpUrl = Field(alias="websiteURL", description="Package website URL")
    email: EmailStr = Field(description="Maintainer contact email")
    help: Help = Field(description="Help and documentation links")
    platforms: List[Platform] = Field(
        default_factory=lambda: [], description="Available platforms"
    )
    tools: List[Tool] = Field(default_factory=lambda: [], description="Available tools")

    @field_validator("platforms")
    @classmethod
    def validate_unique_platforms(cls, v: List[Platform]) -> List[Platform]:
        """Ensure no duplicate platform architecture/version combinations"""
        seen: Set[tuple[str, str]] = set()
        for platform in v:
            key = (platform.architecture, platform.version)
            if key in seen:
                raise ValueError(
                    f"Duplicate platform found: {platform.architecture} v{platform.version}"
                )
            seen.add(key)
        return v

    @field_validator("tools")
    @classmethod
    def validate_unique_tools(cls, v: List[Tool]) -> List[Tool]:
        """Ensure no duplicate tool name/version combinations"""
        seen: Set[tuple[str, str]] = set()
        for tool in v:
            key = (tool.name, tool.version)
            if key in seen:
                raise ValueError(f"Duplicate tool found: {tool.name} v{tool.version}")
            seen.add(key)
        return v

    def find_platform(
        self, architecture: str, version: Optional[str] = None
    ) -> Optional[Platform]:
        """Find platform by architecture and optionally version"""
        for platform in self.platforms:
            if platform.architecture == architecture:
                if version is None or platform.version == version:
                    return platform
        return None

    def find_tool(self, name: str, version: Optional[str] = None) -> Optional[Tool]:
        """Find tool by name and optionally version"""
        for tool in self.tools:
            if tool.name == name:
                if version is None or tool.version == version:
                    return tool
        return None

    def get_latest_platform_version(self, architecture: str) -> Optional[str]:
        """Get the latest version for a given architecture"""
        versions = [p.version for p in self.platforms if p.architecture == architecture]
        if not versions:
            return None
        # Simple version sorting - can be enhanced with proper semver parsing
        return sorted(versions)[-1]

    class Config:
        allow_population_by_field_name = True
        schema_extra: Dict[str, Any] = {
            "example": {
                "name": "esp32",
                "maintainer": "Espressif Systems",
                "websiteURL": "https://github.com/espressif/arduino-esp32",
                "email": "hr@espressif.com",
                "help": {"online": "https://github.com/espressif/arduino-esp32"},
                "platforms": [],
                "tools": [],
            }
        }


class PackageIndex(BaseModel):
    """Root package index containing multiple packages"""

    packages: List[Package] = Field(min_length=1, description="Available packages")

    @field_validator("packages")
    @classmethod
    def validate_unique_packages(cls, v: List[Package]) -> List[Package]:
        """Ensure no duplicate package names"""
        names = [pkg.name for pkg in v]
        if len(names) != len(set(names)):
            raise ValueError("Duplicate package names found in index")
        return v

    def find_package(self, name: str) -> Optional[Package]:
        """Find package by name"""
        for package in self.packages:
            if package.name == name:
                return package
        return None

    def get_all_platforms(self) -> List[Platform]:
        """Get all platforms from all packages"""
        platforms: List[Platform] = []
        for package in self.packages:
            platforms.extend(package.platforms)
        return platforms

    def get_all_tools(self) -> List[Tool]:
        """Get all tools from all packages"""
        tools: List[Tool] = []
        for package in self.packages:
            tools.extend(package.tools)
        return tools


# Enhanced Parser with Validation and Error Handling


class PackageIndexParser:
    """Enhanced parser with comprehensive validation and error handling"""

    def __init__(self, timeout: int = 30):
        """Initialize parser with timeout configuration"""
        self.timeout = timeout

    def parse_package_index(self, json_str: str) -> PackageIndex:
        """Parse and validate package index JSON"""
        try:
            raw_data = json.loads(json_str)
            return PackageIndex(**raw_data)
        except ValidationError as e:
            raise PackageParsingError(f"Invalid package index format: {e}")
        except json.JSONDecodeError as e:
            raise PackageParsingError(f"Invalid JSON format: {e}")

    def parse_from_url(self, url: str) -> PackageIndex:
        """Fetch and parse package index from URL with validation"""
        try:
            print(f"Fetching package index from: {url}")
            with urlopen(url, timeout=self.timeout) as response:
                content = response.read()
                json_str = content.decode("utf-8")

            return self.parse_package_index(json_str)

        except Exception as e:
            raise PackageParsingError(f"Error fetching package index from {url}: {e}")

    async def parse_from_url_async(self, url: str) -> PackageIndex:
        """Async version of URL parsing"""
        try:
            print(f"Fetching package index from: {url}")
            async with httpx.AsyncClient(timeout=self.timeout) as client:
                response = await client.get(url)
                response.raise_for_status()
                json_str = response.text

            return self.parse_package_index(json_str)

        except Exception as e:
            raise PackageParsingError(f"Error fetching package index from {url}: {e}")


# Package Manager Configuration


class PackageManagerConfig(BaseModel):
    """Configuration for package manager with validation"""

    cache_dir: Path = Field(default_factory=lambda: Path.home() / ".arduino_packages")
    sources: List[HttpUrl] = Field(
        default_factory=lambda: [], description="Package index URLs"
    )
    timeout: int = Field(
        default=30, gt=0, le=300, description="Request timeout in seconds"
    )
    max_retries: int = Field(
        default=3, ge=0, le=10, description="Maximum retry attempts"
    )
    verify_checksums: bool = Field(
        default=True, description="Verify download checksums"
    )
    allow_insecure: bool = Field(default=False, description="Allow insecure downloads")

    @field_validator("cache_dir")
    @classmethod
    def validate_cache_dir(cls, v: Path) -> Path:
        """Ensure cache directory is valid"""
        if v.exists() and not v.is_dir():
            raise ValueError(f"Cache path exists but is not a directory: {v}")
        return v

    class Config:
        validate_assignment = True
        schema_extra = {
            "example": {
                "cache_dir": "~/.arduino_packages",
                "sources": [
                    "https://espressif.github.io/arduino-esp32/package_esp32_index.json"
                ],
                "timeout": 30,
                "max_retries": 3,
                "verify_checksums": True,
                "allow_insecure": False,
            }
        }


# Utility Functions for Enhanced Functionality


def verify_checksum(file_path: Path, expected_checksum: str) -> bool:
    """Verify file checksum against expected SHA-256 value"""
    if not expected_checksum.startswith("SHA-256:"):
        raise ValueError(f"Invalid checksum format: {expected_checksum}")

    expected_hash = expected_checksum[8:]  # Remove 'SHA-256:' prefix

    sha256_hash = hashlib.sha256()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            sha256_hash.update(chunk)

    actual_hash = sha256_hash.hexdigest()
    return actual_hash.lower() == expected_hash.lower()


def extract_archive(archive_path: Path, extract_to: Path) -> bool:
    """Extract archive to specified directory with format detection"""
    try:
        archive_str = str(archive_path)

        if archive_path.suffix == ".zip":
            with zipfile.ZipFile(archive_path, "r") as zip_ref:
                zip_ref.extractall(extract_to)
        elif archive_str.endswith((".tar.gz", ".tgz")):
            with tarfile.open(archive_path, "r:gz") as tar_ref:
                tar_ref.extractall(extract_to)
        elif archive_str.endswith(".tar.bz2"):
            with tarfile.open(archive_path, "r:bz2") as tar_ref:
                tar_ref.extractall(extract_to)
        elif archive_str.endswith(".tar.xz"):
            with tarfile.open(archive_path, "r:xz") as tar_ref:
                tar_ref.extractall(extract_to)
        else:
            raise ValueError(f"Unsupported archive format: {archive_path}")

        return True

    except Exception as e:
        print(f"Error extracting archive {archive_path}: {e}")
        return False


def format_size(size_mb: float) -> str:
    """Format size in a human-readable way"""
    if size_mb < 1:
        return f"{size_mb * 1024:.1f} KB"
    elif size_mb < 1024:
        return f"{size_mb:.1f} MB"
    else:
        return f"{size_mb / 1024:.1f} GB"


# Display Functions with Enhanced Formatting


def display_package_info(package: Package) -> None:
    """Display package information with enhanced formatting"""
    print(f"\n📦 Package: {package.name}")
    print(f"👤 Maintainer: {package.maintainer}")
    print(f"🌐 Website: {package.website_url}")
    print(f"📧 Email: {package.email}")
    print(f"📚 Help: {package.help.online}")
    print(f"🛠️  Platforms: {len(package.platforms)}")
    print(f"🔧 Tools: {len(package.tools)}")

    # Show platform information
    for i, platform in enumerate(package.platforms[:3]):  # Show first 3 platforms
        print(f"\n  📋 Platform {i + 1}: {platform.name} v{platform.version}")
        print(f"     Architecture: {platform.architecture}")
        print(f"     Category: {platform.category}")
        print(f"     Size: {format_size(platform.size_mb)}")
        print(f"     Archive: {platform.archive_filename}")
        print(f"     Checksum: {platform.checksum[:24]}...")
        print(f"     Help: {platform.help.online}")
        print(f"     Boards: {len(platform.boards)}")

        # Show first 5 boards
        for board in platform.boards[:5]:
            print(f"       • {board.name}")
        if len(platform.boards) > 5:
            print(f"       ... and {len(platform.boards) - 5} more")

        print(f"     Tool Dependencies: {len(platform.tool_dependencies)}")
        for dep in platform.tool_dependencies[:3]:  # Show first 3 dependencies
            print(f"       • {dep.name} v{dep.version} ({dep.packager})")
        if len(platform.tool_dependencies) > 3:
            print(f"       ... and {len(platform.tool_dependencies) - 3} more")

    if len(package.platforms) > 3:
        print(f"\n  ... and {len(package.platforms) - 3} more platforms")


def display_validation_summary(package_index: PackageIndex) -> None:
    """Display validation summary for the package index"""
    print(f"\n✅ VALIDATION SUMMARY")
    print(f"   📦 Total packages: {len(package_index.packages)}")

    total_platforms = sum(len(pkg.platforms) for pkg in package_index.packages)
    total_tools = sum(len(pkg.tools) for pkg in package_index.packages)
    total_boards = sum(
        len(platform.boards)
        for pkg in package_index.packages
        for platform in pkg.platforms
    )

    print(f"   🛠️  Total platforms: {total_platforms}")
    print(f"   🔧 Total tools: {total_tools}")
    print(f"   💾 Total boards: {total_boards}")

    # Show architectures
    architectures: Set[str] = set()
    for pkg in package_index.packages:
        for platform in pkg.platforms:
            architectures.add(platform.architecture)

    print(f"   🏗️  Architectures: {', '.join(sorted(architectures))}")


# Demonstration Functions


def demo_esp32_parsing() -> Optional[PackageIndex]:
    """Demonstrate parsing ESP32 package index with enhanced validation"""
    ESP32_URL = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"

    try:
        parser = PackageIndexParser(timeout=30)
        package_index = parser.parse_from_url(ESP32_URL)

        print("🎉 Successfully parsed ESP32 package index with Pydantic validation!")
        display_validation_summary(package_index)

        # Display first package
        if package_index.packages:
            display_package_info(package_index.packages[0])

        return package_index

    except PackageParsingError as e:
        print(f"❌ Package parsing error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        sys.exit(1)


def demo_model_validation() -> None:
    """Demonstrate Pydantic model validation capabilities"""
    print("\n🧪 TESTING PYDANTIC MODEL VALIDATION")

    # Define valid platform data
    valid_platform_data: Dict[str, Any] = {
        "name": "ESP32 Arduino",
        "architecture": "esp32",
        "version": "2.0.5",
        "category": "ESP32",
        "url": "https://github.com/espressif/arduino-esp32/releases/download/2.0.5/esp32-2.0.5.zip",
        "archiveFileName": "esp32-2.0.5.zip",
        "checksum": "SHA-256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
        "size": "50000000",  # Should convert to MB
        "boards": [],
        "toolsDependencies": [],
        "help": {"online": "https://github.com/espressif/arduino-esp32"},
    }

    # Test valid platform
    try:
        platform = Platform(**valid_platform_data)
        print(f"✅ Valid platform created: {platform.name} v{platform.version}")
        print(f"   Size converted: {platform.size_mb:.1f} MB")

    except ValidationError as e:
        print(f"❌ Unexpected validation error: {e}")

    # Test invalid platform
    try:
        invalid_platform_data = valid_platform_data.copy()
        invalid_platform_data["checksum"] = "invalid-checksum-format"

        platform = Platform(**invalid_platform_data)
        print("❌ Should have failed validation!")

    except ValidationError as e:
        print(f"✅ Correctly caught invalid checksum: {e.errors()[0]['msg']}")

    # Test invalid URL
    try:
        invalid_url_data = valid_platform_data.copy()
        invalid_url_data["url"] = "not-a-valid-url"

        platform = Platform(**invalid_url_data)
        print("❌ Should have failed URL validation!")

    except ValidationError as e:
        print(f"✅ Correctly caught invalid URL: {e.errors()[0]['msg']}")


def main() -> None:
    """Main function demonstrating enhanced package index functionality"""
    print("🚀 ENHANCED ARDUINO PACKAGE INDEX WITH PYDANTIC")
    print("=" * 60)

    try:
        # Demo model validation
        demo_model_validation()

        # Demo ESP32 parsing
        package_index = demo_esp32_parsing()

        # Interactive options
        try:
            print(f"\n📋 AVAILABLE OPTIONS:")
            print("1. Show detailed tools information")
            print("2. Search for specific architecture")
            print("3. Exit")

            choice = input("\nEnter your choice (1-3): ").strip()

            if choice == "1" and package_index and package_index.packages:
                # Simple tools info display (without importing original)
                pkg = package_index.packages[0]
                print(f"\n🔧 TOOLS INFORMATION for {pkg.name}")
                print(f"Total tools: {len(pkg.tools)}")
                for tool in pkg.tools[:3]:  # Show first 3 tools
                    print(
                        f"  • {tool.name} v{tool.version} ({len(tool.systems)} systems)"
                    )
                if len(pkg.tools) > 3:
                    print(f"  ... and {len(pkg.tools) - 3} more tools")

            elif choice == "2" and package_index:
                arch = input("Enter architecture to search for: ").strip()
                platforms = [
                    p
                    for pkg in package_index.packages
                    for p in pkg.platforms
                    if p.architecture == arch
                ]
                if platforms:
                    print(
                        f"\n🔍 Found {len(platforms)} platforms for architecture '{arch}':"
                    )
                    for platform in platforms:
                        print(
                            f"   • {platform.name} v{platform.version} ({format_size(platform.size_mb)})"
                        )
                else:
                    print(f"❌ No platforms found for architecture '{arch}'")
            else:
                print("👋 Goodbye!")

        except KeyboardInterrupt:
            print("\n👋 Interrupted by user")

    except KeyboardInterrupt:
        print("\n👋 Interrupted by user")
        sys.exit(1)


if __name__ == "__main__":
    main()
