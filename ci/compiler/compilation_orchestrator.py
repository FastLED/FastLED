"""
Compilation orchestration module.

This module provides high-level orchestration for compiling examples across boards.
It handles compilation workflow, result collection, and statistics reporting.
Every board build runs through fbuild (``ci/compiler/board_compiler.py``).
"""

import time
from concurrent.futures import as_completed
from dataclasses import dataclass, field
from typing import Optional

from typeguard import typechecked

from ci.boards import Board
from ci.compiler.board_compiler import BoardCompiler
from ci.compiler.board_example_utils import get_filtered_examples
from ci.compiler.compiler import SketchResult
from ci.compiler.path_manager import FastLEDPaths
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


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
    extra_packages: Optional[list[str]] = None,
    max_failures: Optional[int] = None,
    skip_filters: bool = False,
) -> BoardCompilationResult:
    """Compile examples for a single board with fbuild via BoardCompiler."""
    print(f"\n{'=' * 60}")
    print(f"COMPILING BOARD: {board.board_name}")
    print(f"EXAMPLES: {', '.join(examples)}")
    paths = FastLEDPaths(board.board_name)
    if verbose:
        print(f"BUILD DIR: {paths.build_dir}")

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

    print("BUILD BACKEND: fbuild")
    print(f"{'=' * 60}")

    try:
        # CI compile is hermetic w.r.t. the repo-root platformio.ini —
        # per-board flags live in ci/boards.py (#3274, #3278, #3279).
        compiler = BoardCompiler(
            board=board,
            verbose=verbose,
            additional_defines=defines,
            additional_libs=extra_packages,
        )

        futures = compiler.build(examples, max_failures=max_failures)

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

            except KeyboardInterrupt as ki:
                print("\n⏹️  Cancelling builds and cleaning up...")
                compiler.cancel_all()
                for f in futures:
                    f.cancel()
                print("   ✓ Cleanup complete")
                handle_keyboard_interrupt(ki)
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

        any_failures = failure_count > 0
        return BoardCompilationResult(
            ok=not any_failures,
            sketch_results=results,
            stopped_early=stopped_early,
            skipped_examples=skipped_examples,
        )
    except KeyboardInterrupt as ki:
        print("\n⏹️  Cancelling builds and cleaning up...")
        handle_keyboard_interrupt(ki)
        print("   ✓ Cleanup complete")
        # Don't re-raise - handle_keyboard_interrupt(ki) already signaled the main thread
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
