#!/usr/bin/env python3
"""
ESP32 QEMU Installation Script

Installs ESP32-compatible QEMU emulator using ESP-IDF tools.
Cross-platform support for Linux, macOS, and Windows.

WARNING: Never use sys.stdout.flush() in this file!
It causes blocking issues on Windows that hang QEMU processes.
Python's default buffering behavior works correctly across platforms.
"""

import hashlib
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import threading
from pathlib import Path
from typing import Dict, List, Optional, Union

from download import download  # type: ignore

from ci.util.running_process import RunningProcess


def get_platform_info():
    """Get platform-specific information for QEMU installation."""
    print("IMMEDIATE DEBUG: get_platform_info() called")
    print("Detecting platform information...")
    system = platform.system().lower()
    print(f"IMMEDIATE DEBUG: platform.system() = {system}")
    machine = platform.machine().lower()
    print(f"IMMEDIATE DEBUG: platform.machine() = {machine}")
    print(f"System: {system}, Machine: {machine}")

    # Normalize machine architecture
    if machine in ["x86_64", "amd64"]:
        arch = "x86_64"
    elif machine in ["aarch64", "arm64"]:
        arch = "arm64"
    elif machine.startswith("arm"):
        arch = "arm"
    else:
        arch = machine

    return system, arch


def install_system_dependencies():
    """Install system dependencies required for QEMU."""
    system, _ = get_platform_info()

    print("Installing system dependencies...")

    if system == "linux":
        # Ubuntu/Debian dependencies
        deps = [
            "libgcrypt20",
            "libglib2.0-0",
            "libpixman-1-0",
            "libsdl2-2.0-0",
            "libslirp0",
        ]

        try:
            # Check if we can install packages (requires sudo)
            print("Updating package list...")
            process = RunningProcess(["sudo", "apt-get", "update"])
            result = process.wait(echo=True)
            if result == 0:
                print("Installing system dependencies:", " ".join(deps))
                process = RunningProcess(["sudo", "apt-get", "install", "-y"] + deps)
                result = process.wait(echo=True)
                if result != 0:
                    raise subprocess.CalledProcessError(
                        result if result is not None else 1,
                        "apt-get install",
                    )
                print("System dependencies installed")
            else:
                print("WARNING: Could not install system dependencies (sudo required)")
                print("Please install manually:", " ".join(deps))
        except subprocess.CalledProcessError:
            print("WARNING: Failed to install system dependencies")

    elif system == "darwin":
        # macOS - dependencies usually available via system
        print("macOS system dependencies should be available")

    elif system == "windows":
        # Windows - QEMU binaries are self-contained
        print("Windows - no additional dependencies required")


def download_esp_idf_tools():
    """Download and setup ESP-IDF tools for QEMU installation."""
    tools_dir = Path.home() / ".espressif"
    tools_dir.mkdir(exist_ok=True)

    # ESP-IDF tools script URL
    tools_url = (
        "https://raw.githubusercontent.com/espressif/esp-idf/master/tools/idf_tools.py"
    )
    tools_script = tools_dir / "idf_tools.py"

    if not tools_script.exists():
        print("Downloading ESP-IDF tools...")
        print(f"Source: {tools_url}")
        print(f"Destination: {tools_script}")
        download(tools_url, tools_script)
        print(f"Downloaded ESP-IDF tools to {tools_script}")
    else:
        print(f"ESP-IDF tools already exist at {tools_script}")

    return tools_script


def check_command_available(command: str) -> bool:
    """Check if a command is available in PATH."""
    print(f"Checking if {command} is available...")
    try:
        result = subprocess.run(
            [command, "--version"], capture_output=True, text=True, timeout=10
        )
        available = result.returncode == 0
        print(f"{command} {'found' if available else 'not found'}")
        return available
    except (subprocess.TimeoutExpired, FileNotFoundError, subprocess.SubprocessError):
        print(f"{command} not found or timed out")
        return False


def verify_file_hash(file_path: Path, expected_hash: str) -> bool:
    """Verify SHA256 hash of a downloaded file."""
    print(f"Verifying SHA256 hash of {file_path}...")
    try:
        with open(file_path, "rb") as f:
            file_hash = hashlib.sha256(f.read()).hexdigest()

        if file_hash == expected_hash:
            print(f"SHA256 verification passed: {file_hash}")
            return True
        else:
            print(f"SHA256 verification failed!")
            print(f"Expected: {expected_hash}")
            print(f"Got:      {file_hash}")
            return False
    except Exception as e:
        print(f"Error verifying hash: {e}")
        return False


def try_espressif_binary_install():
    """Download and install QEMU using Espressif official precompiled binaries."""
    try:
        print("\n--- Espressif Official Binary Installation ---")
        print("Downloading QEMU from Espressif official releases...")

        # Create local QEMU directory (project-local)
        qemu_dir = Path(".cache") / "qemu"
        qemu_dir.mkdir(parents=True, exist_ok=True)

        # Check if already installed
        qemu_xtensa_binary = qemu_dir / "qemu-system-xtensa.exe"
        qemu_riscv32_binary = qemu_dir / "qemu-system-riscv32.exe"

        print(f"Checking for existing QEMU at {qemu_xtensa_binary}...")
        if qemu_xtensa_binary.exists() and qemu_riscv32_binary.exists():
            print(
                f"QEMU already installed at {qemu_xtensa_binary} and {qemu_riscv32_binary}"
            )
            return True
        else:
            print("QEMU not found, proceeding with download...")

        # Espressif official QEMU binaries for Windows x64
        binaries = [
            {
                "name": "QEMU-Xtensa (ESP32/ESP32-S2/ESP32-S3)",
                "url": "https://github.com/espressif/qemu/releases/download/esp-develop-9.2.2-20250817/qemu-xtensa-softmmu-esp_develop_9.2.2_20250817-x86_64-w64-mingw32.tar.xz",
                "sha256": "ef550b912726997f3c1ff4a4fb13c1569e2b692efdc5c9f9c3c926a8f7c540fa",
                "binary_name": "qemu-system-xtensa.exe",
            },
            {
                "name": "QEMU-RISCV32 (ESP32-C3/ESP32-H2/ESP32-C2)",
                "url": "https://github.com/espressif/qemu/releases/download/esp-develop-9.2.2-20250817/qemu-riscv32-softmmu-esp_develop_9.2.2_20250817-x86_64-w64-mingw32.tar.xz",
                "sha256": "9474015f24d27acb7516955ec932e5307226bd9d6652cdc870793ed36010ab73",
                "binary_name": "qemu-system-riscv32.exe",
            },
        ]

        for binary_info in binaries:
            print(f"\nDownloading {binary_info['name']}...")
            print("This may take a few minutes depending on your internet connection.")

            archive_path = qemu_dir / f"{binary_info['binary_name']}.tar.xz"

            print(f"Downloading from: {binary_info['url']}")
            print(f"Saving to: {archive_path}")

            try:
                # Download the archive
                download(binary_info["url"], archive_path)
                print(f"Download completed: {archive_path}")

                # Verify SHA256 hash
                if not verify_file_hash(archive_path, binary_info["sha256"]):
                    print(f"Hash verification failed for {binary_info['name']}")
                    archive_path.unlink()  # Remove corrupted file
                    return False

                # Extract the archive
                print(f"Extracting {binary_info['name']}...")
                with tarfile.open(archive_path, "r:xz") as tar:
                    # List contents to understand structure
                    members = tar.getnames()
                    print(f"Archive contains {len(members)} files")

                    # Extract to temporary directory first
                    temp_extract_dir = qemu_dir / "temp_extract"
                    temp_extract_dir.mkdir(exist_ok=True)

                    tar.extractall(temp_extract_dir)
                    print(f"Extracted to temporary directory: {temp_extract_dir}")

                    # Find the binary in the extracted contents
                    found_binary = None
                    for root, dirs, files in os.walk(temp_extract_dir):
                        for file in files:
                            if file == binary_info["binary_name"]:
                                found_binary = Path(root) / file
                                break
                        if found_binary:
                            break

                    if found_binary:
                        target_binary = qemu_dir / binary_info["binary_name"]
                        print(
                            f"Copying {binary_info['binary_name']} to {target_binary}"
                        )
                        shutil.copy2(found_binary, target_binary)

                        # Make sure it's executable (though Windows doesn't use execute bit)
                        target_binary.chmod(0o755)

                        print(f"Successfully installed {binary_info['binary_name']}")
                    else:
                        print(
                            f"ERROR: Could not find {binary_info['binary_name']} in extracted archive"
                        )
                        return False

                    # Clean up temporary extraction directory
                    shutil.rmtree(temp_extract_dir)

                # Clean up downloaded archive
                archive_path.unlink()
                print(f"Cleaned up {archive_path}")

            except Exception as e:
                print(f"Error downloading/extracting {binary_info['name']}: {e}")
                # Clean up on error
                if archive_path.exists():
                    archive_path.unlink()
                return False

        print("Espressif binary installation completed successfully")
        return True

    except Exception as e:
        print(f"Espressif binary installation error: {e}")
        return False


def try_direct_download_install():
    """Download and install QEMU directly from the official source."""
    print("\n--- Legacy Installer Method (Requires Admin) ---")
    print("WARNING: This method requires administrator privileges and may fail")
    print("Consider using the Espressif binary method instead")

    try:
        print("Downloading QEMU directly from official source...")

        # Create local QEMU directory (project-local)
        qemu_dir = Path(".cache") / "qemu"
        qemu_dir.mkdir(parents=True, exist_ok=True)

        # Check if already installed
        qemu_binary = qemu_dir / "qemu-system-xtensa.exe"
        print(f"Checking for existing QEMU at {qemu_binary}...")
        if qemu_binary.exists():
            print(f"QEMU already installed at {qemu_binary}")
            return True
        else:
            print("QEMU not found, proceeding with download...")

        # Download QEMU from official source
        print("Downloading QEMU for Windows...")
        print("This may take several minutes depending on your internet connection.")

        # QEMU download URL from the official Windows builds
        qemu_url = "https://qemu.weilnetz.de/w64/2024/qemu-w64-setup-20241220.exe"
        installer_path = qemu_dir / "qemu-installer.exe"

        print(f"Downloading from: {qemu_url}")
        print(f"Saving to: {installer_path}")

        try:
            # Download the installer
            download(qemu_url, installer_path)
            print(f"Download completed: {installer_path}")

            # Run the installer in silent mode
            print("Running QEMU installer in silent mode...")
            print("Note: This may require administrator privileges")

            # Try different installation approaches
            install_attempts = [
                # Attempt 1: Install to custom directory (may require admin)
                [
                    str(installer_path),
                    "/VERYSILENT",  # Very silent install
                    "/SUPPRESSMSGBOXES",  # Suppress message boxes
                    f"/DIR={qemu_dir.absolute()}",  # Install directory
                ],
                # Attempt 2: Install to default location (may require admin)
                [
                    str(installer_path),
                    "/VERYSILENT",  # Very silent install
                    "/SUPPRESSMSGBOXES",  # Suppress message boxes
                ],
                # Attempt 3: Use basic silent install
                [
                    str(installer_path),
                    "/S",  # Silent install
                ],
            ]

            result = None
            process = None
            for i, install_cmd in enumerate(install_attempts, 1):
                print(
                    f"Installation attempt {i}/{len(install_attempts)}: {' '.join(install_cmd)}"
                )
                try:
                    process = RunningProcess(
                        install_cmd,
                        timeout=300,  # 5 minute timeout
                    )
                    result = process.wait(echo=True)

                    if result == 0:
                        print(f"Installation attempt {i} succeeded")
                        break
                    else:
                        print(
                            f"Installation attempt {i} failed with exit code: {result}"
                        )
                        if i < len(install_attempts):
                            print("Trying next installation method...")
                except Exception as e:
                    print(f"Installation attempt {i} failed with error: {e}")
                    if i < len(install_attempts):
                        print("Trying next installation method...")

            if result == 0:
                print("QEMU installer completed successfully")

                # Look for the installed qemu-system-xtensa.exe
                for qemu_path in qemu_dir.rglob("qemu-system-xtensa.exe"):
                    if qemu_path.is_file():
                        print(f"Found QEMU binary at: {qemu_path}")

                        # If it's not in the expected location, copy it there
                        if qemu_path != qemu_binary:
                            print(
                                f"Copying QEMU binary to standard location: {qemu_binary}"
                            )
                            shutil.copy2(qemu_path, qemu_binary)

                        # Clean up installer
                        if installer_path.exists():
                            installer_path.unlink()

                        print("Direct download installation completed successfully")
                        return True

                print("QEMU installer completed but qemu-system-xtensa.exe not found")
            else:
                print(f"QEMU installer failed with exit code: {result}")
                if process:
                    stderr_output = process.stdout
                    if stderr_output:
                        print(f"Error output: {stderr_output[:200]}")

        except Exception as download_error:
            print(f"Download/install error: {download_error}")

        return False

    except Exception as e:
        print(f"Direct download installation error: {e}")
        return False


def try_windows_installation():
    """Try installing QEMU using Espressif binaries for Windows."""
    print("Attempting automatic QEMU installation on Windows...")

    # Try Espressif official binaries first (recommended method)
    print("Using Espressif official binaries...")
    if try_espressif_binary_install():
        print("Espressif binary installation completed, verifying...")
        # Verify installation worked
        if find_qemu_binary():
            print("SUCCESS: QEMU installed via Espressif official binaries")
            return True
        else:
            print(
                "WARNING: Espressif binary installation reported success but QEMU binary not found"
            )
    else:
        print("Espressif binary installation failed")

    # Fall back to legacy installer method (requires admin privileges)
    print("Falling back to legacy installer method...")
    if try_direct_download_install():
        print("Legacy installer completed, verifying...")
        # Verify installation worked
        if find_qemu_binary():
            print("SUCCESS: QEMU installed via legacy installer")
            return True
        else:
            print(
                "WARNING: Legacy installer reported success but QEMU binary not found"
            )
    else:
        print("Legacy installer failed")

    print("Windows QEMU installation failed")
    return False


def install_qemu_esp32():
    """Install ESP32 QEMU using ESP-IDF tools."""
    print("\n=== Starting ESP32 QEMU Installation ===")
    system, arch = get_platform_info()

    print(f"\nChecking for ESP32 QEMU for {system}-{arch}...")

    # Check if already installed
    print("Searching for existing QEMU installation...")
    qemu_binary = find_qemu_binary()
    print(f"IMMEDIATE DEBUG: find_qemu_binary returned: {qemu_binary}")
    if qemu_binary:
        print("ESP32 QEMU already installed")
        return True
    else:
        print("No existing QEMU installation found")

    # Try automated installation
    print("Attempting automatic installation...")

    if system == "linux":
        # Check if we're in CI first for sudo operations
        if os.getenv("CI") == "true" or os.getenv("GITHUB_ACTIONS") == "true":
            try:
                # Try to install QEMU from package manager in CI
                print("Installing QEMU via apt-get...")
                print("Updating package list...")
                process = RunningProcess(["sudo", "apt-get", "update"])
                result = process.wait(echo=True)

                if result == 0:
                    print("Package list updated successfully")
                    print("Installing QEMU packages...")
                    print("Installing: qemu-system-misc qemu-system-xtensa")
                    process = RunningProcess(
                        [
                            "sudo",
                            "apt-get",
                            "install",
                            "-y",
                            "qemu-system-misc",
                            "qemu-system-xtensa",
                        ]
                    )
                    result = process.wait(echo=True)

                    if result == 0:
                        print("QEMU packages installed successfully via apt-get")
                        return True
                    else:
                        print("Failed to install QEMU packages via apt-get")
                        print("Return code:", result)

            except Exception as e:
                print(f"Automatic installation failed: {e}")
        else:
            print(
                "Local Linux environment - automatic installation requires sudo privileges"
            )

    elif system == "windows":
        print("Windows platform detected, using direct download...")
        # Try direct download installation (works in both CI and local environments)
        if try_windows_installation():
            print("Windows QEMU installation successful")
            return True
        else:
            print("Windows installation failed")

    elif system == "darwin":
        # Try macOS installation methods
        print("macOS platform detected")
        print("Attempting macOS installation...")
        print("Note: Homebrew installation could be added here in the future")
        # Could add homebrew installation here in the future

    # If not found, provide installation instructions
    print("ESP32 QEMU not found. Please install using one of these methods:")
    print()

    if system == "windows":
        print("Windows Installation Options:")
        print()
        print("Option 1: Espressif Official Binaries (Recommended)")
        print(
            "  This is what the automatic installer uses - no administrator privileges required"
        )
        print(
            "  Espressif QEMU Xtensa: https://github.com/espressif/qemu/releases/download/esp-develop-9.2.2-20250817/qemu-xtensa-softmmu-esp_develop_9.2.2_20250817-x86_64-w64-mingw32.tar.xz"
        )
        print(
            "  Espressif QEMU RISC-V32: https://github.com/espressif/qemu/releases/download/esp-develop-9.2.2-20250817/qemu-riscv32-softmmu-esp_develop_9.2.2_20250817-x86_64-w64-mingw32.tar.xz"
        )
        print("  Extract to .cache/qemu/ directory")
        print()
        print("Option 2: Official QEMU Installer (Requires Administrator)")
        print(
            "  Download QEMU from: https://qemu.weilnetz.de/w64/2024/qemu-w64-setup-20241220.exe"
        )
        print("  Run the installer and ensure qemu-system-xtensa.exe is in your PATH")
        print()
        print("Option 3: ESP-IDF (includes QEMU)")
        print(
            "  Download ESP-IDF from: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html"
        )
        print()
        print("Option 4: Manual Installation")
        print("  Download QEMU from: https://www.qemu.org/download/")
        print("  Make sure qemu-system-xtensa.exe is in your PATH")

    elif system == "linux":
        print("Option 1: Install via ESP-IDF")
        print("  git clone --recursive https://github.com/espressif/esp-idf.git")
        print("  cd esp-idf && ./install.sh esp32s3")
        print("  source ./export.sh")
        print()
        print("Option 2: Install QEMU from package manager")
        print("  sudo apt-get install qemu-system-misc qemu-system-xtensa")

    elif system == "darwin":
        print("Option 1: Install via ESP-IDF")
        print("  git clone --recursive https://github.com/espressif/esp-idf.git")
        print("  cd esp-idf && ./install.sh esp32s3")
        print("  source ./export.sh")
        print()
        print("Option 2: Install via Homebrew")
        print("  brew install qemu")

    print()
    print("For CI/GitHub Actions, QEMU will be cached automatically once installed.")
    return False


def find_qemu_binary() -> Optional[Path]:
    """Find the installed QEMU binary."""
    print("IMMEDIATE DEBUG: find_qemu_binary() called")
    system, _ = get_platform_info()
    print(f"IMMEDIATE DEBUG: got system: {system}")

    # Look for both Xtensa and RISC-V32 binaries
    binary_names = []
    if system == "windows":
        binary_names = ["qemu-system-xtensa.exe", "qemu-system-riscv32.exe"]
    else:
        binary_names = ["qemu-system-xtensa", "qemu-system-riscv32"]

    print(f"IMMEDIATE DEBUG: looking for binaries: {binary_names}")

    # ESP-IDF installation paths and portable installation
    esp_paths = [
        Path(".cache") / "qemu",  # Project-local portable installation directory
        Path.home()
        / ".cache"
        / "qemu",  # Legacy user cache location (for backward compatibility)
        Path.home() / ".espressif" / "tools" / "qemu-xtensa",
        Path.home() / ".espressif" / "python_env",
        Path("/opt/espressif/tools/qemu-xtensa"),
    ]

    # Search in ESP-IDF paths first
    for base_path in esp_paths:
        print(f"Searching in: {base_path}")
        try:
            if base_path.exists():
                print(f"  Directory exists, searching for QEMU binaries...")
                for binary_name in binary_names:
                    print(
                        f"  Starting recursive search for {binary_name} in {base_path}..."
                    )
                    for qemu_path in base_path.rglob(binary_name):
                        print(f"  Checking: {qemu_path}")
                        if qemu_path.is_file():
                            print(f"Found QEMU binary at: {qemu_path}")
                            return qemu_path
                print(f"  No QEMU binaries found in {base_path}")
            else:
                print(f"  Directory does not exist")
        except Exception as e:
            print(f"  Error searching {base_path}: {e}")

    # Check system PATH for each binary
    print("Checking system PATH...")
    for binary_name in binary_names:
        try:
            cmd = ["where" if system == "windows" else "which", binary_name]
            print(f"Running: {' '.join(cmd)}")
            print("About to call subprocess.run...")
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
            print("subprocess.run completed")
            if result.returncode == 0:
                # Take first line in case of multiple results (Windows)
                first_line = result.stdout.strip().split("\n")[0]
                qemu_path = Path(first_line)
                print(f"Found QEMU binary in PATH: {qemu_path}")
                return qemu_path
            else:
                print(f"Command returned code {result.returncode}")
        except subprocess.TimeoutExpired:
            print(f"PATH check for {binary_name} timed out after 10 seconds")
        except (subprocess.SubprocessError, FileNotFoundError) as e:
            print(f"Error checking PATH for {binary_name}: {e}")

    print("ERROR: Could not find any QEMU binary")
    return None


def create_qemu_wrapper():
    """Create a wrapper script for easy QEMU access."""
    # DISABLED: This function was overwriting the actual qemu-esp32.py runner script
    # The qemu-esp32.py script already exists and handles all the necessary
    # functionality for running ESP32 firmware in QEMU with proper argument parsing
    print("Skipping wrapper creation - using existing qemu-esp32.py runner script")
    return True


def main():
    """Main installation routine."""
    print("IMMEDIATE DEBUG: main() function called")
    print("=== ESP32 QEMU Installation ===")
    print("Step 1/5: Starting installation process...")

    # Show platform info
    print("Step 2/5: Detecting platform...")
    system, arch = get_platform_info()
    print(f"IMMEDIATE DEBUG: get_platform_info completed, got: {system}, {arch}")
    print(f"Platform: {system}-{arch}")

    # Install system dependencies
    print("Step 3/5: Installing system dependencies...")
    install_system_dependencies()
    print("IMMEDIATE DEBUG: install_system_dependencies completed")
    print("System dependencies step completed")

    # Install ESP32 QEMU
    print("Step 4/5: Installing ESP32 QEMU...")
    qemu_installed = install_qemu_esp32()
    print(
        f"IMMEDIATE DEBUG: install_qemu_esp32 completed with result: {qemu_installed}"
    )
    print(f"ESP32 QEMU installation step completed (success: {qemu_installed})")

    if qemu_installed:
        # Create wrapper script
        print("Step 5/5: Creating wrapper scripts...")
        if not create_qemu_wrapper():
            print("WARNING: Could not create wrapper script, but QEMU is available")
    else:
        print("QEMU installation required for ESP32 emulation")
        sys.exit(1)

    print("=== Installation Complete ===")
    print("ESP32 QEMU is ready to use!")

    # Test installation
    print("\n=== Verifying Installation ===")
    print("Searching for QEMU binary...")
    qemu_binary = find_qemu_binary()
    if qemu_binary:
        print(f"Testing QEMU binary: {qemu_binary}")
        try:
            print("Running version check...")
            result = subprocess.run(
                [str(qemu_binary), "--version"],
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode == 0:
                print(f"QEMU version: {result.stdout.strip()}")
                print("Installation verification successful!")
                sys.exit(0)  # Explicit success exit
            else:
                print("WARNING: QEMU installed but version check failed")
                print(f"Version check return code: {result.returncode}")
                sys.exit(1)
        except Exception as e:
            print(f"WARNING: Could not verify QEMU installation: {e}")
            sys.exit(1)
    else:
        print("ERROR: QEMU installation verification failed")
        sys.exit(1)


if __name__ == "__main__":
    main()
