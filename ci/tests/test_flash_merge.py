"""Unit tests for ESP32 flash image merging functionality."""

import tempfile
from pathlib import Path

import pytest

from ci.compiler.esp32_artifacts import ArtifactSet, ESP32FlashImageBuilder


def test_create_flash_manual_merge_esp32() -> None:
    """Test manual flash merge for ESP32 (bootloader at 0x1000)."""

    # Create temporary directory
    with tempfile.TemporaryDirectory() as tmpdir:
        test_dir = Path(tmpdir)

        # Create fake binary files
        bootloader_path = test_dir / "bootloader.bin"
        partitions_path = test_dir / "partitions.bin"
        boot_app0_path = test_dir / "boot_app0.bin"
        firmware_path = test_dir / "firmware.bin"

        bootloader_path.write_bytes(b"\xe9" + b"\x00" * 100)  # ESP32 magic
        partitions_path.write_bytes(b"\xaa" * 100)
        boot_app0_path.write_bytes(b"\xbb" * 100)
        firmware_path.write_bytes(b"\xcc" * 1000)

        # Create artifact set
        artifacts = ArtifactSet(
            firmware=firmware_path,
            bootloader=bootloader_path,
            partitions_bin=partitions_path,
            boot_app0=boot_app0_path,
        )

        # Test ESP32 (bootloader at 0x1000)
        builder = ESP32FlashImageBuilder("esp32dev", artifacts)
        flash_bin = test_dir / "flash.bin"
        result = builder.create_merged_image(flash_bin, flash_size_mb=4)
        assert result is True, "Flash merge should succeed"

        assert flash_bin.exists(), "flash.bin should be created"

        # Verify flash size
        assert flash_bin.stat().st_size == 4 * 1024 * 1024, "Flash should be 4MB"

        # Check bootloader at 0x1000
        with open(flash_bin, "rb") as f:
            f.seek(0x1000)
            magic = f.read(1)
            assert magic == b"\xe9", "Magic byte 0xE9 should be at offset 0x1000"

            # Check partitions at 0x8000
            f.seek(0x8000)
            partition_start = f.read(1)
            assert partition_start == b"\xaa", "Partitions should be at offset 0x8000"

            # Check boot_app0 at 0xe000
            f.seek(0xE000)
            boot_app0_start = f.read(1)
            assert boot_app0_start == b"\xbb", "boot_app0 should be at offset 0xe000"

            # Check firmware at 0x10000
            f.seek(0x10000)
            firmware_start = f.read(1)
            assert firmware_start == b"\xcc", "Firmware should be at offset 0x10000"


def test_create_flash_manual_merge_esp32c3() -> None:
    """Test manual flash merge for ESP32-C3 (bootloader at 0x0)."""
    # Create temporary directory
    with tempfile.TemporaryDirectory() as tmpdir:
        test_dir = Path(tmpdir)

        # Create fake binary files
        bootloader_path = test_dir / "bootloader.bin"
        partitions_path = test_dir / "partitions.bin"
        boot_app0_path = test_dir / "boot_app0.bin"
        firmware_path = test_dir / "firmware.bin"

        bootloader_path.write_bytes(b"\xe9" + b"\x00" * 100)  # ESP32 magic
        partitions_path.write_bytes(b"\xaa" * 100)
        boot_app0_path.write_bytes(b"\xbb" * 100)
        firmware_path.write_bytes(b"\xcc" * 1000)

        # Create artifact set
        artifacts = ArtifactSet(
            firmware=firmware_path,
            bootloader=bootloader_path,
            partitions_bin=partitions_path,
            boot_app0=boot_app0_path,
        )

        # Test ESP32-C3 (bootloader at 0x0)
        builder = ESP32FlashImageBuilder("esp32c3", artifacts)
        flash_bin = test_dir / "flash.bin"
        result = builder.create_merged_image(flash_bin, flash_size_mb=4)
        assert result is True, "Flash merge should succeed"

        assert flash_bin.exists(), "flash.bin should be created"

        # Verify flash size
        assert flash_bin.stat().st_size == 4 * 1024 * 1024, "Flash should be 4MB"

        # Check bootloader at 0x0
        with open(flash_bin, "rb") as f:
            f.seek(0x0)
            magic = f.read(1)
            assert magic == b"\xe9", "Magic byte 0xE9 should be at offset 0x0"

            # Check partitions at 0x8000
            f.seek(0x8000)
            partition_start = f.read(1)
            assert partition_start == b"\xaa", "Partitions should be at offset 0x8000"

            # Check boot_app0 at 0xe000
            f.seek(0xE000)
            boot_app0_start = f.read(1)
            assert boot_app0_start == b"\xbb", "boot_app0 should be at offset 0xe000"

            # Check firmware at 0x10000
            f.seek(0x10000)
            firmware_start = f.read(1)
            assert firmware_start == b"\xcc", "Firmware should be at offset 0x10000"


def test_create_flash_manual_merge_esp32s3() -> None:
    """Test manual flash merge for ESP32-S3 (bootloader at 0x0)."""
    # Create temporary directory
    with tempfile.TemporaryDirectory() as tmpdir:
        test_dir = Path(tmpdir)

        # Create fake binary files
        bootloader_path = test_dir / "bootloader.bin"
        partitions_path = test_dir / "partitions.bin"
        boot_app0_path = test_dir / "boot_app0.bin"
        firmware_path = test_dir / "firmware.bin"

        bootloader_path.write_bytes(b"\xe9" + b"\x00" * 100)  # ESP32 magic
        partitions_path.write_bytes(b"\xaa" * 100)
        boot_app0_path.write_bytes(b"\xbb" * 100)
        firmware_path.write_bytes(b"\xcc" * 1000)

        # Create artifact set
        artifacts = ArtifactSet(
            firmware=firmware_path,
            bootloader=bootloader_path,
            partitions_bin=partitions_path,
            boot_app0=boot_app0_path,
        )

        # Test ESP32-S3 (bootloader at 0x0)
        builder = ESP32FlashImageBuilder("esp32s3", artifacts)
        flash_bin = test_dir / "flash.bin"
        result = builder.create_merged_image(flash_bin, flash_size_mb=4)
        assert result is True, "Flash merge should succeed"

        assert flash_bin.exists(), "flash.bin should be created"

        # Check bootloader at 0x0
        with open(flash_bin, "rb") as f:
            f.seek(0x0)
            magic = f.read(1)
            assert magic == b"\xe9", "Magic byte 0xE9 should be at offset 0x0"


def test_create_flash_manual_merge_missing_files() -> None:
    """Test manual flash merge with missing files."""
    # Create temporary directory
    with tempfile.TemporaryDirectory() as tmpdir:
        test_dir = Path(tmpdir)

        # Create only bootloader file
        bootloader_path = test_dir / "bootloader.bin"
        bootloader_path.write_bytes(b"\xe9" + b"\x00" * 100)
        # Missing: partitions.bin, boot_app0.bin, firmware.bin

        # Create artifact set with missing files
        artifacts = ArtifactSet(
            bootloader=bootloader_path,
            # Missing required files
        )

        # Should raise error due to missing required files
        builder = ESP32FlashImageBuilder("esp32dev", artifacts)
        flash_bin = test_dir / "flash.bin"

        # This should raise ValueError due to missing required files
        with pytest.raises(ValueError, match="Missing required files"):
            builder.create_merged_image(flash_bin, flash_size_mb=4)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
