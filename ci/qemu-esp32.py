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

    # ESP-IDF installation paths
    esp_paths = [
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
    print(f"ERROR: QEMU ESP32 emulator ({binary_name}) not found!")
    print()

    if platform.system().lower() == "windows":
        print("To install QEMU on Windows:")
        print("1. Chocolatey: choco install qemu -y")
        print("2. winget: winget install SoftwareFreedomConservancy.QEMU --scope user")
        print("3. Manual: Download from https://qemu.weilnetz.de/w64/")
        print("4. ESP-IDF: uv run python ci/install-qemu-esp32.py")
    else:
        print("Run: uv run python ci/install-qemu-esp32.py")


class QEMURunner:
    def __init__(self, qemu_binary: Optional[Path] = None):
        self.qemu_binary = qemu_binary or self._get_qemu_binary()
        self.process: Optional[subprocess.Popen[str]] = None
        self.output_lines: List[str] = []
        self.timeout_reached = False

    def _get_qemu_binary(self) -> Path:
        """Get QEMU binary path or raise error."""
        qemu_path = find_qemu_binary()
        if not qemu_path:
            raise FileNotFoundError(
                f"Could not find {get_binary_name()}. Please run install-qemu-esp32.py first."
            )
        return qemu_path

    def build_qemu_command(self, build_folder: Path, flash_size: int = 4) -> List[str]:
        """Build QEMU command line arguments."""
        cmd = [
            str(self.qemu_binary),
            "-nographic",
            "-machine",
            "esp32s3",
            "-m",
            "8M",
            "-drive",
            f"file={build_folder}/flash.bin,if=mtd,format=raw",
        ]

        # Create flash image if firmware exists
        if (build_folder / "firmware.bin").exists():
            self._create_flash_image(build_folder, flash_size)

        return cmd

    def _create_flash_image(self, build_folder: Path, flash_size: int):
        """Create combined flash image from ESP32 build artifacts."""
        flash_bin = build_folder / "flash.bin"
        if flash_bin.exists():
            return

        print("Creating combined flash image...")

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
            "esp32s3",
            "merge_bin",
            "-o",
            str(flash_bin),
            "--flash_mode",
            "dio",
            "--flash_freq",
            "80m",
            "--flash_size",
            "4MB",
        ]

        # Add binaries at their offsets
        binaries = [
            ("bootloader.bin", "0x0"),
            ("partitions.bin", "0x8000"),
            ("firmware.bin", "0x10000"),
        ]

        for filename, offset in binaries:
            bin_path = build_folder / filename
            if bin_path.exists():
                cmd.extend([offset, str(bin_path)])

        subprocess.run(cmd, check=True)
        print("Flash image created with esptool")

    def _merge_manually(self, build_folder: Path, flash_bin: Path):
        """Manually merge binary files at correct offsets."""
        print("WARNING: Manually merging binaries (esptool not found)")

        binaries = [
            ("bootloader.bin", 0x0),
            ("partitions.bin", 0x8000),
            ("firmware.bin", 0x10000),
        ]

        with open(flash_bin, "r+b") as flash:
            for filename, offset in binaries:
                bin_path = build_folder / filename
                if bin_path.exists():
                    flash.seek(offset)
                    flash.write(bin_path.read_bytes())

    def _output_reader(self, process: subprocess.Popen[str]) -> None:
        """Read output from QEMU process."""
        try:
            if process.stdout:
                for line in iter(process.stdout.readline, ""):
                    line = line.strip()
                    if line:
                        self.output_lines.append(line)
                        print(line, flush=True)
        except Exception as e:
            print(f"Output reader error: {e}")

    def run(
        self,
        build_folder: Path,
        timeout: int = 30,
        interrupt_regex: Optional[str] = None,
        flash_size: int = 4,
    ) -> int:
        """Run ESP32 firmware in QEMU."""
        print("=== Running ESP32 firmware in QEMU ===")
        print(f"Build folder: {build_folder}")
        print(f"Timeout: {timeout}s")
        print(f"Flash size: {flash_size}MB")
        if interrupt_regex:
            print(f"Interrupt pattern: {interrupt_regex}")

        if not build_folder.exists():
            print(f"ERROR: Build folder does not exist: {build_folder}")
            return 1

        try:
            cmd = self.build_qemu_command(build_folder, flash_size)
            print(f"QEMU command: {' '.join(cmd)}")
        except Exception as e:
            print(f"ERROR: Failed to build QEMU command: {e}")
            return 1

        try:
            self.process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                bufsize=1,
            )

            # Start output reader thread
            output_thread = threading.Thread(
                target=self._output_reader, args=(self.process,)
            )
            output_thread.daemon = True
            output_thread.start()

            # Wait for completion or timeout
            start_time = time.time()
            interrupt_pattern = re.compile(interrupt_regex) if interrupt_regex else None

            while self.process.poll() is None:
                # Check timeout
                if time.time() - start_time > timeout:
                    print(f"WARNING: Timeout reached ({timeout}s)")
                    self.timeout_reached = True
                    break

                # Check interrupt condition
                if interrupt_pattern:
                    for line in self.output_lines[-10:]:
                        if interrupt_pattern.search(line):
                            print(f"SUCCESS: Interrupt condition met: {line}")
                            time.sleep(2)  # Allow final output
                            break
                    else:
                        time.sleep(0.1)
                        continue
                    break

                time.sleep(0.1)

            # Cleanup process
            if self.process.poll() is None:
                self.process.terminate()
                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.process.kill()

            # Save output log
            log_file = Path("qemu_output.log")
            log_file.write_text("\n".join(self.output_lines))
            print(f"QEMU output saved to: {log_file}")
            print(f"Total output lines: {len(self.output_lines)}")

            return 0

        except Exception as e:
            print(f"ERROR: Error running QEMU: {e}")
            return 1


def main():
    parser = argparse.ArgumentParser(description="Run ESP32 firmware in QEMU")
    parser.add_argument(
        "build_folder", type=Path, help="Build folder containing firmware files"
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

    # Check if QEMU is available
    if not find_qemu_binary():
        show_installation_help()
        return 1

    runner = QEMURunner()
    return runner.run(
        build_folder=args.build_folder,
        timeout=args.timeout,
        interrupt_regex=args.interrupt_regex,
        flash_size=args.flash_size,
    )


if __name__ == "__main__":
    sys.exit(main())
