#!/usr/bin/env python3

import os
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
from pathlib import Path
from typing import Dict, List, Optional, Tuple


def is_linux_or_unix() -> Tuple[bool, str]:
    """Check if running on Linux or Unix-like system."""
    system = platform.system().lower()
    if system == "linux":
        return True, "linux"
    elif system in ["darwin", "freebsd", "openbsd", "netbsd"]:
        return True, system
    else:
        return False, system


def get_tool_version(tool_name: str) -> Optional[str]:
    """Get version of a tool if it exists on system PATH."""
    try:
        result = subprocess.run(
            [tool_name, "--version"], capture_output=True, text=True, timeout=5
        )
        if result.returncode == 0:
            return result.stdout.strip()
        return None
    except (subprocess.TimeoutExpired, FileNotFoundError, subprocess.SubprocessError):
        return None


def get_clang_version_number(version_output: str) -> Optional[int]:
    """Extract major version number from clang version output."""
    try:
        # Look for "clang version X.Y.Z" pattern
        import re

        match = re.search(r"clang version (\d+)\.", version_output)
        if match:
            return int(match.group(1))
        return None
    except (ValueError, AttributeError):
        return None


def find_tool_path(tool_name: str) -> Optional[Path]:
    """Find the full path to a tool if it exists."""
    tool_path = shutil.which(tool_name)
    return Path(tool_path) if tool_path else None


def check_required_tools() -> Tuple[
    Dict[str, Optional[Path]], Dict[str, Optional[Path]]
]:
    """Check for essential and extra LLVM/Clang tools on system PATH."""
    essential_tools = [
        "clang",
        "clang++",
        "llvm-ar",
        "llvm-nm",
        "llvm-objdump",
        "llvm-addr2line",
    ]

    extra_tools = [
        "clangd",
        "clang-format",
        "clang-tidy",
        "lldb",
        "lld",
        "llvm-strip",
        "llvm-objcopy",
    ]

    found_essential: Dict[str, Optional[Path]] = {}
    for tool in essential_tools:
        found_essential[tool] = find_tool_path(tool)

    found_extras: Dict[str, Optional[Path]] = {}
    for tool in extra_tools:
        found_extras[tool] = find_tool_path(tool)

    return found_essential, found_extras


def create_hard_links(source_tools: Dict[str, Path], target_dir: Path) -> None:
    """Create hard links from system tools to target directory."""
    bin_dir = target_dir / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)

    for tool_name, source_path in source_tools.items():
        if source_path and source_path.exists():
            target_path = bin_dir / tool_name
            try:
                # Remove existing file if it exists
                if target_path.exists():
                    target_path.unlink()

                # Create hard link
                target_path.hardlink_to(source_path)
                print(f"   ✓ Linked {tool_name}: {source_path} -> {target_path}")
            except (OSError, PermissionError) as e:
                print(f"   ⚠️  Failed to link {tool_name}: {e}")
                # Fallback to copy if hard link fails
                try:
                    shutil.copy2(source_path, target_path)
                    target_path.chmod(0o755)
                    print(f"   ✓ Copied {tool_name}: {source_path} -> {target_path}")
                except (OSError, PermissionError):
                    print(f"   ❌ Failed to copy {tool_name}")


def download_and_install_llvm(target_dir: Path) -> None:
    """Download and install LLVM using llvm-installer package."""
    try:
        # Import llvm-installer
        import sys_detection
        from llvm_installer import LlvmInstaller
    except ImportError as e:
        print(
            f"Error: Missing required packages. Please install with: uv add llvm-installer"
        )
        print(f"Import error: {e}")
        sys.exit(1)

    print("Detecting system configuration...")
    try:
        local_sys_conf = sys_detection.local_sys_conf()
        detected_os = local_sys_conf.short_os_name_and_version()
        architecture = local_sys_conf.architecture
        print(f"Detected OS: {detected_os}")
        print(f"Architecture: {architecture}")

        # Use Ubuntu 24.04 as fallback since it's known to work
        # This package seems to be from YugabyteDB and has limited OS support
        working_os = "ubuntu24.04"
        if detected_os.startswith("ubuntu24") or detected_os.startswith("ubuntu22"):
            working_os = detected_os

        print(f"Using OS variant: {working_os}")

        llvm_installer = LlvmInstaller(
            short_os_name_and_version=working_os, architecture=architecture
        )

        # Try LLVM 18 (latest stable)
        version = 18
        print(f"Installing LLVM version {version}...")
        llvm_url = llvm_installer.get_llvm_url(major_llvm_version=version)
        print(f"Found LLVM {version} at: {llvm_url}")

        # Download and extract
        print("Downloading LLVM package...")
        with tempfile.NamedTemporaryFile(delete=False, suffix=".tar.gz") as tmp_file:
            urllib.request.urlretrieve(llvm_url, tmp_file.name)

            print("Extracting LLVM package...")
            with tarfile.open(tmp_file.name, "r:gz") as tar:
                tar.extractall(target_dir, filter="data")

        os.unlink(tmp_file.name)

        # Find the extracted directory and verify clang exists
        for extracted_dir in target_dir.iterdir():
            if extracted_dir.is_dir():
                clang_path = extracted_dir / "bin" / "clang"

                if clang_path.exists():
                    print(f"✅ Successfully installed LLVM {version}")
                    print(f"   Installation directory: {extracted_dir}")
                    print(f"   Clang binary: {clang_path}")

                    # List available tools
                    bin_dir = extracted_dir / "bin"
                    if bin_dir.exists():
                        tools = [
                            f.name
                            for f in bin_dir.iterdir()
                            if f.is_file() and not f.name.endswith(".dll")
                        ]
                        print(
                            f"   Available tools: {', '.join(sorted(tools[:10]))}{'...' if len(tools) > 10 else ''}"
                        )

                    return

        print("❌ LLVM downloaded but clang not found in expected location")
        sys.exit(1)

    except Exception as e:
        print(f"Error during installation: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


def main() -> None:
    """Setup LLVM toolchain - check system first, fallback to package installation."""
    # Get target directory from command line
    if len(sys.argv) != 2:
        print("Usage: python setup-llvm.py <target_directory>")
        sys.exit(1)

    target_dir = Path(sys.argv[1])
    target_dir.mkdir(parents=True, exist_ok=True)

    # Check platform compatibility
    is_unix_like, platform_name = is_linux_or_unix()

    if not is_unix_like:
        print(
            f"⚠️ Warning: Linux tools are only auto-installed on Linux/Unix, not {platform_name}"
        )
        return

    print("🔍 Checking for existing LLVM/Clang tools on system...")

    # Check if clang is available and get its version
    clang_version_output = get_tool_version("clang")
    if clang_version_output:
        clang_version = get_clang_version_number(clang_version_output)
        print(
            f"Found system clang: version {clang_version if clang_version else 'unknown'}"
        )

        if clang_version and clang_version >= 19:
            print("✅ System clang 19+ detected - checking tools")

            # Check for essential and extra tools
            found_essential, found_extras = check_required_tools()
            available_essential = {
                name: path for name, path in found_essential.items() if path
            }
            missing_essential = [
                name for name, path in found_essential.items() if not path
            ]
            available_extras = {
                name: path for name, path in found_extras.items() if path
            }
            missing_extras = [name for name, path in found_extras.items() if not path]

            # Check if all essential tools are present
            if len(available_essential) == len(found_essential):
                print("✅ All essential tools found - using system tools")

                # Combine available tools for linking
                all_available_tools = {**available_essential, **available_extras}

                print(f"Creating hard links in {target_dir}...")
                create_hard_links(all_available_tools, target_dir)

                # Create a simple version file
                version_file = target_dir / "VERSION"
                version_file.write_text(f"system-clang-{clang_version}\n")

                print(f"✅ Successfully linked {len(all_available_tools)} system tools")

                # Warn about missing extras in one line
                if missing_extras:
                    print(f"⚠️ Missing extra tools: {', '.join(missing_extras)}")

                return
            else:
                # Missing essential tools - need to install LLVM
                print(f"❌ Missing essential tools: {', '.join(missing_essential)}")
                if platform_name == "linux":
                    print("📦 Installing LLVM package to provide missing tools...")
                    download_and_install_llvm(target_dir)
                    return
                else:
                    print(
                        f"⚠️ Cannot auto-install on {platform_name} - please install manually"
                    )
                    return
        else:
            print(f"System clang version {clang_version} < 19")
            if platform_name == "linux":
                print("📦 Installing LLVM 18 package...")
                download_and_install_llvm(target_dir)
                return
            else:
                print(
                    f"⚠️ Cannot auto-install on {platform_name} - please install manually"
                )
                return
    else:
        print("No system clang found")
        if platform_name == "linux":
            print("📦 Installing LLVM package...")
            download_and_install_llvm(target_dir)
        else:
            print(f"⚠️ Cannot auto-install on {platform_name} - please install manually")


if __name__ == "__main__":
    main()
