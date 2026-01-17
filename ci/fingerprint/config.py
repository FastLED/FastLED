#!/usr/bin/env python3
"""
Centralized Fingerprint Cache Configuration

This module provides a unified configuration system for all fingerprint caches
used throughout the FastLED build system. It defines:

1. Cache types and their recommended use cases
2. Default cache directory locations
3. Build mode-specific cache naming
4. Cache lifecycle rules (when to invalidate, when to persist)
"""

from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Optional


class CacheType(Enum):
    """
    Types of fingerprint caches available.

    LEGACY: Original per-file tracking (backward compatibility only)
    TWO_LAYER: Efficient mtime + MD5 for batch file operations (recommended for linting)
    HASH: Single SHA256 for all files (recommended for build systems)
    """

    LEGACY = "legacy"
    TWO_LAYER = "two_layer"
    HASH = "hash"


class BuildMode(Enum):
    """
    Build modes that affect cache separation.

    Different build modes produce different binaries and should have separate caches.
    """

    QUICK = "quick"  # -O0 -g1: Fast iteration
    DEBUG = "debug"  # -O0 -g3 -fsanitize=address,undefined: Full debugging
    RELEASE = "release"  # -O2: Optimized production


@dataclass
class CacheConfig:
    """
    Configuration for a fingerprint cache instance.

    Attributes:
        name: Unique identifier for this cache (e.g., "cpp_lint", "python_test")
        cache_type: Type of cache to use
        cache_dir: Base directory for all caches (typically .cache/)
        build_mode: Optional build mode (for build-dependent caches)
        timestamp_file: Optional path to write change timestamps
        description: Human-readable description of what this cache tracks
    """

    name: str
    cache_type: CacheType
    cache_dir: Path
    build_mode: Optional[BuildMode] = None
    timestamp_file: Optional[Path] = None
    description: str = ""

    def get_cache_filename(self) -> str:
        """
        Get the cache filename for this configuration.

        Build-mode-dependent caches include the build mode in the filename
        to prevent cache conflicts when switching modes.

        Returns:
            Cache filename (e.g., "cpp_test_quick.json", "python_lint.json")
        """
        if self.build_mode is not None:
            return f"{self.name}_{self.build_mode.value}.json"
        return f"{self.name}.json"

    def get_cache_path(self) -> Path:
        """
        Get the full path to the cache file.

        Returns:
            Full path to cache file in fingerprint directory
        """
        return self.cache_dir / "fingerprint" / self.get_cache_filename()

    def supports_build_modes(self) -> bool:
        """
        Check if this cache supports build mode separation.

        Returns:
            True if cache supports per-mode caching
        """
        return self.build_mode is not None


# ==============================================================================
# Predefined Cache Configurations
# ==============================================================================


def get_cache_config(
    name: str,
    cache_dir: Optional[Path] = None,
    build_mode: Optional[str] = None,
) -> CacheConfig:
    """
    Get a predefined cache configuration by name.

    Args:
        name: Cache name (e.g., "cpp_lint", "python_test", "examples")
        cache_dir: Base cache directory (defaults to .cache/)
        build_mode: Build mode string ("quick", "debug", "release")

    Returns:
        CacheConfig instance for the specified cache

    Raises:
        ValueError: If cache name is unknown
    """
    if cache_dir is None:
        cache_dir = Path(".cache")

    # Parse build mode if provided
    build_mode_enum: Optional[BuildMode] = None
    if build_mode is not None:
        try:
            build_mode_enum = BuildMode(build_mode.lower())
        except ValueError:
            raise ValueError(
                f"Unknown build mode: {build_mode}. "
                f"Valid modes: {[m.value for m in BuildMode]}"
            )

    # Predefined configurations
    configs = {
        # Linting caches (two-layer recommended for fast iteration)
        "cpp_lint": CacheConfig(
            name="cpp_lint",
            cache_type=CacheType.TWO_LAYER,
            cache_dir=cache_dir,
            description="C++ source file linting (clang-format + custom linters)",
        ),
        "python_lint": CacheConfig(
            name="python_lint",
            cache_type=CacheType.TWO_LAYER,
            cache_dir=cache_dir,
            description="Python source file linting (pyright type checking)",
        ),
        "js_lint": CacheConfig(
            name="js_lint",
            cache_type=CacheType.TWO_LAYER,
            cache_dir=cache_dir,
            description="JavaScript/TypeScript linting",
        ),
        # Test caches (support build modes)
        "cpp_test": CacheConfig(
            name="cpp_test",
            cache_type=CacheType.HASH,
            cache_dir=cache_dir,
            build_mode=build_mode_enum,
            description="C++ unit tests (src/ + tests/ + build flags)",
        ),
        "python_test": CacheConfig(
            name="python_test",
            cache_type=CacheType.HASH,
            cache_dir=cache_dir,
            description="Python unit tests (ci/ + pyproject.toml)",
        ),
        # Example compilation caches (support build modes)
        "examples": CacheConfig(
            name="examples",
            cache_type=CacheType.HASH,
            cache_dir=cache_dir,
            build_mode=build_mode_enum,
            description="Example compilation (src/ + examples/ + scripts)",
        ),
        # Whole codebase fingerprint
        "all": CacheConfig(
            name="all",
            cache_type=CacheType.HASH,
            cache_dir=cache_dir,
            description="Entire src/ directory fingerprint",
        ),
        # Native build cache
        "native_build": CacheConfig(
            name="native_build",
            cache_type=CacheType.HASH,
            cache_dir=cache_dir,
            description="Native host compilation fingerprint",
        ),
    }

    if name not in configs:
        raise ValueError(
            f"Unknown cache name: {name}. Valid names: {list(configs.keys())}"
        )

    config = configs[name]

    # Override build mode if provided
    if build_mode_enum is not None:
        config.build_mode = build_mode_enum

    return config


def get_default_cache_dir() -> Path:
    """
    Get the default cache directory.

    Returns:
        Path to .cache/ directory in project root
    """
    return Path(".cache")


def get_fingerprint_dir(cache_dir: Optional[Path] = None) -> Path:
    """
    Get the fingerprint cache directory.

    Args:
        cache_dir: Base cache directory (defaults to .cache/)

    Returns:
        Path to .cache/fingerprint/ directory
    """
    if cache_dir is None:
        cache_dir = get_default_cache_dir()
    return cache_dir / "fingerprint"


# ==============================================================================
# Cache Lifecycle Constants
# ==============================================================================


class CacheStatus(Enum):
    """Status values stored in cache files."""

    SUCCESS = "success"
    FAILURE = "failure"


# Default cache invalidation rules
INVALIDATE_ON_FAILURE = True  # Always invalidate cache when operation fails
PERSIST_ON_SUCCESS = True  # Always persist cache when operation succeeds

# Performance tuning
DEFAULT_HASH_CHUNK_SIZE = 8192  # Bytes to read per chunk when hashing files
DEFAULT_LOCK_TIMEOUT = 30.0  # Seconds to wait for lock acquisition
