#!/usr/bin/env python3

import _thread
import atexit
import json
import lzma
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path
from typing import Any, Dict, List, Optional, Protocol, Tuple, cast

from ci.util.resumable_downloader import ResumableDownloader


class _SysConf(Protocol):
    def short_os_name_and_version(self) -> str: ...

    architecture: str


import sys_detection
from llvm_installer import LlvmInstaller


_HERE = Path(__file__).parent
_PROJECT_ROOT = _HERE.parent

# Global download directory - lazily initialized
_download_dir: Optional[Path] = None


def _get_download_dir() -> Path:
    """Get or create the download directory lazily."""
    global _download_dir
    if _download_dir is None:
        _download_dir = _PROJECT_ROOT / ".cache" / "downloads"
        _download_dir.mkdir(parents=True, exist_ok=True)
        print(f"Created download directory: {_download_dir}")
        # Register cleanup on exit as fallback
        atexit.register(_cleanup_download_dir)
    return _download_dir


def _cleanup_download_dir() -> None:
    """Clean up the download directory."""
    global _download_dir
    if _download_dir and _download_dir.exists():
        try:
            shutil.rmtree(_download_dir)
            print(f"Cleaned up download directory: {_download_dir}")
        except Exception as e:
            print(
                f"Warning: Failed to clean up download directory {_download_dir}: {e}"
            )
        _download_dir = None


def _download(url: str, filename: str) -> Path:
    """Download to the managed download directory.

    Args:
        url: URL to download
        filename: Filename to save as (not full path)

    Returns:
        Path to the downloaded file
    """
    download_dir = _get_download_dir()
    file_path = download_dir / filename

    print(f"download {url} to {file_path}")

    # Use resumable downloader for large files
    downloader = ResumableDownloader(chunk_size=1024 * 1024)  # 1MB chunks

    try:
        downloader.download(url, file_path)
    except Exception as e:
        # Print download failure banner
        url_truncated = url[:100]
        banner_width = max(35, len(url_truncated) + 6)
        banner_line = "#" * banner_width
        print(f"\n{banner_line}")
        print(
            f"# DOWNLOAD FAILED FOR {url_truncated}{'...' if len(url) > 100 else ''} #"
        )
        print(f"{banner_line}")
        print(f"Error: {e}")
        print()
        raise

    return file_path


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


def get_breadcrumb_file(target_dir: Path) -> Path:
    """Get the path to the installation completion breadcrumb file."""
    cache_dir = target_dir / ".cache" / "cc"
    cache_dir.mkdir(parents=True, exist_ok=True)
    return cache_dir / "done.txt"


def create_breadcrumb(target_dir: Path) -> None:
    """Create breadcrumb file to mark successful installation."""
    breadcrumb = get_breadcrumb_file(target_dir)
    breadcrumb.write_text(f"Installation completed at {os.path.basename(target_dir)}\n")
    print(f"Created installation breadcrumb: {breadcrumb}")


def check_existing_installation(target_dir: Path) -> Tuple[bool, bool]:
    """Check if there's an existing installation and if it's complete.

    Returns:
        (has_artifacts, has_breadcrumb) - bool tuple indicating installation state
    """
    breadcrumb = get_breadcrumb_file(target_dir)
    has_breadcrumb = breadcrumb.exists()

    # Check for any installation artifacts
    bin_dir = target_dir / "bin"
    has_artifacts = bin_dir.exists() and any(bin_dir.iterdir())

    return has_artifacts, has_breadcrumb


def verify_clang_installation(target_dir: Path) -> bool:
    """Test if clang is properly installed in target directory."""
    bin_dir = target_dir / "bin"
    clang_exe = (
        bin_dir / "clang.exe"
        if platform.system().lower() == "windows"
        else bin_dir / "clang"
    )

    if not clang_exe.exists():
        return False

    # Test that clang can run and report version
    try:
        result = subprocess.run(
            [str(clang_exe), "--version"], capture_output=True, text=True, timeout=5
        )
        return result.returncode == 0
    except (subprocess.TimeoutExpired, FileNotFoundError, subprocess.SubprocessError):
        return False


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


def install_llvm_windows_direct(target_dir: Path) -> Path:
    """Download and extract official LLVM Windows tar.xz archive into target_dir.

    Returns the extracted LLVM bin directory path on success.
    Fails fast with a descriptive error on failure.
    """
    # Prefer dynamically discovered releases; as fallback, try a small curated list
    preferred_versions: List[str] = [
        "20.1.8",
        "20.1.7",
        "20.1.6",
        "19.1.8",
        "19.1.7",
        "19.1.6",
    ]

    download_errors: List[str] = []

    # First, try to discover the latest available Windows tar.xz for majors 20 and 19
    for probe_major in [20, 19]:
        print(f"Attempting discovery for LLVM major version {probe_major}...")
        discovered_url = discover_latest_github_llvm_win_zip(probe_major)
        if discovered_url is not None:
            print(f"Discovered URL for LLVM {probe_major}: {discovered_url}")
            # Put discovered URL at the front of attempts
            target_dir.mkdir(parents=True, exist_ok=True)
            extract_root: Path = target_dir

            tmp_archive_path: Optional[Path] = None
            try:
                print(f"Attempting to download and extract LLVM {probe_major}...")
                # Download to managed directory
                tmp_archive_path = _download(
                    discovered_url, f"llvm-discovered-{probe_major}.tar.xz"
                )
                extract_root = target_dir
                with lzma.open(tmp_archive_path, "rb") as xz_file:
                    with tarfile.open(fileobj=xz_file, mode="r") as tar:
                        tar.extractall(extract_root, filter="data")
            except KeyboardInterrupt:
                print("Operation interrupted by user")
                _thread.interrupt_main()
                raise
            except Exception as e:
                print(f"Failed to download/extract LLVM {probe_major}: {e}")
                download_errors.append(f"discovered-{probe_major}: {e}")
            finally:
                # Clean up the downloaded file immediately
                if tmp_archive_path and tmp_archive_path.exists():
                    try:
                        tmp_archive_path.unlink()
                        print(f"Cleaned up: {tmp_archive_path}")
                    except Exception as e:
                        print(f"Warning: Failed to clean up {tmp_archive_path}: {e}")

            extracted_bin: Optional[Path] = None
            for entry in extract_root.iterdir():
                if not entry.is_dir():
                    continue
                candidate = entry / "bin" / "clang.exe"
                if candidate.exists():
                    extracted_bin = entry / "bin"
                    break

            if extracted_bin is not None:
                print(
                    f"SUCCESS: LLVM {probe_major} installed successfully via discovery!"
                )
                print("OK: Successfully installed LLVM (Windows tar.xz, discovered)")
                print(f"   Installation directory: {extracted_bin.parent}")
                print(f"   Bin directory: {extracted_bin}")
                create_breadcrumb(target_dir)
                return extracted_bin
            else:
                print(
                    f"WARNING: LLVM {probe_major} downloaded but clang.exe not found in expected location"
                )
        else:
            print(f"No URL discovered for LLVM major version {probe_major}")

    # If discovery did not succeed, try curated versions list
    print("\n" + "=" * 60)
    print("Discovery failed for all major versions. Falling back to curated list...")
    print("=" * 60 + "\n")

    for version in preferred_versions:
        base_url = (
            "https://github.com/llvm/llvm-project/releases/download/"
            f"llvmorg-{version}/clang+llvm-{version}-x86_64-pc-windows-msvc.tar.xz"
        )
        print(f"Attempting direct LLVM download for Windows: {base_url}")
        try:
            target_dir.mkdir(parents=True, exist_ok=True)

            tmp_archive_path: Optional[Path] = None
            try:
                # Download to managed directory
                tmp_archive_path = _download(base_url, f"llvm-{version}.tar.xz")
            except KeyboardInterrupt:
                print("Operation interrupted by user")
                _thread.interrupt_main()
                raise
            except Exception as e:
                download_errors.append(f"{version}: {e}")
                # Clean up on download error
                if tmp_archive_path and tmp_archive_path.exists():
                    try:
                        tmp_archive_path.unlink()
                        print(f"Cleaned up after error: {tmp_archive_path}")
                    except Exception:
                        pass
                continue

            extract_root = target_dir
            try:
                with lzma.open(tmp_archive_path, "rb") as xz_file:
                    with tarfile.open(fileobj=xz_file, mode="r") as tar:
                        tar.extractall(extract_root, filter="data")
            finally:
                # Clean up the downloaded file immediately
                if tmp_archive_path and tmp_archive_path.exists():
                    try:
                        tmp_archive_path.unlink()
                        print(f"Cleaned up: {tmp_archive_path}")
                    except Exception as e:
                        print(f"Warning: Failed to clean up {tmp_archive_path}: {e}")

            # Find extracted directory that contains bin/clang.exe
            extracted_bin: Optional[Path] = None
            for entry in extract_root.iterdir():
                if not entry.is_dir():
                    continue
                candidate = entry / "bin" / "clang.exe"
                if candidate.exists():
                    extracted_bin = entry / "bin"
                    break

            if extracted_bin is None:
                print(
                    "ERROR: LLVM downloaded but clang.exe not found in expected location"
                )
                # Try next version
                continue

            print("OK: Successfully installed LLVM (Windows tar.xz)")
            print(f"   Installation directory: {extracted_bin.parent}")
            print(f"   Bin directory: {extracted_bin}")
            create_breadcrumb(target_dir)
            return extracted_bin

        except KeyboardInterrupt:
            print("Operation interrupted by user")
            _thread.interrupt_main()
            raise
        except Exception as e:
            download_errors.append(f"{version}: {e}")
            continue

    # If we reach here, all attempts failed
    print("CRITICAL: Failed to download LLVM for Windows from official releases.")
    if download_errors:
        for err in download_errors:
            print(f"  - {err}")
    print("Please ensure internet access is available and retry.")
    sys.exit(1)


def discover_latest_github_llvm_win_zip(major: int) -> Optional[str]:
    """Find the latest LLVM Windows tar.xz asset for a given major version via GitHub API.

    Returns a browser_download_url string if found, otherwise None.
    """
    try:
        per_page: int = 50
        page: int = 1
        max_pages: int = 3
        while page <= max_pages:
            api_url: str = (
                "https://api.github.com/repos/llvm/llvm-project/releases"
                f"?per_page={per_page}&page={page}"
            )
            req = urllib.request.Request(
                api_url, headers={"User-Agent": "fastled-setup-llvm"}
            )
            with urllib.request.urlopen(req, timeout=20) as resp:
                raw: str = resp.read().decode("utf-8")
            releases_any: Any = json.loads(raw)
            releases: list[dict[str, Any]] = []
            if isinstance(releases_any, list):
                tmp_list: list[Any] = cast(list[Any], releases_any)
                filtered: list[dict[str, Any]] = []
                k: int = 0
                while k < len(tmp_list):
                    item: Any = tmp_list[k]
                    k += 1
                    if isinstance(item, dict):
                        filtered.append(cast(dict[str, Any], item))
                releases = filtered

            # Iterate releases
            idx: int = 0
            while idx < len(releases):
                rel: dict[str, Any] = releases[idx]
                idx += 1
                tag_val: Any = rel.get("tag_name")
                tag_name: Optional[str] = tag_val if isinstance(tag_val, str) else None
                if tag_name is None:
                    continue
                # Expect tags like llvmorg-19.1.6
                prefix: str = f"llvmorg-{major}."
                if not tag_name.startswith(prefix):
                    continue
                assets_val: Any = rel.get("assets")
                assets: list[dict[str, Any]] = (
                    cast(list[dict[str, Any]], assets_val)
                    if isinstance(assets_val, list)
                    else []
                )
                filtered_assets: list[dict[str, Any]] = [
                    a for a in assets if isinstance(a, dict)
                ]
                j: int = 0
                while j < len(filtered_assets):
                    asset: dict[str, Any] = filtered_assets[j]
                    j += 1
                    name_val: Any = asset.get("name")
                    url_val: Any = asset.get("browser_download_url")
                    name: Optional[str] = (
                        name_val if isinstance(name_val, str) else None
                    )
                    url: Optional[str] = url_val if isinstance(url_val, str) else None
                    if name is not None and url is not None:
                        # Match e.g., clang+llvm-19.1.6-x86_64-pc-windows-msvc.tar.xz
                        if (
                            name.startswith(f"clang+llvm-{major}.")
                            and "x86_64-pc-windows-msvc.tar.xz" in name
                        ):
                            return url
            page += 1
        return None
    except urllib.error.URLError:
        return None


def download_and_install_llvm(target_dir: Path) -> None:
    """Download and install LLVM using llvm-installer package."""
    print("Detecting system configuration...")
    try:
        sys_conf_obj2: Any = sys_detection.local_sys_conf()
        local_conf2: _SysConf = cast(_SysConf, sys_conf_obj2)
        detected_os: str = local_conf2.short_os_name_and_version()
        architecture: str = local_conf2.architecture
        print(f"Detected OS: {detected_os}")
        print(f"Architecture: {architecture}")

        # Use Ubuntu 24.04 as fallback since it's known to work
        # This package seems to be from YugabyteDB and has limited OS support
        working_os: str = "ubuntu24.04"
        if detected_os.startswith("ubuntu24") or detected_os.startswith("ubuntu22"):
            working_os = detected_os

        print(f"Using OS variant: {working_os}")

        llvm_installer: Any = LlvmInstaller(
            short_os_name_and_version=working_os, architecture=architecture
        )

        # Prefer most recent supported major release for linux tarball path
        llvm_url: str = ""
        selected_version: Optional[int] = None
        for version in [20, 19]:
            try:
                print(f"Installing LLVM version {version}...")
                llvm_url = llvm_installer.get_llvm_url(major_llvm_version=version)
                print(f"Found LLVM {version} at: {llvm_url}")
                selected_version = version
                break
            except KeyboardInterrupt:
                print("Operation interrupted by user")
                _thread.interrupt_main()
                raise
            except Exception as e:
                print(f"WARN: Could not resolve LLVM {version}: {e}")

        if not llvm_url:
            raise RuntimeError(
                "Could not resolve a suitable LLVM download URL for Linux"
            )

        # Download and extract
        print("Downloading LLVM package...")

        # Download to managed directory
        tmp_archive_path: Path = _download(
            llvm_url, f"llvm-linux-{selected_version or 'unknown'}.tar.gz"
        )

        try:
            print("Extracting LLVM package...")
            with tarfile.open(tmp_archive_path, "r:gz") as tar:
                tar.extractall(target_dir, filter="data")
        finally:
            # Clean up immediately after extraction
            try:
                tmp_archive_path.unlink()
                print(f"Cleaned up: {tmp_archive_path}")
            except Exception as e:
                print(f"Warning: Failed to clean up {tmp_archive_path}: {e}")

        # Find the extracted directory and verify clang exists
        for extracted_dir in target_dir.iterdir():
            if extracted_dir.is_dir():
                clang_path = extracted_dir / "bin" / "clang"

                if clang_path.exists():
                    print(
                        f"OK: Successfully installed LLVM {selected_version if selected_version is not None else 'unknown'}"
                    )
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

    except KeyboardInterrupt:
        print("Operation interrupted by user")
        _thread.interrupt_main()
        raise
    except Exception as e:
        print(f"Error during installation: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


def main() -> None:
    """Setup LLVM toolchain - check system first, fallback to package installation."""
    # Get target directory from command line (optional)
    if len(sys.argv) > 2:
        print("Usage: uv run python ci/setup-llvm.py [target_directory]")
        print("  target_directory: Optional. Defaults to .cache/cc in project root")
        sys.exit(1)

    if len(sys.argv) == 2:
        target_dir = Path(sys.argv[1])
    else:
        # Default to project root .cache/cc directory
        target_dir = _PROJECT_ROOT / ".cache" / "cc"
        print(f"Using default target directory: {target_dir}")

    target_dir.mkdir(parents=True, exist_ok=True)

    # Check platform compatibility
    is_unix_like, platform_name = is_linux_or_unix()

    if platform_name == "windows":
        print("Checking for existing LLVM/Clang tools on system (Windows)...")

        # Check for existing installation in target directory
        has_artifacts, has_breadcrumb = check_existing_installation(target_dir)

        if has_artifacts and not has_breadcrumb:
            print("WARN: Found installation artifacts without completion marker")
            print("     This suggests an incomplete or corrupted installation")
            print("     Re-downloading LLVM to ensure proper installation...")
        elif has_artifacts and has_breadcrumb:
            if verify_clang_installation(target_dir):
                print("OK: Valid LLVM installation found in target directory")
                print("    Skipping download - clang installation verified")
                return
            else:
                print(
                    "WARN: Installation breadcrumb exists but clang verification failed"
                )
                print("     Re-downloading LLVM to fix installation...")

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
            create_breadcrumb(target_dir)

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

        # Try to auto-install on Windows via direct GitHub download
        print(f"ERROR: Missing core tools: {', '.join(missing_core)}")
        print("Downloading LLVM for Windows from GitHub releases...")
        extracted_bin_dir = install_llvm_windows_direct(target_dir)

        # Collect tools from the extracted bin directory and link/copy them
        tool_names: List[str] = [
            "clang",
            "clang++",
            "lld-link",
            "llvm-addr2line",
            "llvm-ar",
            "llvm-nm",
            "llvm-objdump",
            "llvm-strip",
            "llvm-objcopy",
            "clangd",
            "clang-format",
            "clang-tidy",
            "lldb",
        ]
        discovered: Dict[str, Optional[Path]] = find_tools_in_directories(
            tool_names, [extracted_bin_dir]
        )
        available_tools: Dict[str, Path] = {}
        for name, p in discovered.items():
            if p is not None:
                available_tools[name] = cast(Path, p)

        if not available_tools:
            print(
                "CRITICAL: Extracted LLVM bin directory does not contain expected tools."
            )
            sys.exit(1)

        print(f"Creating hard links in {target_dir}...")
        create_hard_links(available_tools, target_dir)

        version_file = target_dir / "VERSION"
        version_file.write_text("downloaded-llvm-windows\n")
        bin_dir = target_dir / "bin"
        existing = [p for p in bin_dir.iterdir() if p.is_file()]
        print(f"OK: Tools available in {bin_dir}: {len(existing)} files")
        create_breadcrumb(target_dir)
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
    try:
        main()
    finally:
        # Clean up download directory at the end
        _cleanup_download_dir()
