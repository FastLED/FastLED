#!/usr/bin/env python3
"""
PlatformIO artifact cache implementation for speeding up CI builds.

This module implements the cache mechanism described in FEATURE_PIO_SPEEDUP.md,
providing functionality to:
1. Parse platformio.ini files with zip URLs
2. Download and cache platform/framework artifacts
3. Install artifacts via PlatformIO CLI
4. Modify platformio.ini in-place with resolved local paths
"""

import _thread
import configparser
import json
import logging
import os
import shutil
import subprocess
import tempfile
import threading
import time
import urllib.parse
import zipfile
from concurrent.futures import Future, ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from types import TracebackType
from typing import Any, Dict, List, Optional, Tuple, Union, cast

import fasteners
import httpx

from ci.compiler.platformio_ini import PlatformIOIni
from ci.util.running_process import RunningProcess
from ci.util.url_utils import sanitize_url_for_path


@dataclass
class DownloadResult:
    """Result of a download operation."""

    url: str
    temp_path: Path
    exception: Optional[BaseException] = None

    @property
    def success(self) -> bool:
        """True if download succeeded."""
        return self.exception is None


@dataclass
class ArtifactProcessingResult:
    """Result of processing an artifact."""

    url: str
    is_framework: bool
    env_section: str
    resolved_path: Optional[str] = None
    exception: Optional[BaseException] = None

    @property
    def success(self) -> bool:
        """True if processing succeeded."""
        return self.exception is None and self.resolved_path is not None


@dataclass
class ZipUrlInfo:
    """Information about a zip URL found in platformio.ini."""

    url: str
    section_name: str
    option_name: str


@dataclass
class ManifestResult:
    """Result of manifest validation and type detection."""

    is_valid: bool
    manifest_path: Optional[Path]
    is_framework: bool


def _get_remote_file_size(url: str) -> Optional[int]:
    """Get file size from URL using HEAD request."""
    try:
        parsed_url = urllib.parse.urlparse(url)
        if parsed_url.scheme in ("http", "https"):
            with httpx.Client(follow_redirects=True) as client:
                response = client.head(
                    url, headers={"User-Agent": "PlatformIO-Cache/1.0"}
                )
                response.raise_for_status()
                content_length = response.headers.get("Content-Length")
                if content_length:
                    return int(content_length)
    except Exception as e:
        logger.debug(f"Failed to get file size for {url}: {e}")
    return None


def _format_file_size(size_bytes: Optional[int]) -> str:
    """Format file size in human-readable format."""
    if size_bytes is None:
        return "unknown size"

    if size_bytes < 1024:
        return f"{size_bytes} B"
    elif size_bytes < 1024 * 1024:
        return f"{size_bytes / 1024:.1f} KB"
    elif size_bytes < 1024 * 1024 * 1024:
        return f"{size_bytes / (1024 * 1024):.1f} MB"
    else:
        return f"{size_bytes / (1024 * 1024 * 1024):.1f} GB"


# Global cache to track PlatformIO installations in this session
_session_installation_cache: set[str] = set()

# Global cancellation event for handling keyboard interrupts
_global_cancel_event = threading.Event()

# Lock for thread-safe operations
_download_lock = threading.Lock()


def _get_status_file(artifact_dir: Path, cache_key: str) -> Path:
    """Get the JSON status file path for an artifact."""
    # Use simple descriptive filename since artifacts are already in unique directories
    return artifact_dir / "info.json"


def _read_status(status_file: Path) -> Optional[Dict[str, Any]]:
    """Read status from JSON file."""
    if not status_file.exists():
        return None

    try:
        with open(status_file, "r") as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError):
        return None


def _write_status(status_file: Path, status: Dict[str, Any]) -> None:
    """Write status to JSON file."""
    try:
        with open(status_file, "w") as f:
            json.dump(status, f, indent=2)
    except OSError as e:
        logger.warning(f"Failed to write status file {status_file}: {e}")


def _is_processing_complete(status_file: Path) -> bool:
    """Check if processing is complete based on status file."""
    status = _read_status(status_file)
    return status is not None and status.get("status") == "complete"


def _download_with_progress(
    url: str, temp_path: Path, cancel_event: threading.Event
) -> DownloadResult:
    """Download HTTP/HTTPS file with cancellation support."""
    try:
        # HTTP/HTTPS download using httpx with streaming
        with httpx.Client(follow_redirects=True, timeout=30.0) as client:
            with client.stream(
                "GET", url, headers={"User-Agent": "PlatformIO-Cache/1.0"}
            ) as response:
                response.raise_for_status()

                with open(temp_path, "wb") as f:
                    for chunk in response.iter_bytes(chunk_size=8192):
                        # Check for cancellation periodically
                        if cancel_event.is_set():
                            cancelled_error = RuntimeError(
                                f"Download cancelled for {url}"
                            )
                            logger.warning(
                                f"Download cancelled for {url}",
                                exc_info=cancelled_error,
                            )
                            return DownloadResult(url, temp_path, cancelled_error)
                        f.write(chunk)

        return DownloadResult(url, temp_path)  # Success

    except KeyboardInterrupt as e:
        # Set cancel event and interrupt main thread
        cancel_event.set()
        _thread.interrupt_main()
        logger.warning(
            f"Download interrupted by KeyboardInterrupt for {url}", exc_info=e
        )
        return DownloadResult(url, temp_path, e)
    except Exception as e:
        if not cancel_event.is_set():
            logger.error(f"Download failed for {url}: {e}", exc_info=e)
        else:
            logger.warning(f"Download failed for {url} (cancelled): {e}", exc_info=e)
        return DownloadResult(url, temp_path, e)


def _copy_file_with_progress(
    url: str, temp_path: Path, cancel_event: threading.Event
) -> DownloadResult:
    """Copy local file with cancellation support."""
    try:
        parsed_url = urllib.parse.urlparse(url)

        # File URL - copy local file
        logger.debug(f"Parsing file URL: {url}")
        logger.debug(f"Parsed URL path: {parsed_url.path}")

        # Handle both Unix and Windows file URLs
        if os.name == "nt":  # Windows
            # On Windows, file:///C:/path becomes /C:/path, so remove leading slash
            if (
                parsed_url.path.startswith("/")
                and len(parsed_url.path) > 3
                and parsed_url.path[2] == ":"
            ):
                source_path = Path(parsed_url.path[1:])
            else:
                source_path = Path(parsed_url.path)
        else:  # Unix-like
            source_path = Path(parsed_url.path)

        logger.debug(f"Resolved file path: {source_path}")

        if not source_path.exists():
            raise FileNotFoundError(f"Source file not found: {source_path}")

        # Check for cancellation before copy
        if cancel_event.is_set():
            cancelled_error = RuntimeError(f"Copy cancelled for {url}")
            logger.warning(f"Copy cancelled for {url}", exc_info=cancelled_error)
            return DownloadResult(url, temp_path, cancelled_error)

        shutil.copy2(source_path, temp_path)
        return DownloadResult(url, temp_path)  # Success

    except KeyboardInterrupt as e:
        # Set cancel event and interrupt main thread
        cancel_event.set()
        _thread.interrupt_main()
        logger.warning(f"Copy interrupted by KeyboardInterrupt for {url}", exc_info=e)
        return DownloadResult(url, temp_path, e)
    except Exception as e:
        if not cancel_event.is_set():
            logger.error(f"Copy failed for {url}: {e}", exc_info=e)
        else:
            logger.warning(f"Copy failed for {url} (cancelled): {e}", exc_info=e)
        return DownloadResult(url, temp_path, e)


def clear_session_cache() -> None:
    """Clear the session installation cache. Useful for testing."""
    global _session_installation_cache
    _session_installation_cache.clear()
    logger.debug("Cleared session installation cache")


# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Reduce httpx verbosity to avoid log spew from HTTP requests
logging.getLogger("httpx").setLevel(logging.WARNING)


class PlatformIOCache:
    """Enhanced cache manager for PlatformIO artifacts."""

    def __init__(self, cache_dir: Path):
        """Initialize cache manager with directory structure."""
        self.cache_dir = cache_dir
        # Simplified structure: each artifact gets its own directory directly in cache root
        # containing both the .zip and extracted/ folder

        # Create directory structure
        self.cache_dir.mkdir(parents=True, exist_ok=True)

    def _get_cache_key(self, url: str) -> str:
        """Generate cache key from URL - sanitized for filesystem use."""
        return str(sanitize_url_for_path(url))

    def download_artifact(self, url: str) -> str:
        """
        Download and cache an artifact from the given URL.
        Returns the absolute path to the cached zip file.
        """
        cache_key = self._get_cache_key(url)
        # Each artifact gets its own directory
        artifact_dir = self.cache_dir / cache_key
        artifact_dir.mkdir(parents=True, exist_ok=True)
        cached_path = artifact_dir / "artifact.zip"

        # Use read-write locking for concurrent safety
        # Start with write lock for downloading, can upgrade to read if cache hit
        lock_path = str(artifact_dir / "artifact.lock")
        rw_lock = fasteners.InterProcessReaderWriterLock(lock_path)
        with rw_lock.write_lock():
            if cached_path.exists():
                # Check if processing was completed successfully
                status_file = _get_status_file(artifact_dir, cache_key)
                if not _is_processing_complete(status_file):
                    logger.warning(
                        f"Cache incomplete (no completion status), re-downloading: {url}"
                    )
                    # Remove incomplete zip file
                    cached_path.unlink()
                    # Also remove any partial extraction
                    extracted_dir = artifact_dir / "extracted"
                    if extracted_dir.exists():
                        shutil.rmtree(extracted_dir)
                else:
                    print(f"Using cached artifact: {cached_path}")
                    # Cache hit - return the cached path (write lock will be released)
                    return str(cached_path)

            # Check URL scheme to determine action
            parsed_url = urllib.parse.urlparse(url)

            if parsed_url.scheme in ("http", "https"):
                # Download to temporary file first (atomic operation)
                file_size = _get_remote_file_size(url)
                size_str = _format_file_size(file_size)
                print(f"Downloading: {url} ({size_str})")
                temp_file_handle = tempfile.NamedTemporaryFile(
                    delete=False, suffix=".zip"
                )
                temp_path = Path(temp_file_handle.name)
                temp_file_handle.close()  # Close immediately to avoid Windows file locking issues

                # Create a thread-local cancel event
                thread_cancel_event = threading.Event()

                # Download directly (we're already in a thread from the main pool)
                download_result = _download_with_progress(
                    url, temp_path, thread_cancel_event
                )
            elif parsed_url.scheme == "file":
                # File URL - copy from local file
                print(f"Copying from local file: {url}")
                temp_file_handle = tempfile.NamedTemporaryFile(
                    delete=False, suffix=".zip"
                )
                temp_path = Path(temp_file_handle.name)
                temp_file_handle.close()  # Close immediately to avoid Windows file locking issues

                # Create a thread-local cancel event
                thread_cancel_event = threading.Event()

                # Copy directly (we're already in a thread from the main pool)
                download_result = _copy_file_with_progress(
                    url, temp_path, thread_cancel_event
                )
            else:
                raise ValueError(f"Unsupported URL scheme: {parsed_url.scheme}")

            try:
                # Check if global cancellation was requested
                if _global_cancel_event.is_set():
                    raise KeyboardInterrupt(
                        "Download cancelled due to global interrupt"
                    )

                # Process the download result
                if not download_result.success:
                    if download_result.exception is not None:
                        raise download_result.exception
                    else:
                        raise RuntimeError(
                            f"Download failed for unknown reason: {download_result.url}"
                        )
                else:
                    print(f"Download completed successfully: {download_result.url}")

                # Atomic move to final location
                shutil.move(str(temp_path), str(cached_path))
                print(f"Successfully cached: {cached_path}")
                return str(cached_path)

            except Exception as e:
                logger.error(f"Download failed: {e}")
                if temp_path.exists():
                    temp_path.unlink()
                raise


def _is_zip_web_url(value: str) -> bool:
    """Enhanced URL detection for zip artifacts."""
    if not isinstance(value, str):
        return False

    parsed = urllib.parse.urlparse(value)
    # Direct zip URLs
    if value.endswith(".zip"):
        return parsed.scheme in ("http", "https")
    return False


def validate_and_detect_manifest(
    content_path: Path,
) -> ManifestResult:
    """
    Validate manifest files and auto-detect artifact type.
    """
    # Try framework manifests first
    framework_manifests = ["framework.json", "package.json"]
    for manifest in framework_manifests:
        manifest_path = content_path / manifest
        if manifest_path.exists():
            try:
                with open(manifest_path, "r") as f:
                    json.load(f)  # Validate JSON syntax
                print(f"Found valid framework manifest: {manifest_path}")
                return ManifestResult(
                    is_valid=True, manifest_path=manifest_path, is_framework=True
                )
            except json.JSONDecodeError as e:
                logger.warning(f"Invalid JSON in {manifest_path}: {e}")

    # Try platform manifest
    platform_manifest = content_path / "platform.json"
    if platform_manifest.exists():
        try:
            with open(platform_manifest, "r") as f:
                json.load(f)  # Validate JSON syntax
            print(f"Found valid platform manifest: {platform_manifest}")
            return ManifestResult(
                is_valid=True, manifest_path=platform_manifest, is_framework=False
            )
        except json.JSONDecodeError as e:
            logger.warning(f"Invalid JSON in {platform_manifest}: {e}")

    return ManifestResult(is_valid=False, manifest_path=None, is_framework=False)


def get_platformio_command_path(path: Path) -> str:
    """Get path format for PlatformIO command line usage."""
    import platform

    resolved_path = path.resolve()

    # On Windows, PlatformIO has issues with file:// URLs for local directories
    # Use native Windows paths instead
    if platform.system() == "Windows":
        return str(resolved_path)
    else:
        # On Unix systems, use proper file:// URLs
        posix_path = resolved_path.as_posix()
        return f"file://{posix_path}"


def get_proper_file_url(path: Path) -> str:
    """Convert a path to a proper file:// URL for platformio.ini files."""
    # For PlatformIO on Windows, use the Windows path directly (not file:// URL)
    # This avoids issues with path parsing in PlatformIO
    import platform

    resolved_path = path.resolve()

    if platform.system() == "Windows":
        # On Windows, PlatformIO handles Windows paths directly better than file:// URLs
        # Convert to Windows path format
        return str(resolved_path)
    else:
        # On Unix systems, use proper file:// URL format
        posix_path = resolved_path.as_posix()
        return f"file://{posix_path}"


def unzip_and_install(
    cached_zip_path: Path,
    cache_manager: PlatformIOCache,
    is_framework: bool,
    env_section: str,
) -> bool:
    """
    Enhanced unzip and install with manifest validation and cleanup.
    """
    cached_zip_path_obj = cached_zip_path
    # Extract to a directory alongside the zip file
    artifact_dir = cached_zip_path_obj.parent
    extracted_dir = artifact_dir / "extracted"
    temp_unzip_dir = artifact_dir / "temp_extract"

    try:
        # Clean extraction directory
        if temp_unzip_dir.exists():
            shutil.rmtree(temp_unzip_dir)
        temp_unzip_dir.mkdir(parents=True)

        print(f"Extracting {cached_zip_path_obj} to {temp_unzip_dir}")

        # Extract with better error handling
        try:
            with zipfile.ZipFile(cached_zip_path_obj, "r") as zip_ref:
                # Check for zip bombs (basic protection)
                total_size = sum(info.file_size for info in zip_ref.infolist())
                if total_size > 500 * 1024 * 1024:  # 500MB limit
                    raise ValueError("Archive too large, possible zip bomb")

                zip_ref.extractall(temp_unzip_dir)
            print("Extraction completed successfully")

        except zipfile.BadZipFile:
            logger.error(f"Invalid zip file: {cached_zip_path_obj}")
            return False
        except Exception as e:
            logger.error(f"Extraction failed: {e}")
            return False

        # Find content directory (handle nested structures like GitHub archives)
        content_items = list(temp_unzip_dir.iterdir())
        if len(content_items) == 1 and content_items[0].is_dir():
            # Single root directory (common with GitHub/GitLab archives) - use it
            unzipped_content_path = content_items[0]
            print(f"Found nested directory structure: {content_items[0].name}")
        else:
            # Multiple items at root - use temp dir
            unzipped_content_path = temp_unzip_dir
            print("Using flat directory structure")

        # Validate manifest files and auto-detect type
        manifest_result = validate_and_detect_manifest(unzipped_content_path)
        if not manifest_result.is_valid:
            logger.error(f"No valid manifest found in {unzipped_content_path}")
            return False

        # Move to final location
        if extracted_dir.exists():
            shutil.rmtree(extracted_dir)
        shutil.move(unzipped_content_path, extracted_dir)

        # Install via PlatformIO
        return install_with_platformio(
            extracted_dir, manifest_result.is_framework, env_section
        )

    finally:
        # Cleanup temporary extraction
        if temp_unzip_dir.exists():
            print(f"Cleaning up extraction directory: {temp_unzip_dir}")
            shutil.rmtree(temp_unzip_dir)


def install_with_platformio(
    content_path: Path, is_framework: bool, env_section: str
) -> bool:
    """Install extracted content using appropriate PlatformIO command."""
    content_path_obj = Path(content_path)
    command_path = get_platformio_command_path(content_path_obj)

    # Create a cache key based on the installation type and path
    cache_key = f"{'framework' if is_framework else 'platform'}:{command_path}"

    # Check if we've already installed this in this session
    if cache_key in _session_installation_cache:
        print(
            f"Skipping installation for {env_section}: already installed in this session ({command_path})"
        )
        return True

    if is_framework:
        # Framework installation - use pkg install
        command = ["pio", "pkg", "install", "--global", command_path]
    else:
        # Platform installation - also use pkg install (new recommended way)
        command = ["pio", "pkg", "install", "--global", "--platform", command_path]

    try:
        print(f"Installing for {env_section}: {' '.join(command)}")

        # Use RunningProcess for streaming output
        process = RunningProcess(
            command,
            check=False,  # We'll handle errors ourselves
            timeout=300,  # 5 minute timeout
        )

        # Stream output in real-time
        for line in process.line_iter(timeout=300):
            line_str = cast(str, line)
            print(f"PIO: {line_str}")

        # Wait for completion and check result
        process.wait()

        if process.returncode == 0:
            print("PlatformIO installation successful")
            # Add to session cache to avoid redundant installations
            _session_installation_cache.add(cache_key)
            return True
        else:
            logger.error(
                f"PlatformIO installation failed with return code: {process.returncode}"
            )
            return False

    except Exception as e:
        if "No such file or directory" in str(e) or "not found" in str(e).lower():
            logger.error("PlatformIO CLI not found. Is it installed and in PATH?")
        else:
            logger.error(f"PlatformIO installation failed: {e}")
        logger.error(f"Command: {' '.join(command)}")
        return False


def handle_zip_artifact(
    zip_source: str,
    cache_manager: PlatformIOCache,
    env_section: str,
) -> str:
    """
    Enhanced artifact handler with validation and error recovery.
    Returns the resolved local path for the artifact.
    """
    try:
        # Download and cache the artifact
        cached_zip_path = cache_manager.download_artifact(zip_source)

        if not cached_zip_path or not Path(cached_zip_path).exists():
            raise FileNotFoundError(f"Download failed for {zip_source}")

        # Extract and get the content path
        cache_key = cache_manager._get_cache_key(zip_source)
        cached_zip_path_obj = Path(cached_zip_path)

        # Extract to a directory alongside the zip file
        artifact_dir = cached_zip_path_obj.parent
        extracted_dir = artifact_dir / "extracted"
        temp_unzip_dir = artifact_dir / "temp_extract"

        # Check if already extracted
        if extracted_dir.exists():
            # Validate existing extraction and auto-detect type
            manifest_result = validate_and_detect_manifest(extracted_dir)
            if manifest_result.is_valid:
                print(f"Using existing extraction: {extracted_dir}")

                # Check for completion status using URL-based cache key (consistent with status file creation)
                url_cache_key = cache_manager._get_cache_key(zip_source)
                status_file = _get_status_file(artifact_dir, url_cache_key)
                if _is_processing_complete(status_file):
                    print(
                        f"Skipping PlatformIO installation for {env_section}: found completion status"
                    )
                    return get_proper_file_url(extracted_dir)

                # Create a session cache key for this artifact
                session_cache_key = f"{'framework' if manifest_result.is_framework else 'platform'}:{get_platformio_command_path(extracted_dir)}"

                # If we've already handled this exact artifact in this session, skip PlatformIO entirely
                if session_cache_key in _session_installation_cache:
                    print(
                        f"Skipping PlatformIO installation for {env_section}: already processed in this session"
                    )
                    return get_proper_file_url(extracted_dir)

                # Otherwise, install via PlatformIO
                install_success = install_with_platformio(
                    extracted_dir, manifest_result.is_framework, env_section
                )
                # Status file will be created later using URL-based cache key

                if install_success:
                    print(f"Successfully installed {zip_source} for {env_section}")
                else:
                    logger.warning(
                        f"Installation completed with warnings for {zip_source}"
                    )
                return get_proper_file_url(extracted_dir)

        # Clean extraction directory for fresh extraction
        if temp_unzip_dir.exists():
            shutil.rmtree(temp_unzip_dir)
        temp_unzip_dir.mkdir(parents=True)

        # Extract the zip
        with zipfile.ZipFile(cached_zip_path, "r") as zip_ref:
            zip_ref.extractall(temp_unzip_dir)

        # Find content directory (handle nested structures)
        content_items = list(temp_unzip_dir.iterdir())
        if len(content_items) == 1 and content_items[0].is_dir():
            unzipped_content_path = content_items[0]
        else:
            unzipped_content_path = temp_unzip_dir

        # Validate manifest files and auto-detect type
        manifest_result = validate_and_detect_manifest(unzipped_content_path)
        if not manifest_result.is_valid:
            raise ValueError(f"No valid manifest found in {unzipped_content_path}")

        # Move to final location
        if extracted_dir.exists():
            shutil.rmtree(extracted_dir)
        shutil.move(unzipped_content_path, extracted_dir)

        # Clean up temp directory
        if temp_unzip_dir.exists():
            shutil.rmtree(temp_unzip_dir)

        # Install via PlatformIO
        install_success = install_with_platformio(
            extracted_dir, manifest_result.is_framework, env_section
        )

        # Create status file with processing results
        cache_key = cache_manager._get_cache_key(zip_source)
        status_file = _get_status_file(artifact_dir, cache_key)

        # Get zip file size
        zip_file = artifact_dir / "artifact.zip"
        zip_size = zip_file.stat().st_size if zip_file.exists() else 0

        status = {
            "status": "complete" if install_success else "warning",
            "timestamp": datetime.now().isoformat(),
            "url": zip_source,
            "env_section": env_section,
            "extracted_dir": str(extracted_dir.relative_to(artifact_dir)),
            "zip_size_bytes": zip_size,
        }
        _write_status(status_file, status)

        if install_success:
            print(f"Successfully installed {zip_source} for {env_section}")
        else:
            logger.warning(f"Installation completed with warnings for {zip_source}")

        # Return the file URL for the extracted directory
        return get_proper_file_url(extracted_dir)

    except Exception as e:
        import traceback

        traceback.print_exc()
        logger.error(f"Failed to handle artifact {zip_source}: {e}")
        raise


def _process_artifact(
    artifact_url: str,
    env_section: str,
    cache_manager: "PlatformIOCache",
) -> ArtifactProcessingResult:
    """Process a single artifact (download, extract, install)."""
    try:
        resolved_path = handle_zip_artifact(artifact_url, cache_manager, env_section)
        return ArtifactProcessingResult(
            url=artifact_url,
            is_framework=False,  # This will be determined during processing
            env_section=env_section,
            resolved_path=resolved_path,
        )
    except Exception as e:
        logger.error(f"Failed to process {artifact_url}: {e}", exc_info=e)
        return ArtifactProcessingResult(
            url=artifact_url,
            is_framework=False,  # This will be determined during processing
            env_section=env_section,
            exception=e,
        )


def _collect_all_zip_urls(pio_ini: PlatformIOIni) -> List[ZipUrlInfo]:
    """
    Collect all zip URLs from platformio.ini in a single pass.
    """
    all_urls: List[ZipUrlInfo] = []

    # Collect platform URLs
    for section_name, option_name, url in pio_ini.get_platform_urls():
        if _is_zip_web_url(url):
            print(f"Found platform zip: {url} in {section_name}")
            all_urls.append(ZipUrlInfo(url, section_name, option_name))

    # Collect framework URLs
    for section_name, option_name, url in pio_ini.get_framework_urls():
        if _is_zip_web_url(url):
            print(f"Found framework zip: {url} in {section_name}")
            all_urls.append(ZipUrlInfo(url, section_name, option_name))

    return all_urls


def _dedupe_urls(
    all_urls: List[ZipUrlInfo],
) -> Dict[str, str]:
    """
    Deduplicate URLs, keeping the first occurrence of each unique URL.
    Returns dict mapping url -> env_section.
    """
    unique_urls: Dict[str, str] = {}
    for url_info in all_urls:
        if url_info.url not in unique_urls:
            unique_urls[url_info.url] = url_info.section_name
    return unique_urls


def _download_and_process_urls(
    unique_urls: Dict[str, str], cache_manager: PlatformIOCache
) -> Dict[str, str]:
    """
    Download and process all URLs concurrently.
    Returns dict mapping original_url -> resolved_local_path.
    Raises exception immediately if any download fails.
    """
    if not unique_urls:
        return {}

    max_workers = min(4, len(unique_urls))  # Don't create more threads than needed
    with ThreadPoolExecutor(
        max_workers=max_workers, thread_name_prefix="download"
    ) as executor:
        # Submit all download tasks
        futures: List[Future[ArtifactProcessingResult]] = []
        for url, env_section in unique_urls.items():
            future = executor.submit(_process_artifact, url, env_section, cache_manager)
            futures.append(future)

        replacements: Dict[str, str] = {}

        try:
            for future in as_completed(futures):
                try:
                    result = future.result()
                    if result.success and result.resolved_path is not None:
                        replacements[result.url] = result.resolved_path
                        print(f"✅ Resolved {result.url} -> {result.resolved_path}")
                    else:
                        logger.error(
                            f"❌ Failed to process {result.url}: {result.exception}"
                        )
                        # Re-raise all exceptions - no downloads should fail silently
                        if result.exception:
                            raise result.exception
                except Exception as e:
                    logger.error(
                        f"Future failed with unexpected error: {e}", exc_info=e
                    )
                    raise  # Re-raise unexpected exceptions
        except KeyboardInterrupt:
            logger.warning("Processing interrupted, cancelling remaining downloads...")
            _global_cancel_event.set()
            # The context manager will handle cleanup of the executor
            raise

        return replacements


def _replace_all_urls(
    pio_ini: PlatformIOIni,
    all_urls: List[ZipUrlInfo],
    replacements: Dict[str, str],
) -> None:
    """
    Replace all URLs in platformio.ini with their resolved local paths.
    """
    for url_info in all_urls:
        if url_info.url in replacements:
            pio_ini.replace_url(
                url_info.section_name,
                url_info.option_name,
                url_info.url,
                replacements[url_info.url],
            )


def _apply_board_specific_config(
    platformio_ini_path: Path, custom_zip_cache_dir: Path
) -> None:
    """
    Enhanced config parser with improved zip detection and error handling.
    Modifies platformio.ini in-place with resolved local paths.
    Uses concurrent downloads for better performance.
    """
    cache_manager = PlatformIOCache(custom_zip_cache_dir)

    try:
        pio_ini = PlatformIOIni.parseFile(platformio_ini_path)
        print(f"Parsed platformio.ini: {platformio_ini_path}")
    except (configparser.Error, FileNotFoundError) as e:
        logger.error(f"Error reading platformio.ini: {e}")
        return

    # Step 1: Collect all URLs from platformio.ini
    all_urls = _collect_all_zip_urls(pio_ini)
    if not all_urls:
        print("No zip artifacts found to process")
        return

    # Step 2: Deduplicate URLs
    unique_urls = _dedupe_urls(all_urls)
    print(f"Found {len(all_urls)} total URLs, {len(unique_urls)} unique")

    # Step 3: Download and process all unique URLs
    replacements = _download_and_process_urls(unique_urls, cache_manager)

    # Step 4: Replace all URLs in platformio.ini with resolved local paths
    if replacements:
        _replace_all_urls(pio_ini, all_urls, replacements)

        # Write back atomically
        try:
            pio_ini.dump(platformio_ini_path)
            print(f"Updated platformio.ini with {len(replacements)} resolved artifacts")
        except Exception as e:
            logger.error(f"Failed to update platformio.ini: {e}")
            raise


# Public API function
def resolve_and_cache_platform_artifacts(
    platformio_ini_path: Path, cache_dir: Path
) -> None:
    """
    Main entry point for resolving and caching PlatformIO platform artifacts.

    Args:
        platformio_ini_path: Path to the platformio.ini file to process
        cache_dir: Cache directory for storing artifacts
    """
    print(f"Starting platform artifact resolution for {platformio_ini_path}")
    print(f"Using cache directory: {cache_dir}")

    _apply_board_specific_config(platformio_ini_path, cache_dir)

    print("Platform artifact resolution completed")
