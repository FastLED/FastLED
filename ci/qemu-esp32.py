#!/usr/bin/env python3
"""
ESP32 QEMU Runner

Runs ESP32 firmware in QEMU with logging and validation.
Designed to replace the tobozo/esp32-qemu-sim GitHub Action.
"""

import argparse
import platform
import re
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import List, Optional, TextIO

from ci.util.running_process import RunningProcess


def get_binary_name() -> str:
    """Get platform-specific QEMU binary name."""
    return (
        "qemu-system-xtensa.exe"
        if platform.system().lower() == "windows"
        else "qemu-system-xtensa"
    )


def find_executable(binary_name: str) -> Optional[Path]:
    """Find executable in PATH using platform-appropriate command."""
    try:
        cmd = [
            "where" if platform.system().lower() == "windows" else "which",
            binary_name,
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            # Take first line in case of multiple results (Windows 'where')
            return Path(result.stdout.strip().split("\n")[0])
    except (subprocess.SubprocessError, FileNotFoundError):
        pass
    return None


def find_qemu_binary() -> Optional[Path]:
    """Find QEMU ESP32 binary in common locations."""
    binary_name = get_binary_name()

    # ESP-IDF installation paths and portable installation
    esp_paths = [
        Path(".cache") / "qemu",  # Project-local portable installation directory
        Path.home() / ".espressif" / "tools" / "qemu-xtensa",
        Path.home() / ".espressif" / "python_env",
    ]

    for base_path in esp_paths:
        if base_path.exists():
            for qemu_path in base_path.rglob(binary_name):
                if qemu_path.is_file():
                    return qemu_path

    # Check system PATH
    return find_executable(binary_name)


def show_installation_help():
    """Display platform-specific installation instructions."""
    binary_name = get_binary_name()
    print(f"ERROR: QEMU ESP32 emulator ({binary_name}) not found!", file=sys.stderr)
    print()

    if platform.system().lower() == "windows":
        print("To install QEMU on Windows:")
        print("1. Chocolatey: choco install qemu -y")
        print("2. winget: winget install SoftwareFreedomConservancy.QEMU --scope user")
        print("3. Manual: Download from https://qemu.weilnetz.de/w64/")
        print("4. ESP-IDF: uv run ci/install-qemu.py")
    else:
        print("Run: uv run ci/install-qemu.py")


class QEMURunner:
    def __init__(self, qemu_binary: Optional[Path] = None):
        self.qemu_binary = qemu_binary or self._get_qemu_binary()
        self.output_lines: List[str] = []
        self.interrupt_met = False

    def _get_qemu_binary(self) -> Path:
        """Get QEMU binary path or raise error."""
        qemu_path = find_qemu_binary()
        if not qemu_path:
            raise FileNotFoundError(
                f"Could not find {get_binary_name()}. Please run install-qemu.py first."
            )
        return qemu_path

    def build_qemu_command(self, firmware_path: Path, flash_size: int = 4) -> List[str]:
        """Build QEMU command line arguments.

        Args:
            firmware_path: Path to firmware.bin or directory containing build artifacts
            flash_size: Flash size in MB
        """
        # Validate that flash_size is used correctly internally only
        if not isinstance(flash_size, int) or flash_size <= 0:
            raise ValueError(
                f"Invalid flash_size: {flash_size}. Must be a positive integer."
            )

        # Determine if we have a direct firmware.bin or a build directory
        if firmware_path.is_file() and firmware_path.suffix == ".bin":
            # Direct firmware.bin path
            flash_image_path = firmware_path.parent / "flash.bin"
            print(f"Creating flash image for firmware: {firmware_path}")
            self._create_flash_image_from_firmware(
                firmware_path, flash_image_path, flash_size
            )
        else:
            # Build directory path
            flash_image_path = firmware_path / "flash.bin"
            print(
                f"Creating combined flash image from build directory: {firmware_path}"
            )
            self._create_flash_image_from_build_dir(
                firmware_path, flash_image_path, flash_size
            )

        # Check for ROM file in multiple locations
        rom_paths = [
            Path(".cache") / "qemu" / "esp32-v3-rom.bin",
            Path(".cache") / "qemu" / "qemu" / "share" / "qemu" / "esp32-v3-rom.bin",
            Path(".cache") / "qemu" / "extracted" / "share" / "esp32-v3-rom.bin",
        ]

        rom_path = None
        for path in rom_paths:
            if path.exists():
                rom_path = path
                break

        # Ensure flash image path is absolute and exists
        flash_image_path = flash_image_path.resolve()
        if not flash_image_path.exists():
            raise FileNotFoundError(f"Flash image not found: {flash_image_path}")

        cmd = [
            str(self.qemu_binary),
            "-nographic",
            "-machine",
            "esp32",
            "-drive",
            f"file={str(flash_image_path)},if=mtd,format=raw",
            "-global",
            "driver=timer.esp32.timg,property=wdt_disable,value=true",
        ]

        # Set QEMU data directory if ROM files are available
        if rom_path and rom_path.exists():
            qemu_data_dir = rom_path.parent
            cmd.extend(["-L", str(qemu_data_dir)])
            print(f"Setting QEMU data directory: {qemu_data_dir}")

        # Note: ESP32 ROM is built into QEMU machine, no need to load external ROM
        # ESP32 QEMU machine has the ROM integrated, so we don't need to specify it
        if rom_path and rom_path.exists():
            print(
                f"ROM file available at: {rom_path} (but not needed for ESP32 machine)"
            )
        else:
            print("Note: ROM file not found, but ESP32 machine has integrated ROM")

        # Validate that no custom script arguments are accidentally included
        custom_args = ["--flash-size", "--timeout", "--interrupt-regex"]
        for arg in cmd:
            if any(custom_arg in str(arg) for custom_arg in custom_args):
                raise ValueError(
                    f"Custom script argument {arg} should not be passed to QEMU"
                )

        return cmd

    def _create_flash_image_from_firmware(
        self, firmware_bin: Path, flash_bin: Path, flash_size: int
    ):
        """Create flash image from a direct firmware.bin file."""
        if flash_bin.exists():
            print(f"Using existing flash image: {flash_bin}")
            return

        print(f"Creating flash image from firmware: {firmware_bin}")

        # Create empty flash image
        flash_size_bytes = flash_size * 1024 * 1024
        flash_bin.write_bytes(b"\xff" * flash_size_bytes)

        try:
            esptool_cmd = self._find_esptool()
            if esptool_cmd:
                # Use esptool to create proper flash image
                self._merge_with_esptool_firmware(esptool_cmd, firmware_bin, flash_bin)
            else:
                # Simple manual merge - just write firmware at 0x10000
                self._merge_manually_firmware(firmware_bin, flash_bin)
        except Exception as e:
            print(f"Warning: Could not create proper flash image: {e}")

    def _create_flash_image_from_build_dir(
        self, build_folder: Path, flash_bin: Path, flash_size: int
    ):
        """Create combined flash image from ESP32 build artifacts in a directory."""
        if flash_bin.exists():
            print(f"Using existing flash image: {flash_bin}")
            return

        print("Creating combined flash image from build directory...")

        # Create empty flash image
        flash_size_bytes = flash_size * 1024 * 1024
        flash_bin.write_bytes(b"\xff" * flash_size_bytes)

        try:
            esptool_cmd = self._find_esptool()
            if esptool_cmd:
                self._merge_with_esptool(esptool_cmd, build_folder, flash_bin)
            else:
                self._merge_manually(build_folder, flash_bin)
        except Exception as e:
            print(f"Warning: Could not create proper flash image: {e}")

    def _find_esptool(self) -> Optional[str]:
        """Find esptool.py executable."""
        # Try direct esptool.py
        if find_executable("esptool.py"):
            return "esptool.py"

        # Try python -m esptool
        try:
            result = subprocess.run(
                [sys.executable, "-m", "esptool", "--help"],
                capture_output=True,
                timeout=5,
            )
            if result.returncode == 0:
                return f"{sys.executable} -m esptool"
        except (subprocess.SubprocessError, subprocess.TimeoutExpired):
            pass

        return None

    def _merge_with_esptool(
        self, esptool_cmd: str, build_folder: Path, flash_bin: Path
    ):
        """Merge using esptool.py merge_bin command."""
        cmd = esptool_cmd.split() + [
            "--chip",
            "esp32",  # Use esp32 chip type for esp32dev machine
            "merge-bin",  # Updated command name
            "-o",
            str(flash_bin),
            "--flash-mode",  # Updated option name
            "dio",
            "--flash-freq",  # Updated option name
            "40m",  # Standard frequency for esp32
            "--flash-size",  # Updated option name
            "4MB",
        ]

        # Add binaries at their offsets - check if we're in the actual build directory
        if (build_folder / "firmware.bin").exists():
            firmware_path = build_folder / "firmware.bin"
        else:
            firmware_path = (
                build_folder / ".pio" / "build" / "esp32dev" / "firmware.bin"
            )

        if not firmware_path.exists():
            print(
                f"FATAL: ESP32dev firmware not found at {firmware_path}",
                file=sys.stderr,
            )
            print("Please build for esp32dev target first.", file=sys.stderr)
            sys.exit(1)

        binaries = [
            ("0x1000", build_folder / "bootloader.bin"),  # ESP32 bootloader offset
            ("0x8000", build_folder / "partitions.bin"),
            ("0x10000", firmware_path),
        ]

        for offset, bin_path in binaries:
            if bin_path.exists():
                cmd.extend([offset, str(bin_path)])

        # Add padding to 4MB at the end
        cmd.extend(["--pad-to-size", "4MB"])

        print(f"Running esptool command: {' '.join(cmd)}")
        esptool_proc = RunningProcess(cmd, timeout=60, auto_run=True)

        # Stream esptool output and check for errors
        has_errors = False
        with esptool_proc.line_iter(timeout=None) as it:
            for line in it:
                print(f"[esptool] {line}")
                # Check for common esptool error patterns
                if any(
                    error_pattern in line.lower()
                    for error_pattern in ["error", "failed", "exception"]
                ):
                    has_errors = True

        returncode = esptool_proc.wait()
        if returncode != 0:
            raise subprocess.CalledProcessError(returncode, cmd)
        if has_errors:
            print("Warning: Errors detected in esptool output")
        print("Flash image created with esptool")

    def _merge_with_esptool_firmware(
        self, esptool_cmd: str, firmware_bin: Path, flash_bin: Path
    ):
        """Merge firmware.bin using esptool with minimal bootloader."""
        cmd = esptool_cmd.split() + [
            "--chip",
            "esp32",
            "merge-bin",  # Updated command name
            "-o",
            str(flash_bin),
            "--flash-mode",  # Updated option name
            "dio",
            "--flash-freq",  # Updated option name
            "40m",
            "--flash-size",  # Updated option name
            "4MB",
            "0x10000",  # Application offset
            str(firmware_bin),
        ]

        # Note: Without bootloader and partition table, QEMU may not boot properly
        # but we'll try with just the app for now
        print(f"Running esptool firmware command: {' '.join(cmd)}")
        esptool_proc = RunningProcess(cmd, timeout=60, auto_run=True)

        # Stream esptool output and check for errors
        has_errors = False
        with esptool_proc.line_iter(timeout=None) as it:
            for line in it:
                print(f"[esptool] {line}")
                # Check for common esptool error patterns
                if any(
                    error_pattern in line.lower()
                    for error_pattern in ["error", "failed", "exception"]
                ):
                    has_errors = True

        returncode = esptool_proc.wait()
        if returncode != 0:
            raise subprocess.CalledProcessError(returncode, cmd)
        if has_errors:
            print("Warning: Errors detected in esptool output")
        print("Flash image created with esptool (firmware only)")

    def _merge_manually_firmware(self, firmware_bin: Path, flash_bin: Path):
        """Manually write firmware.bin to flash image."""
        print("WARNING: Manually merging firmware (esptool not found)")
        print("Note: Without bootloader, QEMU may not boot properly")

        with open(flash_bin, "r+b") as flash:
            # Write firmware at application offset
            flash.seek(0x10000)
            flash.write(firmware_bin.read_bytes())
        print(f"Firmware written to flash image at offset 0x10000")

    def _merge_manually(self, build_folder: Path, flash_bin: Path):
        """Manually merge binary files at correct offsets."""
        print("WARNING: Manually merging binaries (esptool not found)")

        # Find firmware binary - check if we're in the actual build directory
        if (build_folder / "firmware.bin").exists():
            firmware_path = build_folder / "firmware.bin"
        else:
            firmware_path = (
                build_folder / ".pio" / "build" / "esp32dev" / "firmware.bin"
            )

        if not firmware_path.exists():
            print(
                f"FATAL: ESP32dev firmware not found at {firmware_path}",
                file=sys.stderr,
            )
            print("Please build for esp32dev target first.", file=sys.stderr)
            sys.exit(1)

        binaries = [
            ("bootloader.bin", 0x1000),  # ESP32 bootloader offset
            ("partitions.bin", 0x8000),
            (firmware_path, 0x10000),
        ]

        with open(flash_bin, "r+b") as flash:
            for filename, offset in binaries:
                bin_path = build_folder / filename
                if bin_path.exists():
                    flash.seek(offset)
                    flash.write(bin_path.read_bytes())

    def run(
        self,
        firmware_path: Path,
        timeout: int = 30,
        interrupt_regex: Optional[str] = None,
        flash_size: int = 4,
    ) -> int:
        """Run ESP32 firmware in QEMU.

        Args:
            firmware_path: Path to firmware.bin file or build directory
            timeout: Timeout in seconds
            interrupt_regex: Regex pattern to interrupt execution
            flash_size: Flash size in MB
        """
        print("=== Running ESP32 firmware in QEMU ===")
        print(f"Firmware path: {firmware_path}")
        print(f"Timeout: {timeout}s")
        print(f"Flash size: {flash_size}MB")
        if interrupt_regex:
            print(f"Interrupt pattern: {interrupt_regex}")

        if not firmware_path.exists():
            print(f"ERROR: Path does not exist: {firmware_path}", file=sys.stderr)
            return 1

        try:
            cmd = self.build_qemu_command(firmware_path, flash_size)
            print(f"QEMU command: {' '.join(cmd)}")
        except Exception as e:
            print(f"ERROR: Failed to build QEMU command: {e}", file=sys.stderr)
            return 1

        try:
            qemu_proc = RunningProcess(
                cmd,
                timeout=timeout,
                auto_run=True,
            )

            self.interrupt_met = False
            interrupt_pattern = re.compile(interrupt_regex) if interrupt_regex else None
            self.output_lines = []

            try:
                with qemu_proc.line_iter(timeout=timeout) as it:
                    for line in it:
                        print(line, flush=True)
                        self.output_lines.append(line)
                        if interrupt_pattern and interrupt_pattern.search(line):
                            print(f"SUCCESS: Interrupt condition met: {line}")
                            self.interrupt_met = True
                            time.sleep(2)
            except TimeoutError:
                print(
                    f"ERROR: QEMU timed out after {timeout} seconds of no output.",
                    file=sys.stderr,
                )
                qemu_proc.kill()
                return 1

            return_code = qemu_proc.wait()

            log_file = Path("qemu_output.log")
            log_file.write_text("\n".join(self.output_lines))
            print(f"QEMU output saved to: {log_file}")
            print(f"Total output lines: {len(self.output_lines)}")

            if self.interrupt_met:
                return 0

            return return_code

        except Exception as e:
            print(f"ERROR: Error running QEMU: {e}", file=sys.stderr)
            return 1


def main():
    parser = argparse.ArgumentParser(description="Run ESP32 firmware in QEMU")
    parser.add_argument(
        "firmware_path",
        type=Path,
        help="Path to firmware.bin file or build directory containing firmware files",
    )
    parser.add_argument(
        "--flash-size", type=int, default=4, help="Flash size in MB (default: 4)"
    )
    parser.add_argument(
        "--timeout", type=int, default=30, help="Timeout in seconds (default: 30)"
    )
    parser.add_argument(
        "--interrupt-regex", type=str, help="Regex pattern to interrupt execution"
    )

    args = parser.parse_args()

    # Validate arguments to prevent issues
    if args.flash_size <= 0:
        print(
            f"ERROR: Invalid flash size: {args.flash_size}. Must be positive.",
            file=sys.stderr,
        )
        return 1

    if args.timeout <= 0:
        print(
            f"ERROR: Invalid timeout: {args.timeout}. Must be positive.",
            file=sys.stderr,
        )
        return 1

    # Ensure we're not accidentally being called as a wrapper for qemu-system-xtensa
    if len(sys.argv) > 1 and any("qemu-system-xtensa" in arg for arg in sys.argv):
        print(
            "ERROR: This script should not be used as a wrapper for qemu-system-xtensa",
            file=sys.stderr,
        )
        print(
            "Use this script directly: python qemu-esp32.py <build_folder> [options]",
            file=sys.stderr,
        )
        return 1

    # Check if QEMU is available
    if not find_qemu_binary():
        show_installation_help()
        return 1

    runner = QEMURunner()
    return runner.run(
        firmware_path=args.firmware_path,
        timeout=args.timeout,
        interrupt_regex=args.interrupt_regex,
        flash_size=args.flash_size,
    )


if __name__ == "__main__":
    sys.exit(main())
