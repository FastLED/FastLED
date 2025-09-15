#!/usr/bin/env python3
"""
ESP32 QEMU Installation Script

Installs ESP32-compatible QEMU emulator using ESP-IDF tools.
Cross-platform support for Linux, macOS, and Windows.

WARNING: Never use sys.stdout.flush() in this file!
It causes blocking issues on Windows that hang QEMU processes.
Python's default buffering behavior works correctly across platforms.
"""

import os
import platform
import shutil
import subprocess
import sys
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


def try_direct_download_install():
    """Download and install QEMU directly from the official source."""
    try:
        print("\n--- Direct Download Installation ---")
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
    """Try installing QEMU using direct download for Windows."""
    print("Attempting automatic QEMU installation on Windows...")

    # Use direct download as the primary method
    print("Using direct download from official source...")
    if try_direct_download_install():
        print("Direct download installation completed, verifying...")
        # Verify installation worked
        if find_qemu_binary():
            print("SUCCESS: QEMU installed via direct download")
            return True
        else:
            print("WARNING: Direct download reported success but QEMU binary not found")
    else:
        print("Direct download installation failed")

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
        print("Option 1: Direct Download (Recommended)")
        print(
            "  Download QEMU from: https://qemu.weilnetz.de/w64/2024/qemu-w64-setup-20241220.exe"
        )
        print("  Run the installer and ensure qemu-system-xtensa.exe is in your PATH")
        print()
        print("Option 2: ESP-IDF (includes QEMU)")
        print(
            "  Download ESP-IDF from: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html"
        )
        print()
        print("Option 3: Manual Installation")
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
    binary_name = (
        "qemu-system-xtensa.exe" if system == "windows" else "qemu-system-xtensa"
    )
    print(f"IMMEDIATE DEBUG: looking for binary: {binary_name}")

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

    for base_path in esp_paths:
        print(f"Searching in: {base_path}")
        try:
            if base_path.exists():
                print(f"  Directory exists, searching for {binary_name}...")
                # Use a more targeted search to avoid hanging on large directories
                print(f"  Starting recursive search in {base_path}...")
                for qemu_path in base_path.rglob(binary_name):
                    print(f"  Checking: {qemu_path}")
                    if qemu_path.is_file():
                        print(f"Found QEMU binary at: {qemu_path}")
                        return qemu_path
                print(f"  No {binary_name} found in {base_path}")
            else:
                print(f"  Directory does not exist")
        except Exception as e:
            print(f"  Error searching {base_path}: {e}")

    # Check system PATH
    print("Checking system PATH...")
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
        print("PATH check timed out after 10 seconds")
    except (subprocess.SubprocessError, FileNotFoundError) as e:
        print(f"Error checking PATH: {e}")

    print("ERROR: Could not find QEMU binary")
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
