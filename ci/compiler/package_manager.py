"""Package management for FastLED PlatformIO builds."""

import json
import re
import subprocess
from pathlib import Path
from typing import TYPE_CHECKING, Any, Optional

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


if TYPE_CHECKING:
    from ci.boards import Board
    from ci.compiler.path_manager import FastLEDPaths


def find_platform_path_from_board(
    board: "Board", paths: "FastLEDPaths"
) -> Optional[Path]:
    """Find the platform path from board's platform URL using cache directory naming."""
    from ci.util.url_utils import sanitize_url_for_path

    if not board.platform:
        print(f"No platform URL defined for board {board.board_name}")
        return None

    print(f"Looking for platform cache: {board.platform}")

    # Convert platform URL to expected cache directory name
    expected_cache_name = sanitize_url_for_path(board.platform)
    print(f"Expected cache directory: {expected_cache_name}")

    # Search in global cache directory
    cache_dir = paths.global_platformio_cache_dir
    expected_cache_path = cache_dir / expected_cache_name / "extracted"

    if (
        expected_cache_path.exists()
        and (expected_cache_path / "platform.json").exists()
    ):
        print(f"Found platform cache: {expected_cache_path}")
        return expected_cache_path

    # Fallback: search for any directory that contains the platform name
    # Extract platform name from URL (e.g., "platform-espressif32" from github URL)
    platform_name = None
    if "platform-" in board.platform:
        # Extract platform name from URL path
        parts = board.platform.split("/")
        for part in parts:
            if (
                "platform-" in part
                and not part.endswith(".git")
                and not part.endswith(".zip")
            ):
                platform_name = part
                break

    if platform_name:
        print(f"Searching for platform by name: {platform_name}")
        for cache_item in cache_dir.glob(f"*{platform_name}*"):
            extracted_path = cache_item / "extracted"
            if extracted_path.exists() and (extracted_path / "platform.json").exists():
                print(f"Found platform by name search: {extracted_path}")
                return extracted_path

    print(f"Platform cache not found for {board.board_name}")
    return None


def get_platform_required_packages(platform_path: Path) -> list[str]:
    """Extract required package names from platform.json."""
    try:
        platform_json = platform_path / "platform.json"
        if not platform_json.exists():
            return []

        with open(platform_json, "r", encoding="utf-8") as f:
            data = json.load(f)

        packages = data.get("packages", {})
        # Return all package names from the platform
        return list(packages.keys())
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Warning: Could not parse platform.json: {e}")
        return []


def get_installed_packages_from_pio() -> dict[str, str]:
    """Get installed packages using PlatformIO CLI."""
    import os

    try:
        # Force UTF-8 encoding for PlatformIO subprocess to avoid Windows CP1252 encoding errors
        env = os.environ.copy()
        env["PYTHONIOENCODING"] = "utf-8"

        result = subprocess.run(
            ["pio", "pkg", "list", "--global"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
            timeout=30,
            env=env,
        )

        if result.returncode != 0:
            print(f"Warning: pio pkg list failed: {result.stderr}")
            return {}

        packages: dict[str, str] = {}
        # Parse output like: "├── framework-arduinoespressif32-libs @ 5.3.0+sha.083aad99cf"
        for line in result.stdout.split("\n"):
            match = re.search(r"[├└]── ([^@\s]+)\s*@\s*([^\s]+)", line)
            if match:
                package_name, version = match.groups()
                packages[package_name] = version

        return packages
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Warning: Could not get installed packages: {e}")
        return {}


def detect_package_exception_in_output(output: str) -> tuple[bool, Optional[str]]:
    """Detect PackageException errors in PlatformIO output.

    Returns:
        Tuple of (has_error, package_url_or_name)
    """
    if "PackageException" in output and "Could not install package" in output:
        # Try to extract the package URL or name
        match = re.search(
            r"Could not install package\s*['\"]?([^'\"]+)['\"]?\s*for", output
        )
        if match:
            return True, match.group(1)
        return True, None
    return False, None


def aggressive_clean_pio_packages(paths: "FastLEDPaths", board_name: str) -> bool:
    """Aggressively clean PlatformIO packages cache to recover from download failures.

    This removes all framework and toolchain packages to force re-download.
    Returns True if any packages were cleaned.
    """
    from ci.util.lock_handler import force_remove_path

    print("\n⚠️  Detected PackageException - Performing aggressive package cleanup...")
    packages_dir = paths.packages_dir

    if not packages_dir.exists():
        print(f"Packages directory doesn't exist: {packages_dir}")
        return False

    cleaned = False

    # List of package patterns that commonly cause issues
    problematic_patterns = [
        "framework-",  # All frameworks
        "tool-",  # All tools
        "esp32-arduino-libs",  # Specific ESP32 libs
        "esptoolpy",  # ESP tool
    ]

    for package_dir in packages_dir.iterdir():
        if not package_dir.is_dir():
            continue

        package_name = package_dir.name

        # Check if matches problematic patterns
        is_problematic = any(
            pattern in package_name.lower() for pattern in problematic_patterns
        )

        if is_problematic:
            print(f"  Removing problematic package: {package_name}")
            try:
                success = force_remove_path(package_dir, max_retries=5)
                if success:
                    print(f"    ✓ Removed {package_name}")
                    cleaned = True
                else:
                    print(f"    ✗ Failed to remove {package_name}")
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                print(f"    ✗ Error removing {package_name}: {e}")

    if cleaned:
        print(
            "  ✓ Package cleanup complete - PlatformIO will re-download on next build"
        )

    return cleaned


def detect_and_fix_corrupted_packages_dynamic(
    paths: "FastLEDPaths", board_name: str, platform_path: Optional[Path] = None
) -> dict[str, Any]:
    """Dynamically detect and fix corrupted packages based on platform requirements."""
    from ci.util.lock_handler import force_remove_path, is_psutil_available

    # Only print header when verbose or corruption is detected
    verbose = False  # Could be made configurable later

    if verbose:
        print("=== Dynamic Package Corruption Detection & Fix ===")
        print(f"Board: {board_name}")
        print(f"Packages dir: {paths.packages_dir}")

        if is_psutil_available():
            print("  Lock detection: psutil available ✓")
        else:
            print(
                "  Lock detection: psutil NOT available (install with: uv pip install psutil)"
            )

    results: dict[str, Any] = {}

    # Get required packages from platform.json if available
    platform_packages = []
    if platform_path and platform_path.exists():
        platform_packages = get_platform_required_packages(platform_path)
        if verbose:
            print(f"Platform packages found: {len(platform_packages)}")
            if platform_packages:
                print(
                    f"  Required packages: {', '.join(platform_packages[:5])}{'...' if len(platform_packages) > 5 else ''}"
                )

    # Get installed packages from PIO CLI
    installed_packages = get_installed_packages_from_pio()
    if verbose:
        print(f"Installed packages found: {len(installed_packages)}")

    # If we have platform info, focus on those packages, otherwise scan all installed
    packages_to_check = []
    if platform_packages:
        # Check intersection of platform requirements and installed packages
        packages_to_check = [
            pkg for pkg in platform_packages if pkg in installed_packages
        ]
        if verbose:
            print(
                f"Checking {len(packages_to_check)} packages that are both required and installed"
            )
    else:
        # Fallback: check all installed packages that look like frameworks
        packages_to_check = [
            pkg
            for pkg in installed_packages.keys()
            if "framework" in pkg.lower() or "toolchain" in pkg.lower()
        ]
        if verbose:
            print(
                f"Fallback: Checking {len(packages_to_check)} framework/toolchain packages"
            )

    if not packages_to_check:
        if verbose:
            print("No packages to check - using fallback hardcoded list")
        packages_to_check = ["framework-arduinoespressif32-libs", "tool-esptoolpy"]

    # Check each package for corruption (silent for OK packages, only print on issues)
    corrupted_count = 0
    for package_name in packages_to_check:
        if verbose:
            print(f"Checking package: {package_name}")
        package_path = paths.packages_dir / package_name
        if verbose:
            print(f"  Package path: {package_path}")

        exists = package_path.exists()
        piopm_exists = (package_path / ".piopm").exists() if exists else False
        manifest_exists = (package_path / "package.json").exists() if exists else False

        if verbose:
            print(f"  Package exists: {exists}")
            print(f"  .piopm exists: {piopm_exists}")
            print(f"  package.json exists: {manifest_exists}")

        # Two types of corruption:
        # 1. Has .piopm but missing package.json (installed but corrupted)
        # 2. Exists but has neither .piopm nor package.json (failed during installation)
        is_corrupted = exists and (
            (piopm_exists and not manifest_exists)  # Type 1
            or (not piopm_exists and not manifest_exists)  # Type 2
        )
        if is_corrupted:
            corrupted_count += 1
            # Only show header on first corruption
            if corrupted_count == 1:
                print("=== Fixing corrupted packages ===")
            print(f"  {package_name}: ", end="")
            if piopm_exists and not manifest_exists:
                print("CORRUPTED (missing package.json)")
            elif not piopm_exists and not manifest_exists:
                print("CORRUPTED (incomplete installation)")
            try:
                # Use lock-aware deletion that kills holding processes
                success = force_remove_path(package_path, max_retries=3)
                if success:
                    print(f"    -> Removed, will re-download")
                    results[package_name] = True  # Was corrupted, now fixed
                else:
                    print(f"    -> ERROR: Failed to remove after retries")
                    results[package_name] = False  # Still corrupted
                handle_keyboard_interrupt_properly()
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                print(f"    -> ERROR: {e}")
                results[package_name] = False  # Still corrupted
        else:
            if verbose:
                print("  -> OK: Not corrupted")
            results[package_name] = False  # Not corrupted

    if verbose:
        print("=== Dynamic Detection & Fix Complete ===")
    return results


def ensure_platform_installed(board: "Board") -> bool:
    """Ensure the required platform is installed for the board."""
    if not board.platform_needs_install:
        return True

    # Platform installation is handled by existing platform management code
    # This is a placeholder for future platform installation logic
    print(f"Platform installation needed for {board.board_name}: {board.platform}")
    return True
