#!/usr/bin/env python3
"""
ESP32 QEMU Installation Script

Installs ESP32-compatible QEMU emulator using ESP-IDF tools.
Cross-platform support for Linux, macOS, and Windows.
"""

import os
import platform
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
        # But we'll try anyway as some packages can be installed locally
        result = subprocess.run(
            ["choco", "install", "qemu", "-y", "--no-progress"],
            capture_output=True,
            text=True,
            timeout=300,
        )

        if result.returncode == 0:
            print("QEMU installed successfully via Chocolatey")
            return True
        else:
            print(
                f"Chocolatey installation failed. This may require administrator privileges."
            )
            print(f"Error details: {result.stderr.strip()}")
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

        # Create local QEMU directory
        qemu_dir = Path.home() / ".fastled" / "qemu"
        qemu_dir.mkdir(parents=True, exist_ok=True)

        print(f"Downloading portable QEMU to {qemu_dir}...")

        # This is a simplified approach - in practice, we'd need to:
        # 1. Download QEMU Windows binaries
        # 2. Extract to local directory
        # 3. Add to PATH or create wrapper script

        print("WARNING: Portable QEMU installation not yet implemented")
        print("Please install QEMU manually or use package managers")
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

    # Try automated installation for CI environments
    if os.getenv("CI") == "true" or os.getenv("GITHUB_ACTIONS") == "true":
        print("CI environment detected, attempting automatic installation...")

        if system == "linux":
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

        elif system == "windows":
            # Try Windows package managers
            if try_windows_package_managers():
                return True

        # Fall back to providing instructions
        print("Automatic installation not available for this platform in CI")

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

    # ESP-IDF installation paths
    esp_paths = [
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
    qemu_binary = find_qemu_binary()
    if not qemu_binary:
        return False

    # Create wrapper in ci directory
    ci_dir = Path(__file__).parent
    wrapper_script = ci_dir / "qemu-esp32.py"

    wrapper_content = f'''#!/usr/bin/env python3
"""
ESP32 QEMU Runner Wrapper

Automatically generated by install-qemu-esp32.py
"""

import subprocess
import sys
from pathlib import Path

QEMU_BINARY = Path(r"{qemu_binary}")

def run_qemu(args):
    """Run QEMU with the given arguments."""
    cmd = [str(QEMU_BINARY)] + args
    return subprocess.run(cmd)

if __name__ == "__main__":
    # Pass through all arguments to QEMU
    sys.exit(run_qemu(sys.argv[1:]).returncode)
'''

    wrapper_script.write_text(wrapper_content)
    wrapper_script.chmod(0o755)

    print(f"Created QEMU wrapper at: {wrapper_script}")
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
            else:
                print("WARNING: QEMU installed but version check failed")
        except Exception as e:
            print(f"WARNING: Could not verify QEMU installation: {e}")


if __name__ == "__main__":
    main()
