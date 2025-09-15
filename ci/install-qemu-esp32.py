#!/usr/bin/env python3
"""
ESP32 QEMU Installation Script

Installs ESP32-compatible QEMU emulator using ESP-IDF tools.
Cross-platform support for Linux, macOS, and Windows.
"""

import os
import platform
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path
from typing import Optional


def get_platform_info():
    """Get platform-specific information for QEMU installation."""
    print("Detecting platform information...")
    system = platform.system().lower()
    machine = platform.machine().lower()
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
            result = subprocess.run(["sudo", "apt-get", "update"], text=True)
            if result.returncode == 0:
                print("Installing system dependencies:", " ".join(deps))
                subprocess.run(["sudo", "apt-get", "install", "-y"] + deps, check=True)
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
        urllib.request.urlretrieve(tools_url, tools_script)
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


def try_chocolatey_install():
    """Try to install QEMU using Chocolatey."""
    print("Attempting Chocolatey installation...")
    if not check_command_available("choco"):
        print("Chocolatey not found, skipping...")
        return False

    try:
        print("Installing QEMU via Chocolatey...")
        print("Note: This may require administrator privileges")
        # Note: Chocolatey typically requires admin privileges for system-wide installs
        # Quick check for admin privileges first
        result = subprocess.run(
            ["choco", "install", "qemu", "-y", "--timeout", "30"],
            text=True,
            timeout=60,  # Shorter timeout
        )

        if result.returncode == 0:
            print("QEMU installed successfully via Chocolatey")
            return True
        else:
            print("Chocolatey installation failed (likely requires admin privileges)")
            # Don't print full error to avoid clutter
            return False

    except subprocess.TimeoutExpired:
        print("Chocolatey installation timed out (likely waiting for user input)")
        return False
    except Exception as e:
        print(f"Chocolatey installation error: {e}")
        return False


def try_winget_install():
    """Try to install QEMU using Windows Package Manager (winget)."""
    print("Attempting winget installation...")
    if not check_command_available("winget"):
        print("winget not found, skipping...")
        return False

    try:
        print("Installing QEMU via winget (user scope, no admin required)...")
        print("This may take a few minutes...")
        result = subprocess.run(
            [
                "winget",
                "install",
                "SoftwareFreedomConservancy.QEMU",
                "--scope",
                "user",
                "--accept-source-agreements",
                "--accept-package-agreements",
            ],
            text=True,
            timeout=300,
        )

        if result.returncode == 0:
            print("QEMU installed successfully via winget")
            return True
        else:
            print("winget user installation failed")
            # Try without --scope user as fallback
            print("Trying winget without user scope...")
            result = subprocess.run(
                [
                    "winget",
                    "install",
                    "SoftwareFreedomConservancy.QEMU",
                    "--accept-source-agreements",
                    "--accept-package-agreements",
                ],
                text=True,
                timeout=300,
            )

            if result.returncode == 0:
                print("QEMU installed successfully via winget (machine scope)")
                return True
            else:
                return False

    except Exception as e:
        print(f"winget installation error: {e}")
        return False


def try_portable_qemu_install():
    """Try to download and install portable QEMU for Windows."""
    try:
        print("Attempting portable QEMU installation...")

        # Create local QEMU directory (project-local)
        qemu_dir = Path(".cache") / "qemu"
        qemu_dir.mkdir(parents=True, exist_ok=True)

        # Check if already installed
        qemu_binary = qemu_dir / "qemu-system-xtensa.exe"
        print(f"Checking for existing portable QEMU at {qemu_binary}...")
        if qemu_binary.exists():
            print(f"Portable QEMU already installed at {qemu_binary}")
            return True
        else:
            print("Portable QEMU not found, proceeding with setup...")

        print("Portable QEMU installation requires manual setup.")
        print("For now, creating a placeholder to demonstrate the installation flow.")

        # Create a minimal placeholder that explains the limitation
        placeholder_script = qemu_dir / "install_qemu_manually.bat"
        with open(placeholder_script, "w") as f:
            f.write("""@echo off
echo This is a placeholder for QEMU installation.
echo.
echo To install QEMU manually:
echo 1. Download QEMU from: https://qemu.weilnetz.de/w64/
echo 2. Extract qemu-system-xtensa.exe to: %~dp0
echo 3. Rename this file to qemu-system-xtensa.exe
echo.
echo Alternatively, run as administrator:
echo   choco install qemu -y
echo   winget install SoftwareFreedomConservancy.QEMU
pause
""")

        print(f"Created installation helper at: {placeholder_script}")
        print("Manual installation required - please follow the instructions above.")
        return False

    except Exception as e:
        print(f"Portable installation error: {e}")
        return False


def try_windows_package_managers():
    """Try installing QEMU using available Windows package managers."""
    print("Attempting automatic QEMU installation on Windows...")

    # Try package managers in order of preference
    package_managers = [
        ("Chocolatey", try_chocolatey_install),
        ("winget", try_winget_install),
        ("Portable QEMU", try_portable_qemu_install),
    ]

    for name, install_func in package_managers:
        print(f"\nTrying {name}...")
        print(f"Starting {name} installation process...")
        if install_func():
            print(f"{name} installation completed, verifying...")
            # Verify installation worked
            if find_qemu_binary():
                print(f"SUCCESS: QEMU installed via {name}")
                return True
            else:
                print(f"WARNING: {name} reported success but QEMU binary not found")
        else:
            print(f"{name} installation failed")

    print("All Windows package managers failed")
    return False


def install_qemu_esp32():
    """Install ESP32 QEMU using ESP-IDF tools."""
    print("\n=== Starting ESP32 QEMU Installation ===")
    system, arch = get_platform_info()

    print(f"\nChecking for ESP32 QEMU for {system}-{arch}...")

    # Check if already installed
    print("Searching for existing QEMU installation...")
    qemu_binary = find_qemu_binary()
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
                result = subprocess.run(["sudo", "apt-get", "update"], text=True)

                if result.returncode == 0:
                    print("Package list updated successfully")
                    print("Installing QEMU packages...")
                    print("Installing: qemu-system-misc qemu-system-xtensa")
                    result = subprocess.run(
                        [
                            "sudo",
                            "apt-get",
                            "install",
                            "-y",
                            "qemu-system-misc",
                            "qemu-system-xtensa",
                        ],
                        text=True,
                    )

                    if result.returncode == 0:
                        print("QEMU packages installed successfully via apt-get")
                        return True
                    else:
                        print("Failed to install QEMU packages via apt-get")
                        print("Return code:", result.returncode)

            except Exception as e:
                print(f"Automatic installation failed: {e}")
        else:
            print(
                "Local Linux environment - automatic installation requires sudo privileges"
            )

    elif system == "windows":
        print("Windows platform detected, trying package managers...")
        # Try Windows package managers (works in both CI and local environments)
        if try_windows_package_managers():
            print("Windows QEMU installation successful")
            return True
        else:
            print("All Windows installation methods failed")

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
        print("Option 1: Package Managers")
        if check_command_available("choco"):
            print("  choco install qemu -y  (may require admin privileges)")
        else:
            print("  First install Chocolatey: https://chocolatey.org/install")
            print("  Then run: choco install qemu -y")
        if check_command_available("winget"):
            print(
                "  winget install SoftwareFreedomConservancy.QEMU --scope user  (user-level install)"
            )
        else:
            print(
                "  Update Windows to get winget, then: winget install SoftwareFreedomConservancy.QEMU --scope user"
            )
        print()
        print("Option 2: ESP-IDF (includes QEMU)")
        print(
            "  Download ESP-IDF from: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html"
        )
        print()
        print("Option 3: Manual QEMU Installation")
        print("  Download QEMU from: https://www.qemu.org/download/")
        print("  Or portable version from: https://qemu.weilnetz.de/w64/")
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
    system, _ = get_platform_info()
    binary_name = (
        "qemu-system-xtensa.exe" if system == "windows" else "qemu-system-xtensa"
    )

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
        if base_path.exists():
            print(f"  Directory exists, searching for {binary_name}...")
            for qemu_path in base_path.rglob(binary_name):
                if qemu_path.is_file():
                    print(f"Found QEMU binary at: {qemu_path}")
                    return qemu_path
            print(f"  No {binary_name} found in {base_path}")
        else:
            print(f"  Directory does not exist")

    # Check system PATH
    print("Checking system PATH...")
    try:
        cmd = ["where" if system == "windows" else "which", binary_name]
        print(f"Running: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            # Take first line in case of multiple results (Windows)
            first_line = result.stdout.strip().split("\n")[0]
            qemu_path = Path(first_line)
            print(f"Found QEMU binary in PATH: {qemu_path}")
            return qemu_path
        else:
            print(f"Command returned code {result.returncode}")
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
    print("=== ESP32 QEMU Installation ===")

    # Show platform info
    system, arch = get_platform_info()
    print(f"Platform: {system}-{arch}")

    # Install system dependencies
    install_system_dependencies()

    # Install ESP32 QEMU
    qemu_installed = install_qemu_esp32()

    if qemu_installed:
        # Create wrapper script
        if not create_qemu_wrapper():
            print("WARNING: Could not create wrapper script, but QEMU is available")
    else:
        print("QEMU installation required for ESP32 emulation")
        sys.exit(1)

    print("=== Installation Complete ===")
    print("ESP32 QEMU is ready to use!")

    # Test installation
    print("\n=== Verifying Installation ===")
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
