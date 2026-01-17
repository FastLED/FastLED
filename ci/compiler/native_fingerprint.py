#!/usr/bin/env python3
"""
Native Host Compilation Fingerprint Cache

This module provides fingerprinting capabilities for native host example compilation,
tracking changes in source files, examples, launch scripts, and compiler configurations
to enable fast incremental builds.
"""

import hashlib
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional

from ci.fingerprint import FingerprintCache
from ci.util.test_types import fingerprint_code_base


@dataclass
class NativeBuildFingerprint:
    """Fingerprint data for native build dependencies"""

    src_hash: str
    example_hash: str
    script_hash: str
    compiler_config_hash: str
    combined_hash: str
    timestamp: float


class NativeCompilationCache:
    """Cache system for native host compilation fingerprinting"""

    def __init__(self, cache_dir: Optional[Path] = None):
        """
        Initialize the native compilation cache.

        Args:
            cache_dir: Directory to store cache files. Defaults to project/.cache/native_build
        """
        if cache_dir is None:
            cache_dir = Path.cwd() / ".cache" / "native_build"

        self.cache_dir = cache_dir
        self.cache_dir.mkdir(parents=True, exist_ok=True)

        # Use FingerprintCache for file-level change detection
        self.file_cache = FingerprintCache(cache_dir / "files.json")

        # Cache file for build fingerprints
        self.build_cache_file = cache_dir / "builds.json"

    def _get_src_fingerprint(self) -> str:
        """Get fingerprint for src/ directory"""
        src_dir = Path.cwd() / "src"
        if not src_dir.exists():
            return "no_src"

        result = fingerprint_code_base(src_dir, "**/*.h,**/*.cpp,**/*.hpp,**/*.c")
        return result.hash

    def _get_example_fingerprint(self, example_path: Optional[Path] = None) -> str:
        """
        Get fingerprint for examples directory or specific example.

        Args:
            example_path: Specific example to fingerprint. If None, fingerprints all examples.
        """
        if example_path and example_path.exists():
            # Fingerprint specific example
            result = fingerprint_code_base(
                example_path.parent, "**/*.ino,**/*.h,**/*.cpp,**/*.hpp"
            )
            return result.hash
        else:
            # Fingerprint all examples
            examples_dir = Path.cwd() / "examples"
            if not examples_dir.exists():
                return "no_examples"

            result = fingerprint_code_base(
                examples_dir, "**/*.ino,**/*.h,**/*.cpp,**/*.hpp"
            )
            return result.hash

    def _get_script_fingerprint(self) -> str:
        """Get fingerprint for launch scripts"""
        hasher = hashlib.sha256()

        # Include the main compile script
        script_files = [
            Path.cwd() / "ci" / "ci-compile-native.py",
            Path.cwd() / "ci" / "wasm_compile.py",  # May influence native builds
        ]

        for script_file in script_files:
            if script_file.exists():
                with open(script_file, "rb") as f:
                    hasher.update(f.read())
                hasher.update(str(script_file).encode("utf-8"))

        return hasher.hexdigest()

    def _get_compiler_config_fingerprint(self) -> str:
        """Get fingerprint for compiler configuration files"""
        hasher = hashlib.sha256()

        # Include ci/compiler/ directory
        compiler_dir = Path.cwd() / "ci" / "compiler"
        if compiler_dir.exists():
            result = fingerprint_code_base(compiler_dir, "**/*.py")
            hasher.update(f"compiler_scripts:{result.hash}".encode("utf-8"))

        # Include platformio.ini for native builds
        platformio_ini = Path.cwd() / "ci" / "native" / "platformio.ini"
        if platformio_ini.exists():
            with open(platformio_ini, "rb") as f:
                content = f.read()
                hasher.update(content)
                hasher.update(b"platformio_ini")

        return hasher.hexdigest()

    def get_build_fingerprint(
        self, example_path: Optional[Path] = None
    ) -> NativeBuildFingerprint:
        """
        Generate complete fingerprint for native build dependencies.

        Args:
            example_path: Specific example to build. If None, uses all examples.

        Returns:
            Complete fingerprint data for the build
        """
        import time

        src_hash = self._get_src_fingerprint()
        example_hash = self._get_example_fingerprint(example_path)
        script_hash = self._get_script_fingerprint()
        compiler_config_hash = self._get_compiler_config_fingerprint()

        # Combine all hashes for a single build fingerprint
        combined_hasher = hashlib.sha256()
        combined_hasher.update(f"src:{src_hash}".encode("utf-8"))
        combined_hasher.update(f"example:{example_hash}".encode("utf-8"))
        combined_hasher.update(f"script:{script_hash}".encode("utf-8"))
        combined_hasher.update(f"compiler:{compiler_config_hash}".encode("utf-8"))

        return NativeBuildFingerprint(
            src_hash=src_hash,
            example_hash=example_hash,
            script_hash=script_hash,
            compiler_config_hash=compiler_config_hash,
            combined_hash=combined_hasher.hexdigest(),
            timestamp=time.time(),
        )

    def has_build_changed(
        self, example_path: Optional[Path] = None, build_id: str = "default"
    ) -> bool:
        """
        Check if native build needs to be regenerated based on fingerprint changes.

        Args:
            example_path: Specific example to check
            build_id: Identifier for this specific build configuration

        Returns:
            True if build should be regenerated, False if cached build is valid
        """
        import json

        current_fingerprint = self.get_build_fingerprint(example_path)
        # Store for later use in mark_build_success
        self._pending_fingerprint = (build_id, current_fingerprint)

        # Load previous fingerprint if exists
        if not self.build_cache_file.exists():
            # No cache - need to build
            return True

        try:
            with open(self.build_cache_file, "r") as f:
                cache_data = json.load(f)

            if build_id not in cache_data:
                # Build ID not in cache - need to build
                return True

            cached_fingerprint = cache_data[build_id]

            # Compare combined hash
            if cached_fingerprint["combined_hash"] != current_fingerprint.combined_hash:
                # Fingerprint changed - need to rebuild
                return True

            # No changes detected
            return False

        except (json.JSONDecodeError, KeyError, TypeError):
            # Cache corrupted - rebuild
            return True

    def _save_build_fingerprint(
        self, build_id: str, fingerprint: NativeBuildFingerprint
    ):
        """Save build fingerprint to cache"""
        import json

        # Load existing cache
        cache_data = {}
        if self.build_cache_file.exists():
            try:
                with open(self.build_cache_file, "r") as f:
                    cache_data = json.load(f)
            except (json.JSONDecodeError, TypeError):
                cache_data = {}

        # Update with new fingerprint
        cache_data[build_id] = {
            "src_hash": fingerprint.src_hash,
            "example_hash": fingerprint.example_hash,
            "script_hash": fingerprint.script_hash,
            "compiler_config_hash": fingerprint.compiler_config_hash,
            "combined_hash": fingerprint.combined_hash,
            "timestamp": fingerprint.timestamp,
        }

        # Save back to file
        with open(self.build_cache_file, "w") as f:
            json.dump(cache_data, f, indent=2)

    def mark_build_success(
        self, example_path: Optional[Path] = None, build_id: str = "default"
    ) -> None:
        """Mark the build as successful and save the fingerprint.

        Args:
            example_path: Specific example that was built
            build_id: Identifier for this build configuration
        """
        # Use pending fingerprint if available (from has_build_changed call)
        if (
            hasattr(self, "_pending_fingerprint")
            and self._pending_fingerprint[0] == build_id
        ):
            _, fingerprint = self._pending_fingerprint
            self._save_build_fingerprint(build_id, fingerprint)
            del self._pending_fingerprint
        else:
            # Fallback: generate new fingerprint
            fingerprint = self.get_build_fingerprint(example_path)
            self._save_build_fingerprint(build_id, fingerprint)
        print("✓ Native build cache updated")

    def invalidate_build(self, build_id: str = "default") -> None:
        """Invalidate a specific build cache entry when build fails.

        Args:
            build_id: Identifier for this build configuration
        """
        import json

        if not self.build_cache_file.exists():
            return

        try:
            with open(self.build_cache_file, "r") as f:
                cache_data = json.load(f)

            if build_id in cache_data:
                del cache_data[build_id]
                with open(self.build_cache_file, "w") as f:
                    json.dump(cache_data, f, indent=2)
                print("🔄 Native build cache invalidated - will rebuild next time")
        except (json.JSONDecodeError, IOError):
            # If there's an error, just delete the entire cache file
            self.build_cache_file.unlink()
            print("🔄 Native build cache cleared due to error - will rebuild next time")

    def clear_cache(self):
        """Clear all cached fingerprints"""
        self.file_cache.clear_cache()
        if self.build_cache_file.exists():
            self.build_cache_file.unlink()

    def get_cache_stats(self) -> dict[str, Any]:
        """Get cache statistics"""
        import json

        file_stats = self.file_cache.get_cache_stats()

        build_entries = 0
        if self.build_cache_file.exists():
            try:
                with open(self.build_cache_file, "r") as f:
                    cache_data = json.load(f)
                    build_entries = len(cache_data)
            except (json.JSONDecodeError, TypeError):
                build_entries = 0

        return {
            "file_cache_entries": file_stats["total_entries"],
            "build_cache_entries": build_entries,
            "cache_dir": str(self.cache_dir),
            "build_cache_size_bytes": (
                self.build_cache_file.stat().st_size
                if self.build_cache_file.exists()
                else 0
            ),
        }


def should_rebuild_native(
    example_path: Optional[Path] = None, build_id: str = "default"
) -> bool:
    """
    Convenience function to check if native build should be regenerated.

    Args:
        example_path: Specific example to check for changes
        build_id: Unique identifier for this build configuration

    Returns:
        True if build should be regenerated, False if cache is valid
    """
    cache = NativeCompilationCache()
    return cache.has_build_changed(example_path, build_id)


if __name__ == "__main__":
    # Demo/test the fingerprinting system
    cache = NativeCompilationCache()

    print("Native Compilation Fingerprint Cache Demo")
    print("=" * 50)

    # Test basic functionality
    fingerprint = cache.get_build_fingerprint()
    print(f"Current build fingerprint: {fingerprint.combined_hash[:16]}...")
    print(f"Source fingerprint: {fingerprint.src_hash[:16]}...")
    print(f"Example fingerprint: {fingerprint.example_hash[:16]}...")
    print(f"Script fingerprint: {fingerprint.script_hash[:16]}...")
    print(f"Compiler config fingerprint: {fingerprint.compiler_config_hash[:16]}...")

    # Test cache checking
    needs_rebuild = cache.has_build_changed()
    print(f"\nNeeds rebuild: {needs_rebuild}")

    # Second check should return False (no changes)
    needs_rebuild_2 = cache.has_build_changed()
    print(f"Needs rebuild (2nd check): {needs_rebuild_2}")

    # Show cache stats
    stats = cache.get_cache_stats()
    print("\nCache Statistics:")
    print(f"  File cache entries: {stats['file_cache_entries']}")
    print(f"  Build cache entries: {stats['build_cache_entries']}")
    print(f"  Cache directory: {stats['cache_dir']}")
    print(f"  Build cache size: {stats['build_cache_size_bytes']} bytes")
