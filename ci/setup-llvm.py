#!/usr/bin/env python3

import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
from pathlib import Path
from typing import Dict, List, Optional, Tuple


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


def check_required_tools() -> Dict[str, Optional[Path]]:
    """Check for all required LLVM/Clang tools on system PATH."""
    required_tools = [
        "clang",
        "clang++",
        "clangd",
        "clang-format",
        "clang-tidy",
        "lldb",
        "lld",
        "llvm-ar",
        "llvm-nm",
        "llvm-objdump",
        "llvm-addr2line",
        "llvm-strip",
        "llvm-objcopy",
    ]

    found_tools: Dict[str, Optional[Path]] = {}
    for tool in required_tools:
        found_tools[tool] = find_tool_path(tool)

    return found_tools


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

    print("🔍 Checking for existing LLVM/Clang tools on system...")

    # Check if clang is available and get its version
    clang_version_output = get_tool_version("clang")
    if clang_version_output:
        clang_version = get_clang_version_number(clang_version_output)
        print(
            f"Found system clang: version {clang_version if clang_version else 'unknown'}"
        )

        if clang_version and clang_version >= 19:
            print("✅ System clang 19+ detected - using system tools")

            # Check for all required tools
            found_tools = check_required_tools()
            available_tools = {name: path for name, path in found_tools.items() if path}
            missing_tools = [name for name, path in found_tools.items() if not path]

            print(f"Found {len(available_tools)} of {len(found_tools)} required tools:")
            for tool_name, tool_path in available_tools.items():
                print(f"   ✓ {tool_name}: {tool_path}")

            if missing_tools:
                print(f"Missing tools: {', '.join(missing_tools)}")
                print(
                    "   Some tools may not be available, but core functionality should work"
                )

            if available_tools:
                print(f"Creating hard links in {target_dir}...")
                create_hard_links(available_tools, target_dir)

                # Create a simple version file
                version_file = target_dir / "VERSION"
                version_file.write_text(f"system-clang-{clang_version}\n")

                print(f"✅ Successfully linked {len(available_tools)} system tools")
                return
            else:
                print("❌ No system tools found, falling back to package installation")
        else:
            print(
                f"System clang version {clang_version} < 19, falling back to package installation"
            )
    else:
        print("No system clang found, falling back to package installation")

    print("\n📦 Installing LLVM from package...")
    download_and_install_llvm(target_dir)


if __name__ == "__main__":
    main()
