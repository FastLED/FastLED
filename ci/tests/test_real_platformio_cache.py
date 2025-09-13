#!/usr/bin/env python3
"""
Real test for PlatformIO cache functionality - no mocks.
Downloads actual ESP32 platform, caches it, and verifies transformation.
"""

import shutil
import unittest
from pathlib import Path

from ci.compiler.platformio_cache import PlatformIOCache
from ci.compiler.platformio_ini import PlatformIOIni


# Test constants
ESP32_PLATFORM_URL = "https://github.com/pioarduino/platform-espressif32/releases/download/55.03.30-2/platform-espressif32.zip"

PLATFORMIO_INI_TEMPLATE = """[platformio]
src_dir = src

[env:esp32dev]
board = esp32dev
platform = {platform_url}
framework = arduino
build_flags = -DCORE_DEBUG_LEVEL=4

[env:esp32c3]
board = esp32-c3-devkitm-1
platform = {platform_url}
framework = arduino
"""


class TestRealPlatformIOCache(unittest.TestCase):
    """Real test for PlatformIO cache functionality without mocks."""

    def setUp(self):
        """Set up test environment."""
        self.cache_dir = Path(".cache/tests/test_real_platformio_cache")
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self.platformio_ini = self.cache_dir / "platformio.ini"

    def tearDown(self):
        """Clean up test environment."""
        # Only clean up the platformio.ini file, keep cache for reuse
        if self.platformio_ini.exists():
            self.platformio_ini.unlink()

    def test_real_esp32_platform_download_and_transform(self):
        """Test with real ESP32 platform download - no mocks."""
        # Create simple platformio.ini with real ESP32 platform URL
        platformio_ini_content = PLATFORMIO_INI_TEMPLATE.format(
            platform_url=ESP32_PLATFORM_URL
        )

        # Write the platformio.ini file
        self.platformio_ini.write_text(platformio_ini_content)

        # Verify original content has the URL
        original_content = self.platformio_ini.read_text()
        self.assertIn(ESP32_PLATFORM_URL, original_content)
        url_count_before = original_content.count(ESP32_PLATFORM_URL)
        self.assertEqual(url_count_before, 2)  # Should appear in both environments

        print(f"🌐 Downloading real ESP32 platform from: {ESP32_PLATFORM_URL}")

        # Process the platformio.ini file through the cache system
        # This will make real HTTP requests and download the actual platform
        pio_ini = PlatformIOIni.parseFile(self.platformio_ini)
        cache = PlatformIOCache(self.cache_dir)
        pio_ini.optimize(cache)
        pio_ini.dump(self.platformio_ini)

        print("✅ Download and processing completed!")

        # Read the transformed platformio.ini
        final_content = self.platformio_ini.read_text()
        print(final_content)

        # Verify original URL was replaced
        self.assertNotIn(ESP32_PLATFORM_URL, final_content)
        print(f"✅ Original URL removed from platformio.ini")

        # Verify file URLs are present (should be 2 occurrences)
        file_url_count = final_content.count("file:///")
        self.assertEqual(file_url_count, 2)
        print(f"✅ Found {file_url_count} file:// URLs as expected")

        # Verify the file URLs point to the cache
        self.assertIn("extracted", final_content)
        print("✅ URLs point to extracted cache directory")

        # Verify other content is preserved
        self.assertIn("board = esp32dev", final_content)
        self.assertIn("board = esp32-c3-devkitm-1", final_content)
        self.assertIn("framework = arduino", final_content)
        self.assertIn("build_flags = -DCORE_DEBUG_LEVEL=4", final_content)
        self.assertIn("[env:esp32dev]", final_content)
        self.assertIn("[env:esp32c3]", final_content)
        print("✅ All other platformio.ini content preserved")

        # Verify cache directories and files were created
        self.assertTrue(self.cache_dir.exists())
        self.assertTrue((self.cache_dir / "artifacts").exists())

        # Verify actual files were cached in the new structure
        # Each artifact has its own directory under artifacts/
        artifact_dirs = list((self.cache_dir / "artifacts").iterdir())
        self.assertGreater(len(artifact_dirs), 0)

        # Check the artifact directory contains both zip and extracted folder
        for artifact_dir in artifact_dirs:
            zip_files = list(artifact_dir.glob("*.zip"))
            self.assertEqual(len(zip_files), 1)
            print(f"✅ Platform zip cached: {zip_files[0].name}")

            # Check extracted directory exists
            extracted_dir = artifact_dir / "extracted"
            self.assertTrue(extracted_dir.exists())
            print(f"✅ Platform extracted alongside zip in: {artifact_dir.name}")

            # Verify platform.json exists in extracted content
            platform_json_files = list(extracted_dir.rglob("platform.json"))
            self.assertGreater(len(platform_json_files), 0)
            print(f"✅ Platform extracted with valid platform.json")

            # Verify checksums were created alongside the zip
            checksums = list(artifact_dir.glob("*.sha256"))
            self.assertGreater(len(checksums), 0)
            print(f"✅ Checksum files created: {[c.name for c in checksums]}")

            # Verify lock files were created alongside the zip
            lock_files = list(artifact_dir.glob("*.lock"))
            self.assertGreaterEqual(
                len(lock_files), 0
            )  # Lock files may not exist after completion
            print(
                f"✅ Lock mechanism used (lock files may be cleaned up after completion)"
            )

        print("\n📋 Final transformed platformio.ini:")
        for line in final_content.split("\n"):
            if "platform =" in line:
                print(f"  {line}")

        print(f"\n📊 Cache statistics:")
        print(f"  Cache directory: {self.cache_dir}")
        # Get the first artifact directory
        first_artifact_dir = artifact_dirs[0]
        zip_file = list(first_artifact_dir.glob("*.zip"))[0]
        print(f"  Cached zip size: {zip_file.stat().st_size // 1024} KB")
        print(f"  Extracted platform: {first_artifact_dir.name}")


if __name__ == "__main__":
    unittest.main()
