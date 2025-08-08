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
from ci.compiler.test_example_compilation import CompilationResult


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
        except KeyboardInterrupt:
            import _thread

            _thread.interrupt_main()
            raise
        except Exception as e:
            if self.verbose:
                print(f"[CACHE] Error checking {file_path.name}: {e}")
            self.stats["cache_misses"] += 1
            self.stats["files_compiled"] += 1
            return True

    def _headers_changed(self, baseline_time: float) -> bool:
        """Detect if any relevant header dependency has changed since last run.

        Uses the compiler's PCH dependency discovery to collect a conservative
        set of headers that impact example compilation, then queries the
        fingerprint cache to see if any actually changed content.
        """
        try:
            # Reuse the compiler's dependency discovery (covers src/** and platforms/**)
            dependencies: List[Path] = []
            if hasattr(self.compiler, "_get_pch_dependencies"):
                dependencies = self.compiler._get_pch_dependencies()  # type: ignore[attr-defined]
            else:
                # Fallback: hash all headers under the compiler's include path
                include_root = Path(self.compiler.settings.include_path)
                for pattern in ("**/*.h", "**/*.hpp"):
                    dependencies.extend(include_root.glob(pattern))

            changed_any = False
            for dep in dependencies:
                try:
                    if self.cache.has_changed(dep, baseline_time):
                        if self.verbose:
                            print(f"[CACHE] Header changed: {dep}")
                        changed_any = True
                        break
                except FileNotFoundError:
                    # Missing dependency means we must rebuild conservatively
                    if self.verbose:
                        print(f"[CACHE] Header missing (forces rebuild): {dep}")
                    changed_any = True
                    break
            return changed_any
        except KeyboardInterrupt:
            import _thread

            _thread.interrupt_main()
            raise
        except Exception as e:
            if self.verbose:
                print(f"[CACHE] Header scan failed, forcing rebuild: {e}")
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

        # If any header dependency changed, conservatively rebuild all example files
        force_recompile_due_to_headers = self._headers_changed(baseline_time)
        if force_recompile_due_to_headers and self.verbose:
            print(
                "[CACHE] Header dependency changes detected - forcing recompilation of example sources"
            )

        # Check which files need compilation
        files_to_compile: List[Path] = []
        for ino_file in ino_files:
            if force_recompile_due_to_headers or self.should_compile(
                ino_file, baseline_time
            ):
                files_to_compile.append(ino_file)

        # Check .cpp files too
        cpp_files_to_compile: List[Path] = []
        for ino_file in ino_files:
            cpp_files = self.compiler.find_cpp_files_for_example(ino_file)
            for cpp_file in cpp_files:
                if force_recompile_due_to_headers or self.should_compile(
                    cpp_file, baseline_time
                ):
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
            result = self._create_success_result(len(ino_files), ino_files)

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

    def _create_success_result(
        self, file_count: int, ino_files: Optional[List[Path]] = None
    ) -> CompilationResult:
        """Create a successful compilation result for cached files."""

        # For cached files, we need to populate object_file_map with existing object files
        # so that linking can proceed properly
        object_file_map: Dict[Path, List[Path]] = {}
        if ino_files:
            for ino_file in ino_files:
                # Find existing object files for this example
                example_name = ino_file.parent.name
                build_dir = Path(".build/examples") / example_name

                obj_files: List[Path] = []
                if build_dir.exists():
                    # Look for the main .ino object file
                    ino_obj = build_dir / f"{ino_file.stem}.o"
                    if ino_obj.exists():
                        obj_files.append(ino_obj)

                    # Look for additional .cpp files in the same directory as the .ino
                    cpp_files = self.compiler.find_cpp_files_for_example(ino_file)
                    for cpp_file in cpp_files:
                        cpp_obj = build_dir / f"{cpp_file.stem}.o"
                        if cpp_obj.exists():
                            obj_files.append(cpp_obj)

                if obj_files:
                    object_file_map[ino_file] = obj_files

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
