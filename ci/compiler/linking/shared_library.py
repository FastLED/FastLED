"""Shared FastLED library builder for both unit tests and examples.

This module provides a unified library build that serves both unit tests and
examples, eliminating duplicate compilation and enabling shared PCH headers.

Key features:
- Single libfastled.a at .build/shared/libfastled.a
- Compiled with FASTLED_TESTING=1 (safe for examples, required for tests)
- Unity chunk compilation for maximum speed
- Fingerprint cache + config hash for efficient rebuilds
- Shared by both unit tests and examples
"""

import hashlib
import os
from pathlib import Path
from typing import List, Optional, Tuple

from ci.ci.fingerprint_cache import FingerprintCache
from ci.compiler.clang_compiler import Compiler, CompilerOptions, Result
from ci.compiler.test_example_compilation import create_fastled_compiler
from ci.util.paths import PROJECT_ROOT


def get_shared_library_sources() -> List[Path]:
    """Get all FastLED .cpp files for shared library (union of unit test + example sources)."""
    project_root = PROJECT_ROOT
    src_dir = project_root / "src"

    # Core FastLED files that must be included
    core_files: List[Path] = [
        src_dir / "FastLED.cpp",
        src_dir / "colorutils.cpp",
        src_dir / "hsv2rgb.cpp",
    ]

    # Find all .cpp and .c files in key directories
    additional_sources: List[Path] = []
    for pattern in ["*.cpp", "lib8tion/*.cpp", "platforms/stub/*.cpp"]:
        additional_sources.extend(list(src_dir.glob(pattern)))

    # Include essential .cpp files from nested directories
    additional_sources.extend(list(src_dir.rglob("*.cpp")))

    # Include .c files (for third-party C libraries like libhelix_mp3)
    additional_sources.extend(list(src_dir.rglob("*.c")))

    # Filter out duplicates and ensure files exist
    all_sources: List[Path] = []
    seen_files: set[Path] = set()

    for cpp_file in core_files + additional_sources:
        # Skip stub_main.cpp since unit tests and examples have their own main
        if cpp_file.name == "stub_main.cpp":
            continue

        # Skip third_party files - they need special handling (not for unity builds)
        if "third_party" in cpp_file.parts:
            continue

        # Skip platform-specific files that aren't needed for stub builds
        rel_path_str = str(cpp_file)
        if any(
            skip in rel_path_str for skip in ["wasm", "esp", "avr", "arm", "teensy"]
        ):
            continue

        if cpp_file.exists() and cpp_file not in seen_files:
            all_sources.append(cpp_file)
            seen_files.add(cpp_file)

    return all_sources


def get_third_party_sources() -> List[Path]:
    """Get third_party .cpp/.c files that need separate compilation."""
    project_root = PROJECT_ROOT
    src_dir = project_root / "src"
    third_party_dir = src_dir / "third_party"

    if not third_party_dir.exists():
        return []

    # Find all .cpp and .c files in third_party
    sources: List[Path] = []
    sources.extend(list(third_party_dir.rglob("*.cpp")))
    sources.extend(list(third_party_dir.rglob("*.c")))

    # Filter out duplicates
    seen_files: set[Path] = set()
    unique_sources: List[Path] = []

    for source_file in sources:
        if source_file.exists() and source_file not in seen_files:
            unique_sources.append(source_file)
            seen_files.add(source_file)

    return unique_sources


def _calculate_library_config_hash(
    defines: List[str], compiler_args: List[str], compiler_cmd: str
) -> str:
    """Calculate a hash representing the library build configuration."""
    # Sort defines and args for deterministic hashing
    sorted_defines = sorted(defines)
    sorted_args = sorted(compiler_args)

    # Combine all configuration into a single string
    config_parts = [
        f"compiler:{compiler_cmd}",
        f"defines:{' '.join(sorted_defines)}",
        f"args:{' '.join(sorted_args)}",
    ]
    combined = "|".join(config_parts)

    # Calculate SHA256 hash
    hash_sha256 = hashlib.sha256()
    hash_sha256.update(combined.encode("utf-8"))

    return hash_sha256.hexdigest()[:16]  # Use first 16 chars for readability


def _should_rebuild_library(
    lib_file: Path,
    fastled_sources: List[Path],
    cache_file: Path,
    config_hash: str,
    config_hash_file: Path,
) -> bool:
    """Check if shared library needs to be rebuilt based on source or config changes."""
    # If library file doesn't exist, need to build
    if not lib_file.exists():
        print("[SHARED LIBRARY] Library file doesn't exist, rebuilding required")
        return True

    # Check if compilation configuration has changed
    if config_hash_file.exists():
        try:
            with open(config_hash_file, "r") as f:
                cached_config_hash = f.read().strip()

            if cached_config_hash != config_hash:
                print(
                    f"[SHARED LIBRARY] Config changed (cached: {cached_config_hash}, current: {config_hash}) - rebuild required"
                )
                return True
            else:
                print(f"[SHARED LIBRARY] Config unchanged ({config_hash})")
        except (IOError, OSError) as e:
            print(
                f"[SHARED LIBRARY] Failed to read config hash: {e} - rebuild required"
            )
            return True
    else:
        print("[SHARED LIBRARY] No config hash file found - rebuild required")
        return True

    # Get library file modification time as baseline
    lib_modtime = os.path.getmtime(lib_file)

    # Create fingerprint cache instance
    fingerprint_cache = FingerprintCache(cache_file)

    # Check if any FastLED source files have changed
    print(f"[SHARED LIBRARY] Checking {len(fastled_sources)} source files...")

    for src_file in fastled_sources:
        try:
            if fingerprint_cache.has_changed(src_file, lib_modtime):
                print(
                    f"[SHARED LIBRARY] Source changed: {src_file.name} - rebuild required"
                )
                return True
        except FileNotFoundError:
            print(
                f"[SHARED LIBRARY] Source not found: {src_file.name} - rebuild required"
            )
            return True

    print("[SHARED LIBRARY] All sources unchanged - using cached library")
    return False


def create_shared_fastled_library(
    clean: bool = False, use_pch: bool = True, verbose: bool = False
) -> Optional[Path]:
    """Create shared libfastled.a for both unit tests and examples.

    This library is compiled with FASTLED_TESTING=1 to include test utilities,
    which are safe for examples (they simply won't call them).

    Args:
        clean: Force rebuild even if cached
        use_pch: Use precompiled headers
        verbose: Enable verbose output

    Returns:
        Path to the created library, or None on failure
    """
    # Shared library goes in .build/shared/
    shared_build_dir = PROJECT_ROOT / ".build" / "shared"
    shared_build_dir.mkdir(parents=True, exist_ok=True)
    lib_file = shared_build_dir / "libfastled.a"

    # Get sources for cache checking
    fastled_sources = get_shared_library_sources()
    third_party_sources = get_third_party_sources()

    # Set up fingerprint cache
    cache_file = shared_build_dir / "library_fingerprint_cache.json"
    config_hash_file = shared_build_dir / "libfastled.a.config_hash"

    print("[SHARED LIBRARY] Creating shared FastLED library compiler...")

    # Save current directory and change to project root
    original_cwd = os.getcwd()
    os.chdir(str(PROJECT_ROOT))

    try:
        # Create compiler using example configuration (minimal defines)
        library_compiler = create_fastled_compiler(
            use_pch=use_pch,
            parallel=True,
            strict_mode=True,
        )

        # Add FASTLED_TESTING=1 to include test utility functions
        if library_compiler.settings.defines is None:
            library_compiler.settings.defines = []

        library_compiler.settings.defines.append("FASTLED_TESTING=1")
        print("[SHARED LIBRARY] Added FASTLED_TESTING=1 for test utilities")

    finally:
        # Restore original working directory
        os.chdir(original_cwd)

    # Calculate configuration hash
    config_hash = _calculate_library_config_hash(
        library_compiler.settings.defines or [],
        library_compiler.settings.compiler_args or [],
        library_compiler.settings.compiler,
    )
    print(f"[SHARED LIBRARY] Configuration hash: {config_hash}")

    # Check if library needs rebuilding
    if not clean and not _should_rebuild_library(
        lib_file, fastled_sources, cache_file, config_hash, config_hash_file
    ):
        print(f"[SHARED LIBRARY] Using existing library: {lib_file}")
        return lib_file

    print(
        f"[SHARED LIBRARY] Building shared library with {len(fastled_sources)} sources..."
    )

    # Use unity chunk compilation for speed
    from concurrent.futures import Future

    from ci.compiler.unity_archive import partition_by_subdirectory

    fastled_objects: List[Path] = []
    unity_dir = shared_build_dir / "unity"
    unity_dir.mkdir(exist_ok=True)

    # Sort sources deterministically
    fastled_sources_sorted = sorted(fastled_sources, key=lambda p: p.as_posix())

    # Partition by subdirectory for logical grouping
    partitions = partition_by_subdirectory(fastled_sources_sorted)
    print(f"[SHARED LIBRARY] Partitioned into {len(partitions)} subdirectory chunks")

    # Compile each unity chunk
    compile_opts = CompilerOptions(
        include_path=library_compiler.settings.include_path,
        compiler=library_compiler.settings.compiler,
        defines=library_compiler.settings.defines,
        std_version=library_compiler.settings.std_version,
        compiler_args=library_compiler.settings.compiler_args,
        use_pch=False,
        additional_flags=["-c"],
        parallel=True,
    )

    compiled_count = 0
    failed_count = 0

    # Submit all chunks for parallel compilation
    unity_futures: List[Tuple[Future[Result], int, int, Path]] = []
    for i, chunk_files in enumerate(partitions):
        if not chunk_files:
            continue

        unity_cpp = unity_dir / f"unity_chunk{i + 1}.cpp"
        if verbose:
            print(
                f"[SHARED LIBRARY] Submitting chunk {i + 1}/{len(partitions)} ({len(chunk_files)} files)..."
            )

        future = library_compiler.compile_unity(compile_opts, chunk_files, unity_cpp)
        unity_futures.append((future, i + 1, len(chunk_files), unity_cpp))

    # Wait for all chunks to complete
    print(
        f"[SHARED LIBRARY] Compiling {len(unity_futures)} unity chunks in parallel..."
    )
    for future, chunk_num, file_count, unity_cpp in unity_futures:
        try:
            result = future.result()

            if result.ok:
                unity_obj = unity_cpp.with_suffix(".o")
                fastled_objects.append(unity_obj)
                compiled_count += file_count
                if verbose:
                    print(
                        f"[SHARED LIBRARY] SUCCESS: Chunk {chunk_num} compiled ({file_count} files)"
                    )
            else:
                failed_count += file_count
                print(
                    f"[SHARED LIBRARY] ERROR: Chunk {chunk_num} failed: {result.stderr[:500]}..."
                )
        except Exception as e:
            failed_count += file_count
            print(f"[SHARED LIBRARY] ERROR: Chunk {chunk_num} exception: {e}")

    print(
        f"[SHARED LIBRARY] Compiled {compiled_count}/{len(fastled_sources)} sources via {len(partitions)} chunks"
    )

    # FAIL FAST: If any unity chunk failed, abort
    if failed_count > 0:
        raise Exception(
            f"CRITICAL: {failed_count} source files failed to compile in unity chunks."
        )

    # Compile third_party files separately
    if third_party_sources:
        print(
            f"[SHARED LIBRARY] Compiling {len(third_party_sources)} third_party files..."
        )
        third_party_dir = unity_dir / "third_party"
        third_party_dir.mkdir(exist_ok=True)

        src_dir = PROJECT_ROOT / "src"
        third_party_futures: List[Tuple[Future[Result], Path, Path]] = []

        for tp_file in third_party_sources:
            # Create unique object file name
            rel_path = (
                tp_file.relative_to(src_dir)
                if tp_file.is_relative_to(src_dir)
                else tp_file
            )
            obj_name = (
                str(rel_path.with_suffix(".o")).replace("/", "_").replace("\\", "_")
            )
            obj_file = third_party_dir / obj_name

            future = library_compiler.compile_cpp_file(tp_file, obj_file)
            third_party_futures.append((future, obj_file, tp_file))

        # Wait for third_party compilations
        tp_compiled_count = 0
        tp_failed_count = 0
        for future, obj_file, tp_file in third_party_futures:
            try:
                result = future.result()
                if result.ok:
                    fastled_objects.append(obj_file)
                    tp_compiled_count += 1
                else:
                    tp_failed_count += 1
                    print(
                        f"[SHARED LIBRARY] ERROR: Failed to compile {tp_file}: {result.stderr[:300]}..."
                    )
            except Exception as e:
                tp_failed_count += 1
                print(f"[SHARED LIBRARY] ERROR: Exception compiling {tp_file}: {e}")

        print(
            f"[SHARED LIBRARY] Compiled {tp_compiled_count}/{len(third_party_sources)} third_party files"
        )

        if tp_failed_count > 0:
            raise Exception(
                f"CRITICAL: {tp_failed_count} third_party files failed to compile."
            )

    if not fastled_objects:
        print("[SHARED LIBRARY] ERROR: No object files compiled successfully")
        return None

    # Create static library
    print(f"[SHARED LIBRARY] Creating archive: {lib_file}")

    archive_future = library_compiler.create_archive(fastled_objects, lib_file)
    archive_result = archive_future.result()

    if not archive_result.ok:
        print(
            f"[SHARED LIBRARY] ERROR: Archive creation failed: {archive_result.stderr}"
        )
        return None

    # Save configuration hash
    try:
        with open(config_hash_file, "w") as f:
            f.write(config_hash)
        print(f"[SHARED LIBRARY] Saved config hash ({config_hash})")
    except (IOError, OSError) as e:
        print(f"[SHARED LIBRARY] WARNING: Failed to save config hash: {e}")

    print(f"[SHARED LIBRARY] SUCCESS: Created {lib_file}")
    return lib_file
