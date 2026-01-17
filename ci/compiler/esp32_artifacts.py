from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""ESP32 artifact management for QEMU and flashing.

This module provides clean separation of concerns for ESP32-specific artifact
operations including discovery, validation, flash image building, and copying.
"""

import shutil
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Optional

from ci.compiler.esp32_constants import (
    BOOT_APP0_SIZE,
    DEFAULT_PARTITIONS_CSV,
    FILE_NAMES,
    FLASH_OFFSETS,
    VALID_FLASH_SIZES,
    get_bootloader_offset,
)


class FlashMode(Enum):
    """ESP32 flash modes."""

    QIO = "qio"  # Quad I/O - not QEMU compatible
    DIO = "dio"  # Dual I/O - QEMU compatible
    QOUT = "qout"
    DOUT = "dout"
    UNKNOWN = "unknown"


@dataclass(frozen=True)
class FlashConfig:
    """ESP32 flash configuration (immutable)."""

    mode: FlashMode
    size_mb: int
    frequency: str = "80m"

    def is_qemu_compatible(self) -> bool:
        """Check if configuration is QEMU compatible."""
        return (
            self.mode in [FlashMode.DIO, FlashMode.DOUT]
            and self.size_mb in VALID_FLASH_SIZES
        )


@dataclass
class ArtifactSet:
    """Collection of ESP32 build artifacts."""

    firmware: Optional[Path] = None
    bootloader: Optional[Path] = None
    partitions_bin: Optional[Path] = None
    partitions_csv: Optional[Path] = None
    boot_app0: Optional[Path] = None
    spiffs: Optional[Path] = None
    merged: Optional[Path] = None

    @property
    def required_files_present(self) -> bool:
        """Check if all required files are present."""
        return all(
            [
                self.firmware,
                self.bootloader,
                self.partitions_bin or self.partitions_csv,
            ]
        )

    def missing_files(self) -> list[str]:
        """Get list of missing required files."""
        missing: list[str] = []
        if not self.firmware:
            missing.append("firmware.bin")
        if not self.bootloader:
            missing.append("bootloader.bin")
        if not self.partitions_bin and not self.partitions_csv:
            missing.append("partitions.bin or partitions.csv")
        return missing


class ESP32ArtifactDiscovery:
    """Discovers ESP32 build artifacts from PlatformIO build directory."""

    def __init__(self, build_dir: Path, board_name: str):
        self.build_dir = build_dir
        self.board_name = board_name
        self.artifact_dir = build_dir / ".pio" / "build" / board_name

    def discover(self) -> ArtifactSet:
        """Discover all ESP32 artifacts in build directory."""
        if not self.artifact_dir.exists():
            return ArtifactSet()

        return ArtifactSet(
            firmware=self._find_file(FILE_NAMES.FIRMWARE),
            bootloader=self._find_file(FILE_NAMES.BOOTLOADER),
            partitions_bin=self._find_file(FILE_NAMES.PARTITIONS_BIN),
            partitions_csv=self._find_file(FILE_NAMES.PARTITIONS_CSV),
            boot_app0=self._find_file(FILE_NAMES.BOOT_APP0),
            spiffs=self._find_file(FILE_NAMES.SPIFFS),
            merged=self._find_file(FILE_NAMES.MERGED),
        )

    def _find_file(self, filename: str) -> Optional[Path]:
        """Find a file in the artifact directory."""
        path = self.artifact_dir / filename
        return path if path.exists() else None

    def read_flash_config(self) -> FlashConfig:
        """Read flash configuration from build artifacts."""
        flash_args_file = self.artifact_dir / "flash_args"
        if not flash_args_file.exists():
            return FlashConfig(FlashMode.UNKNOWN, 4)

        content = flash_args_file.read_text()

        # Parse flash mode
        mode = FlashMode.UNKNOWN
        if "--flash_mode dio" in content:
            mode = FlashMode.DIO
        elif "--flash_mode qio" in content:
            mode = FlashMode.QIO
        elif "--flash_mode dout" in content:
            mode = FlashMode.DOUT
        elif "--flash_mode qout" in content:
            mode = FlashMode.QOUT

        # Parse flash size (default 4MB)
        size_mb = 4
        if "--flash_size 2MB" in content:
            size_mb = 2
        elif "--flash_size 8MB" in content:
            size_mb = 8
        elif "--flash_size 16MB" in content:
            size_mb = 16

        return FlashConfig(mode, size_mb)


class ESP32FlashImageBuilder:
    """Builds merged flash images for ESP32 QEMU."""

    def __init__(self, board_name: str, artifacts: ArtifactSet):
        self.board_name = board_name
        self.artifacts = artifacts
        self.bootloader_offset = get_bootloader_offset(board_name)

    def create_merged_image(self, output_path: Path, flash_size_mb: int = 4) -> bool:
        """Create a merged flash image from individual artifacts."""
        if flash_size_mb not in VALID_FLASH_SIZES:
            raise ValueError(
                f"Invalid flash size: {flash_size_mb}MB. Must be in {VALID_FLASH_SIZES}"
            )

        if not self.artifacts.required_files_present:
            raise ValueError(
                f"Missing required files: {self.artifacts.missing_files()}"
            )

        flash_size = flash_size_mb * 1024 * 1024
        flash_data = bytearray(b"\xff" * flash_size)

        # Merge files at their offsets
        self._write_at_offset(
            flash_data, self.artifacts.bootloader, self.bootloader_offset
        )
        self._write_at_offset(
            flash_data, self.artifacts.partitions_bin, FLASH_OFFSETS.PARTITIONS
        )
        self._write_at_offset(
            flash_data, self.artifacts.boot_app0, FLASH_OFFSETS.BOOT_APP0
        )
        self._write_at_offset(
            flash_data, self.artifacts.firmware, FLASH_OFFSETS.FIRMWARE
        )

        # Optional: SPIFFS if present (board-specific offset)
        # Note: SPIFFS typically starts after firmware, offset varies by partition table

        output_path.write_bytes(flash_data)
        return True

    def _write_at_offset(
        self, flash_data: bytearray, file_path: Optional[Path], offset: int
    ) -> None:
        """Write file contents at specified offset."""
        if not file_path or not file_path.exists():
            return

        file_data = file_path.read_bytes()
        if offset + len(file_data) > len(flash_data):
            raise ValueError(
                f"File {file_path.name} at offset 0x{offset:x} exceeds flash size"
            )

        flash_data[offset : offset + len(file_data)] = file_data


class ESP32ArtifactCopier:
    """Copies ESP32 artifacts to output directory with proper structure."""

    def __init__(self, artifacts: ArtifactSet, flash_config: FlashConfig):
        self.artifacts = artifacts
        self.flash_config = flash_config

    def copy_for_qemu(self, output_dir: Path, board_name: str) -> bool:
        """Copy all artifacts needed for QEMU testing."""
        output_dir.mkdir(parents=True, exist_ok=True)

        copied: list[str] = []

        # Copy required files
        if not self._copy_required_files(output_dir, copied):
            return False

        # Copy optional files
        self._copy_optional_files(output_dir, copied)

        # Ensure partitions.csv exists (required by tobozo QEMU)
        self._ensure_partitions_csv(output_dir, copied)

        # Create merged flash image if artifacts are present
        if self.artifacts.firmware:
            self._create_flash_image(output_dir, board_name, copied)

        self._print_summary(output_dir, copied)
        return True

    def _copy_required_files(self, output_dir: Path, copied: list[str]) -> bool:
        """Copy required files for QEMU."""
        success = True
        required = [
            (self.artifacts.firmware, FILE_NAMES.FIRMWARE),
            (self.artifacts.bootloader, FILE_NAMES.BOOTLOADER),
            (self.artifacts.partitions_bin, FILE_NAMES.PARTITIONS_BIN),
            (self.artifacts.boot_app0, FILE_NAMES.BOOT_APP0),
        ]

        for source, dest_name in required:
            if source and source.exists():
                shutil.copy2(source, output_dir / dest_name)
                copied.append(dest_name)
            elif dest_name == FILE_NAMES.FIRMWARE:
                print(f"❌ Required file missing: {dest_name}")
                success = False
            elif dest_name == FILE_NAMES.BOOT_APP0:
                # Create default boot_app0.bin
                self._create_default_boot_app0(output_dir)
                copied.append(dest_name)

        return success

    def _copy_optional_files(self, output_dir: Path, copied: list[str]) -> None:
        """Copy optional files if present."""
        if self.artifacts.spiffs:
            shutil.copy2(self.artifacts.spiffs, output_dir / FILE_NAMES.SPIFFS)
            copied.append(FILE_NAMES.SPIFFS)

    def _ensure_partitions_csv(self, output_dir: Path, copied: list[str]) -> None:
        """Ensure partitions.csv exists (create default if missing)."""
        if self.artifacts.partitions_csv:
            shutil.copy2(
                self.artifacts.partitions_csv, output_dir / FILE_NAMES.PARTITIONS_CSV
            )
            copied.append(FILE_NAMES.PARTITIONS_CSV)
        else:
            (output_dir / FILE_NAMES.PARTITIONS_CSV).write_text(DEFAULT_PARTITIONS_CSV)
            copied.append(f"{FILE_NAMES.PARTITIONS_CSV} (default)")

    def _create_default_boot_app0(self, output_dir: Path) -> None:
        """Create a default boot_app0.bin file."""
        boot_app0_data = b"\xff" * BOOT_APP0_SIZE
        (output_dir / FILE_NAMES.BOOT_APP0).write_bytes(boot_app0_data)

    def _create_flash_image(
        self, output_dir: Path, board_name: str, copied: list[str]
    ) -> None:
        """Create merged flash image for QEMU."""
        try:
            builder = ESP32FlashImageBuilder(board_name, self.artifacts)
            flash_path = output_dir / FILE_NAMES.FLASH
            if builder.create_merged_image(flash_path, self.flash_config.size_mb):
                copied.append(FILE_NAMES.FLASH)
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"⚠️  Warning: Could not create flash.bin: {e}")

    def _print_summary(self, output_dir: Path, copied: list[str]) -> None:
        """Print summary of copied files."""
        print("\n📋 ESP32 QEMU artifacts summary:")
        print(f"   Output directory: {output_dir}")
        print(f"   Files copied: {len(copied)}")
        for filename in sorted(copied):
            file_path = (
                output_dir / filename.split(" ")[0]
            )  # Handle " (default)" suffix
            if file_path.exists():
                size = file_path.stat().st_size
                print(f"     ✅ {filename}: {size} bytes")


class ESP32ArtifactManager:
    """High-level manager coordinating ESP32 artifact operations."""

    def __init__(self, build_dir: Path, board_name: str):
        self.build_dir = build_dir
        self.board_name = board_name
        self.discovery = ESP32ArtifactDiscovery(build_dir, board_name)

    def copy_qemu_artifacts(self, output_path: str, sketch_name: str) -> bool:
        """Copy all ESP32 QEMU artifacts to output location."""
        # Discover artifacts
        artifacts = self.discovery.discover()
        if not artifacts.required_files_present:
            print(f"❌ Missing required artifacts: {artifacts.missing_files()}")
            return False

        # Read flash configuration
        flash_config = self.discovery.read_flash_config()

        # Warn about QEMU compatibility
        if not flash_config.is_qemu_compatible():
            print(
                f"⚠️  Warning: Flash mode {flash_config.mode.value} may not be QEMU compatible"
            )
            print("   QEMU works best with DIO mode")

        # Determine output directory
        output_dir = self._resolve_output_dir(output_path)

        # Copy artifacts
        copier = ESP32ArtifactCopier(artifacts, flash_config)
        return copier.copy_for_qemu(output_dir, self.board_name)

    def _resolve_output_dir(self, output_path: str) -> Path:
        """Resolve output path to directory, making relative paths absolute."""
        path = Path(output_path)

        # Extract directory from the path
        if output_path.endswith(".bin"):
            dir_path = path.parent
        else:
            dir_path = path

        # If path is relative, resolve it against project root
        if not dir_path.is_absolute():
            # build_dir is like: /fastled/.build/pio/esp32dev
            # Project root is 3 levels up from build_dir
            project_root = self.build_dir.parent.parent.parent
            dir_path = project_root / dir_path

        return dir_path
