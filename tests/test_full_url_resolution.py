#!/usr/bin/env python3
"""
Test full URL resolution from version requirements.

This test validates that we can resolve PlatformIO package version requirements
(like "14.2.0+20241119") to actual download URLs via the PlatformIO Registry API.
"""

import unittest
from pathlib import Path

from ci.compiler.platformio_ini import PlatformIOIni


class TestFullUrlResolution(unittest.TestCase):
    """Test resolution of version requirements to full download URLs."""

    def test_resolve_toolchain_version_to_url(self) -> None:
        """
        Test that toolchain version requirements can be resolved to download URLs.

        This validates the full pipeline:
        1. Parse platformio.ini
        2. Extract platform packages
        3. Resolve version requirements to actual download URLs
        """
        # Load the main platformio.ini
        pio_ini_path = Path("platformio.ini")
        if not pio_ini_path.exists():
            self.skipTest("platformio.ini not found in project root")

        pio_ini = PlatformIOIni.parseFile(pio_ini_path)

        # Get the esp32c3 environment
        esp32c3_env = pio_ini.get_environment("esp32c3")
        self.assertIsNotNone(esp32c3_env, "esp32c3 environment not found")

        # Resolve platform to get toolchains
        platform_resolution = pio_ini.resolve_platform_url_enhanced(
            esp32c3_env.platform, force_resolve=True
        )
        self.assertIsNotNone(platform_resolution, "Failed to resolve platform")

        # Find RISC-V toolchain
        toolchains = [pkg for pkg in platform_resolution.packages if pkg.type == "toolchain"]
        riscv_toolchain = None
        for tc in toolchains:
            if "riscv" in tc.name.lower():
                riscv_toolchain = tc
                break

        self.assertIsNotNone(riscv_toolchain, "RISC-V toolchain not found")

        # Now resolve the version requirement to a full download URL
        download_url = riscv_toolchain.get_download_url()

        # Verify we got a valid download URL
        self.assertIsNotNone(download_url, "Failed to resolve download URL")
        self.assertTrue(
            download_url.startswith("https://"),
            f"Download URL should be HTTPS: {download_url}"
        )
        self.assertIn(
            "dl.registry.platformio.org",
            download_url,
            f"Download URL should be from PlatformIO registry: {download_url}"
        )
        self.assertIn(
            riscv_toolchain.name,
            download_url,
            f"Download URL should contain package name: {download_url}"
        )

        # Check that it's an archive file
        self.assertTrue(
            any(ext in download_url for ext in [".tar.gz", ".zip", ".tar.xz"]),
            f"Download URL should point to an archive: {download_url}"
        )

        print(f"\n✓ Resolved Toolchain Download URL:")
        print(f"  Package: {riscv_toolchain.name}")
        print(f"  Version: {riscv_toolchain.requirements}")
        print(f"  URL: {download_url}")

    def test_resolve_multiple_toolchains(self) -> None:
        """
        Test resolving download URLs for multiple toolchains.

        Validates that we can get download URLs for all toolchains in a platform.
        """
        pio_ini_path = Path("platformio.ini")
        if not pio_ini_path.exists():
            self.skipTest("platformio.ini not found in project root")

        pio_ini = PlatformIOIni.parseFile(pio_ini_path)
        esp32c3_env = pio_ini.get_environment("esp32c3")

        if not esp32c3_env or not esp32c3_env.platform:
            self.skipTest("esp32c3 environment not properly configured")

        platform_resolution = pio_ini.resolve_platform_url_enhanced(
            esp32c3_env.platform, force_resolve=True
        )

        if not platform_resolution:
            self.skipTest("Failed to resolve platform")

        # Get all toolchains
        toolchains = [pkg for pkg in platform_resolution.packages if pkg.type == "toolchain"]

        self.assertGreater(len(toolchains), 0, "No toolchains found")

        print(f"\n✓ Resolved {len(toolchains)} Toolchain URLs:")

        resolved_count = 0
        for tc in toolchains:
            download_url = tc.get_download_url()
            if download_url:
                resolved_count += 1
                print(f"  {tc.name}")
                print(f"    Version: {tc.requirements or tc.url}")
                print(f"    URL: {download_url}")
            else:
                print(f"  {tc.name}: Could not resolve (may already be a URL)")

        # At least some toolchains should be resolvable
        self.assertGreater(
            resolved_count, 0, "Should be able to resolve at least one toolchain URL"
        )

    def test_framework_url_extraction(self) -> None:
        """
        Test that framework URLs are properly extracted (they're already direct URLs).
        """
        pio_ini_path = Path("platformio.ini")
        if not pio_ini_path.exists():
            self.skipTest("platformio.ini not found in project root")

        pio_ini = PlatformIOIni.parseFile(pio_ini_path)
        esp32c3_env = pio_ini.get_environment("esp32c3")

        if not esp32c3_env or not esp32c3_env.platform:
            self.skipTest("esp32c3 environment not properly configured")

        platform_resolution = pio_ini.resolve_platform_url_enhanced(
            esp32c3_env.platform, force_resolve=True
        )

        if not platform_resolution:
            self.skipTest("Failed to resolve platform")

        # Get frameworks (these typically have direct URLs)
        frameworks = [pkg for pkg in platform_resolution.packages if pkg.type == "framework"]

        self.assertGreater(len(frameworks), 0, "No frameworks found")

        print(f"\n✓ Framework URLs:")

        for fw in frameworks:
            download_url = fw.get_download_url()
            self.assertIsNotNone(download_url, f"Framework {fw.name} should have a download URL")
            print(f"  {fw.name}: {download_url}")


if __name__ == "__main__":
    unittest.main()
