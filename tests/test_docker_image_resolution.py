#!/usr/bin/env python3
"""
Unit tests for Docker image resolution in QEMU testing.

Tests the platform family resolution, Docker image naming, and fallback logic
to ensure robust handling of Docker image configuration.
"""

import unittest
from typing import Optional

from ci.docker.build_platforms import (
    DOCKER_PLATFORMS,
    get_platform_for_board,
    get_docker_image_name,
    get_boards_for_platform,
)


class TestPlatformFamilyResolution(unittest.TestCase):
    """Test platform family resolution for different boards."""

    def test_esp32s3_resolves_to_esp_xtensa(self):
        """ESP32-S3 should resolve to esp-xtensa platform family."""
        platform_family = get_platform_for_board("esp32s3")
        self.assertEqual(platform_family, "esp-xtensa")

    def test_esp32c3_resolves_to_esp_riscv(self):
        """ESP32-C3 should resolve to esp-riscv platform family."""
        platform_family = get_platform_for_board("esp32c3")
        self.assertEqual(platform_family, "esp-riscv")

    def test_esp32dev_resolves_to_esp_xtensa(self):
        """ESP32 (original) should resolve to esp-xtensa platform family."""
        platform_family = get_platform_for_board("esp32dev")
        self.assertEqual(platform_family, "esp-xtensa")

    def test_uno_resolves_to_avr(self):
        """Arduino Uno should resolve to avr platform family."""
        platform_family = get_platform_for_board("uno")
        self.assertEqual(platform_family, "avr")

    def test_unknown_board_returns_none(self):
        """Unknown board should return None."""
        platform_family = get_platform_for_board("nonexistent_board")
        self.assertIsNone(platform_family)


class TestDockerImageNaming(unittest.TestCase):
    """Test Docker image name generation."""

    def test_esp_xtensa_image_name(self):
        """esp-xtensa platform should generate correct Docker image name."""
        image_name = get_docker_image_name("esp-xtensa")
        self.assertEqual(image_name, "niteris/fastled-compiler-esp-xtensa")

    def test_esp_riscv_image_name(self):
        """esp-riscv platform should generate correct Docker image name."""
        image_name = get_docker_image_name("esp-riscv")
        self.assertEqual(image_name, "niteris/fastled-compiler-esp-riscv")

    def test_avr_image_name(self):
        """avr platform should generate correct Docker image name."""
        image_name = get_docker_image_name("avr")
        self.assertEqual(image_name, "niteris/fastled-compiler-avr")

    def test_teensy_image_name(self):
        """teensy platform should generate correct Docker image name."""
        image_name = get_docker_image_name("teensy")
        self.assertEqual(image_name, "niteris/fastled-compiler-teensy")


class TestFullResolutionChain(unittest.TestCase):
    """Test the complete resolution chain from board to Docker image."""

    def test_esp32s3_full_chain(self):
        """Test ESP32-S3 resolves through full chain correctly."""
        # Step 1: Board -> Platform Family
        platform_family = get_platform_for_board("esp32s3")
        self.assertEqual(platform_family, "esp-xtensa")

        # Step 2: Platform Family -> Docker Image
        image_name = get_docker_image_name(platform_family)
        self.assertEqual(image_name, "niteris/fastled-compiler-esp-xtensa")

    def test_esp32c3_full_chain(self):
        """Test ESP32-C3 resolves through full chain correctly."""
        # Step 1: Board -> Platform Family
        platform_family = get_platform_for_board("esp32c3")
        self.assertEqual(platform_family, "esp-riscv")

        # Step 2: Platform Family -> Docker Image
        image_name = get_docker_image_name(platform_family)
        self.assertEqual(image_name, "niteris/fastled-compiler-esp-riscv")


class TestPlatformBoardLists(unittest.TestCase):
    """Test platform to boards mapping."""

    def test_esp_xtensa_includes_esp32s3(self):
        """esp-xtensa platform should include esp32s3."""
        boards = get_boards_for_platform("esp-xtensa")
        self.assertIn("esp32s3", boards)

    def test_esp_xtensa_includes_esp32dev(self):
        """esp-xtensa platform should include esp32dev."""
        boards = get_boards_for_platform("esp-xtensa")
        self.assertIn("esp32dev", boards)

    def test_esp_riscv_includes_esp32c3(self):
        """esp-riscv platform should include esp32c3."""
        boards = get_boards_for_platform("esp-riscv")
        self.assertIn("esp32c3", boards)

    def test_avr_includes_uno(self):
        """avr platform should include uno."""
        boards = get_boards_for_platform("avr")
        self.assertIn("uno", boards)

    def test_unknown_platform_returns_empty_list(self):
        """Unknown platform should return empty list."""
        boards = get_boards_for_platform("nonexistent_platform")
        self.assertEqual(boards, [])


class TestPlatformDataIntegrity(unittest.TestCase):
    """Test data integrity of DOCKER_PLATFORMS."""

    def test_no_duplicate_boards(self):
        """Ensure no board appears in multiple platform families."""
        all_boards = []
        for boards in DOCKER_PLATFORMS.values():
            all_boards.extend(boards)

        # Check for duplicates
        seen = set()
        duplicates = set()
        for board in all_boards:
            if board in seen:
                duplicates.add(board)
            seen.add(board)

        self.assertEqual(
            len(duplicates),
            0,
            f"Found duplicate boards across platforms: {duplicates}",
        )

    def test_all_platforms_have_boards(self):
        """Ensure all platform families have at least one board."""
        for platform, boards in DOCKER_PLATFORMS.items():
            self.assertGreater(
                len(boards), 0, f"Platform '{platform}' has no boards defined"
            )

    def test_platform_names_follow_convention(self):
        """Platform family names should follow naming convention."""
        for platform in DOCKER_PLATFORMS.keys():
            # Platform names should be lowercase
            self.assertEqual(platform, platform.lower())
            # Platform names should not contain spaces
            self.assertNotIn(" ", platform)


class TestDockerImageResolutionEdgeCases(unittest.TestCase):
    """Test edge cases and error handling in Docker image resolution."""

    def test_empty_board_name(self):
        """Empty board name should return None."""
        platform_family = get_platform_for_board("")
        self.assertIsNone(platform_family)

    def test_case_sensitivity(self):
        """Board names should be case-sensitive."""
        # Assuming boards are lowercase in DOCKER_PLATFORMS
        platform_family = get_platform_for_board("ESP32S3")
        # This should return None because our boards are lowercase
        self.assertIsNone(platform_family)

        # Lowercase should work
        platform_family = get_platform_for_board("esp32s3")
        self.assertEqual(platform_family, "esp-xtensa")


if __name__ == "__main__":
    unittest.main()
