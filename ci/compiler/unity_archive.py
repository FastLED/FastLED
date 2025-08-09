#!/usr/bin/env python3

import argparse
import os
import sys
from concurrent.futures import Future
from pathlib import Path
from typing import List, Sequence

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


def build_unity_chunks_and_archive(
    compiler: Compiler,
    chunks: int,
    output_archive: Path,
    unity_dir: Path,
    no_parallel: bool,
) -> Path:
    unity_dir.mkdir(parents=True, exist_ok=True)
    # Gather and sort source files consistently with existing library logic
    files = get_fastled_core_sources()
    # Exclude any main-like TU
    files = [p for p in files if p.name != "stub_main.cpp"]
    files = _sorted_src_files(files)
    if not files:
        raise RuntimeError("No source files found under src/** for unity build")

    partitions = _partition(files, chunks)

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

    # Create archive from chunk objects
    from ci.compiler.clang_compiler import LibarchiveOptions

    output_archive.parent.mkdir(parents=True, exist_ok=True)
    archive_result = compiler.create_archive(
        unity_obj_paths, output_archive, LibarchiveOptions()
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
    args = parser.parse_args()

    # Anchor to project root (two levels up from this file)
    here = Path(__file__).resolve()
    project_root = here.parent.parent.parent
    os.chdir(project_root)

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
