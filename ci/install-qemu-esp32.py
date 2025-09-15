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
    system = platform.system().lower()
    machine = platform.machine().lower()

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
            result = subprocess.run(
                ["sudo", "apt-get", "update"], capture_output=True, text=True
            )
            if result.returncode == 0:
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
        urllib.request.urlretrieve(tools_url, tools_script)
        print(f"Downloaded ESP-IDF tools to {tools_script}")

    return tools_script


def check_command_available(command: str) -> bool:
    """Check if a command is available in PATH."""
    try:
        result = subprocess.run(
            [command, "--version"], capture_output=True, text=True, timeout=10
        )
        return result.returncode == 0
    except (subprocess.TimeoutExpired, FileNotFoundError, subprocess.SubprocessError):
        return False


def try_chocolatey_install():
    """Try to install QEMU using Chocolatey."""
    if not check_command_available("choco"):
        print("Chocolatey not found, skipping...")
        return False

    try:
        print("Installing QEMU via Chocolatey...")
        # Note: Chocolatey typically requires admin privileges for system-wide installs
        # Quick check for admin privileges first
        result = subprocess.run(
            ["choco", "install", "qemu", "-y", "--no-progress", "--timeout", "30"],
            capture_output=True,
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
    if not check_command_available("winget"):
        print("winget not found, skipping...")
        return False

    try:
        print("Installing QEMU via winget (user scope, no admin required)...")
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
            capture_output=True,
            text=True,
            timeout=300,
        )

        if result.returncode == 0:
            print("QEMU installed successfully via winget")
            return True
        else:
            print(f"winget user installation failed: {result.stderr}")
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
                capture_output=True,
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
        if qemu_binary.exists():
            print(f"Portable QEMU already installed at {qemu_binary}")
            return True

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
        print(f"Trying {name}...")
        if install_func():
            # Verify installation worked
            if find_qemu_binary():
                print(f"SUCCESS: QEMU installed via {name}")
                return True
            else:
                print(f"WARNING: {name} reported success but QEMU binary not found")

    print("All Windows package managers failed")
    return False


def install_qemu_esp32():
    """Install ESP32 QEMU using ESP-IDF tools."""
    system, arch = get_platform_info()

    print(f"Checking for ESP32 QEMU for {system}-{arch}...")

    # Check if already installed
    qemu_binary = find_qemu_binary()
    if qemu_binary:
        print("ESP32 QEMU already installed")
        return True

    # Try automated installation
    print("Attempting automatic installation...")

    if system == "linux":
        # Check if we're in CI first for sudo operations
        if os.getenv("CI") == "true" or os.getenv("GITHUB_ACTIONS") == "true":
            try:
                # Try to install QEMU from package manager in CI
                print("Installing QEMU via apt-get...")
                result = subprocess.run(
                    ["sudo", "apt-get", "update"], capture_output=True, text=True
                )

                if result.returncode == 0:
                    result = subprocess.run(
                        [
                            "sudo",
                            "apt-get",
                            "install",
                            "-y",
                            "qemu-system-misc",
                            "qemu-system-xtensa",
                        ],
                        capture_output=True,
                        text=True,
                    )

                    if result.returncode == 0:
                        print("QEMU installed successfully via apt-get")
                        return True
                    else:
                        print("Failed to install QEMU via apt-get")

            except Exception as e:
                print(f"Automatic installation failed: {e}")
        else:
            print(
                "Local Linux environment - automatic installation requires sudo privileges"
            )

    elif system == "windows":
        # Try Windows package managers (works in both CI and local environments)
        if try_windows_package_managers():
            return True

    elif system == "darwin":
        # Try macOS installation methods
        print("Attempting macOS installation...")
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
        if base_path.exists():
            for qemu_path in base_path.rglob(binary_name):
                if qemu_path.is_file():
                    print(f"Found QEMU binary at: {qemu_path}")
                    return qemu_path

    # Check system PATH
    try:
        cmd = ["where" if system == "windows" else "which", binary_name]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            # Take first line in case of multiple results (Windows)
            first_line = result.stdout.strip().split("\n")[0]
            qemu_path = Path(first_line)
            print(f"Found QEMU binary in PATH: {qemu_path}")
            return qemu_path
    except (subprocess.SubprocessError, FileNotFoundError):
        pass

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
    qemu_binary = find_qemu_binary()
    if qemu_binary:
        try:
            result = subprocess.run(
                [str(qemu_binary), "--version"],
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode == 0:
                print(f"QEMU version: {result.stdout.strip()}")
                sys.exit(0)  # Explicit success exit
            else:
                print("WARNING: QEMU installed but version check failed")
                sys.exit(1)
        except Exception as e:
            print(f"WARNING: Could not verify QEMU installation: {e}")
            sys.exit(1)
    else:
        print("ERROR: QEMU installation verification failed")
        sys.exit(1)


if __name__ == "__main__":
    main()
