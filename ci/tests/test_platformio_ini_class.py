#!/usr/bin/env python3
"""
Test the PlatformIOIni class functionality.
"""

import tempfile
import unittest
from pathlib import Path

from ci.compiler.platformio_ini import PlatformIOIni


# Test data constants
BASIC_INI_CONTENT = """[platformio]
src_dir = src

[env:esp32dev]
board = esp32dev
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.30-2/platform-espressif32.zip
framework = arduino

[env:uno]
board = uno
platform = atmelavr
framework = arduino
"""

INVALID_INI_CONTENT = """[platformio]
src_dir = src

[env:missing_board]
platform = atmelavr
framework = arduino

[env:missing_platform]
board = uno
framework = arduino
"""

DICT_TEST_INI_CONTENT = """[platformio]
src_dir = src

[env:test]
board = esp32dev
platform = espressif32
framework = arduino
"""

STRING_TEST_INI_CONTENT = """[platformio]
src_dir = src

[env:test]
board = esp32dev
platform = espressif32
"""

PARSE_TEST_INI_CONTENT = """[platformio]
src_dir = src

[env:test]
board = esp32dev
platform = espressif32
framework = arduino
"""

ESP32_PLATFORM_URL = "https://github.com/pioarduino/platform-espressif32/releases/download/55.03.30-2/platform-espressif32.zip"
NEW_CACHED_URL = "file:///path/to/cached/platform"


class TestPlatformIOIni(unittest.TestCase):
    """Test the PlatformIOIni class."""

    def setUp(self):
        """Set up test environment."""
        self.temp_dir = Path(tempfile.mkdtemp())
        self.test_ini = self.temp_dir / "platformio.ini"

    def tearDown(self):
        """Clean up test environment."""
        import shutil

        if self.temp_dir.exists():
            shutil.rmtree(self.temp_dir)

    def test_parse_and_dump(self):
        """Test basic parse and dump functionality."""
        # Create a test platformio.ini file
        self.test_ini.write_text(BASIC_INI_CONTENT)

        # Parse the file
        pio_ini = PlatformIOIni.parseFile(self.test_ini)

        # Test basic functionality
        self.assertIn("env:esp32dev", pio_ini.get_sections())
        self.assertIn("env:uno", pio_ini.get_sections())
        self.assertEqual(len(pio_ini.get_env_sections()), 2)

        # Test option access
        self.assertEqual(pio_ini.get_option("env:esp32dev", "board"), "esp32dev")
        self.assertEqual(pio_ini.get_option("env:uno", "platform"), "atmelavr")

        # Test URL extraction
        platform_urls = pio_ini.get_platform_urls()
        self.assertEqual(len(platform_urls), 2)

        # Find the ESP32 platform URL
        esp32_url = None
        for section, option, url in platform_urls:
            if section == "env:esp32dev":
                esp32_url = url
                break

        self.assertEqual(esp32_url, ESP32_PLATFORM_URL)

        # Test URL replacement
        self.assertIsNotNone(esp32_url)  # Ensure esp32_url is not None
        result = pio_ini.replace_url(
            "env:esp32dev", "platform", esp32_url or "", NEW_CACHED_URL
        )
        self.assertTrue(result)
        self.assertEqual(pio_ini.get_option("env:esp32dev", "platform"), NEW_CACHED_URL)

        # Test dump
        output_file = self.temp_dir / "output.ini"
        pio_ini.dump(output_file)

        # Verify the file was written
        self.assertTrue(output_file.exists())

        # Parse the output and verify the change
        pio_ini2 = PlatformIOIni.parseFile(output_file)
        self.assertEqual(
            pio_ini2.get_option("env:esp32dev", "platform"), NEW_CACHED_URL
        )
        self.assertEqual(pio_ini2.get_option("env:uno", "platform"), "atmelavr")

    def test_validation(self):
        """Test validation functionality."""
        # Create an invalid platformio.ini
        self.test_ini.write_text(INVALID_INI_CONTENT)
        pio_ini = PlatformIOIni.parseFile(self.test_ini)

        issues = pio_ini.validate_structure()
        self.assertEqual(len(issues), 2)
        self.assertIn("missing 'board' option", issues[0])
        self.assertIn("missing required 'platform' option", issues[1])

    def test_dict_conversion(self):
        """Test to_dict and from_dict functionality."""
        self.test_ini.write_text(DICT_TEST_INI_CONTENT)
        pio_ini = PlatformIOIni.parseFile(self.test_ini)

        # Convert to dict
        config_dict = pio_ini.to_dict()
        self.assertIn("env:test", config_dict)
        self.assertEqual(config_dict["env:test"]["board"], "esp32dev")

        # Create new instance from dict
        pio_ini2 = PlatformIOIni.create()
        pio_ini2.from_dict(config_dict)

        self.assertEqual(pio_ini2.get_option("env:test", "board"), "esp32dev")
        self.assertEqual(pio_ini2.get_option("env:test", "platform"), "espressif32")

    def test_string_representation(self):
        """Test string representation."""
        self.test_ini.write_text(STRING_TEST_INI_CONTENT)
        pio_ini = PlatformIOIni.parseFile(self.test_ini)

        str_repr = str(pio_ini)
        self.assertIn("[platformio]", str_repr)
        self.assertIn("src_dir = src", str_repr)
        self.assertIn("[env:test]", str_repr)

    def test_static_parse_method(self):
        """Test static parse factory method."""
        self.test_ini.write_text(PARSE_TEST_INI_CONTENT)

        # Test static parse method
        pio_ini = PlatformIOIni.parseFile(self.test_ini)

        # Verify it worked correctly
        self.assertEqual(pio_ini.get_option("env:test", "board"), "esp32dev")
        self.assertEqual(pio_ini.get_option("env:test", "platform"), "espressif32")
        self.assertEqual(pio_ini.get_option("platformio", "src_dir"), "src")

        # Verify file_path is set correctly
        self.assertEqual(pio_ini.file_path, self.test_ini)

    def test_parse_string_method(self):
        """Test static parseString factory method."""
        # Test static parseString method
        pio_ini = PlatformIOIni.parseString(PARSE_TEST_INI_CONTENT)

        # Verify it worked correctly
        self.assertEqual(pio_ini.get_option("env:test", "board"), "esp32dev")
        self.assertEqual(pio_ini.get_option("env:test", "platform"), "espressif32")
        self.assertEqual(pio_ini.get_option("platformio", "src_dir"), "src")

        # Verify file_path is None since parsed from string
        self.assertIsNone(pio_ini.file_path)

    def test_optimize_method(self):
        """Test the optimize() method that downloads and caches packages."""
        # Create content with a real zip URL
        test_content = """[platformio]
src_dir = src

[env:esp32dev]
board = esp32dev
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.30-2/platform-espressif32.zip
framework = arduino

[env:esp32c3]
board = esp32-c3-devkitm-1
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.30-2/platform-espressif32.zip
framework = arduino
"""

        # Parse from string
        pio_ini = PlatformIOIni.parseString(test_content)

        # Verify original URL is present
        original_str = str(pio_ini)
        self.assertIn("https://github.com/pioarduino", original_str)

        # Run optimize with test cache instance
        from pathlib import Path

        from ci.compiler.platformio_cache import PlatformIOCache

        cache_dir = Path(".cache/tests/test_platformio_ini_optimize")
        cache = PlatformIOCache(cache_dir)
        pio_ini.optimize(cache)

        # Verify URLs were replaced
        optimized_str = str(pio_ini)
        self.assertNotIn("https://github.com/pioarduino", optimized_str)
        # On Windows, paths are returned directly, not as file:// URLs
        import platform

        if platform.system() == "Windows":
            self.assertIn(
                "extracted", optimized_str
            )  # Check for the extracted directory
        else:
            self.assertIn("file:///", optimized_str)

        # Verify cache directory was created
        self.assertTrue(cache_dir.exists())

        # Verify both environments have the same local path
        esp32dev_platform = pio_ini.get_option("env:esp32dev", "platform")
        esp32c3_platform = pio_ini.get_option("env:esp32c3", "platform")
        self.assertEqual(esp32dev_platform, esp32c3_platform)
        # On Windows, paths are returned directly, not as file:// URLs
        import platform

        if platform.system() == "Windows":
            self.assertTrue(esp32dev_platform.endswith("extracted"))
        else:
            self.assertTrue(esp32dev_platform.startswith("file:///"))


if __name__ == "__main__":
    unittest.main()
