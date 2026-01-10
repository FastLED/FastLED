#!/usr/bin/env python3
"""
Test toolchain URL extraction from platformio.ini files.

This test validates that we can extract fully resolved toolchain URLs
from a platformio.ini configuration, which is critical for understanding
platform download requirements (toolchains represent ~90% of download size).
"""

import unittest
from pathlib import Path

import pytest

from ci.compiler.platformio_ini import PackageInfo, PlatformIOIni


class TestToolchainUrlExtraction(unittest.TestCase):
    """Test extraction of toolchain URLs from platformio.ini."""

    @pytest.mark.slow
    def test_extract_toolchains_from_esp32c3(self) -> None:
        """
        Test that we can extract toolchain URLs for ESP32C3 (RISC-V platform).

        ESP32C3 uses the RISC-V toolchain (toolchain-riscv32-esp) which is
        a critical download for compilation.
        """
        # Load the main platformio.ini
        pio_ini_path = Path("platformio.ini")
        if not pio_ini_path.exists():
            self.skipTest("platformio.ini not found in project root")

        pio_ini = PlatformIOIni.parseFile(pio_ini_path)

        # Get the esp32c3 environment
        esp32c3_env = pio_ini.get_environment("esp32c3")
        self.assertIsNotNone(
            esp32c3_env, "esp32c3 environment not found in platformio.ini"
        )

        # Extract platform information with enhanced resolution
        platform_name = esp32c3_env.platform
        self.assertIsNotNone(platform_name, "esp32c3 environment missing platform")
        assert platform_name is not None  # Type narrowing for type checker

        # Resolve platform to get package/toolchain information
        # force_resolve=True ensures we query PlatformIO even if platform_name is already a URL
        platform_resolution = pio_ini.resolve_platform_url_enhanced(
            platform_name, force_resolve=True
        )
        self.assertIsNotNone(
            platform_resolution, f"Failed to resolve platform: {platform_name}"
        )

        # Extract toolchain packages
        toolchains = [
            pkg for pkg in platform_resolution.packages if pkg.type == "toolchain"
        ]

        # Verify we found at least one toolchain
        self.assertGreater(
            len(toolchains), 0, "No toolchains found for ESP32C3 platform"
        )

        # Look for the RISC-V toolchain specifically
        riscv_toolchain = None
        for tc in toolchains:
            if "riscv" in tc.name.lower():
                riscv_toolchain = tc
                break

        self.assertIsNotNone(riscv_toolchain, "RISC-V toolchain not found for ESP32C3")

        # Verify the toolchain has either a direct URL or a version requirement
        # PlatformIO platforms can specify toolchains via:
        #   1. Direct URLs (newer platforms like espressif32@55.3.31+)
        #   2. Version requirements (older platforms) which PlatformIO resolves via registry
        toolchain_url = riscv_toolchain.requirements or riscv_toolchain.url
        self.assertTrue(
            toolchain_url,
            f"No URL/requirement found for toolchain: {riscv_toolchain.name}",
        )

        # Check if it's a direct URL or version requirement
        is_direct_url = toolchain_url.startswith("http://") or toolchain_url.startswith(
            "https://"
        )

        if is_direct_url:
            # Verify it's a downloadable artifact (zip, tar, etc.)
            self.assertTrue(
                any(
                    ext in toolchain_url.lower()
                    for ext in [".zip", ".tar", ".gz", ".xz"]
                ),
                f"Toolchain URL doesn't point to a downloadable archive: {toolchain_url}",
            )
            print("\n✓ ESP32C3 RISC-V Toolchain (Direct URL):")
            print(f"  Name: {riscv_toolchain.name}")
            print(f"  URL: {toolchain_url}")
        else:
            # Version requirement - PlatformIO will resolve this via package registry
            print("\n✓ ESP32C3 RISC-V Toolchain (Version Requirement):")
            print(f"  Name: {riscv_toolchain.name}")
            print(f"  Requirement: {toolchain_url}")
            print("  Note: PlatformIO resolves this via package registry")

        print(f"  Version: {riscv_toolchain.version or 'N/A'}")
        print(f"  Optional: {riscv_toolchain.optional}")

    @pytest.mark.slow
    def test_extract_toolchains_from_esp32s3(self) -> None:
        """
        Test that we can extract toolchain URLs for ESP32S3 (Xtensa platform).

        ESP32S3 uses the Xtensa toolchain (toolchain-xtensa-esp-elf) which is
        different from the RISC-V toolchain used by ESP32C3.
        """
        # Load the main platformio.ini
        pio_ini_path = Path("platformio.ini")
        if not pio_ini_path.exists():
            self.skipTest("platformio.ini not found in project root")

        pio_ini = PlatformIOIni.parseFile(pio_ini_path)

        # Get the esp32s3 environment
        esp32s3_env = pio_ini.get_environment("esp32s3")
        self.assertIsNotNone(
            esp32s3_env, "esp32s3 environment not found in platformio.ini"
        )

        # Extract platform information
        platform_name = esp32s3_env.platform
        self.assertIsNotNone(platform_name, "esp32s3 environment missing platform")
        assert platform_name is not None  # Type narrowing for type checker

        # Resolve platform to get package/toolchain information
        # force_resolve=True ensures we query PlatformIO even if platform_name is already a URL
        platform_resolution = pio_ini.resolve_platform_url_enhanced(
            platform_name, force_resolve=True
        )
        self.assertIsNotNone(
            platform_resolution, f"Failed to resolve platform: {platform_name}"
        )

        # Extract toolchain packages
        toolchains = [
            pkg for pkg in platform_resolution.packages if pkg.type == "toolchain"
        ]

        # Verify we found at least one toolchain
        self.assertGreater(
            len(toolchains), 0, "No toolchains found for ESP32S3 platform"
        )

        # Look for the Xtensa toolchain specifically
        xtensa_toolchain = None
        for tc in toolchains:
            if "xtensa" in tc.name.lower():
                xtensa_toolchain = tc
                break

        self.assertIsNotNone(xtensa_toolchain, "Xtensa toolchain not found for ESP32S3")

        # Verify the toolchain has either a URL or version requirement
        toolchain_url = xtensa_toolchain.requirements or xtensa_toolchain.url
        self.assertTrue(
            toolchain_url,
            f"No URL/version found for toolchain: {xtensa_toolchain.name}",
        )

        # Check if it's a direct URL or a version requirement
        is_direct_url = toolchain_url.startswith("http://") or toolchain_url.startswith(
            "https://"
        )

        print("\n✓ ESP32S3 Xtensa Toolchain:")
        print(f"  Name: {xtensa_toolchain.name}")
        if is_direct_url:
            print(f"  URL: {toolchain_url}")
        else:
            print(f"  Version Requirement: {toolchain_url}")
        print(f"  Resolved Version: {xtensa_toolchain.version or 'N/A'}")

    @pytest.mark.slow
    def test_compare_toolchains_across_environments(self) -> None:
        """
        Test that different ESP32 variants use different toolchains.

        This validates that our URL extraction correctly distinguishes between:
        - ESP32C3 (RISC-V) -> toolchain-riscv32-esp
        - ESP32S3 (Xtensa) -> toolchain-xtensa-esp-elf
        """
        pio_ini_path = Path("platformio.ini")
        if not pio_ini_path.exists():
            self.skipTest("platformio.ini not found in project root")

        pio_ini = PlatformIOIni.parseFile(pio_ini_path)

        # Extract toolchains for both environments
        env_toolchains: dict[str, list[PackageInfo]] = {}

        for env_name in ["esp32c3", "esp32s3"]:
            env = pio_ini.get_environment(env_name)
            if not env or not env.platform:
                continue

            platform_resolution = pio_ini.resolve_platform_url_enhanced(
                env.platform, force_resolve=True
            )
            if platform_resolution:
                toolchains = [
                    pkg
                    for pkg in platform_resolution.packages
                    if pkg.type == "toolchain"
                ]
                env_toolchains[env_name] = toolchains

        # Verify we got toolchains for both
        self.assertIn("esp32c3", env_toolchains, "Failed to extract ESP32C3 toolchains")
        self.assertIn("esp32s3", env_toolchains, "Failed to extract ESP32S3 toolchains")

        # Find RISC-V and Xtensa toolchains
        c3_riscv = [
            tc for tc in env_toolchains["esp32c3"] if "riscv" in tc.name.lower()
        ]
        s3_xtensa = [
            tc for tc in env_toolchains["esp32s3"] if "xtensa" in tc.name.lower()
        ]

        self.assertGreater(len(c3_riscv), 0, "ESP32C3 should have RISC-V toolchain")
        self.assertGreater(len(s3_xtensa), 0, "ESP32S3 should have Xtensa toolchain")

        # Verify they use different toolchains (different architectures)
        c3_name = c3_riscv[0].name
        s3_name = s3_xtensa[0].name

        self.assertNotEqual(
            c3_name, s3_name, "ESP32C3 and ESP32S3 should use different toolchains"
        )
        self.assertIn(
            "riscv", c3_name.lower(), "ESP32C3 toolchain should be RISC-V based"
        )
        self.assertIn(
            "xtensa", s3_name.lower(), "ESP32S3 toolchain should be Xtensa based"
        )

        c3_url = c3_riscv[0].requirements or c3_riscv[0].url
        s3_url = s3_xtensa[0].requirements or s3_xtensa[0].url

        print("\n✓ Toolchain Comparison:")
        print(f"  ESP32C3 (RISC-V): {c3_name}")
        print(f"    URL/Version: {c3_url}")
        print(f"  ESP32S3 (Xtensa): {s3_name}")
        print(f"    URL/Version: {s3_url}")

    @pytest.mark.slow
    def test_extract_all_packages_by_type(self) -> None:
        """
        Test that we can categorize all packages by type (toolchains, frameworks, debuggers, etc.).

        This is a stretch goal test showing comprehensive package extraction.
        """
        pio_ini_path = Path("platformio.ini")
        if not pio_ini_path.exists():
            self.skipTest("platformio.ini not found in project root")

        pio_ini = PlatformIOIni.parseFile(pio_ini_path)

        # Get esp32c3 as example
        esp32c3_env = pio_ini.get_environment("esp32c3")
        if not esp32c3_env or not esp32c3_env.platform:
            self.skipTest("esp32c3 environment not properly configured")

        platform_resolution = pio_ini.resolve_platform_url_enhanced(
            esp32c3_env.platform, force_resolve=True
        )
        if not platform_resolution:
            self.skipTest("Failed to resolve esp32c3 platform")

        # Categorize packages by type
        packages_by_type: dict[str, list[PackageInfo]] = {}
        for pkg in platform_resolution.packages:
            pkg_type = pkg.type or "unknown"
            if pkg_type not in packages_by_type:
                packages_by_type[pkg_type] = []
            packages_by_type[pkg_type].append(pkg)

        # Print summary
        print("\n✓ ESP32C3 Package Summary:")
        for pkg_type, packages in sorted(packages_by_type.items()):
            print(f"  {pkg_type}: {len(packages)} package(s)")
            for pkg in packages:
                url = pkg.requirements or pkg.url
                if url:
                    print(f"    - {pkg.name}: {url[:80]}...")

        # Verify critical types exist
        self.assertIn("toolchain", packages_by_type, "No toolchain packages found")
        self.assertIn("framework", packages_by_type, "No framework packages found")


if __name__ == "__main__":
    unittest.main()
