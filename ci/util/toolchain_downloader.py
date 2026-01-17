from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Toolchain Pre-downloader for PlatformIO

Pre-downloads toolchains from platformio.ini into the global cache directory.
Handles GitHub URL conversion, breadcrumb tracking, and provides warnings for
toolchains that require manual installation.
"""

import logging
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from ci.compiler.platformio_ini import PackageInfo, PlatformIOIni
from ci.util.cache_lock import acquire_artifact_lock, force_unlock_cache
from ci.util.download_breadcrumb import (
    check_cached_download,
    create_download_breadcrumb,
    get_cache_artifact_dir,
)
from ci.util.github_url_converter import (
    convert_github_url_to_zip,
    format_manual_install_warning,
)
from ci.util.resumable_downloader import ResumableDownloader


logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO, format="%(message)s")


@dataclass
class ToolchainDownloadResult:
    """Result of a toolchain download operation."""

    package_name: str
    url: str
    converted_url: Optional[str] = None
    cache_dir: Optional[Path] = None
    success: bool = False
    needs_manual_install: bool = False
    error_message: Optional[str] = None


def extract_toolchains_from_platformio_ini(
    platformio_ini_path: Path,
) -> list[PackageInfo]:
    """
    Extract all toolchain packages from a platformio.ini file.

    Args:
        platformio_ini_path: Path to platformio.ini file

    Returns:
        List of PackageInfo objects for all toolchains
    """
    try:
        pio_ini = PlatformIOIni.parseFile(platformio_ini_path)
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        logger.error(f"Failed to parse {platformio_ini_path}: {e}")
        return []

    all_packages: list[PackageInfo] = []

    # Get all platform URLs from the ini file
    for section_name, option_name, platform_value in pio_ini.get_platform_urls():
        if not platform_value:
            continue

        # Resolve platform to get package information
        # Use force_resolve=True to query PlatformIO even for direct URLs
        try:
            resolution = pio_ini.resolve_platform_url_enhanced(
                platform_value, force_resolve=True
            )
            if resolution and resolution.packages:
                logger.info(
                    f"Found {len(resolution.packages)} packages for platform {platform_value}"
                )
                all_packages.extend(resolution.packages)
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            logger.warning(
                f"Failed to resolve platform {platform_value} in {section_name}: {e}"
            )
            continue

    return all_packages


def download_toolchain(
    package: PackageInfo, cache_dir: Path, downloader: ResumableDownloader
) -> ToolchainDownloadResult:
    """
    Download a single toolchain package.

    Args:
        package: Package information
        cache_dir: Cache directory for downloads
        downloader: ResumableDownloader instance

    Returns:
        ToolchainDownloadResult with download status
    """
    result = ToolchainDownloadResult(package_name=package.name, url="")

    # Get download URL from package
    try:
        download_url = package.get_download_url()
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        result.error_message = f"Failed to resolve download URL: {e}"
        return result

    if not download_url:
        result.error_message = "No download URL available"
        return result

    result.url = download_url

    # Try to convert GitHub URLs to zip format
    converted_url, needs_manual = convert_github_url_to_zip(download_url)

    if needs_manual:
        result.needs_manual_install = True
        result.error_message = "Requires manual git installation"
        return result

    # Use converted URL if available, otherwise use original
    final_url = converted_url if converted_url else download_url
    result.converted_url = final_url if converted_url else None

    # Acquire fine-grained lock for this specific artifact (5-second timeout with stale detection)
    try:
        with acquire_artifact_lock(
            cache_dir, final_url, operation="download", timeout=5.0
        ):
            # Check cache again (another process might have completed while we waited for lock)
            cached_artifact = check_cached_download(
                cache_dir, final_url, expected_filename="toolchain.zip"
            )

            if cached_artifact:
                logger.info(f"✅ Using cached toolchain: {package.name}")
                result.success = True
                result.cache_dir = cached_artifact
                return result

            # Download the toolchain inside lock
            artifact_dir = get_cache_artifact_dir(cache_dir, final_url)
            artifact_dir.mkdir(parents=True, exist_ok=True)
            toolchain_path = artifact_dir / "toolchain.zip"

            logger.info(f"📦 Downloading toolchain: {package.name}")
            logger.info(f"   URL: {final_url}")

            downloader.download(final_url, toolchain_path)

            # Create breadcrumb inside lock
            create_download_breadcrumb(
                cache_dir,
                final_url,
                status="complete",
                package_name=package.name,
                package_type=package.type or "toolchain",
                original_url=download_url,
                converted_url=converted_url,
                size_bytes=(
                    toolchain_path.stat().st_size if toolchain_path.exists() else 0
                ),
            )

            result.success = True
            result.cache_dir = artifact_dir
            logger.info(f"✅ Successfully downloaded: {package.name}")

    except TimeoutError as e:
        result.error_message = f"Lock timeout: {e}"
        logger.error(f"❌ Failed to acquire lock for {package.name}: {e}")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        result.error_message = str(e)
        logger.error(f"❌ Failed to download {package.name}: {e}")

    return result


def predownload_toolchains(
    platformio_ini_path: Path,
    cache_dir: Optional[Path] = None,
    max_workers: int = 4,
) -> dict[str, ToolchainDownloadResult]:
    """
    Pre-download all toolchains from a platformio.ini file.

    Args:
        platformio_ini_path: Path to platformio.ini file
        cache_dir: Cache directory (defaults to ~/.platformio/global_cache)
        max_workers: Maximum number of concurrent downloads

    Returns:
        Dictionary mapping package names to download results
    """
    if cache_dir is None:
        cache_dir = Path.home() / ".platformio" / "global_cache"

    cache_dir.mkdir(parents=True, exist_ok=True)

    logger.info(f"🔍 Extracting toolchains from: {platformio_ini_path}")
    logger.info(f"📁 Cache directory: {cache_dir}")

    # Extract all packages
    packages = extract_toolchains_from_platformio_ini(platformio_ini_path)

    if not packages:
        logger.warning("No packages found in platformio.ini")
        return {}

    logger.info(f"Found {len(packages)} total packages")

    # Deduplicate by name and version
    unique_packages: dict[str, PackageInfo] = {}
    for package in packages:
        key = f"{package.name}:{package.version or 'latest'}"
        if key not in unique_packages:
            unique_packages[key] = package

    logger.info(f"Processing {len(unique_packages)} unique packages")

    # Download toolchains concurrently
    results: dict[str, ToolchainDownloadResult] = {}
    downloader = ResumableDownloader(chunk_size=8192, max_retries=5)

    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {
            executor.submit(download_toolchain, package, cache_dir, downloader): package
            for package in unique_packages.values()
        }

        for future in as_completed(futures):
            package = futures[future]
            try:
                result = future.result()
                results[package.name] = result

                # Print warnings for manual install packages
                if result.needs_manual_install:
                    print(format_manual_install_warning(package.name, result.url))

            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                logger.warning("Download interrupted by user")
                raise
            except Exception as e:
                logger.error(f"Unexpected error processing {package.name}: {e}")
                results[package.name] = ToolchainDownloadResult(
                    package_name=package.name,
                    url="",
                    success=False,
                    error_message=str(e),
                )

    # Print summary
    print("\n" + "=" * 60)
    print("📊 DOWNLOAD SUMMARY")
    print("=" * 60)

    successful = sum(1 for r in results.values() if r.success)
    manual_install = sum(1 for r in results.values() if r.needs_manual_install)
    failed = sum(
        1 for r in results.values() if not r.success and not r.needs_manual_install
    )

    print(f"✅ Successfully downloaded: {successful}")
    print(f"⚠️  Require manual install: {manual_install}")
    print(f"❌ Failed: {failed}")

    return results


def main() -> int:
    """Main entry point for command-line usage."""
    import argparse

    parser = argparse.ArgumentParser(
        description="Pre-download toolchains from platformio.ini"
    )
    parser.add_argument(
        "platformio_ini",
        type=Path,
        help="Path to platformio.ini file",
    )
    parser.add_argument(
        "--cache-dir",
        type=Path,
        default=None,
        help="Cache directory (default: ~/.platformio/global_cache)",
    )
    parser.add_argument(
        "--max-workers",
        type=int,
        default=4,
        help="Maximum number of concurrent downloads (default: 4)",
    )
    parser.add_argument(
        "--force-unlock",
        action="store_true",
        help="Break all locks before starting (recovery from crashes)",
    )

    args = parser.parse_args()

    if not args.platformio_ini.exists():
        logger.error(f"platformio.ini not found: {args.platformio_ini}")
        return 1

    # Get cache directory
    cache_dir = (
        args.cache_dir
        if args.cache_dir
        else Path.home() / ".platformio" / "global_cache"
    )

    # Handle force unlock if requested
    if args.force_unlock:
        logger.info("🔓 Breaking all locks in cache directory...")
        broken = force_unlock_cache(cache_dir)
        if broken > 0:
            logger.info(f"   Broke {broken} lock(s)")
        else:
            logger.info("   No locks found")

    try:
        results = predownload_toolchains(
            args.platformio_ini,
            cache_dir=cache_dir,
            max_workers=args.max_workers,
        )

        # Return non-zero if any downloads failed
        failed_count = sum(
            1 for r in results.values() if not r.success and not r.needs_manual_install
        )
        return 1 if failed_count > 0 else 0

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        logger.warning("\nInterrupted by user")
        return 130
    except Exception as e:
        logger.error(f"Fatal error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
