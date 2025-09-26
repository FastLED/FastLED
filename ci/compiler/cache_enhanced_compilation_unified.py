#!/usr/bin/env python3
"""
Unified Safe Cache-Enhanced Compilation

Simple wrapper that adds unified safe fingerprint cache to FastLED compilation.
Uses the same safe pre-computed pattern as JS and Python linting.
"""

import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Set

from ci.compiler.clang_compiler import Compiler
from ci.compiler.test_example_compilation import CompilationResult
from ci.util.hash_fingerprint_cache import HashFingerprintCache


class UnifiedCacheAwareCompiler:
    """Compiler wrapper using unified safe fingerprint caching."""

    def __init__(self, compiler: Compiler, cache_dir: Path, verbose: bool = False):
        self.compiler = compiler
        cache_dir.mkdir(parents=True, exist_ok=True)
        self.cache = HashFingerprintCache(cache_dir, "example_compilation")
        self.verbose = verbose

    def _get_all_files_to_monitor(self, ino_files: List[Path]) -> List[Path]:
        """Get all files that should be monitored for changes."""
        all_files: List[Path] = []

        # Add the example files
        all_files.extend(ino_files)

        # Add associated .cpp files
        for ino_file in ino_files:
            cpp_files = self.compiler.find_cpp_files_for_example(ino_file)
            all_files.extend(cpp_files)

        # Add compiler dependencies (headers, etc.)
        try:
            if hasattr(self.compiler, "_get_pch_dependencies"):
                all_files.extend(self.compiler._get_pch_dependencies())  # type: ignore[attr-defined]
            else:
                # Fallback: add all headers under the compiler's include path
                include_root = Path(self.compiler.settings.include_path)
                for pattern in ("**/*.h", "**/*.hpp"):
                    all_files.extend(include_root.glob(pattern))
        except Exception as e:
            if self.verbose:
                print(f"[CACHE] Warning: Could not get compiler dependencies: {e}")

        # Remove duplicates while preserving order
        seen: Set[Path] = set()
        unique_files: List[Path] = []
        for f in all_files:
            if f not in seen:
                seen.add(f)
                unique_files.append(f)

        return unique_files

    def compile_with_cache(
        self,
        ino_files: List[Path],
        pch_compatible_files: Set[Path],
        log_fn: Callable[[str], None],
        full_compilation: bool,
        verbose: bool = False,
        baseline_time: Optional[float] = None,  # Deprecated, kept for compatibility
    ) -> Dict[str, Any]:
        """
        Compile using unified safe fingerprint cache pattern.

        This method uses the pre-computed fingerprint approach to safely cache
        compilation results across process boundaries.
        """
        start_time = time.time()

        # Get all files to monitor for changes
        all_monitored_files = self._get_all_files_to_monitor(ino_files)

        # Use unified safe pattern to check if compilation is needed
        needs_compilation = self.cache.check_needs_update(all_monitored_files)

        total_source_files = len(ino_files) + sum(
            len(self.compiler.find_cpp_files_for_example(f)) for f in ino_files
        )

        if needs_compilation:
            log_fn(
                f"[CACHE] Files changed - compiling {total_source_files} source files"
            )
            log_fn(f"[CACHE] Monitoring {len(all_monitored_files)} files for changes")

            # Run the actual compilation
            result = self._run_compilation(
                ino_files,
                pch_compatible_files,
                log_fn,
                full_compilation,
                verbose,
            )

            # Mark as successful or invalidate cache based on result
            if result.successful_count > 0 and result.failed_count == 0:
                try:
                    self.cache.mark_success()
                    log_fn("[CACHE] Compilation successful - fingerprint cached")
                except Exception as e:
                    if self.verbose:
                        print(f"[CACHE] Failed to mark success: {e}")
            else:
                try:
                    self.cache.invalidate()
                    log_fn("[CACHE] Compilation failed - cache invalidated")
                except Exception as e:
                    if self.verbose:
                        print(f"[CACHE] Failed to invalidate cache: {e}")

        else:
            log_fn(
                f"[CACHE] All {len(all_monitored_files)} monitored files unchanged - skipping compilation"
            )
            result = self._create_cached_success_result(len(ino_files), ino_files)

        return {
            "compilation_result": result,
            "cache_stats": {
                "files_monitored": len(all_monitored_files),
                "compilation_needed": needs_compilation,
            },
            "total_time": time.time() - start_time,
        }

    def _run_compilation(
        self,
        ino_files: List[Path],
        pch_compatible_files: Set[Path],
        log_fn: Callable[[str], None],
        full_compilation: bool,
        verbose: bool,
    ) -> CompilationResult:
        """Run the actual compilation for all files."""
        from ci.compiler.test_example_compilation import compile_examples_simple

        return compile_examples_simple(
            self.compiler,
            ino_files,
            pch_compatible_files,
            log_fn,
            full_compilation,
            verbose,
        )

    def _create_cached_success_result(
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


def create_unified_cache_compiler(
    compiler: Compiler, cache_dir: Optional[Path] = None, verbose: bool = False
) -> UnifiedCacheAwareCompiler:
    """Create a unified cache-aware compiler. Factory function."""
    if cache_dir is None:
        cache_dir = Path(".cache")
    return UnifiedCacheAwareCompiler(compiler, cache_dir, verbose)
