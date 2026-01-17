from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
AVR8JS Docker Runner for FastLED

Runs Arduino firmware in avr8js simulator via Docker container.
Compatible with Windows (MSYS2/Git Bash) and Unix systems.
"""

import platform
import subprocess
import sys
from pathlib import Path
from typing import Optional


class DockerAVR8jsRunner:
    """Run AVR firmware in avr8js Docker container"""

    def __init__(
        self,
        docker_image: str = "niteris/fastled-avr8js:latest",
    ):
        self.docker_image = docker_image

    def _convert_to_docker_volume_path(self, path: Path) -> str:
        """
        Convert Windows path to Docker-compatible volume mount path.

        Windows: C:\\Users\\... -> /c/Users/...
        Unix: /home/... -> /home/...
        """
        if platform.system() == "Windows":
            # Convert Windows path to MSYS2/Docker format
            # C:\Users\... -> /c/Users/...
            path_str = str(path).replace("\\", "/")
            if len(path_str) >= 2 and path_str[1] == ":":
                drive = path_str[0].lower()
                rest = path_str[2:]
                return f"/{drive}{rest}"
            return path_str
        else:
            return str(path)

    def check_docker_available(self) -> bool:
        """Check if Docker is available and running"""
        try:
            result = subprocess.run(
                ["docker", "version"], capture_output=True, timeout=5
            )
            return result.returncode == 0
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return False

    def check_image_exists(self, image_name: str) -> bool:
        """Check if Docker image exists locally"""
        try:
            result = subprocess.run(
                ["docker", "images", "-q", image_name],
                capture_output=True,
                text=True,
                timeout=10,
            )
            return bool(result.stdout.strip())
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return False

    def pull_image(self) -> None:
        """Pull Docker image from registry"""
        subprocess.run(["docker", "pull", self.docker_image], check=True)

    def run(
        self,
        elf_path: Path,
        mcu: str,
        frequency: int,
        timeout: int = 30,
        output_file: Optional[str] = None,
    ) -> int:
        """
        Run firmware in Docker container

        Args:
            elf_path: Path to the firmware ELF file
            mcu: MCU type (e.g., "atmega328p")
            frequency: MCU frequency in Hz
            timeout: Execution timeout in seconds
            output_file: Optional file to write output to

        Returns:
            Exit code (0 for success)
        """
        elf_path = Path(elf_path).resolve()

        if not elf_path.exists():
            raise FileNotFoundError(f"Firmware not found: {elf_path}")

        # avr8js requires HEX file, not ELF
        # PlatformIO generates both .elf and .hex in the same directory
        hex_path = elf_path.with_suffix(".hex")
        if not hex_path.exists():
            raise FileNotFoundError(f"HEX file not found: {hex_path}")

        print("Preparing Docker execution environment:")
        print("  Firmware format conversion: ELF → HEX (required by avr8js)")
        print(f"    ELF file: {elf_path}")
        print(f"    HEX file: {hex_path}")
        print()

        # Prepare Docker command with proper path handling
        firmware_dir = hex_path.parent
        firmware_name = hex_path.name

        # Convert to Docker-compatible path for volume mounting
        docker_firmware_dir = self._convert_to_docker_volume_path(firmware_dir)

        print("  Docker volume mounting:")
        print(f"    Host directory: {firmware_dir}")
        print(f"    Docker path: {docker_firmware_dir}")
        print("    Container mount: /firmware (read-only)")
        print()

        docker_cmd: list[str] = [
            "docker",
            "run",
            "--rm",  # Remove container after run
            "-v",
            f"{docker_firmware_dir}:/firmware:ro",  # Mount firmware directory read-only
            self.docker_image,
            f"/firmware/{firmware_name}",
            str(timeout),
        ]

        print("  Docker execution:")
        print(f"    Image: {self.docker_image}")
        print(f"    Target MCU: {mcu} @ {frequency}Hz")
        print(f"    Firmware (in container): /firmware/{firmware_name}")
        print(f"    Timeout: {timeout}s")
        print()
        print(f"  Docker command: {' '.join(docker_cmd)}")
        print()
        print("Starting avr8js emulator...")
        print(f"{'-' * 70}")

        try:
            # Run Docker container and capture output
            result = subprocess.run(
                docker_cmd,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=timeout + 10,  # Add buffer to Docker timeout
            )

            # Combine stdout and stderr
            output = result.stdout or ""
            if result.stderr:
                output += "\n" + result.stderr

            print(f"{'-' * 70}")
            print("Emulator output:")
            print(f"{'-' * 70}")

            # Print output to console
            if output:
                print(output)
            else:
                print("(no output)")

            print(f"{'-' * 70}")

            # Write to output file if specified
            if output_file and output:
                output_path = Path(output_file)
                output_path.write_text(output, encoding="utf-8")
                print(f"\nOutput saved to: {output_path.absolute()}")

            # Print result summary
            if result.returncode == 0:
                print("✅ Emulation completed successfully (exit code: 0)")
            else:
                print(f"❌ Emulation failed (exit code: {result.returncode})")

            return result.returncode

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except subprocess.TimeoutExpired:
            print(f"\n❌ Docker timeout after {timeout}s")
            return 1
        except Exception as e:
            print(f"\n❌ Docker error: {e}")
            return 1

    def ensure_image_available(self) -> bool:
        """Check if Docker image exists, build if needed"""
        try:
            check_cmd = ["docker", "images", "-q", self.docker_image]
            result = subprocess.run(check_cmd, capture_output=True, text=True)

            if not result.stdout.strip():
                print(f"Docker image {self.docker_image} not found")
                print("Building image...")
                return self.build_image()

            return True
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"❌ Error checking Docker image: {e}")
            return False

    def build_image(self) -> bool:
        """Build Docker image from Dockerfile"""
        try:
            dockerfile_path = Path(__file__).parent / "Dockerfile.avr8js"

            if not dockerfile_path.exists():
                print(f"❌ Dockerfile not found: {dockerfile_path}")
                return False

            build_cmd = [
                "docker",
                "build",
                "-f",
                str(dockerfile_path),
                "-t",
                self.docker_image,
                str(dockerfile_path.parent),
            ]

            print(f"Building Docker image: {self.docker_image}")
            print(f"Dockerfile: {dockerfile_path}")
            print()

            result = subprocess.run(build_cmd)
            return result.returncode == 0
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"❌ Error building Docker image: {e}")
            return False


def main() -> None:
    """CLI interface for testing"""
    if len(sys.argv) < 2:
        print("Usage: python avr8js_docker.py <firmware.elf> [timeout]")
        print()
        print("Example:")
        print("  python avr8js_docker.py .build/pio/uno/.pio/build/uno/firmware.elf 30")
        sys.exit(1)

    firmware_path = Path(sys.argv[1])
    timeout = int(sys.argv[2]) if len(sys.argv) > 2 else 30

    try:
        runner = DockerAVR8jsRunner()

        # Ensure Docker is available
        if not runner.check_docker_available():
            print("❌ Docker is not available")
            sys.exit(1)

        # Ensure image exists
        if not runner.ensure_image_available():
            print("❌ Failed to prepare Docker image")
            sys.exit(1)

        # Run tests
        returncode = runner.run(
            elf_path=firmware_path,
            mcu="atmega328p",
            frequency=16000000,
            timeout=timeout,
        )

        sys.exit(returncode)

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        print("\n⚠️  Interrupted by user")
        sys.exit(130)
    except Exception as e:
        print(f"❌ Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
