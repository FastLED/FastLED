from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
FastLED Example Compiler

Streamlined compiler that uses the PioCompiler to build FastLED examples for various boards.
This replaces the previous complex compilation system with a simpler approach using the Pio compiler.

ESP32 QEMU Support:
-------------------
When using -o/--out with ESP32 boards and a directory path or filename containing "qemu",
the compiler automatically generates properly merged flash images for QEMU testing.
The merged flash image includes:
  - Bootloader at 0x1000 (ESP32) or 0x0 (ESP32-C3/S3)
  - Partition table at 0x8000
  - boot_app0 at 0xe000
  - Application firmware at 0x10000

Uses esptool.py when available, falls back to manual binary merge if not installed.
"""

import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional

from ci.boards import Board, create_board
from ci.compiler.argument_parser import CompilationConfig
from ci.compiler.board_example_utils import (
    get_default_boards,
    resolve_example_path,
)
from ci.compiler.compilation_orchestrator import (
    compile_board_examples,
    format_elapsed_time,
)
from ci.compiler.docker_manager import (
    DockerCompilationOrchestrator,
    DockerConfig,
    DockerContainerManager,
)
from ci.compiler.formatting_utils import green_text, red_text, yellow_text
from ci.compiler.output_utils import copy_build_artifact, validate_output_path
from ci.util.docker_command import get_docker_command
from ci.util.docker_helper import should_use_docker_for_board
from ci.util.global_interrupt_handler import (
    install_signal_handler,
    signal_interrupt,
    wait_for_cleanup,
)


def handle_docker_compilation(config: CompilationConfig) -> int:
    """
    Handle Docker compilation workflow using the new Docker manager.

    This function builds the Docker image (if needed) and runs the compilation
    inside a Docker container with pre-cached dependencies.

    Args:
        config: Compilation configuration

    Returns:
        Exit code (0 for success, non-zero for failure)
    """

    # Validate arguments
    if not config.boards:
        print("Error: --docker requires at least a board name")
        print("Usage: compile --docker <board> <example1> [example2 ...]")
        return 1

    board = config.boards[0]  # Use first board for Docker
    board_name = board.board_name

    # Get examples
    examples = config.examples
    if not examples:
        print("Error: --docker requires at least one example")
        print("Usage: compile --docker <board> <example1> [example2 ...]")
        return 1

    print("🐳 Docker compilation mode enabled")
    print(f"Board: {board_name}")
    print(f"Examples: {', '.join(examples)}")
    print()

    # Get project root
    project_root = Path(__file__).parent.parent.absolute()

    # Determine output directory for volume mounting
    output_dir = None
    if config.output_path:
        output_path_str = str(config.output_path)
        # Determine the directory to mount
        if output_path_str.endswith((".bin", ".hex", ".elf")):
            # Output is a file, use its parent directory
            output_dir = Path(output_path_str).parent
        else:
            # Output is already a directory
            output_dir = Path(output_path_str)

    # Create Docker configuration with optional output directory
    docker_config = DockerConfig.from_board(board_name, project_root, output_dir)

    # Create container manager
    container_mgr = DockerContainerManager(docker_config)

    # Create orchestrator
    orchestrator = DockerCompilationOrchestrator(docker_config, container_mgr)

    # Ensure image exists
    if not orchestrator.ensure_image_exists(build_if_missing=config.docker_build):
        return 1

    # Pass actual image name to container manager (may differ from config if local image exists)
    actual_image_name = orchestrator._get_image_name()
    if actual_image_name != docker_config.image_name:
        container_mgr.set_image_name(actual_image_name)

    # Print container management info
    print()
    print(f"Managing Docker container: {docker_config.container_name}")

    # Build the compile command with optional flags
    example_name = examples[0] if examples else "Blink"
    compile_cmd = f"bash compile {board_name} {example_name}"
    if config.defines:
        compile_cmd += f" --defines {','.join(config.defines)}"
    if config.merged_bin:
        compile_cmd += " --merged-bin"
    if config.output_path:
        # If output directory is mounted, use the container path (/fastled/output)
        # Otherwise, use POSIX path format for Docker/Linux compatibility
        if docker_config.output_dir is not None:
            # Preserve filename for file outputs (e.g., merged.bin)
            if config.output_path.suffix:  # Has extension (.bin, .hex, etc.)
                filename = config.output_path.name
                compile_cmd += f" -o output/{filename}"
            else:
                # Directory output - use as-is
                compile_cmd += " -o output"
        else:
            # Use POSIX path format (forward slashes) for Docker/Linux compatibility
            # On Windows, backslashes in paths get interpreted as escape characters in bash
            output_path_posix = config.output_path.as_posix()
            compile_cmd += f" -o {output_path_posix}"
    if config.max_failures is not None:
        compile_cmd += f" --max-failures {config.max_failures}"

    # Run compilation
    result = orchestrator.run_compilation(compile_cmd, stream_output=True)

    # Copy output artifacts from container to host if compilation succeeded and -o flag was used
    # Skip if output directory is already mounted as a volume (artifacts are already on host)
    if (
        result.returncode == 0
        and config.output_path
        and docker_config.output_dir is None
    ):
        output_path = str(config.output_path)
        # Determine the directory to copy
        if output_path.endswith(".bin"):
            # Output is a file, copy its parent directory
            output_dir_str = str(Path(output_path).parent)
        else:
            # Output is already a directory
            output_dir_str = output_path

        # Copy artifacts from container to host using docker cp
        container_path = f"/fastled/{output_dir_str}"
        host_path = project_root / output_dir_str
        orchestrator.copy_artifacts(container_path, host_path)
    elif result.returncode == 0 and docker_config.output_dir is not None:
        print()
        print(
            f"✅ Build artifacts available in mounted volume: {docker_config.output_dir}"
        )

    # Clean up container registration (keeps container alive for debugging)
    print()
    print(f"Cleaning up container registration: {docker_config.container_name}")
    container_mgr.cleanup()

    # Stop container after compilation to trigger auto-removal (--rm flag)
    # This ensures fresh bind mounts on next run and prevents stale source issues
    print(f"Stopping container: {docker_config.container_name}")
    try:
        subprocess.run(
            [get_docker_command(), "stop", docker_config.container_name],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=30,
        )
    except (FileNotFoundError, subprocess.TimeoutExpired):
        # Silently fail if Docker is not available
        pass

    return result.returncode


def main() -> int:
    """Main function."""
    # Install signal handler for Ctrl-C to work properly on all platforms
    install_signal_handler()

    # Parse arguments using new CompilationArgumentParser
    from ci.compiler.argument_parser import CompilationArgumentParser, WorkflowType

    # Detect if running inside Docker container
    if os.environ.get("FASTLED_DOCKER") == "1":
        project_root = Path("/fastled")
    else:
        project_root = Path(__file__).parent.parent.absolute()
    parser = CompilationArgumentParser(project_root)

    # First check if --supported-boards was requested before full parsing
    if "--supported-boards" in sys.argv:
        print(",".join(get_default_boards()))
        return 0

    config = parser.parse()

    # Handle verbose mode
    if config.verbose:
        os.environ["VERBOSE"] = "1"

    # Handle default boards if none specified
    if not config.boards:
        board_names = get_default_boards()
        boards: list[Board] = []
        for board_name in board_names:
            try:
                board = create_board(board_name, no_project_options=False)
                boards.append(board)
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                print(f"ERROR: Failed to get board '{board_name}': {e}")
                return 1
        # Create new config with default boards
        from ci.compiler.argument_parser import CompilationConfig

        config = CompilationConfig(
            boards=boards,
            examples=config.examples,
            workflow=config.workflow,
            defines=config.defines,
            extra_packages=config.extra_packages,
            verbose=config.verbose,
            enable_cache=config.enable_cache,
            output_path=config.output_path,
            merged_bin=config.merged_bin,
            log_failures=config.log_failures,
            max_failures=config.max_failures,
            docker_build=config.docker_build,
            force_local=config.force_local,
            wasm_run=config.wasm_run,
            global_cache_dir=config.global_cache_dir,
            skip_filters=config.skip_filters,
            no_parallel=config.no_parallel,
        )

    # Auto-detect Docker availability if neither --docker nor --local is specified
    # and not already running inside Docker
    # IMPORTANT: Force --local on GitHub Actions to avoid pulling Docker images
    if (
        config.workflow == WorkflowType.NATIVE
        and not config.force_local
        and not os.environ.get("FASTLED_DOCKER")
        and config.boards
        and len(config.boards) == 1
    ):
        # Check if we're on GitHub Actions - if so, force local compilation
        from ci.util.github_env import is_github_actions

        if is_github_actions():
            print(
                yellow_text(
                    "ℹ️  GitHub Actions detected - using --local (native compilation)"
                )
            )
            print("   This avoids pulling Docker images on CI runners")
            # Force local compilation by setting force_local=True
            from ci.compiler.argument_parser import CompilationConfig

            config = CompilationConfig(
                boards=config.boards,
                examples=config.examples,
                workflow=WorkflowType.NATIVE,
                defines=config.defines,
                extra_packages=config.extra_packages,
                verbose=config.verbose,
                enable_cache=config.enable_cache,
                output_path=config.output_path,
                merged_bin=config.merged_bin,
                log_failures=config.log_failures,
                max_failures=config.max_failures,
                docker_build=config.docker_build,
                force_local=True,  # Force local on GitHub Actions
                wasm_run=config.wasm_run,
                global_cache_dir=config.global_cache_dir,
                skip_filters=config.skip_filters,
                no_parallel=config.no_parallel,
            )
        else:
            # Normal Docker auto-detection for non-CI environments
            board_name = config.boards[0].board_name
            use_docker, reason = should_use_docker_for_board(board_name, verbose=False)
            if use_docker:
                print(
                    green_text(
                        "🐳 Docker detected and will be used for faster compilation"
                    )
                )
                print("   Use --local to force native compilation instead")
                # Update workflow to Docker
                from ci.compiler.argument_parser import CompilationConfig

                config = CompilationConfig(
                    boards=config.boards,
                    examples=config.examples,
                    workflow=WorkflowType.DOCKER,
                    defines=config.defines,
                    extra_packages=config.extra_packages,
                    verbose=config.verbose,
                    enable_cache=config.enable_cache,
                    output_path=config.output_path,
                    merged_bin=config.merged_bin,
                    log_failures=config.log_failures,
                    max_failures=config.max_failures,
                    docker_build=config.docker_build,
                    force_local=config.force_local,
                    wasm_run=config.wasm_run,
                    global_cache_dir=config.global_cache_dir,
                    skip_filters=config.skip_filters,
                    no_parallel=config.no_parallel,
                )

    # Handle Docker compilation mode
    # Skip if already running inside Docker (FASTLED_DOCKER env var is set)
    if config.workflow == WorkflowType.DOCKER and not os.environ.get("FASTLED_DOCKER"):
        return handle_docker_compilation(config)

    # Handle WASM compilation by delegating to wasm_compile.py
    if config.workflow == WorkflowType.WASM:
        if len(config.boards) > 1:
            print("ERROR: WASM compilation cannot be combined with other boards")
            return 1

        examples = config.examples
        if not examples:
            examples = ["wasm"]

        if len(examples) != 1:
            print(
                f"ERROR: WASM compilation requires exactly one example, got {len(examples)}: {examples}"
            )
            return 1

        # Delegate to wasm_compile.py
        import subprocess

        example = examples[0]
        cmd = [sys.executable, "-m", "ci.wasm_compile", f"examples/{example}"]
        if config.wasm_run:
            cmd.append("--run")

        result = subprocess.run(cmd)
        return result.returncode

    # Get boards and examples from config
    boards = config.boards
    examples = config.examples

    # Validate examples exist
    for example in examples:
        try:
            resolve_example_path(example)
        except FileNotFoundError as e:
            print(f"ERROR: {e}")
            return 1

    # Validate the output path if specified
    if config.output_path:
        sketch_name = examples[0]
        board = boards[0]
        is_valid, resolved_output_path, error_msg = validate_output_path(
            str(config.output_path), sketch_name, board
        )
        if not is_valid:
            print(f"ERROR: {error_msg}")
            return 1

    # Set up defines (make a mutable copy)
    defines = list(config.defines)

    # Check if we're compiling Esp32C3_SPI_ISR and auto-enable validation
    for example in examples:
        if "Esp32C3_SPI_ISR" in example and len(examples) == 1:
            if "FL_SPI_ISR_VALIDATE" not in defines:
                defines.append("FL_SPI_ISR_VALIDATE")
                print(
                    yellow_text(
                        "⚠️  Auto-enabling FL_SPI_ISR_VALIDATE for Esp32C3_SPI_ISR validation testing"
                    )
                )
            break

    # Start compilation
    start_time = time.time()
    print(
        f"Starting compilation for {len(boards)} boards with {len(examples)} examples"
    )

    compilation_errors: list[str] = []
    failed_example_names: list[str] = []
    failure_logs_dir: Optional[Path] = config.log_failures
    stopped_early: bool = False

    # Compile for each board
    for board in boards:
        # Determine merged-bin output path if requested
        merged_bin_output = None
        if config.merged_bin and config.output_path:
            merged_bin_output = config.output_path

        result = compile_board_examples(
            board=board,
            examples=examples,
            defines=defines,
            verbose=config.verbose,
            enable_cache=config.enable_cache,
            global_cache_dir=config.global_cache_dir,
            merged_bin=config.merged_bin,
            merged_bin_output=merged_bin_output,
            extra_packages=config.extra_packages if config.extra_packages else None,
            max_failures=config.max_failures,
            skip_filters=config.skip_filters,
        )

        if not result.ok:
            # Record board-level error
            compilation_errors.append(f"Board {board.board_name} failed")
            print(red_text(f"ERROR: Compilation failed for board {board.board_name}"))
            # Print each failing sketch's stdout and collect names for summary
            for sketch in result.sketch_results:
                if not sketch.success:
                    if sketch.example and sketch.example not in failed_example_names:
                        failed_example_names.append(sketch.example)
                    # Write per-example failure logs when requested
                    if failure_logs_dir is not None:
                        try:
                            failure_logs_dir.mkdir(parents=True, exist_ok=True)
                            safe_name = f"{sketch.example}.log"
                            log_path = failure_logs_dir / safe_name
                            with log_path.open(
                                "a", encoding="utf-8", errors="ignore"
                            ) as f:
                                f.write(sketch.output)
                                f.write("\n")
                        except KeyboardInterrupt:
                            print("Keyboard interrupt detected, cancelling builds")
                            import _thread

                            _thread.interrupt_main()
                            raise
                        except Exception as e:
                            print(
                                f"Warning: Could not write failure log for {sketch.example}: {e}"
                            )
                    print(f"\n{'-' * 60}")
                    print(f"Sketch: {sketch.example}")
                    print(f"{'-' * 60}")
                    # Print the collected output for this sketch
                    print(sketch.output)

            # Check if compilation stopped early due to max_failures
            if result.stopped_early:
                stopped_early = True
                break

            # Continue with other boards instead of stopping
        else:
            # Compilation succeeded - handle -o/--out option if specified
            if config.output_path:
                sketch_name = examples[0]  # We already validated there's exactly one
                is_valid, resolved_output_path, _ = validate_output_path(
                    str(config.output_path), sketch_name, board
                )
                if is_valid:
                    # Find the build directory for this board
                    project_root = Path(__file__).parent.parent.resolve()
                    build_dir = project_root / ".build" / "pio" / board.board_name

                    if not copy_build_artifact(
                        build_dir, board, sketch_name, resolved_output_path
                    ):
                        compilation_errors.append(
                            f"Failed to copy artifact for {board.board_name}"
                        )
                        print(
                            red_text(
                                f"ERROR: Failed to copy build artifact for {board.board_name}"
                            )
                        )
                        return 1

    # Report results
    elapsed_time = time.time() - start_time
    time_str = format_elapsed_time(elapsed_time)

    if compilation_errors:
        print(
            f"\nCompilation finished in {time_str} with {len(compilation_errors)} error(s):"
        )
        for error in compilation_errors:
            print(f"  - {error}")
        if failed_example_names:
            print("")
            print(red_text(f"ERROR! There were {len(failed_example_names)} failures:"))
            # Sort for stable output
            for name in sorted(failed_example_names):
                # print(f"  {name}")
                # same but in red
                print(red_text(f"  {name}"))
            print("")
        if stopped_early:
            print(
                yellow_text(
                    f"⚠️  Compilation stopped early after {len(failed_example_names)} failures (--max-failures={config.max_failures})"
                )
            )
            print("")
        return 1
    else:
        print(f"\nAll compilations completed successfully in {time_str}")
        return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        print("\nInterrupted by user")
        signal_interrupt()
        wait_for_cleanup()
        sys.exit(130)
