#!/usr/bin/env python3
"""
Clean Cache-Enhanced Compilation

Simple wrapper that adds fingerprint cache to FastLED compilation.
Dramatically speeds up incremental builds by skipping unchanged files.
"""

import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Set

from ci.ci.fingerprint_cache import FingerprintCache
from ci.compiler.clang_compiler import Compiler


class CacheAwareCompiler:
    """Simple wrapper that adds cache checking to compilation."""

    def __init__(self, compiler: Compiler, cache_file: Path, verbose: bool = False):
        self.compiler = compiler
        self.cache = FingerprintCache(cache_file)
        self.verbose = verbose
        self.stats = {
            "files_checked": 0,
            "files_skipped": 0,
            "files_compiled": 0,
            "cache_hits": 0,
            "cache_misses": 0,
        }

    def should_compile(self, file_path: Path, baseline_time: float) -> bool:
        """Check if file needs compilation using cache."""
        self.stats["files_checked"] += 1

        try:
            needs_compile = self.cache.has_changed(file_path, baseline_time)

            if needs_compile:
                self.stats["cache_misses"] += 1
                self.stats["files_compiled"] += 1
                return True
            else:
                self.stats["cache_hits"] += 1
                self.stats["files_skipped"] += 1
                if self.verbose:
                    print(f"[CACHE] Skipping unchanged: {file_path.name}")
                return False

        except Exception as e:
            if self.verbose:
                print(f"[CACHE] Error checking {file_path.name}: {e}")
            self.stats["cache_misses"] += 1
            self.stats["files_compiled"] += 1
            return True

    def compile_with_cache(
        self,
        ino_files: List[Path],
        pch_compatible_files: Set[Path],
        log_fn: Callable[[str], None],
        full_compilation: bool,
        verbose: bool = False,
        baseline_time: Optional[float] = None,
    ) -> Dict[str, Any]:
        """Compile only files that have changed."""

        if baseline_time is None:
            baseline_time = time.time() - 3600  # Default: 1 hour ago

        start_time = time.time()

        # Check which files need compilation
        files_to_compile: List[Path] = []
        for ino_file in ino_files:
            if self.should_compile(ino_file, baseline_time):
                files_to_compile.append(ino_file)

        # Check .cpp files too
        cpp_files_to_compile: List[Path] = []
        for ino_file in ino_files:
            cpp_files = self.compiler.find_cpp_files_for_example(ino_file)
            for cpp_file in cpp_files:
                if self.should_compile(cpp_file, baseline_time):
                    cpp_files_to_compile.append(cpp_file)

        # Log cache results
        total_files = len(ino_files) + sum(
            len(self.compiler.find_cpp_files_for_example(f)) for f in ino_files
        )
        log_fn(
            f"[CACHE] {self.stats['files_skipped']}/{total_files} files unchanged, {self.stats['files_compiled']}/{total_files} need compilation"
        )

        if self.stats["files_skipped"] > 0:
            time_saved = self.stats["files_skipped"] * 0.5  # Estimate
            log_fn(f"[CACHE] Estimated time saved: {time_saved:.1f}s")

        # Compile files that changed
        if files_to_compile or cpp_files_to_compile:
            result = self._run_actual_compilation(
                files_to_compile,
                cpp_files_to_compile,
                pch_compatible_files,
                log_fn,
                full_compilation,
                verbose,
            )
        else:
            log_fn("[CACHE] All files unchanged - no compilation needed!")
            result = self._create_success_result(len(ino_files), ino_files, full_compilation)

        # Log final stats
        if total_files > 0:
            hit_rate = (
                (
                    self.stats["cache_hits"]
                    / (self.stats["cache_hits"] + self.stats["cache_misses"])
                    * 100
                )
                if (self.stats["cache_hits"] + self.stats["cache_misses"]) > 0
                else 0
            )
            skip_rate = self.stats["files_skipped"] / total_files * 100
            log_fn(
                f"[CACHE] {hit_rate:.1f}% cache hit rate, {skip_rate:.1f}% files skipped"
            )

        return {
            "compilation_result": result,
            "cache_stats": self.stats,
            "total_time": time.time() - start_time,
        }

    def _run_actual_compilation(
        self,
        ino_files: List[Path],
        cpp_files_to_compile: List[Path],
        pch_compatible_files: Set[Path],
        log_fn: Callable[[str], None],
        full_compilation: bool,
        verbose: bool,
    ):
        """Run the actual compilation for changed files."""
        from ci.compiler.test_example_compilation import compile_examples_simple

        # Temporarily filter .cpp files to only compile changed ones
        original_method = self.compiler.find_cpp_files_for_example

        def filtered_cpp_files(ino_file: Path) -> List[Path]:
            all_cpp = original_method(ino_file)
            return [cpp for cpp in all_cpp if cpp in cpp_files_to_compile]

        self.compiler.find_cpp_files_for_example = filtered_cpp_files

        try:
            return compile_examples_simple(
                self.compiler,
                ino_files,
                pch_compatible_files,
                log_fn,
                full_compilation,
                verbose,
            )
        finally:
            self.compiler.find_cpp_files_for_example = original_method

    def _create_success_result(self, file_count: int, ino_files: Optional[List[Path]] = None, full_compilation: bool = False):
        """Create a successful compilation result for cached files."""
        from ci.compiler.test_example_compilation import CompilationResult

        object_file_map = {}
        
        # Rebuild object file map from existing files if full compilation is enabled
        if full_compilation and ino_files:
            build_dir = Path(".build/examples")
            for ino_file in ino_files:
                example_name = ino_file.parent.name
                example_build_dir = build_dir / example_name
                
                # Check for existing object files
                example_obj_files = []
                
                # Main .ino object file
                ino_obj_path = example_build_dir / f"{ino_file.stem}.o"
                if ino_obj_path.exists():
                    example_obj_files.append(ino_obj_path)
                
                # Additional .cpp object files
                cpp_files = self.compiler.find_cpp_files_for_example(ino_file)
                for cpp_file in cpp_files:
                    cpp_obj_path = example_build_dir / f"{cpp_file.stem}.o"
                    if cpp_obj_path.exists():
                        example_obj_files.append(cpp_obj_path)
                
                # Only add to map if we found object files
                if example_obj_files:
                    object_file_map[ino_file] = example_obj_files

        return CompilationResult(
            successful_count=file_count,
            failed_count=0,
            compile_time=0.0,
            failed_examples=[],
            object_file_map=object_file_map,
        )


def create_cache_compiler(
    compiler: Compiler, cache_file: Optional[Path] = None, verbose: bool = False
) -> CacheAwareCompiler:
    """Create a cache-aware compiler. Simple factory function."""
    if cache_file is None:
        cache_file = Path(".build/fingerprint_cache.json")
    return CacheAwareCompiler(compiler, cache_file, verbose)
