#!/usr/bin/env python3
"""
ESP32 QEMU Runner

Runs ESP32 firmware in QEMU with logging and validation.
Designed to replace the tobozo/esp32-qemu-sim GitHub Action.
"""

import argparse
import os
import re
import signal
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import List, Optional, Tuple


class QEMURunner:
    def __init__(self, qemu_binary: Optional[Path] = None):
        self.qemu_binary = qemu_binary or self._find_qemu_binary()
        self.process = None
        self.output_lines: List[str] = []
        self.timeout_reached = False

    def _find_qemu_binary(self) -> Path:
        """Find the QEMU ESP32 binary."""
        import platform

        system = platform.system().lower()
        binary_name = (
            "qemu-system-xtensa.exe" if system == "windows" else "qemu-system-xtensa"
        )

        # Check if installed via ESP-IDF tools
        esp_paths = [
            Path.home() / ".espressif" / "tools" / "qemu-xtensa",
            Path.home() / ".espressif" / "python_env",
        ]

        for base_path in esp_paths:
            if base_path.exists():
                for qemu_path in base_path.rglob(binary_name):
                    if qemu_path.is_file():
                        return qemu_path

        # Try system PATH
        try:
            result = subprocess.run(
                ["which", binary_name], capture_output=True, text=True
            )
            if result.returncode == 0:
                return Path(result.stdout.strip())
        except:
            pass

        raise FileNotFoundError(
            f"Could not find {binary_name}. Please run install-qemu-esp32.py first."
        )

    def build_qemu_command(self, build_folder: Path, flash_size: int = 4) -> List[str]:
        """Build the QEMU command line arguments."""
        cmd = [str(self.qemu_binary)]

        # Basic ESP32-S3 machine configuration
        cmd.extend(
            [
                "-nographic",  # No GUI
                "-machine",
                "esp32s3",  # ESP32-S3 machine
                "-m",
                "8M",  # 8MB RAM
            ]
        )

        # Flash configuration
        flash_size_mb = f"{flash_size}MB"
        cmd.extend(["-drive", f"file={build_folder}/flash.bin,if=mtd,format=raw"])

        # Look for firmware files in build folder
        firmware_bin = build_folder / "firmware.bin"
        bootloader_bin = build_folder / "bootloader.bin"
        partitions_bin = build_folder / "partitions.bin"

        if firmware_bin.exists():
            # Create combined flash image if needed
            self._create_flash_image(build_folder, flash_size)

        return cmd

    def _create_flash_image(self, build_folder: Path, flash_size: int):
        """Create a combined flash image from ESP32 build artifacts."""
        flash_bin = build_folder / "flash.bin"

        if flash_bin.exists():
            return  # Already exists

        print("Creating combined flash image...")

        # Create empty flash image
        flash_size_bytes = flash_size * 1024 * 1024
        with open(flash_bin, "wb") as f:
            f.write(b"\xff" * flash_size_bytes)

        # Use esptool.py to merge if available
        try:
            # Look for esptool in the system or ESP-IDF
            esptool_cmd = self._find_esptool()
            if esptool_cmd:
                self._merge_with_esptool(esptool_cmd, build_folder, flash_bin)
            else:
                self._merge_manually(build_folder, flash_bin)

        except Exception as e:
            print(f"Warning: Could not create proper flash image: {e}")

    def _find_esptool(self) -> Optional[str]:
        """Find esptool.py executable."""
        try:
            # Try direct esptool.py
            result = subprocess.run(["esptool.py", "--help"], capture_output=True)
            if result.returncode == 0:
                return "esptool.py"
        except:
            pass

        try:
            # Try python -m esptool
            result = subprocess.run(
                [sys.executable, "-m", "esptool", "--help"], capture_output=True
            )
            if result.returncode == 0:
                return f"{sys.executable} -m esptool"
        except:
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

        # Add bootloader at 0x0
        bootloader = build_folder / "bootloader.bin"
        if bootloader.exists():
            cmd.extend(["0x0", str(bootloader)])

        # Add partition table at 0x8000
        partitions = build_folder / "partitions.bin"
        if partitions.exists():
            cmd.extend(["0x8000", str(partitions)])

        # Add firmware at 0x10000
        firmware = build_folder / "firmware.bin"
        if firmware.exists():
            cmd.extend(["0x10000", str(firmware)])

        subprocess.run(cmd, check=True)
        print("Flash image created with esptool")

    def _merge_manually(self, build_folder: Path, flash_bin: Path):
        """Manually merge binary files at correct offsets."""
        print("WARNING: Manually merging binaries (esptool not found)")

        with open(flash_bin, "r+b") as flash:
            # Bootloader at 0x0
            bootloader = build_folder / "bootloader.bin"
            if bootloader.exists():
                flash.seek(0x0)
                flash.write(bootloader.read_bytes())

            # Partition table at 0x8000
            partitions = build_folder / "partitions.bin"
            if partitions.exists():
                flash.seek(0x8000)
                flash.write(partitions.read_bytes())

            # Firmware at 0x10000
            firmware = build_folder / "firmware.bin"
            if firmware.exists():
                flash.seek(0x10000)
                flash.write(firmware.read_bytes())

    def _output_reader(self, process: "subprocess.Popen[bytes]") -> None:
        """Read output from QEMU process."""
        try:
            while True:
                output: bytes = process.stdout.readline()
                if output == b"" and process.poll() is not None:
                    break
                if output:
                    line: str = output.decode("utf-8").strip()
                    self.output_lines.append(line)
                    print(line, flush=True)
        except Exception as e:
            print(f"Output reader error: {e}")

    def run(
        self,
        build_folder: Path,
        timeout: int = 30,
        interrupt_regex: Optional[str] = None,
    ) -> int:
        """Run ESP32 firmware in QEMU."""
        print(f"=== Running ESP32 firmware in QEMU ===")
        print(f"Build folder: {build_folder}")
        print(f"Timeout: {timeout}s")
        if interrupt_regex:
            print(f"Interrupt pattern: {interrupt_regex}")

        # Verify build folder exists
        if not build_folder.exists():
            print(f"ERROR: Build folder does not exist: {build_folder}")
            return 1

        # Build QEMU command
        try:
            cmd = self.build_qemu_command(build_folder)
            print(f"QEMU command: {' '.join(cmd)}")
        except Exception as e:
            print(f"ERROR: Failed to build QEMU command: {e}")
            return 1

        # Start QEMU process
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

            # Wait for timeout or interrupt condition
            start_time = time.time()
            interrupt_pattern = re.compile(interrupt_regex) if interrupt_regex else None

            while True:
                # Check if process ended
                if self.process.poll() is not None:
                    break

                # Check timeout
                if time.time() - start_time > timeout:
                    print(f"WARNING: Timeout reached ({timeout}s)")
                    self.timeout_reached = True
                    break

                # Check interrupt condition
                if interrupt_pattern:
                    for line in self.output_lines[-10:]:  # Check recent lines
                        if interrupt_pattern.search(line):
                            print(f"SUCCESS: Interrupt condition met: {line}")
                            time.sleep(2)  # Give a bit more time for final output
                            break
                    else:
                        time.sleep(0.1)
                        continue
                    break

                time.sleep(0.1)

            # Terminate QEMU
            if self.process.poll() is None:
                self.process.terminate()
                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.process.kill()

            # Save output log
            log_file = Path("qemu_output.log")
            with open(log_file, "w") as f:
                f.write("\n".join(self.output_lines))

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

    runner = QEMURunner()
    return runner.run(
        build_folder=args.build_folder,
        timeout=args.timeout,
        interrupt_regex=args.interrupt_regex,
    )


if __name__ == "__main__":
    sys.exit(main())
