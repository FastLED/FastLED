#!/usr/bin/env python3
"""
Cache-Enhanced Compilation Module

Integrates the fingerprint cache with FastLED's existing compilation system
to provide significant speedup for incremental builds by skipping unchanged files.
"""

import os
import time
from concurrent.futures import Future
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Set

from ci.ci.fingerprint_cache import FingerprintCache
from ci.compiler.clang_compiler import Compiler, Result


@dataclass
class CacheStats:
    """Statistics for cache performance tracking."""
    cache_hits: int = 0
    cache_misses: int = 0
    files_skipped: int = 0
    files_compiled: int = 0
    time_saved_seconds: float = 0.0
    
    @property
    def total_files(self) -> int:
        """Total number of files processed."""
        return self.files_skipped + self.files_compiled
    
    @property
    def cache_hit_rate(self) -> float:
        """Cache hit rate as percentage."""
        total = self.cache_hits + self.cache_misses
        return (self.cache_hits / total * 100) if total > 0 else 0.0
    
    @property
    def skip_rate(self) -> float:
        """File skip rate as percentage."""
        return (self.files_skipped / self.total_files * 100) if self.total_files > 0 else 0.0


@dataclass
class CacheConfiguration:
    """Configuration for cache-enhanced compilation."""
    cache_file: Path
    enable_cache: bool = True
    cache_verbose: bool = False
    force_recompile: bool = False  # If True, ignore cache and recompile everything
    
    
class CacheEnhancedCompiler:
    """
    Wrapper around the standard Compiler that adds fingerprint cache support.
    
    This class provides the same interface as the standard compilation functions
    but adds intelligent file change detection to skip unchanged files.
    """
    
    def __init__(self, compiler: Compiler, cache_config: CacheConfiguration):
        """
        Initialize cache-enhanced compiler.
        
        Args:
            compiler: The underlying Compiler instance
            cache_config: Cache configuration options
        """
        self.compiler = compiler
        self.cache_config = cache_config
        self.cache = FingerprintCache(cache_config.cache_file) if cache_config.enable_cache else None
        self.stats = CacheStats()
        
    def should_compile_file(self, source_file: Path, baseline_time: float) -> bool:
        """
        Determine if a source file needs compilation based on fingerprint cache.
        
        Args:
            source_file: Path to source file to check
            baseline_time: Baseline modification time to compare against
            
        Returns:
            True if file should be compiled, False if it can be skipped
        """
        if not self.cache or self.cache_config.force_recompile:
            return True
            
        try:
            has_changed = self.cache.has_changed(source_file, baseline_time)
            
            if has_changed:
                self.stats.cache_misses += 1
                return True
            else:
                self.stats.cache_hits += 1
                if self.cache_config.cache_verbose:
                    print(f"[CACHE] Skipping unchanged file: {source_file.name}")
                return False
                
        except FileNotFoundError:
            # File doesn't exist, definitely need to "compile" (will fail appropriately)
            self.stats.cache_misses += 1
            return True
        except Exception as e:
            # Cache error - fall back to compilation
            if self.cache_config.cache_verbose:
                print(f"[CACHE] Warning: Cache error for {source_file.name}: {e}, falling back to compilation")
            self.stats.cache_misses += 1
            return True

    def compile_examples_with_cache(
        self,
        ino_files: List[Path],
        pch_compatible_files: Set[Path],
        log_timing: Callable[[str], None],
        full_compilation: bool,
        verbose: bool = False,
        baseline_time: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Cache-aware version of compile_examples_simple.
        
        This function wraps the standard compilation flow but adds fingerprint
        cache checking to skip unchanged files for dramatic speedup.
        
        Args:
            ino_files: List of .ino files to compile
            pch_compatible_files: Set of files that are PCH compatible  
            log_timing: Logging function
            full_compilation: If True, preserve object files for linking
            verbose: Enable verbose output
            baseline_time: Baseline time for change detection (defaults to 1 hour ago)
            
        Returns:
            Dictionary with compilation results and cache statistics
        """
        if baseline_time is None:
            baseline_time = time.time() - 3600  # 1 hour ago as default
            
        compile_start = time.time()
        
        if self.cache_config.cache_verbose:
            log_timing(f"[CACHE] Using fingerprint cache: {self.cache_config.cache_file}")
            log_timing(f"[CACHE] Baseline time: {time.ctime(baseline_time)}")
        
        # Phase 1: Determine which files need compilation
        files_to_compile: List[Path] = []
        files_skipped: List[Path] = []
        
        for ino_file in ino_files:
            if self.should_compile_file(ino_file, baseline_time):
                files_to_compile.append(ino_file)
                self.stats.files_compiled += 1
            else:
                files_skipped.append(ino_file)
                self.stats.files_skipped += 1
                
        # Also check .cpp files that would be compiled
        all_cpp_files: List[Path] = []
        cpp_files_to_compile: List[Path] = []
        cpp_files_skipped: List[Path] = []
        
        for ino_file in ino_files:
            cpp_files = self.compiler.find_cpp_files_for_example(ino_file)
            all_cpp_files.extend(cpp_files)
            
            for cpp_file in cpp_files:
                if self.should_compile_file(cpp_file, baseline_time):
                    cpp_files_to_compile.append(cpp_file)
                    self.stats.files_compiled += 1
                else:
                    cpp_files_skipped.append(cpp_file)
                    self.stats.files_skipped += 1
        
        # Log cache analysis results
        total_files = len(ino_files) + len(all_cpp_files)
        if self.cache_config.enable_cache:
            log_timing(f"[CACHE] File analysis: {self.stats.files_skipped}/{total_files} files unchanged, {self.stats.files_compiled}/{total_files} need compilation")
            if self.stats.files_skipped > 0:
                estimated_time_saved = self.stats.files_skipped * 0.5  # Estimate 0.5s per file saved
                self.stats.time_saved_seconds = estimated_time_saved
                log_timing(f"[CACHE] Estimated time savings: {estimated_time_saved:.1f}s")
        
        # Phase 2: Compile only the files that need it
        if files_to_compile or cpp_files_to_compile:
            if self.cache_config.cache_verbose:
                log_timing(f"[CACHE] Compiling {len(files_to_compile)} .ino files and {len(cpp_files_to_compile)} .cpp files")
            
            # Use the original compilation function for files that need compilation
            from ci.compiler.test_example_compilation import compile_examples_simple
            
            # Temporarily override the file lists to only compile changed files
            original_find_cpp_files = self.compiler.find_cpp_files_for_example
            
            def cached_find_cpp_files(ino_file: Path) -> List[Path]:
                """Return only .cpp files that need compilation for this .ino file."""
                all_cpp = original_find_cpp_files(ino_file)
                # Only return .cpp files that are in our "to compile" list
                return [cpp for cpp in all_cpp if cpp in cpp_files_to_compile]
            
            # Temporarily replace the method
            self.compiler.find_cpp_files_for_example = cached_find_cpp_files
            
            try:
                # Call original compilation function with filtered file list
                result = compile_examples_simple(
                    self.compiler,
                    files_to_compile,  # Only compile changed .ino files
                    pch_compatible_files,
                    log_timing,
                    full_compilation,
                    verbose
                )
            finally:
                # Restore original method
                self.compiler.find_cpp_files_for_example = original_find_cpp_files
        else:
            # No files need compilation - create a successful result
            log_timing("[CACHE] All files unchanged - no compilation needed!")
            from ci.compiler.test_example_compilation import CompilationResult
            result = CompilationResult(
                successful_count=len(ino_files),
                failed_count=0,
                compile_time=0.0,
                failed_examples=[],
                object_file_map={}
            )
        
        # Phase 3: Update cache statistics and return enhanced results
        compile_time = time.time() - compile_start
        
        # Log final cache statistics
        if self.cache_config.enable_cache:
            log_timing(f"[CACHE] Final stats: {self.stats.cache_hit_rate:.1f}% cache hit rate")
            log_timing(f"[CACHE] Skipped {self.stats.files_skipped}/{total_files} files ({self.stats.skip_rate:.1f}% skip rate)")
            if self.stats.time_saved_seconds > 0:
                log_timing(f"[CACHE] Time saved: {self.stats.time_saved_seconds:.1f}s")
        
        # Return enhanced results with cache information
        return {
            'compilation_result': result,
            'cache_stats': self.stats,
            'files_skipped': files_skipped,
            'cpp_files_skipped': cpp_files_skipped,
            'total_time': compile_time,
            'cache_enabled': self.cache_config.enable_cache
        }

    def get_cache_statistics(self) -> CacheStats:
        """Get current cache statistics."""
        return self.stats
        
    def clear_cache_statistics(self) -> None:
        """Reset cache statistics."""
        self.stats = CacheStats()


def create_cache_enhanced_compiler(
    compiler: Compiler, 
    cache_file: Optional[Path] = None,
    enable_cache: bool = True,
    cache_verbose: bool = False,
    force_recompile: bool = False
) -> CacheEnhancedCompiler:
    """
    Create a cache-enhanced compiler with default configuration.
    
    Args:
        compiler: The base Compiler instance
        cache_file: Path to cache file (defaults to .build/fingerprint_cache.json)
        enable_cache: Whether to enable caching
        cache_verbose: Enable verbose cache logging
        force_recompile: Force recompilation ignoring cache
        
    Returns:
        CacheEnhancedCompiler instance
    """
    if cache_file is None:
        cache_file = Path(".build/fingerprint_cache.json")
        
    cache_config = CacheConfiguration(
        cache_file=cache_file,
        enable_cache=enable_cache,
        cache_verbose=cache_verbose,
        force_recompile=force_recompile
    )
    
    return CacheEnhancedCompiler(compiler, cache_config)