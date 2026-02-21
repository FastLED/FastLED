"""
Build Optimizer: Binary fingerprint caching to suppress unnecessary DLL relinks.

Problem:
    When sccache returns cached .obj files on a cache hit, it updates the output file
    mtime to NOW. This causes ninja to see .obj files as newer than fastled shared library,
    triggering a re-archive of fastled shared library (same content, new mtime). Ninja then
    sees fastled shared library as newer than all 328 DLLs, triggering relinking of all DLLs
    even though the final output is bit-for-bit identical.

Solution:
    1. After fastled shared library is archived during streaming compilation, hash its content.
    2. If the content matches the saved fingerprint from the last build, touch all
       DLL files to update their mtime past fastled shared library's mtime.
    3. Ninja then sees DLLs as newer than their inputs → skips relinking.
    4. After each build, save the binary fingerprints of fastled shared library + all DLLs
       so the next run can apply the same optimization.

Usage:
    optimizer = BuildOptimizer(cache_file=Path(".cache/build_optimizer_quick.json"))
    # ... during streaming build, after fastled shared library archive message ...
    touched = optimizer.touch_dlls_if_lib_unchanged(build_dir)
    # ... after build completes ...
    optimizer.save_fingerprints(build_dir)
"""

import hashlib
import json
import os
import time
from pathlib import Path
from typing import Optional


# Path of fastled shared library relative to the meson build directory
# name_prefix='' in meson.build produces 'fastled.dll' on Windows, 'fastled.so' on Unix
_LIBFASTLED_DIR = Path("ci") / "meson" / "native"
_LIBFASTLED_REL_PATH = _LIBFASTLED_DIR / (
    "fastled.dll" if os.name == "nt" else "fastled.so"
)

# DLL glob patterns to search within the build directory
_DLL_PATTERNS = ["tests/*.dll", "examples/*.dll"]


def _hash_file(path: Path) -> str:
    """Compute SHA-256 hash of a file's content. Returns empty string on error."""
    try:
        hasher = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(65536), b""):
                hasher.update(chunk)
        return hasher.hexdigest()
    except OSError:
        return ""


def _find_dlls(build_dir: Path) -> list[Path]:
    """Find all test/example DLL files in the build directory."""
    dlls: list[Path] = []
    for pattern in _DLL_PATTERNS:
        dlls.extend(build_dir.glob(pattern))
    return sorted(dlls)


class BuildOptimizer:
    """
    Manages binary fingerprints for fastled shared library and test DLLs to suppress
    unnecessary relinking when library content hasn't changed.

    Thread-safe: touch_dlls_if_lib_unchanged() is designed to be called from
    a background thread while ninja continues building.
    """

    def __init__(self, cache_file: Path) -> None:
        self._cache_file = cache_file
        self._saved: dict = {}
        self._load()

    def _load(self) -> None:
        """Load saved fingerprints from the cache file."""
        if not self._cache_file.exists():
            return
        try:
            with open(self._cache_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            if isinstance(data, dict):
                self._saved = data
        except (json.JSONDecodeError, OSError):
            self._saved = {}

    def touch_dlls_if_lib_unchanged(self, build_dir: Path) -> int:
        """
        Check if fastled shared library content matches the saved fingerprint.
        If so, touch all DLL files whose content also matches the saved fingerprint,
        making their mtime newer than fastled shared library so ninja skips relinking them.

        This is called immediately after the "Linking static target fastled shared library"
        line appears in ninja output - at that point fastled shared library has been freshly
        archived and its content can be compared to the saved fingerprint.

        Returns: number of DLLs touched (had mtime updated)
        """
        lib_path = build_dir / _LIBFASTLED_REL_PATH
        if not lib_path.exists():
            return 0

        saved_lib_hash = self._saved.get("libfastled_hash", "")
        if not saved_lib_hash:
            return 0

        current_lib_hash = _hash_file(lib_path)
        if not current_lib_hash or current_lib_hash != saved_lib_hash:
            # Library content changed - let ninja relink normally
            return 0

        # Library content unchanged - touch DLLs whose content also matches
        saved_dll_hashes: dict[str, str] = self._saved.get("dll_hashes", {})
        if not saved_dll_hashes:
            return 0

        now = time.time()
        touched = 0

        for dll_path in _find_dlls(build_dir):
            dll_key = str(dll_path)
            saved_hash = saved_dll_hashes.get(dll_key, "")
            if not saved_hash:
                continue

            current_hash = _hash_file(dll_path)
            if current_hash and current_hash == saved_hash:
                try:
                    # Update mtime to just after fastled shared library was archived
                    # so ninja sees the DLL as newer than its inputs
                    os.utime(str(dll_path), (now, now))
                    touched += 1
                except OSError:
                    pass

        return touched

    def save_fingerprints(self, build_dir: Path) -> None:
        """
        Save binary fingerprints of fastled shared library and all DLLs to the cache file.
        Called after each successful build to prepare for the next run's optimization.
        """
        lib_path = build_dir / _LIBFASTLED_REL_PATH
        data: dict = {
            "libfastled_hash": "",
            "dll_hashes": {},
        }

        if lib_path.exists():
            data["libfastled_hash"] = _hash_file(lib_path)

        dll_hashes: dict[str, str] = {}
        for dll_path in _find_dlls(build_dir):
            h = _hash_file(dll_path)
            if h:
                dll_hashes[str(dll_path)] = h
        data["dll_hashes"] = dll_hashes

        try:
            self._cache_file.parent.mkdir(parents=True, exist_ok=True)
            with open(self._cache_file, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2)
        except OSError:
            pass


def make_build_optimizer(
    build_dir: Path, cache_root: Optional[Path] = None
) -> BuildOptimizer:
    """
    Create a BuildOptimizer with an appropriate cache file path for the given build directory.

    The cache file name is derived from the build directory name (e.g., "meson-quick")
    so different build modes (quick/debug/release) maintain separate fingerprint caches.

    Args:
        build_dir: The meson build directory (e.g., .build/meson-quick)
        cache_root: Directory for cache files (defaults to .cache/ relative to CWD)

    Returns:
        BuildOptimizer instance with loaded saved fingerprints
    """
    if cache_root is None:
        cache_root = Path(".cache")
    mode_name = build_dir.name  # e.g., "meson-quick"
    cache_file = cache_root / f"build_optimizer_{mode_name}.json"
    return BuildOptimizer(cache_file=cache_file)
