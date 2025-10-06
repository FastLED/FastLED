"""Unit tests for ESP32 flash image merging functionality."""

import shutil
import tempfile
from pathlib import Path

import pytest


def test_create_flash_manual_merge_esp32():
    """Test manual flash merge for ESP32 (bootloader at 0x1000)."""
    # Import the function
    import importlib.util

    spec = importlib.util.spec_from_file_location(
        "ci_compile", Path(__file__).parent.parent / "ci-compile.py"
    )
    assert spec is not None, "Could not load module spec"
    ci_compile = importlib.util.module_from_spec(spec)
    assert spec.loader is not None, "Module spec has no loader"
    spec.loader.exec_module(ci_compile)

    # Create temporary directory
    with tempfile.TemporaryDirectory() as tmpdir:
        test_dir = Path(tmpdir)

        # Create fake binary files
        (test_dir / "bootloader.bin").write_bytes(
            b"\xe9" + b"\x00" * 100
        )  # ESP32 magic
        (test_dir / "partitions.bin").write_bytes(b"\xaa" * 100)
        (test_dir / "boot_app0.bin").write_bytes(b"\xbb" * 100)
        (test_dir / "firmware.bin").write_bytes(b"\xcc" * 1000)

        # Test ESP32 (bootloader at 0x1000)
        result = ci_compile._create_flash_manual_merge(test_dir, "esp32dev", 4)
        assert result is True, "Flash merge should succeed"

        flash_bin = test_dir / "flash.bin"
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


def test_create_flash_manual_merge_esp32c3():
    """Test manual flash merge for ESP32-C3 (bootloader at 0x0)."""
    # Import the function
    import importlib.util

    spec = importlib.util.spec_from_file_location(
        "ci_compile", Path(__file__).parent.parent / "ci-compile.py"
    )
    assert spec is not None, "Could not load module spec"
    ci_compile = importlib.util.module_from_spec(spec)
    assert spec.loader is not None, "Module spec has no loader"
    spec.loader.exec_module(ci_compile)

    # Create temporary directory
    with tempfile.TemporaryDirectory() as tmpdir:
        test_dir = Path(tmpdir)

        # Create fake binary files
        (test_dir / "bootloader.bin").write_bytes(
            b"\xe9" + b"\x00" * 100
        )  # ESP32 magic
        (test_dir / "partitions.bin").write_bytes(b"\xaa" * 100)
        (test_dir / "boot_app0.bin").write_bytes(b"\xbb" * 100)
        (test_dir / "firmware.bin").write_bytes(b"\xcc" * 1000)

        # Test ESP32-C3 (bootloader at 0x0)
        result = ci_compile._create_flash_manual_merge(test_dir, "esp32c3", 4)
        assert result is True, "Flash merge should succeed"

        flash_bin = test_dir / "flash.bin"
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


def test_create_flash_manual_merge_esp32s3():
    """Test manual flash merge for ESP32-S3 (bootloader at 0x0)."""
    # Import the function
    import importlib.util

    spec = importlib.util.spec_from_file_location(
        "ci_compile", Path(__file__).parent.parent / "ci-compile.py"
    )
    assert spec is not None, "Could not load module spec"
    ci_compile = importlib.util.module_from_spec(spec)
    assert spec.loader is not None, "Module spec has no loader"
    spec.loader.exec_module(ci_compile)

    # Create temporary directory
    with tempfile.TemporaryDirectory() as tmpdir:
        test_dir = Path(tmpdir)

        # Create fake binary files
        (test_dir / "bootloader.bin").write_bytes(
            b"\xe9" + b"\x00" * 100
        )  # ESP32 magic
        (test_dir / "partitions.bin").write_bytes(b"\xaa" * 100)
        (test_dir / "boot_app0.bin").write_bytes(b"\xbb" * 100)
        (test_dir / "firmware.bin").write_bytes(b"\xcc" * 1000)

        # Test ESP32-S3 (bootloader at 0x0)
        result = ci_compile._create_flash_manual_merge(test_dir, "esp32s3", 4)
        assert result is True, "Flash merge should succeed"

        flash_bin = test_dir / "flash.bin"
        assert flash_bin.exists(), "flash.bin should be created"

        # Check bootloader at 0x0
        with open(flash_bin, "rb") as f:
            f.seek(0x0)
            magic = f.read(1)
            assert magic == b"\xe9", "Magic byte 0xE9 should be at offset 0x0"


def test_create_flash_manual_merge_missing_files():
    """Test manual flash merge with missing files."""
    # Import the function
    import importlib.util

    spec = importlib.util.spec_from_file_location(
        "ci_compile", Path(__file__).parent.parent / "ci-compile.py"
    )
    assert spec is not None, "Could not load module spec"
    ci_compile = importlib.util.module_from_spec(spec)
    assert spec.loader is not None, "Module spec has no loader"
    spec.loader.exec_module(ci_compile)

    # Create temporary directory
    with tempfile.TemporaryDirectory() as tmpdir:
        test_dir = Path(tmpdir)

        # Create only some files
        (test_dir / "bootloader.bin").write_bytes(b"\xe9" + b"\x00" * 100)
        # Missing: partitions.bin, boot_app0.bin, firmware.bin

        # Should still succeed but with warnings
        result = ci_compile._create_flash_manual_merge(test_dir, "esp32dev", 4)
        assert result is True, "Flash merge should succeed even with missing files"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
