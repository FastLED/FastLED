"""
Compilation orchestration module.

This module provides high-level orchestration for compiling examples across boards.
It handles compilation workflow, result collection, and statistics reporting.
"""

import time
from concurrent.futures import Future, as_completed
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from typeguard import typechecked

from ci.boards import Board
from ci.compiler.board_example_utils import get_filtered_examples
from ci.compiler.compiler import CacheType, SketchResult
from ci.compiler.pio import FastLEDPaths, PioCompiler
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


@typechecked
@dataclass
class BoardCompilationResult:
    """Aggregated result for compiling a set of examples on a single board."""

    ok: bool
    sketch_results: list[SketchResult]
    stopped_early: bool = False
    skipped_examples: list[tuple[str, str]] = field(
        default_factory=lambda: []
    )  # List of (example, reason) tuples


def compile_board_examples(
    board: Board,
    examples: list[str],
    defines: list[str],
    verbose: bool,
    enable_cache: bool,
    global_cache_dir: Optional[Path] = None,
    merged_bin: bool = False,
    merged_bin_output: Optional[Path] = None,
    extra_packages: Optional[list[str]] = None,
    max_failures: Optional[int] = None,
    skip_filters: bool = False,
) -> BoardCompilationResult:
    """Compile examples for a single board using PioCompiler."""

    # Resolve global cache directory immediately for display
    resolved_cache_dir = None
    if global_cache_dir is not None:
        # User specified a path - use it exactly as provided
        resolved_cache_dir = global_cache_dir.resolve()
    else:
        # Default path ends with 'global_cache'
        resolved_cache_dir = Path.home() / ".fastled" / "global_cache"

    print(f"\n{'=' * 60}")
    print(f"COMPILING BOARD: {board.board_name}")
    print(f"EXAMPLES: {', '.join(examples)}")
    if merged_bin:
        print("MERGED-BIN MODE: Enabled")
        if merged_bin_output:
            print(f"MERGED-BIN OUTPUT: {merged_bin_output}")

    # Show cache directories in verbose mode only
    paths = FastLEDPaths(board.board_name)
    if verbose:
        print(f"GLOBAL CACHE: {resolved_cache_dir}")
        print(f"BUILD CACHE: {paths.build_cache_dir}")
        print(f"CORE DIR: {paths.core_dir}")
        print(f"PACKAGES DIR: {paths.packages_dir}")

    # Apply filters based on @filter directives (unless skip_filters is True)
    if skip_filters:
        # User explicitly requested to skip filters (--no-filter flag)
        filtered_examples = examples
        skipped_examples: list[tuple[str, str]] = []
    else:
        # Apply filters to prevent compilation failures
        filtered_examples, skipped_examples = get_filtered_examples(board, examples)

    if skipped_examples:
        print(
            f"\nSKIPPED {len(skipped_examples)} example(s) due to @filter constraints:"
        )
        for example, reason in skipped_examples:
            print(f"  - {example}: {reason}")
        print("  Use --no-filter to override and attempt compilation anyway")

    if filtered_examples:
        print(f"COMPILING {len(filtered_examples)} example(s)...")
        # Update examples to use only filtered ones
        examples = filtered_examples
    else:
        print("No examples to compile after filtering")
        print(f"{'=' * 60}")
        # Return success with no compilation
        return BoardCompilationResult(
            ok=True, sketch_results=[], skipped_examples=skipped_examples
        )

    print(f"{'=' * 60}")

    try:
        # Determine cache type based on flag and board frameworks
        frameworks = [f.strip().lower() for f in (board.framework or "").split(",")]
        mixed_frameworks = "arduino" in frameworks and "espidf" in frameworks
        cache_type = (
            CacheType.SCCACHE
            if enable_cache and not mixed_frameworks
            else CacheType.NO_CACHE
        )

        # Create PioCompiler instance
        compiler = PioCompiler(
            board=board,
            verbose=verbose,
            global_cache_dir=resolved_cache_dir,
            additional_defines=defines,
            additional_libs=extra_packages,
            cache_type=cache_type,
        )

        # Build all examples - use merged-bin method if requested
        if merged_bin:
            # Validate board supports merged binary
            if not compiler.supports_merged_bin():
                raise RuntimeError(
                    f"Board {board.board_name} does not support merged binary. "
                    f"Supported platforms: ESP32, ESP8266"
                )

            # Build with merged binary (only one example allowed in this mode)
            if len(examples) != 1:
                raise RuntimeError(
                    f"Merged-bin mode requires exactly one example, got {len(examples)}"
                )

            result = compiler.build_with_merged_bin(
                examples[0], output_path=merged_bin_output
            )
            futures: list[Future[SketchResult]] = []

            # Wrap the result in a completed future for consistency
            from concurrent.futures import Future as ConcurrentFuture

            future: ConcurrentFuture[SketchResult] = ConcurrentFuture()
            future.set_result(result)
            futures.append(future)
        else:
            # Build all examples normally
            futures = compiler.build(examples)

        # Wait for completion and collect results
        results: list[SketchResult] = []
        failure_count = 0
        stopped_early = False

        # Use as_completed to process results as they finish (faster failure detection)
        for future in as_completed(futures):
            try:
                result = future.result()
                results.append(result)

                # Track failures
                if not result.success:
                    failure_count += 1

                # SUCCESS/FAILED messages are printed by worker threads

                # Check if we've hit the max_failures threshold
                if max_failures is not None and failure_count >= max_failures:
                    stopped_early = True
                    print(
                        f"\n⚠️  Reached failure threshold ({failure_count} failures, max={max_failures}). "
                        f"Cancelling remaining builds..."
                    )
                    # Cancel all remaining futures
                    compiler.cancel_all()
                    for f in futures:
                        if not f.done():
                            f.cancel()
                    break

            except KeyboardInterrupt:
                print("\n⏹️  Cancelling builds and cleaning up...")
                compiler.cancel_all()
                for f in futures:
                    f.cancel()
                print("   ✓ Cleanup complete")
                handle_keyboard_interrupt_properly()
            except Exception as e:
                # Represent unexpected exception as a failed SketchResult for consistency
                from pathlib import Path as _Path

                results.append(
                    SketchResult(
                        success=False,
                        output=f"Build exception: {str(e)}",
                        build_dir=_Path("."),
                        example="<exception>",
                    )
                )
                failure_count += 1
                print(f"EXCEPTION during build: {e}")
                # Cleanup
                compiler.cancel_all()

                # Check max_failures after exception too
                if max_failures is not None and failure_count >= max_failures:
                    stopped_early = True
                    print(
                        f"\n⚠️  Reached failure threshold ({failure_count} failures, max={max_failures}). "
                        f"Cancelling remaining builds..."
                    )
                    for f in futures:
                        if not f.done():
                            f.cancel()
                    break

        # Show compiler statistics after all builds complete
        try:
            stats = compiler.get_cache_stats()
            if stats:
                print("\n" + "=" * 60)
                print("COMPILER STATISTICS")
                print("=" * 60)
                print(stats)
                print("=" * 60)
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Warning: Could not retrieve compiler statistics: {e}")

        any_failures = failure_count > 0
        return BoardCompilationResult(
            ok=not any_failures,
            sketch_results=results,
            stopped_early=stopped_early,
            skipped_examples=skipped_examples,
        )
    except KeyboardInterrupt:
        print("\n⏹️  Cancelling builds and cleaning up...")
        handle_keyboard_interrupt_properly()
        print("   ✓ Cleanup complete")
        # Don't re-raise - handle_keyboard_interrupt_properly() already signaled the main thread
        return BoardCompilationResult(
            ok=False,
            sketch_results=[],
            skipped_examples=skipped_examples,
        )
    except Exception as e:
        # Compiler could not be set up; return a single failed result to carry message
        from pathlib import Path as _Path

        return BoardCompilationResult(
            ok=False,
            sketch_results=[
                SketchResult(
                    success=False,
                    output=f"Compiler setup failed: {str(e)}",
                    build_dir=_Path("."),
                    example="<setup>",
                )
            ],
            skipped_examples=skipped_examples,
        )


def format_elapsed_time(elapsed_seconds: float) -> str:
    """Format elapsed time in a human-readable format (e.g., '2m:35s')."""
    return time.strftime("%Mm:%Ss", time.gmtime(elapsed_seconds))
