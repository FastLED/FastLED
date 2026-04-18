#!/usr/bin/env python3
"""Bootloader / partitions artifact cache for ESP32 merged-bin builds.

ESP-IDF's bootloader and partition-table binaries are deterministic for a
given ``(board, IDF version, partition CSV, sdkconfig defaults)`` tuple —
no embedded timestamps / git hashes / paths. That makes them ideal cache
candidates: the expensive part of an ESP32 merged-bin build is compiling
the application firmware; the bootloader + partitions only change when the
board definition or IDF version changes.

This module implements a small content-addressed cache (mirrored on
``PlatformIOCache`` at ``ci/compiler/platformio_cache.py``) so that CI and
developer workflows can skip the bootloader/partitions compile step when
the inputs haven't changed. On cache miss, the caller runs the normal
build (which produces the artifacts) and populates the cache; on cache
hit, the caller copies the cached artifacts into the active backend's
output directory and proceeds straight to ``esptool merge_bin``.

Cache layout::

    ~/.fastled/bootloader_cache/
        <sha256-key>/
            bootloader.bin
            partitions.bin
            manifest.json   (records the key inputs for traceability)
            .populated      (breadcrumb — present iff cache is complete)

The cache key is a SHA-256 over the canonicalised key inputs. Two distinct
input tuples that differ in *any* component (board name, platform URL,
partition CSV content, sdkconfig defaults content) produce distinct keys,
so there is no risk of serving a stale artifact for a new configuration.

Context: FastLED/FastLED#2287.
"""

from __future__ import annotations

import hashlib
import json
import logging
import shutil
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

from ci.util.file_lock_rw import custom_lock


logger = logging.getLogger(__name__)

# Breadcrumb filename written inside each cache entry once both files land.
# Presence of this file signals a fully populated entry; its absence means
# the entry is partially written (and should be treated as a miss).
_BREADCRUMB = ".populated"

# Manifest filename — records the key inputs so the cache is auditable /
# debuggable without having to recompute the key from the consumer side.
_MANIFEST = "manifest.json"


def default_cache_dir() -> Path:
    """Return the default bootloader cache directory.

    Matches ``PlatformIOCache`` convention of ``~/.fastled/<name>/`` so
    both caches land under a single sibling for easy cleanup.
    """
    return Path.home() / ".fastled" / "bootloader_cache"


@dataclass(frozen=True)
class BootloaderCacheKey:
    """Inputs that uniquely determine the bootloader + partitions output.

    All four fields are hashed together to produce the cache key — any
    change in any field invalidates the entry, which is the correct
    behaviour (the IDF build would produce different bytes).

    Attributes:
        board: Board name (e.g. ``esp32dev``, ``esp32c3``, ``esp32s3``).
            Affects chip-type-specific bootloader code paths.
        idf_version: IDF version identifier. For PIO/fbuild boards this
            is typically the pioarduino platform URL (e.g.
            ``https://github.com/pioarduino/platform-espressif32/releases/
            download/54.03.20/platform-espressif32.zip``). The URL is a
            cryptographic handle over the whole toolchain.
        partition_csv_content: Text contents of the active
            ``partitions.csv`` (NOT the filename). Using contents rather
            than a file path means two boards that point at the same CSV
            file share cache entries, and a rename of the CSV file does
            NOT invalidate the cache.
        sdkconfig_defaults_content: Text contents of
            ``sdkconfig.defaults`` (or the equivalent ``custom_sdkconfig``
            project option blob) used for this build. Empty string is
            acceptable when no custom sdkconfig is in use.
    """

    board: str
    idf_version: str
    partition_csv_content: str
    sdkconfig_defaults_content: str

    def digest(self) -> str:
        """Compute the SHA-256 cache key over the canonicalised inputs.

        The hash input is a newline-separated string with explicit field
        labels — this keeps the key stable across Python versions and
        makes accidental collisions between fields impossible. Content
        fields are normalised to LF line endings before hashing so the
        key doesn't flip just because a checkout happened on Windows
        (CRLF) vs Linux (LF).
        """
        payload = "\n".join(
            [
                f"board={self.board}",
                f"idf_version={self.idf_version}",
                "partition_csv=",
                _normalise_text(self.partition_csv_content),
                "sdkconfig_defaults=",
                _normalise_text(self.sdkconfig_defaults_content),
            ]
        )
        return hashlib.sha256(payload.encode("utf-8")).hexdigest()

    def as_manifest_dict(self) -> dict[str, str]:
        """Return a JSON-serialisable view of the key inputs."""
        return {
            "board": self.board,
            "idf_version": self.idf_version,
            # Record hashes of content fields rather than the raw text
            # so the manifest stays small even when partition CSVs or
            # sdkconfigs are large. The raw content is not needed for
            # debugging — the hash is enough to confirm whether two
            # entries came from identical inputs.
            "partition_csv_sha256": hashlib.sha256(
                _normalise_text(self.partition_csv_content).encode("utf-8")
            ).hexdigest(),
            "sdkconfig_defaults_sha256": hashlib.sha256(
                _normalise_text(self.sdkconfig_defaults_content).encode("utf-8")
            ).hexdigest(),
        }


def _normalise_text(s: str) -> str:
    """Normalise text for hashing — strip BOM, force LF line endings."""
    if s.startswith("\ufeff"):
        s = s[1:]
    return s.replace("\r\n", "\n").replace("\r", "\n")


@dataclass(frozen=True)
class CachedArtifacts:
    """Resolved paths for a cache hit."""

    bootloader_bin: Path
    partitions_bin: Path
    manifest: Path
    entry_dir: Path


class BootloaderCache:
    """Content-addressed cache for ESP32 bootloader + partitions binaries.

    Thread- and process-safe via the existing ``file_lock_rw`` helper
    (same mechanism ``PlatformIOCache`` uses for its artifact downloads).
    Entries are considered complete only when the ``.populated``
    breadcrumb exists — partial writes are automatically re-populated on
    the next miss.
    """

    def __init__(self, cache_dir: Optional[Path] = None) -> None:
        """Create (or reuse) a cache rooted at ``cache_dir``.

        Args:
            cache_dir: Root directory. Defaults to
                ``~/.fastled/bootloader_cache``. Created on demand.
        """
        self.cache_dir = cache_dir if cache_dir is not None else default_cache_dir()
        self.cache_dir.mkdir(parents=True, exist_ok=True)

    def entry_dir(self, key: BootloaderCacheKey) -> Path:
        """Return the directory for a given key (may not exist yet)."""
        return self.cache_dir / key.digest()

    def lookup(self, key: BootloaderCacheKey) -> Optional[CachedArtifacts]:
        """Return artifact paths if the entry is fully populated, else None.

        A populated entry has all three of ``bootloader.bin``,
        ``partitions.bin``, and the ``.populated`` breadcrumb present.
        Missing any of those is treated as a miss — the caller should
        rebuild and re-populate.
        """
        entry = self.entry_dir(key)
        breadcrumb = entry / _BREADCRUMB
        bootloader = entry / "bootloader.bin"
        partitions = entry / "partitions.bin"
        manifest = entry / _MANIFEST

        if not breadcrumb.exists():
            return None
        if not bootloader.exists() or not partitions.exists():
            # Breadcrumb says complete but files are gone — something
            # deleted them out from under us. Treat as a miss.
            logger.warning(
                f"Bootloader cache entry {entry} has breadcrumb but is missing "
                f"artifacts — treating as miss"
            )
            return None
        return CachedArtifacts(
            bootloader_bin=bootloader,
            partitions_bin=partitions,
            manifest=manifest,
            entry_dir=entry,
        )

    def populate(
        self,
        key: BootloaderCacheKey,
        bootloader_bin_src: Path,
        partitions_bin_src: Path,
    ) -> CachedArtifacts:
        """Copy freshly built artifacts into the cache for this key.

        Safe to call concurrently — a file lock serialises writers.
        The breadcrumb is written last, so a reader that sees it is
        guaranteed to see both artifact files and the manifest.

        Args:
            key: The cache key the artifacts belong to.
            bootloader_bin_src: Path to the freshly built ``bootloader.bin``.
            partitions_bin_src: Path to the freshly built ``partitions.bin``.

        Returns:
            Resolved paths inside the cache entry (same shape as
            ``lookup``).

        Raises:
            FileNotFoundError: Either source artifact does not exist.
        """
        if not bootloader_bin_src.exists():
            raise FileNotFoundError(
                f"Cannot populate bootloader cache: {bootloader_bin_src} missing"
            )
        if not partitions_bin_src.exists():
            raise FileNotFoundError(
                f"Cannot populate bootloader cache: {partitions_bin_src} missing"
            )

        entry = self.entry_dir(key)
        entry.mkdir(parents=True, exist_ok=True)

        lock_path = entry / ".bootloader_cache.lock"
        with custom_lock(
            lock_path, timeout=60.0, operation="bootloader_cache_populate"
        ):
            bootloader_dst = entry / "bootloader.bin"
            partitions_dst = entry / "partitions.bin"
            manifest_dst = entry / _MANIFEST

            # Copy artifacts with ``copy2`` so metadata (mtime) is
            # preserved — useful for ``find``-style cleanup scripts that
            # age entries out.
            shutil.copy2(bootloader_bin_src, bootloader_dst)
            shutil.copy2(partitions_bin_src, partitions_dst)

            now_iso = datetime.now(timezone.utc).isoformat()
            manifest_payload: dict[str, object] = {
                "schema_version": 1,
                "created_at": now_iso,
                "cache_key": key.digest(),
                "inputs": key.as_manifest_dict(),
                "bootloader_size": bootloader_dst.stat().st_size,
                "partitions_size": partitions_dst.stat().st_size,
            }
            manifest_dst.write_text(
                json.dumps(manifest_payload, indent=2, sort_keys=True),
                encoding="utf-8",
            )

            # Write the breadcrumb LAST so readers never see a partially
            # populated entry.
            (entry / _BREADCRUMB).write_text(now_iso, encoding="utf-8")

            logger.info(
                f"Bootloader cache populated: {entry.name} "
                f"(board={key.board}, bootloader={bootloader_dst.stat().st_size}B, "
                f"partitions={partitions_dst.stat().st_size}B)"
            )

            return CachedArtifacts(
                bootloader_bin=bootloader_dst,
                partitions_bin=partitions_dst,
                manifest=manifest_dst,
                entry_dir=entry,
            )

    def materialise_into(
        self,
        hit: CachedArtifacts,
        output_dir: Path,
    ) -> tuple[Path, Path]:
        """Copy cached artifacts into an output directory.

        Intended for the fbuild-hit path: fbuild writes firmware.bin into
        its release dir, but the cache has the bootloader + partitions.
        This helper drops the cached files next to firmware.bin so the
        downstream ``esptool merge_bin`` call finds a complete artifact
        set in a single directory.

        Args:
            hit: A cache-hit result from ``lookup``.
            output_dir: Directory to copy into (usually
                ``.fbuild/build/<env>/release/``). Created if missing.

        Returns:
            (bootloader_dst, partitions_dst) — the final paths.
        """
        output_dir.mkdir(parents=True, exist_ok=True)
        bootloader_dst = output_dir / "bootloader.bin"
        partitions_dst = output_dir / "partitions.bin"
        shutil.copy2(hit.bootloader_bin, bootloader_dst)
        shutil.copy2(hit.partitions_bin, partitions_dst)
        return bootloader_dst, partitions_dst


def build_cache_key_for_board(
    board_name: str,
    platform_url_or_version: str,
    build_dir: Path,
    partition_csv_override: Optional[str] = None,
    sdkconfig_defaults_override: Optional[str] = None,
) -> BootloaderCacheKey:
    """Assemble a ``BootloaderCacheKey`` from a live build directory.

    Discovers the partition CSV and sdkconfig.defaults content by
    reading them from the build dir — falls back to empty string if
    either is missing (some boards use PIO-managed defaults with no
    local CSV / sdkconfig). Callers that know the content ahead of
    time can pass it directly via the override args.

    Args:
        board_name: Board short name (matches ``FBUILD_BOARDS`` entries).
        platform_url_or_version: IDF version handle — the pioarduino
            platform URL is the canonical choice since it pins the
            whole toolchain.
        build_dir: The project's PlatformIO build dir
            (``.build/pio/<board>``) — where ``partitions.csv`` and
            ``sdkconfig.defaults`` live for ESP-IDF projects.
        partition_csv_override: Explicit CSV contents (skips file read).
        sdkconfig_defaults_override: Explicit sdkconfig contents
            (skips file read).

    Returns:
        A fully populated ``BootloaderCacheKey``.
    """
    if partition_csv_override is None:
        partition_csv_override = _read_text_if_exists(build_dir / "partitions.csv")
    if sdkconfig_defaults_override is None:
        sdkconfig_defaults_override = _read_text_if_exists(
            build_dir / "sdkconfig.defaults"
        )
    return BootloaderCacheKey(
        board=board_name,
        idf_version=platform_url_or_version,
        partition_csv_content=partition_csv_override,
        sdkconfig_defaults_content=sdkconfig_defaults_override,
    )


def _read_text_if_exists(path: Path) -> str:
    """Read a file's text if it exists, otherwise return ''."""
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except FileNotFoundError:
        return ""
    except OSError as e:
        logger.debug(f"Failed to read {path} for cache key: {e}")
        return ""
