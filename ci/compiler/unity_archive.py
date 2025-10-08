#!/usr/bin/env python3

import argparse
import os
import sys
from concurrent.futures import Future
from pathlib import Path
from typing import Dict, List, Sequence

from ci.compiler.clang_compiler import Compiler, CompilerOptions, Result
from ci.compiler.test_example_compilation import (
    create_fastled_compiler,
    get_fastled_core_sources,
)


def _sorted_src_files(files: Sequence[Path]) -> List[Path]:
    project_root = Path.cwd()

    def key_fn(p: Path) -> str:
        try:
            rel = p.relative_to(project_root)
        except Exception:
            rel = p
        return rel.as_posix()

    return sorted(files, key=key_fn)


def _partition(files: List[Path], chunks: int) -> List[List[Path]]:
    total = len(files)
    if chunks <= 0:
        raise ValueError("chunks must be >= 1")
    chunks = min(chunks, total) if total > 0 else 1
    base = total // chunks
    rem = total % chunks
    partitions: List[List[Path]] = []
    start = 0
    for i in range(chunks):
        size = base + (1 if i < rem else 0)
        end = start + size
        if size > 0:
            partitions.append(files[start:end])
        start = end
    return partitions


def partition_by_subdirectory(files: List[Path]) -> List[List[Path]]:
    """
    Partition files by top-level subdirectory for more logical unity chunks.

    Groups files into subdirectory-based chunks:
    - src/fl/**/*.cpp
    - src/platforms/**/*.cpp
    - src/fx/**/*.cpp
    - src/*.cpp (root level files)

    Returns chunks in descending order by file count for better load balancing.
    """
    from collections import defaultdict

    project_root = Path.cwd()
    src_dir = project_root / "src"

    # Group files by subdirectory
    groups: Dict[str, List[Path]] = defaultdict(list)

    for f in files:
        # Convert to absolute path first if it's relative
        if not f.is_absolute():
            f_abs = project_root / f
        else:
            f_abs = f

        try:
            rel = f_abs.relative_to(src_dir)
            # Get top-level subdirectory or 'root' for src/*.cpp
            parts = rel.parts
            if len(parts) == 1:
                # File directly in src/
                groups["src_root"].append(f)
            else:
                # File in subdirectory
                subdir = parts[0]
                groups[subdir].append(f)
        except ValueError:
            # File not under src/, put in 'other'
            groups["other"].append(f)

    # Sort each group internally for deterministic ordering
    for group_name in groups:
        groups[group_name] = sorted(groups[group_name], key=lambda p: p.as_posix())

    # Create partitions in descending order by size for load balancing
    # Larger groups compile first in parallel builds
    sorted_groups = sorted(groups.items(), key=lambda x: len(x[1]), reverse=True)

    partitions: List[List[Path]] = []
    for group_name, group_files in sorted_groups:
        if group_files:
            partitions.append(group_files)

    return partitions if partitions else [[]]


def build_unity_chunks_and_archive(
    compiler: Compiler,
    chunks: int,
    output_archive: Path,
    unity_dir: Path,
    no_parallel: bool,
    subdirectory_mode: bool = True,
) -> Path:
    unity_dir.mkdir(parents=True, exist_ok=True)
    # Gather and sort source files consistently with existing library logic
    files = get_fastled_core_sources()
    # Exclude any main-like TU
    files = [p for p in files if p.name != "stub_main.cpp"]
    files = _sorted_src_files(files)
    if not files:
        raise RuntimeError("No source files found under src/** for unity build")

    # Choose partitioning strategy
    if subdirectory_mode:
        partitions = partition_by_subdirectory(files)
        print(
            f"[UNITY] Using subdirectory-based partitioning: {len(partitions)} chunks"
        )
        for i, partition in enumerate(partitions):
            print(f"[UNITY]   Chunk {i + 1}: {len(partition)} files")
    else:
        partitions = _partition(files, chunks)
        print(f"[UNITY] Using ordinal partitioning: {len(partitions)} chunks")

    # Prepare per-chunk unity paths
    unity_cpp_paths: List[Path] = [
        unity_dir / f"unity{i + 1}.cpp" for i in range(len(partitions))
    ]
    unity_obj_paths: List[Path] = [p.with_suffix(".o") for p in unity_cpp_paths]

    # Compile chunks
    futures: List[Future[Result]] = []
    compile_opts = CompilerOptions(
        include_path=compiler.settings.include_path,
        compiler=compiler.settings.compiler,
        defines=compiler.settings.defines,
        std_version=compiler.settings.std_version,
        compiler_args=compiler.settings.compiler_args,
        use_pch=False,
        additional_flags=["-c"],
        parallel=not no_parallel,
    )

    def submit(idx: int) -> Future[Result]:
        chunk_files = partitions[idx]
        unity_cpp = unity_cpp_paths[idx]
        return compiler.compile_unity(compile_opts, chunk_files, unity_cpp)

    if no_parallel:
        # Sequential execution
        for i in range(len(partitions)):
            result = submit(i).result()
            if not result.ok:
                raise RuntimeError(f"Unity chunk {i + 1} failed: {result.stderr}")
    else:
        futures = [submit(i) for i in range(len(partitions))]
        for i, fut in enumerate(futures):
            result = fut.result()
            if not result.ok:
                raise RuntimeError(f"Unity chunk {i + 1} failed: {result.stderr}")

    # Compile third_party files separately (they can't be in unity builds)
    from ci.compiler.linking.library_builder import get_third_party_sources

    third_party_sources = get_third_party_sources()
    third_party_objects: List[Path] = []

    if third_party_sources:
        print(
            f"[UNITY] Compiling {len(third_party_sources)} third_party files separately..."
        )
        third_party_dir = unity_dir / "third_party"
        third_party_dir.mkdir(parents=True, exist_ok=True)

        project_root = Path.cwd()
        for tp_file in third_party_sources:
            # Create unique object file name
            try:
                rel_path = tp_file.relative_to(project_root / "src")
            except ValueError:
                rel_path = tp_file
            obj_name = (
                str(rel_path.with_suffix(".o")).replace("/", "_").replace("\\", "_")
            )
            obj_file = third_party_dir / obj_name

            # Compile the third_party file
            result = compiler.compile_cpp_file(tp_file, obj_file).result()
            if not result.ok:
                raise RuntimeError(
                    f"Failed to compile third_party file {tp_file}: {result.stderr}"
                )
            third_party_objects.append(obj_file)

        print(
            f"[UNITY] Successfully compiled {len(third_party_objects)} third_party files"
        )

    # Create archive from chunk objects + third_party objects
    from ci.compiler.clang_compiler import LibarchiveOptions

    all_objects = unity_obj_paths + third_party_objects
    output_archive.parent.mkdir(parents=True, exist_ok=True)
    archive_result = compiler.create_archive(
        all_objects, output_archive, LibarchiveOptions()
    ).result()
    if not archive_result.ok:
        raise RuntimeError(f"Archive creation failed: {archive_result.stderr}")

    return output_archive


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Chunked UNITY build and archive for FastLED src/**"
    )
    parser.add_argument(
        "--chunks", type=int, default=1, help="Number of unity chunks (1-4 typical)"
    )
    parser.add_argument(
        "--no-parallel", action="store_true", help="Compile unity chunks sequentially"
    )
    parser.add_argument(
        "--output",
        type=str,
        default=str(Path(".build/fastled/libfastled.a")),
        help="Path to output archive libfastled.a",
    )
    parser.add_argument(
        "--unity-dir",
        type=str,
        default=str(Path(".build/fastled/unity")),
        help="Directory to place generated unity{N}.cpp/.o files",
    )
    parser.add_argument(
        "--use-pch",
        action="store_true",
        help="Enable PCH (not recommended for unity builds)",
    )
    parser.add_argument(
        "--subdirectory-mode",
        action="store_true",
        default=True,
        help="Use subdirectory-based partitioning (default: True)",
    )
    parser.add_argument(
        "--ordinal-mode",
        action="store_true",
        help="Use ordinal partitioning instead of subdirectory-based",
    )
    args = parser.parse_args()

    # Anchor to project root (two levels up from this file)
    here = Path(__file__).resolve()
    project_root = here.parent.parent.parent
    os.chdir(project_root)

    # Determine subdirectory mode
    subdirectory_mode = (
        not args.ordinal_mode
        if hasattr(args, "ordinal_mode")
        else args.subdirectory_mode
    )

    try:
        compiler = create_fastled_compiler(
            use_pch=args.use_pch, parallel=not args.no_parallel
        )
        out = build_unity_chunks_and_archive(
            compiler=compiler,
            chunks=args.chunks,
            output_archive=Path(args.output),
            unity_dir=Path(args.unity_dir),
            no_parallel=args.no_parallel,
            subdirectory_mode=subdirectory_mode,
        )
        print(f"[UNITY] Created archive: {out}")
        return 0
    except KeyboardInterrupt:
        raise
    except Exception as e:
        print(f"\x1b[31m[UNITY] FAILED: {e}\x1b[0m")
        return 1


if __name__ == "__main__":
    sys.exit(main())
