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
from typing import Dict, List, Optional, Tuple, cast


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
    """Check for essential and extra LLVM/Clang tools on system PATH.

    On Windows, prefer lld-link as the linker front-end.
    """
    system_name = platform.system().lower()

    essential_tools: List[str] = [
        "clang",
        "clang++",
        "llvm-ar",
        "llvm-nm",
        "llvm-objdump",
        "llvm-addr2line",
        "llvm-strip",
        "llvm-objcopy",
    ]

    # Linker tools differ across platforms
    if system_name == "windows":
        essential_tools.append("lld-link")
    else:
        essential_tools.append("lld")

    # Useful developer tools (not strictly required for building)
    extra_tools: List[str] = [
        "clangd",
        "clang-format",
        "clang-tidy",
        "lldb",
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

    linked_count: int = 0
    copied_count: int = 0
    failed_tools: list[str] = []

    for tool_name, source_path in source_tools.items():
        if source_path and source_path.exists():
            # Preserve original filename to keep extensions on Windows (e.g., .exe)
            target_filename = source_path.name
            target_path = bin_dir / target_filename
            try:
                # Remove existing file if it exists
                if target_path.exists():
                    target_path.unlink()

                # Create hard link
                target_path.hardlink_to(source_path)
                print(f"[OK] Linked {tool_name}: {source_path} -> {target_path}")
                linked_count += 1
            except (OSError, PermissionError) as e:
                print(f"[WARN] Failed to link {tool_name}: {e}")
                # Fallback to copy if hard link fails
                try:
                    shutil.copy2(source_path, target_path)
                    target_path.chmod(0o755)
                    print(f"[OK] Copied {tool_name}: {source_path} -> {target_path}")
                    copied_count += 1
                except (OSError, PermissionError):
                    print(f"[ERROR] Failed to copy {tool_name}")
                    failed_tools.append(tool_name)

    total_prepared: int = linked_count + copied_count
    print(
        f"Summary: prepared {total_prepared} tools (linked: {linked_count}, copied: {copied_count})"
    )
    if failed_tools:
        print(f"ERROR: Failed to mirror tools: {', '.join(failed_tools)}")


def find_tools_in_directories(
    tool_names: List[str], directories: List[Path]
) -> Dict[str, Optional[Path]]:
    """Search for tools across a list of directories, returning first matches.

    On Windows, will match executables with .exe suffix.
    """
    found: Dict[str, Optional[Path]] = {}
    for tool in tool_names:
        found[tool] = None

    for directory in directories:
        if not directory.exists():
            continue
        for tool in tool_names:
            if found[tool] is not None:
                continue
            candidate: Path = directory / tool
            if candidate.exists():
                found[tool] = candidate
                continue
            # Try platform-specific suffix
            if platform.system().lower() == "windows":
                exe_candidate: Path = directory / f"{tool}.exe"
                if exe_candidate.exists():
                    found[tool] = exe_candidate
    return found


def get_windows_default_llvm_bins() -> List[Path]:
    """Return common Windows LLVM bin directories to probe."""
    roots: List[Path] = []
    program_files: Optional[str] = os.environ.get("ProgramFiles")
    program_files_x86: Optional[str] = os.environ.get("ProgramFiles(x86)")
    chocolatey_lib: Optional[str] = os.environ.get("ChocolateyInstall")
    scoop: Optional[str] = os.environ.get("SCOOP")

    if program_files:
        roots.append(Path(program_files) / "LLVM" / "bin")
    if program_files_x86:
        roots.append(Path(program_files_x86) / "LLVM" / "bin")
    if chocolatey_lib:
        roots.append(Path(chocolatey_lib) / "lib" / "llvm" / "tools" / "llvm" / "bin")
    if scoop:
        roots.append(Path(scoop) / "apps" / "llvm" / "current" / "bin")

    # Also consider PATH entries explicitly
    path_entries: List[str] = os.environ.get("PATH", "").split(os.pathsep)
    for entry in path_entries:
        # Only consider entries that look like LLVM bin directories
        if "llvm" in entry.lower() and "bin" in entry.lower():
            roots.append(Path(entry))
    return roots


def _run_subprocess(args: List[str], timeout_seconds: int) -> tuple[int, str, str]:
    """Run a subprocess command and return (returncode, stdout, stderr)."""
    try:
        result = subprocess.run(
            args,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
        )
        return result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired as e:
        return 124, "", f"Timeout after {timeout_seconds}s: {e}"
    except subprocess.SubprocessError as e:
        return 1, "", f"Subprocess error: {e}"


def install_llvm_windows() -> bool:
    """Attempt to install LLVM toolchain on Windows using available package managers.

    Order of attempts:
      1) winget install LLVM.LLVM (silent)
      2) choco install llvm -y
      3) scoop install llvm

    Returns True if any installer reports success, otherwise False.
    """
    # Try winget
    winget_path = find_tool_path("winget")
    if winget_path is not None:
        print("Attempting LLVM installation via winget ...")
        code, out, err = _run_subprocess(
            [
                str(winget_path),
                "install",
                "-e",
                "--id",
                "LLVM.LLVM",
                "--source",
                "winget",
                "--silent",
                "--accept-package-agreements",
                "--accept-source-agreements",
            ],
            timeout_seconds=1800,
        )
        if code == 0:
            print("OK: winget reports LLVM installation completed")
            return True
        print("WARN: winget failed to install LLVM")
        if out:
            print(out)
        if err:
            print(err)

    # Try Chocolatey
    choco_path = find_tool_path("choco")
    if choco_path is not None:
        print("Attempting LLVM installation via Chocolatey ...")
        code, out, err = _run_subprocess(
            [str(choco_path), "install", "llvm", "-y"], timeout_seconds=1800
        )
        if code == 0:
            print("OK: Chocolatey reports LLVM installation completed")
            return True
        print("WARN: Chocolatey failed to install LLVM")
        if out:
            print(out)
        if err:
            print(err)

    # Try Scoop
    scoop_path = find_tool_path("scoop")
    if scoop_path is not None:
        print("Attempting LLVM installation via Scoop ...")
        code, out, err = _run_subprocess(
            [str(scoop_path), "install", "llvm"], timeout_seconds=1800
        )
        if code == 0:
            print("OK: Scoop reports LLVM installation completed")
            return True
        print("WARN: Scoop failed to install LLVM")
        if out:
            print(out)
        if err:
            print(err)

    print(
        "CRITICAL: No supported Windows package manager (winget/choco/scoop) succeeded in installing LLVM."
    )
    return False


def download_and_install_llvm(target_dir: Path) -> None:
    """Download and install LLVM using llvm-installer package."""
    try:
        # Import llvm-installer
        import sys_detection
        from llvm_installer import LlvmInstaller
    except ImportError as e:
        print(
            "CRITICAL: Missing required packages for LLVM installation helper.\n"
            "Please install them with: uv add llvm-installer sys-detection"
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
                    print(f"OK: Successfully installed LLVM {version}")
                    print(f"   Installation directory: {extracted_dir}")
                    print(f"   Clang binary: {clang_path}")

                    # List available tools (first few) and provide PATH guidance
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
                        print("")
                        print("Next steps:")
                        print(f"  - Add to PATH: {bin_dir}")
                        print("  - Example:")
                        print(f'      export PATH="{bin_dir}:$PATH"')
                        print("  - Verify:")
                        print("      clang --version")

                    return

        print("ERROR: LLVM downloaded but clang not found in expected location")
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
        print("Usage: uv run python ci/setup-llvm.py <target_directory>")
        sys.exit(1)

    target_dir = Path(sys.argv[1])
    target_dir.mkdir(parents=True, exist_ok=True)

    # Check platform compatibility
    is_unix_like, platform_name = is_linux_or_unix()

    if platform_name == "windows":
        print("Checking for existing LLVM/Clang tools on system (Windows)...")
        clang_version_output = get_tool_version("clang")
        if clang_version_output:
            clang_version = get_clang_version_number(clang_version_output)
            print(
                f"Found system clang: version {clang_version if clang_version else 'unknown'}"
            )
        else:
            clang_version = None

        found_essential, found_extras = check_required_tools()
        available_essential = {
            name: path for name, path in found_essential.items() if path
        }
        missing_essential = [name for name, path in found_essential.items() if not path]

        if len(available_essential) != len(found_essential):
            # Probe common install directories (e.g., C:\Program Files\LLVM\bin)
            probe_dirs = get_windows_default_llvm_bins()
            print(
                f"Probing default Windows LLVM locations: {', '.join(str(p) for p in probe_dirs)}"
            )
            probed = find_tools_in_directories(list(found_essential.keys()), probe_dirs)
            for name, p in probed.items():
                if p is not None and found_essential.get(name) is None:
                    found_essential[name] = p

            available_essential = {
                name: path for name, path in found_essential.items() if path
            }
            missing_essential = [
                name for name, path in found_essential.items() if not path
            ]

        if len(available_essential) == len(found_essential) and (
            clang_version is None or clang_version < 19
        ):
            print(
                "Note: Detected toolchain but clang < 19. Continuing; ensure compatibility with your build settings."
            )

        # On Windows, require core tools including llvm-addr2line for stack traces
        core_required = ["clang", "clang++", "lld-link", "llvm-addr2line"]
        missing_core = [name for name in core_required if not found_essential.get(name)]

        if not missing_core:
            print("OK: Core tools present - proceeding with available tools")
            all_available_tools: Dict[str, Path] = {}
            for name, path in found_essential.items():
                if path is not None:
                    all_available_tools[name] = cast(Path, path)
            for name, path in found_extras.items():
                if path is not None:
                    all_available_tools[name] = cast(Path, path)

            print(f"Creating hard links in {target_dir}...")
            create_hard_links(all_available_tools, target_dir)

            version_file = target_dir / "VERSION"
            version_file.write_text(
                f"system-clang-{clang_version if clang_version else 'unknown'}\n"
            )
            bin_dir = target_dir / "bin"
            existing = [p for p in bin_dir.iterdir() if p.is_file()]
            print(f"OK: Tools available in {bin_dir}: {len(existing)} files")

            # Warn about missing non-core tools
            missing_noncore = [
                name
                for name in list(found_essential.keys())
                if name not in core_required and not found_essential.get(name)
            ]
            if missing_noncore:
                print(f"WARN: Missing non-core tools: {', '.join(missing_noncore)}")
            missing_extras = [name for name, path in found_extras.items() if not path]
            if missing_extras:
                print(f"WARN: Missing extra tools: {', '.join(missing_extras)}")
            return

        # Try to auto-install on Windows using available package managers
        print(f"ERROR: Missing core tools: {', '.join(missing_core)}")
        print("Attempting automatic LLVM installation on Windows...")
        if not install_llvm_windows():
            print(
                "CRITICAL: Failed to install LLVM automatically. Please install LLVM for Windows manually (official installer or via your package manager), add its bin directory to PATH, and re-run.\n"
                "Note: llvm-addr2line is REQUIRED for stack traces and test infrastructure."
            )
            print("    uv run python ci/setup-llvm.py <target_directory>")
            sys.exit(1)

        # Re-scan after installation
        found_essential, found_extras = check_required_tools()
        # Probe default locations again
        probed_after = find_tools_in_directories(
            list(found_essential.keys()), get_windows_default_llvm_bins()
        )
        for name, p in probed_after.items():
            if p is not None and found_essential.get(name) is None:
                found_essential[name] = p

        # Re-evaluate core
        missing_core = [name for name in core_required if not found_essential.get(name)]
        if missing_core:
            print(
                f"CRITICAL: LLVM installation incomplete. Still missing core tools: {', '.join(missing_core)}"
            )
            print(
                "Please ensure LLVM's bin directory is on PATH and includes llvm-addr2line."
            )
            sys.exit(1)

        # Proceed with linking
        print("OK: LLVM installed - proceeding with available tools")
        all_available_tools: Dict[str, Path] = {}
        for name, path in found_essential.items():
            if path is not None:
                all_available_tools[name] = cast(Path, path)
        for name, path in found_extras.items():
            if path is not None:
                all_available_tools[name] = cast(Path, path)

        print(f"Creating hard links in {target_dir}...")
        create_hard_links(all_available_tools, target_dir)

        version_file = target_dir / "VERSION"
        version_file.write_text(
            f"system-clang-{clang_version if clang_version else 'unknown'}\n"
        )
        bin_dir = target_dir / "bin"
        existing = [p for p in bin_dir.iterdir() if p.is_file()]
        print(f"OK: Tools available in {bin_dir}: {len(existing)} files")
        return

        print("Checking for existing LLVM/Clang tools on system...")

    # Check if clang is available and get its version
    clang_version_output = get_tool_version("clang")
    if clang_version_output:
        clang_version = get_clang_version_number(clang_version_output)
        print(
            f"Found system clang: version {clang_version if clang_version else 'unknown'}"
        )

        if clang_version and clang_version >= 19:
            print("OK: System clang 19+ detected - checking tools")

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
                print("OK: All essential tools found - using system tools")

                # Combine available tools for linking
                all_available_tools = {**available_essential, **available_extras}

                print(f"Creating hard links in {target_dir}...")
                create_hard_links(all_available_tools, target_dir)

                # Create a simple version file
                version_file = target_dir / "VERSION"
                version_file.write_text(f"system-clang-{clang_version}\n")

                print(
                    f"OK: Successfully linked {len(all_available_tools)} system tools"
                )

                # Warn about missing extras in one line
                if missing_extras:
                    print(f"WARN: Missing extra tools: {', '.join(missing_extras)}")

                return
            else:
                # Missing essential tools - need to install LLVM
                print(f"ERROR: Missing essential tools: {', '.join(missing_essential)}")
                if platform_name == "linux":
                    print("Installing LLVM package to provide missing tools...")
                    download_and_install_llvm(target_dir)
                    return
                else:
                    print(
                        f"WARN: Cannot auto-install on {platform_name} - please install manually"
                    )
                    return
        else:
            print(f"System clang version {clang_version} < 19")
            if platform_name == "linux":
                print("Installing LLVM 18 package...")
                download_and_install_llvm(target_dir)
                return
            else:
                print(
                    f"WARN: Cannot auto-install on {platform_name} - please install manually"
                )
                return
    else:
        print("No system clang found")
        if platform_name == "linux":
            print("Installing LLVM package...")
            download_and_install_llvm(target_dir)
        else:
            print(
                f"WARN: Cannot auto-install on {platform_name} - please install manually"
            )


if __name__ == "__main__":
    main()
