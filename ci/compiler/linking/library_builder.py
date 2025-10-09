"""FastLED static library builder."""

from concurrent.futures import Future
from pathlib import Path
from typing import Callable, List

from ci.compiler.clang_compiler import Compiler, Result


def get_fastled_core_sources() -> List[Path]:
    """Get essential FastLED .cpp files for library creation (excludes third_party)."""
    src_dir = Path("src")

    # Core FastLED files that must be included
    core_files: List[Path] = [
        src_dir / "FastLED.cpp",
        src_dir / "colorutils.cpp",
        src_dir / "hsv2rgb.cpp",
    ]

    # Find all .cpp files in key directories
    additional_sources: List[Path] = []
    for pattern in ["*.cpp", "lib8tion/*.cpp", "platforms/stub/*.cpp"]:
        additional_sources.extend(list(src_dir.glob(pattern)))

    # Include essential .cpp files from nested directories
    additional_sources.extend(list(src_dir.rglob("*.cpp")))

    # Filter out duplicates and ensure files exist
    all_sources: List[Path] = []
    seen_files: set[Path] = set()

    for cpp_file in core_files + additional_sources:
        # Skip stub_main.cpp since we create individual main.cpp files for each example
        if cpp_file.name == "stub_main.cpp":
            continue

        # Skip third_party files - they need special handling (not for unity builds)
        if "third_party" in cpp_file.parts:
            continue

        if cpp_file.exists() and cpp_file not in seen_files:
            all_sources.append(cpp_file)
            seen_files.add(cpp_file)

    return all_sources


def get_third_party_sources() -> List[Path]:
    """Get third_party .cpp/.c files that need separate compilation."""
    # Find project root - go up from this file's location
    # This file is at ci/compiler/linking/library_builder.py
    this_file = Path(__file__).resolve()
    project_root = this_file.parent.parent.parent.parent  # Go up to project root

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


def create_fastled_library(
    compiler: Compiler,
    fastled_build_dir: Path,
    log_timing: Callable[[str], None],
    verbose: bool,
) -> Path:
    """Create libfastled.a static library."""

    log_timing("[LIBRARY] Creating FastLED static library...")

    # Compile all FastLED sources to object files
    fastled_sources = get_fastled_core_sources()
    third_party_sources = get_third_party_sources()
    all_sources = fastled_sources + third_party_sources
    fastled_objects: List[Path] = []

    obj_dir = fastled_build_dir / "obj"
    obj_dir.mkdir(exist_ok=True)

    log_timing(f"[LIBRARY] Compiling {len(all_sources)} FastLED source files...")

    # Compile each source file
    futures: List[tuple[Future[Result], Path, Path]] = []
    for cpp_file in all_sources:
        # Create unique object file name by including relative path to prevent collisions
        # Convert path separators to underscores to create valid filename
        src_dir = Path("src")
        if cpp_file.is_relative_to(src_dir):
            rel_path = cpp_file.relative_to(src_dir)
        else:
            rel_path = cpp_file

        # Replace path separators with underscores for unique object file names
        obj_name = str(rel_path.with_suffix(".o")).replace("/", "_").replace("\\", "_")
        obj_file = obj_dir / obj_name
        if verbose:
            log_timing(f"[LIBRARY] Compiling src/{rel_path} -> {obj_file.name}")
        future = compiler.compile_cpp_file(cpp_file, obj_file)
        futures.append((future, obj_file, cpp_file))

    # Wait for compilation to complete
    compiled_count = 0
    failed_count = 0
    for future, obj_file, cpp_file in futures:
        try:
            result: Result = future.result()
            if result.ok:
                fastled_objects.append(obj_file)
                compiled_count += 1
                if verbose:
                    src_dir_check = Path("src")
                    display_path = (
                        cpp_file.relative_to(src_dir_check)
                        if cpp_file.is_relative_to(src_dir_check)
                        else cpp_file.name
                    )
                    log_timing(f"[LIBRARY] SUCCESS: {display_path}")
            else:
                failed_count += 1
                src_dir_check = Path("src")
                display_path = (
                    cpp_file.relative_to(src_dir_check)
                    if cpp_file.is_relative_to(src_dir_check)
                    else cpp_file.name
                )
                log_timing(
                    f"[LIBRARY] ERROR: Failed to compile {display_path}: {result.stderr[:300]}..."
                )
        except Exception as e:
            failed_count += 1
            src_dir_check = Path("src")
            display_path = (
                cpp_file.relative_to(src_dir_check)
                if cpp_file.is_relative_to(src_dir_check)
                else cpp_file.name
            )
            log_timing(f"[LIBRARY] ERROR: Exception compiling {display_path}: {e}")

    log_timing(
        f"[LIBRARY] Successfully compiled {compiled_count}/{len(all_sources)} FastLED sources"
    )

    # FAIL FAST: If any source files failed to compile, abort immediately
    if failed_count > 0:
        raise Exception(
            f"CRITICAL: {failed_count} FastLED source files failed to compile. "
            f"This indicates a serious build environment issue (likely PCH invalidation). "
            f"All source files must compile successfully."
        )

    if not fastled_objects:
        raise Exception("No FastLED source files compiled successfully")

    # Create static library using ar
    lib_file = fastled_build_dir / "libfastled.a"
    log_timing(f"[LIBRARY] Creating static library: {lib_file}")

    archive_future = compiler.create_archive(fastled_objects, lib_file)
    archive_result = archive_future.result()

    if not archive_result.ok:
        raise Exception(f"Library creation failed: {archive_result.stderr}")

    log_timing(f"[LIBRARY] SUCCESS: FastLED library created: {lib_file}")
    return lib_file
