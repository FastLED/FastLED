#!/usr/bin/env python3
"""Test PlatformIO merged binary generation for ESP32 QEMU testing."""

import subprocess
from pathlib import Path

from ci.boards import create_board
from ci.compiler.pio import PioCompiler


def test_platformio_merge_bin_target_exists() -> None:
    """Verify PlatformIO can generate merged.bin for ESP32.

    This is the foundational test - if this fails, the whole approach won't work.
    """
    # 1. Build normally using existing PioCompiler
    board = create_board("esp32dev")
    compiler = PioCompiler(
        board=board,
        verbose=True,
        global_cache_dir=Path.home() / ".fastled" / "platformio_cache",
    )

    # Build Blink example
    futures = compiler.build(["Blink"])
    result = futures[0].result()

    assert result.success, f"Normal build failed: {result.output}"

    # 2. Manually run merge_bin target
    merge_cmd = [
        "pio",
        "run",
        "--project-dir",
        str(result.build_dir),
        "--target",
        "merge_bin",
    ]

    print(f"Running: {' '.join(merge_cmd)}")
    merge_proc = subprocess.run(merge_cmd, capture_output=True, text=True, timeout=300)

    # 3. Verify merge_bin succeeded
    assert merge_proc.returncode == 0, f"merge_bin failed: {merge_proc.stderr}"

    # 4. Check merged.bin exists
    merged_bin = result.build_dir / ".pio" / "build" / "esp32dev" / "merged.bin"
    assert merged_bin.exists(), f"merged.bin not found at {merged_bin}"

    # 5. Verify size is reasonable (bootloader + partitions + app)
    size = merged_bin.stat().st_size
    assert size > 100_000, f"Merged binary too small: {size} bytes"
    assert size < 10_000_000, f"Merged binary too large: {size} bytes"

    # 6. Verify ESP32 bootloader magic byte (0xE9)
    with open(merged_bin, "rb") as f:
        magic_byte = f.read(1)[0]
        assert magic_byte == 0xE9, (
            f"Invalid magic byte: {hex(magic_byte)}, expected 0xE9"
        )

    print(f"SUCCESS: merged.bin generated at {merged_bin}")
    print(f"   Size: {size:,} bytes")
    print(f"   Magic byte: 0x{magic_byte:02X}")


def test_build_with_merged_bin_esp32() -> None:
    """Test PioCompiler.build_with_merged_bin() method."""
    board = create_board("esp32dev")
    compiler = PioCompiler(
        board=board,
        verbose=True,
        global_cache_dir=Path.home() / ".fastled" / "platformio_cache",
    )

    # Build with merged binary
    result = compiler.build_with_merged_bin("Blink")

    # Assertions
    assert result.success, f"Build failed: {result.output}"
    assert result.merged_bin_path is not None
    assert result.merged_bin_path.exists()

    # Size check
    merged_size = result.merged_bin_path.stat().st_size
    assert 100_000 < merged_size < 10_000_000

    # Magic byte check
    with open(result.merged_bin_path, "rb") as f:
        assert f.read(1)[0] == 0xE9

    print(f"SUCCESS: build_with_merged_bin() generated {result.merged_bin_path}")
    print(f"   Size: {merged_size:,} bytes")


def test_supports_merged_bin() -> None:
    """Test supports_merged_bin() method for various boards."""
    # ESP32 should support merged bin
    esp32_board = create_board("esp32dev")
    esp32_compiler = PioCompiler(
        board=esp32_board,
        verbose=False,
        global_cache_dir=Path.home() / ".fastled" / "platformio_cache",
    )
    assert esp32_compiler.supports_merged_bin()

    # UNO should not support merged bin
    uno_board = create_board("uno")
    uno_compiler = PioCompiler(
        board=uno_board,
        verbose=False,
        global_cache_dir=Path.home() / ".fastled" / "platformio_cache",
    )
    assert not uno_compiler.supports_merged_bin()

    print("SUCCESS: supports_merged_bin() correctly identifies board support")


def test_merge_bin_unsupported_board() -> None:
    """Test that unsupported boards fail gracefully."""
    board = create_board("uno")  # AVR board
    compiler = PioCompiler(
        board=board,
        verbose=False,
        global_cache_dir=Path.home() / ".fastled" / "platformio_cache",
    )

    assert not compiler.supports_merged_bin()

    # Should return failure with informative error
    result = compiler.build_with_merged_bin("Blink")
    assert not result.success
    assert "does not support merged binary" in result.output

    print("SUCCESS: Unsupported board failed gracefully with clear error message")


def test_merge_bin_custom_output_path() -> None:
    """Test custom output path for merged binary."""
    board = create_board("esp32dev")
    compiler = PioCompiler(
        board=board,
        verbose=True,
        global_cache_dir=Path.home() / ".fastled" / "platformio_cache",
    )

    import tempfile

    with tempfile.TemporaryDirectory() as tmpdir:
        output_path = Path(tmpdir) / "test_merged.bin"
        result = compiler.build_with_merged_bin("Blink", output_path=output_path)

        assert result.success
        assert result.merged_bin_path == output_path
        assert output_path.exists()
        assert output_path.stat().st_size > 100_000

        print(f"SUCCESS: Custom output path works: {output_path}")
