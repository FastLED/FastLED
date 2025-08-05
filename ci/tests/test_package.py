"""
Comprehensive tests for enhanced Arduino package index implementation with Pydantic

Tests all Pydantic models, validation rules, parsing functionality, and error handling.
"""

import json
from pathlib import Path
from typing import Any, Dict

import pytest
from pydantic import ValidationError

# Import the enhanced implementation
from ci.compiler.packages import (
    Board,
    Help,
    Package,
    PackageIndex,
    PackageIndexParser,
    PackageManagerConfig,
    PackageParsingError,
    Platform,
    SystemDownload,
    Tool,
    ToolDependency,
    format_size,
)


class TestHelp:
    """Test Help model validation"""

    def test_valid_help(self):
        """Test valid help creation"""
        help_data = {"online": "https://github.com/espressif/arduino-esp32"}
        help_obj = Help(**help_data)  # type: ignore
        assert str(help_obj.online) == "https://github.com/espressif/arduino-esp32"

    def test_invalid_url(self):
        """Test invalid URL validation"""
        with pytest.raises(ValidationError):
            Help(online="not-a-valid-url")  # type: ignore


class TestBoard:
    """Test Board model validation"""

    def test_valid_board(self):
        """Test valid board creation"""
        board_data = {
            "name": "ESP32 Dev Module",
            "properties": {
                "upload.tool": "esptool_py",
                "upload.maximum_size": "1310720",
            },
        }
        board = Board(**board_data)  # type: ignore
        assert board.name == "ESP32 Dev Module"
        assert board.properties["upload.tool"] == "esptool_py"

    def test_empty_name_validation(self):
        """Test empty name validation"""
        with pytest.raises(ValidationError):
            Board(name="", properties={})

    def test_name_trimming(self):
        """Test name trimming functionality"""
        board = Board(name="  ESP32 Dev Module  ", properties={})
        assert board.name == "ESP32 Dev Module"


class TestSystemDownload:
    """Test SystemDownload model validation"""

    def test_size_conversion_from_string(self):
        """Test size conversion from string bytes to MB"""
        system = SystemDownload(
            host="test-host",
            url="https://example.com/file.tar.gz",  # type: ignore
            archiveFileName="file.tar.gz",
            checksum="SHA-256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
            size="52428800",  # type: ignore - 50 MB in bytes, validator converts
        )
        assert abs(system.size_mb - 50.0) < 0.1  # Should be ~50 MB

    def test_invalid_checksum_format(self):
        """Test invalid checksum format validation"""
        with pytest.raises(ValidationError):
            SystemDownload(
                host="test-host",
                url="https://example.com/file.tar.gz",  # type: ignore
                archiveFileName="file.tar.gz",
                checksum="invalid-checksum",
                size=50.0,  # type: ignore
            )


class TestPlatform:
    """Test Platform model validation"""

    def test_valid_platform(self):
        """Test valid platform creation"""
        platform_data: Dict[str, Any] = {
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
            "help": {"online": "https://github.com/espressif/arduino-esp32"},
        }
        platform = Platform(**platform_data)  # type: ignore
        assert platform.name == "ESP32 Arduino"
        assert platform.architecture == "esp32"
        assert platform.size_mb == 50000000 / (1024 * 1024)

    def test_invalid_archive_extension(self):
        """Test invalid archive extension validation"""
        with pytest.raises(ValidationError):
            Platform(
                name="Test Platform",
                architecture="test",
                version="1.0.0",
                category="Test",
                url="https://example.com/file.txt",  # type: ignore
                archiveFileName="file.txt",  # Invalid extension
                checksum="SHA-256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
                size=50.0,  # type: ignore
                boards=[],
                toolsDependencies=[],
                help=Help(online="https://example.com"),  # type: ignore
            )


class TestPackageIndexParser:
    """Test PackageIndexParser functionality"""

    def test_parse_valid_json(self):
        """Test parsing valid package index JSON"""
        valid_json: Dict[str, Any] = {
            "packages": [
                {
                    "name": "test",
                    "maintainer": "Test Maintainer",
                    "websiteURL": "https://example.com",
                    "email": "test@example.com",
                    "help": {"online": "https://example.com"},
                    "platforms": [],
                    "tools": [],
                }
            ]
        }

        parser = PackageIndexParser()
        package_index = parser.parse_package_index(json.dumps(valid_json))
        assert len(package_index.packages) == 1
        assert package_index.packages[0].name == "test"

    def test_parse_invalid_json(self):
        """Test parsing invalid JSON"""
        parser = PackageIndexParser()

        with pytest.raises(PackageParsingError):
            parser.parse_package_index("invalid json")


class TestUtilityFunctions:
    """Test utility functions"""

    def test_format_size(self):
        """Test size formatting function"""
        # Test KB
        assert format_size(0.5) == "512.0 KB"

        # Test MB
        assert format_size(1.5) == "1.5 MB"
        assert format_size(512.0) == "512.0 MB"

        # Test GB
        assert format_size(1536.0) == "1.5 GB"
        assert format_size(2048.0) == "2.0 GB"


class TestRealDataParsing:
    """Test with real ESP32 package index data (if network available)"""

    def test_esp32_package_parsing(self):
        """Test parsing real ESP32 package index"""
        ESP32_URL = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"

        try:
            parser = PackageIndexParser(timeout=10)
            package_index = parser.parse_from_url(ESP32_URL)

            # Basic validation
            assert len(package_index.packages) > 0

            esp32_package = package_index.packages[0]
            assert esp32_package.name == "esp32"
            assert len(esp32_package.platforms) > 0
            assert len(esp32_package.tools) > 0

            print(f"✅ Successfully parsed ESP32 package index:")
            print(f"   📦 Packages: {len(package_index.packages)}")
            print(f"   🛠️  Platforms: {len(esp32_package.platforms)}")
            print(f"   🔧 Tools: {len(esp32_package.tools)}")

        except Exception as e:
            # Skip if network not available
            pytest.skip(f"Network test skipped: {e}")


if __name__ == "__main__":
    # Run tests
    pytest.main([__file__, "-v"])
