#!/usr/bin/env python3
"""
Cache Invalidation Rules and Policies

This module defines when caches should be invalidated, persisted, or skipped.
It centralizes the decision logic for cache lifecycle management across all
FastLED build and test operations.
"""

from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Optional


class InvalidationTrigger(Enum):
    """Events that can trigger cache invalidation."""

    TEST_FAILURE = "test_failure"  # Test or lint operation failed
    BUILD_FAILURE = "build_failure"  # Build operation failed
    MANUAL = "manual"  # User explicitly requested invalidation (--no-fingerprint)
    FILE_DELETED = "file_deleted"  # Monitored file was deleted
    DEPENDENCY_CHANGED = "dependency_changed"  # Build dependency changed
    CONFIG_CHANGED = "config_changed"  # Configuration file changed


class CacheAction(Enum):
    """Actions that can be taken on a cache."""

    SKIP = "skip"  # Skip operation - cache is valid
    RUN = "run"  # Run operation - cache invalid or missing
    INVALIDATE = "invalidate"  # Clear cache - force rerun next time
    PERSIST = "persist"  # Save cache - operation succeeded


@dataclass
class CacheDecision:
    """
    Result of evaluating cache invalidation rules.

    Attributes:
        action: Action to take (skip, run, invalidate, persist)
        reason: Human-readable explanation for the action
        trigger: Optional trigger that caused invalidation
    """

    action: CacheAction
    reason: str
    trigger: Optional[InvalidationTrigger] = None


class CacheInvalidationRules:
    """
    Centralized cache invalidation rule engine.

    This class encodes the logic for when caches should be invalidated,
    persisted, or cause operations to be skipped.
    """

    @staticmethod
    def should_skip(
        cache_exists: bool,
        hash_matches: bool,
        previous_status: Optional[str],
        force_run: bool = False,
    ) -> CacheDecision:
        """
        Determine if an operation should be skipped based on cache state.

        Skip logic:
        - Skip if hash matches AND previous status was success AND not forced
        - Run if hash differs (code changed)
        - Run if previous status was failure (retry failed tests)
        - Run if no cache exists (first run)
        - Run if forced (--no-fingerprint flag)

        Args:
            cache_exists: Whether cache file exists
            hash_matches: Whether current hash matches cached hash
            previous_status: Status from previous run ("success" or "failure")
            force_run: Whether user forced operation (--no-fingerprint)

        Returns:
            CacheDecision indicating whether to skip or run
        """
        if force_run:
            return CacheDecision(
                action=CacheAction.RUN,
                reason="Force run requested (--no-fingerprint flag)",
                trigger=InvalidationTrigger.MANUAL,
            )

        if not cache_exists:
            return CacheDecision(
                action=CacheAction.RUN,
                reason="No cache exists (first run)",
            )

        if not hash_matches:
            return CacheDecision(
                action=CacheAction.RUN,
                reason="Hash mismatch (code changed)",
                trigger=InvalidationTrigger.DEPENDENCY_CHANGED,
            )

        if previous_status == "failure":
            return CacheDecision(
                action=CacheAction.RUN,
                reason="Previous run failed (retrying)",
                trigger=InvalidationTrigger.TEST_FAILURE,
            )

        if previous_status == "success" and hash_matches:
            return CacheDecision(
                action=CacheAction.SKIP,
                reason="Cache valid (no changes, previous success)",
            )

        # Default: run if uncertain
        return CacheDecision(
            action=CacheAction.RUN,
            reason="Unknown previous state",
        )

    @staticmethod
    def should_invalidate_on_failure(operation_type: str) -> bool:
        """
        Determine if cache should be invalidated when operation fails.

        Generally, failed operations should invalidate the cache to force
        a rerun next time. However, some operations may want to preserve
        cache state even on failure.

        Args:
            operation_type: Type of operation (e.g., "test", "lint", "build")

        Returns:
            True if cache should be invalidated on failure
        """
        # Always invalidate on failure for now
        # Future: Could make this configurable per operation type
        return True

    @staticmethod
    def should_persist_on_success(operation_type: str) -> bool:
        """
        Determine if cache should be persisted when operation succeeds.

        Generally, successful operations should persist the cache to enable
        skipping on subsequent runs.

        Args:
            operation_type: Type of operation (e.g., "test", "lint", "build")

        Returns:
            True if cache should be persisted on success
        """
        # Always persist on success
        return True

    @staticmethod
    def get_monitored_files_for_cache(cache_name: str) -> list[str]:
        """
        Get glob patterns for files monitored by a specific cache.

        This defines what files each cache should track. Changes to these
        files will invalidate the cache.

        Args:
            cache_name: Name of cache (e.g., "cpp_lint", "python_test")

        Returns:
            List of glob patterns to monitor

        Raises:
            ValueError: If cache name is unknown
        """
        patterns = {
            "cpp_lint": [
                "src/**/*.cpp",
                "src/**/*.h",
                "src/**/*.hpp",
                "src/**/*.c",
                "examples/**/*.ino",
                "examples/**/*.cpp",
                "examples/**/*.h",
                "examples/**/*.hpp",
                "ci/lint_cpp/**/*.py",  # Monitor linter scripts too
            ],
            "python_lint": [
                "test.py",
                "ci/**/*.py",
                "pyproject.toml",  # Pyright configuration
            ],
            "js_lint": [
                "src/platforms/wasm/compiler/**/*",
                "ci/docker_utils/avr8js/**/*",
            ],
            "cpp_test": [
                "src/**/*.cpp",
                "src/**/*.h",
                "src/**/*.hpp",
                "src/**/*.c",
                "tests/**/*.cpp",
                "tests/**/*.h",
                "tests/**/*.hpp",
                "meson.build",
                "tests/meson.build",
            ],
            "python_test": [
                "ci/tests/**/*.py",
                "ci/**/*.py",
                "pyproject.toml",
                "uv.lock",
            ],
            "examples": [
                "src/**/*.cpp",
                "src/**/*.h",
                "src/**/*.hpp",
                "src/**/*.c",
                "examples/**/*.ino",
                "examples/**/*.cpp",
                "examples/**/*.h",
                "examples/**/*.hpp",
                "meson.build",
                "ci/util/meson_example_runner.py",
                "examples/meson.build",
            ],
            "all": [
                "src/**/*.cpp",
                "src/**/*.h",
                "src/**/*.hpp",
                "src/**/*.c",
            ],
        }

        if cache_name not in patterns:
            raise ValueError(
                f"Unknown cache name: {cache_name}. "
                f"Valid names: {list(patterns.keys())}"
            )

        return patterns[cache_name]

    @staticmethod
    def excludes_for_cache(cache_name: str) -> list[str]:
        """
        Get exclude patterns for files that should NOT be monitored.

        Args:
            cache_name: Name of cache

        Returns:
            List of path substrings to exclude
        """
        # Common excludes for all caches
        common_excludes = [
            ".cache/",
            ".build/",
            ".venv/",
            "__pycache__/",
            ".git/",
        ]

        # Cache-specific excludes
        specific_excludes = {
            "python_lint": ["ci/tmp/", "ci/wasm/"],
            "python_test": ["ci/tmp/", "ci/wasm/"],
        }

        excludes = common_excludes.copy()
        if cache_name in specific_excludes:
            excludes.extend(specific_excludes[cache_name])

        return excludes

    @staticmethod
    def get_cache_lifetime(cache_name: str) -> Optional[int]:
        """
        Get the maximum lifetime (in seconds) for a cache before forcing rebuild.

        Args:
            cache_name: Name of cache

        Returns:
            Maximum lifetime in seconds, or None for no expiration
        """
        # No automatic expiration for now
        # Future: Could add time-based invalidation for certain caches
        return None

    @staticmethod
    def supports_concurrent_access(cache_name: str) -> bool:
        """
        Check if cache supports concurrent access from multiple processes.

        Args:
            cache_name: Name of cache

        Returns:
            True if cache uses file locking for concurrent access
        """
        # All hash and two-layer caches support concurrent access via file locking
        # Legacy cache does not use locking
        concurrent_caches = [
            "cpp_lint",
            "python_lint",
            "js_lint",
            "cpp_test",
            "python_test",
            "examples",
            "all",
        ]
        return cache_name in concurrent_caches


# ==============================================================================
# Helper Functions
# ==============================================================================


def evaluate_cache_state(
    cache_name: str,
    cache_path: Path,
    current_hash: str,
    force_run: bool = False,
) -> CacheDecision:
    """
    Evaluate cache state and determine action to take.

    This is a convenience function that combines cache reading and rule evaluation.

    Args:
        cache_name: Name of cache (for rule lookup)
        cache_path: Path to cache file
        current_hash: Current hash of monitored files
        force_run: Whether user forced operation

    Returns:
        CacheDecision indicating what action to take
    """
    import json

    # Check if cache exists and read it
    cache_exists = cache_path.exists()
    previous_status = None
    previous_hash = None

    if cache_exists:
        try:
            with open(cache_path, "r") as f:
                cache_data = json.load(f)
                previous_hash = cache_data.get("hash")
                previous_status = cache_data.get("status")
        except (json.JSONDecodeError, OSError):
            # Corrupted cache - treat as missing
            cache_exists = False

    hash_matches = previous_hash == current_hash

    # Apply invalidation rules
    return CacheInvalidationRules.should_skip(
        cache_exists=cache_exists,
        hash_matches=hash_matches,
        previous_status=previous_status,
        force_run=force_run,
    )
