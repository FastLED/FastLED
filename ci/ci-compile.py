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

import argparse
import os
import sys
import threading
import time
from concurrent.futures import Future, as_completed
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, cast

from typeguard import typechecked

from ci.boards import ALL, Board, create_board
from ci.compiler.compiler import CacheType, SketchResult
from ci.compiler.pio import PioCompiler
from ci.docker.build_image import extract_architecture


def green_text(text: str) -> str:
    """Return text in green color."""
    return f"\033[32m{text}\033[0m"


def red_text(text: str) -> str:
    """Return text in red color."""
    return f"\033[31m{text}\033[0m"


def handle_docker_compilation(args: argparse.Namespace) -> int:
    """
    Handle Docker compilation workflow.

    This function builds the Docker image (if needed) and runs the compilation
    inside a Docker container with pre-cached dependencies.

    Args:
        args: Parsed command-line arguments

    Returns:
        Exit code (0 for success, non-zero for failure)
    """
    import subprocess

    # Validate arguments
    if not args.boards:
        print("Error: --docker requires at least a board name")
        print("Usage: compile --docker <board> <example1> [example2 ...]")
        return 1

    board_name = args.boards.split(",")[0]  # Use first board for Docker

    # Get examples
    examples: List[str] = []
    if args.positional_examples:
        examples = args.positional_examples
    elif args.examples:
        examples = args.examples.split(",")
    else:
        print("Error: --docker requires at least one example")
        print("Usage: compile --docker <board> <example1> [example2 ...]")
        return 1

    print("🐳 Docker compilation mode enabled")
    print(f"Board: {board_name}")
    print(f"Examples: {', '.join(examples)}")
    print()

    # Determine architecture for image name
    architecture = extract_architecture(board_name)

    # Generate config hash for image name BEFORE building
    from ci.docker.build_image import generate_config_hash

    try:
        config_hash = generate_config_hash(board_name)
        image_name = f"fastled-platformio-{architecture}-{board_name}-{config_hash}"
    except:
        # Fallback if hash generation fails
        image_name = f"fastled-platformio-{architecture}-{board_name}"

    # Check if Docker image exists
    image_check = subprocess.run(
        ["docker", "image", "inspect", image_name], capture_output=True, text=True
    )

    if image_check.returncode != 0:
        # Image doesn't exist, build it
        print(f"Building Docker image for platform: {board_name}")
        build_cmd = [
            sys.executable,
            "ci/build_docker_image_pio.py",
            "--platform",
            board_name,
            "--image-name",
            image_name,
        ]

        result = subprocess.run(build_cmd)
        if result.returncode != 0:
            print("Error: Failed to build Docker image")
            return 1
    else:
        print(f"✓ Using existing Docker image: {image_name}")

    # Get absolute path to project root
    project_root = str(Path(__file__).parent.parent.absolute())

    # Container name based on platform (one container per platform)
    container_name = f"fastled-{board_name}"

    print()
    print(f"Managing Docker container: {container_name}")

    # Check if container exists
    container_check = subprocess.run(
        ["docker", "container", "inspect", container_name],
        capture_output=True,
        text=True,
    )

    if container_check.returncode != 0:
        # Container doesn't exist, create it
        print(f"Creating new container: {container_name}")

        # Use MSYS_NO_PATHCONV to prevent git-bash from mangling paths on Windows
        env = os.environ.copy()
        env["MSYS_NO_PATHCONV"] = "1"

        # Escape paths for git-bash on Windows (use // prefix)
        mount_target = "//host" if sys.platform == "win32" else "/host"

        create_cmd = [
            "docker",
            "create",
            "--name",
            container_name,
            "-v",
            f"{project_root}:{mount_target}:ro",
            "--stop-timeout",
            "300",  # Auto-stop after 5 minutes of inactivity
            image_name,
            "tail",
            "-f",
            "/dev/null",  # Keep container running
        ]

        result = subprocess.run(create_cmd, env=env)
        if result.returncode != 0:
            print("Error: Failed to create container")
            return 1
    else:
        print(f"✓ Using existing container: {container_name}")

    # Check if container is running and paused
    container_state = subprocess.run(
        [
            "docker",
            "inspect",
            "-f",
            "{{.State.Running}} {{.State.Paused}}",
            container_name,
        ],
        capture_output=True,
        text=True,
    )

    state_parts = container_state.stdout.strip().split()
    is_running = state_parts[0] == "true"
    is_paused = state_parts[1] == "true" if len(state_parts) > 1 else False

    if not is_running:
        print(f"Starting container: {container_name}")
        result = subprocess.run(["docker", "start", container_name])
        if result.returncode != 0:
            print("Error: Failed to start container")
            return 1
    elif is_paused:
        print(f"Unpausing container: {container_name}")
        result = subprocess.run(["docker", "unpause", container_name])
        if result.returncode != 0:
            print("Error: Failed to unpause container")
            return 1

    print()
    print(f"Running compilation in container: {container_name}")
    print()

    # Build command to run inside Docker
    # Replicate the full entrypoint logic since docker exec bypasses the entrypoint
    example_name = examples[0] if examples else "Blink"

    # Full entrypoint logic: sync files, compile, handle artifacts
    entrypoint_logic = f"""
set -e

# Sync host directories to container working directory if they exist
if command -v rsync &> /dev/null; then
    echo "Syncing directories from host..."
    [ -d "/host/src" ] && rsync -a --delete /host/src/ /fastled/src/
    [ -d "/host/examples" ] && rsync -a --delete /host/examples/ /fastled/examples/
    [ -d "/host/ci" ] && rsync -a --delete /host/ci/ /fastled/ci/
    echo "Directory sync complete"
else
    echo "Warning: rsync not available, skipping directory sync"
fi

# Execute the compile command
bash compile {board_name} {example_name}
EXIT_CODE=$?

# If /fastled/output is mounted and compilation succeeded, copy build artifacts
if [ -d "/fastled/output" ] && [ $EXIT_CODE -eq 0 ]; then
    echo ""
    echo "Copying build artifacts to output directory..."

    # Find and copy all build artifacts
    if [ -d "/fastled/.pio/build" ]; then
        # Copy all binary files
        find /fastled/.pio/build -type f \\( -name "*.bin" -o -name "*.hex" -o -name "*.elf" -o -name "*.factory.bin" \\) \\
            -exec cp -v {{}} /fastled/output/ \\; 2>/dev/null || true

        echo "Build artifacts copied to output directory"
    fi
fi

exit $EXIT_CODE
"""

    # Execute command in running container
    # Ensure FASTLED_DOCKER=1 is set to skip .venv installation
    exec_cmd = [
        "docker",
        "exec",
        "-e",
        "FASTLED_DOCKER=1",
        container_name,
        "bash",
        "-c",
        entrypoint_logic,
    ]

    print(f"Executing: bash compile {board_name} {example_name}")
    print()

    # Stream output in real-time with unbuffered I/O
    # Use Popen to ensure proper streaming without buffering issues
    sys.stdout.flush()
    sys.stderr.flush()
    process = subprocess.Popen(
        exec_cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=1,  # Line buffered
        universal_newlines=True,
    )

    # Stream output line by line
    if process.stdout:
        for line in process.stdout:
            print(line, end="", flush=True)

    returncode = process.wait()
    result = subprocess.CompletedProcess(exec_cmd, returncode, stdout="", stderr="")

    # Pause container immediately after compilation
    # This keeps the container state but frees resources
    print()
    print(f"Pausing container: {container_name}")
    subprocess.run(
        ["docker", "pause", container_name],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    return result.returncode


def get_default_boards() -> List[str]:
    """Get all board names from the ALL boards list, with preferred boards first."""
    # These are the boards we want to compile first (preferred order)
    # Order matters: UNO first because it's used for global init and builds faster
    preferred_board_names = [
        "uno",  # Build is faster if this is first, because it's used for global init.
        "esp32dev",
        "esp8266",  # ESP8266
        "esp32c3",
        "esp32c6",
        "esp32s3",
        "teensylc",
        "teensy31",
        "teensy41",
        "digix",
        "rpipico",
        "rpipico2",
    ]

    # Get all available board names from the ALL list
    available_board_names = {board.board_name for board in ALL}

    # Start with preferred boards that exist, warn about missing ones
    default_boards: List[str] = []
    for board_name in preferred_board_names:
        if board_name in available_board_names:
            default_boards.append(board_name)
        else:
            print(
                f"WARNING: Preferred board '{board_name}' not found in available boards"
            )

    # Add all remaining boards (sorted for consistency)
    remaining_boards = sorted(available_board_names - set(default_boards))
    default_boards.extend(remaining_boards)

    return default_boards


def get_all_examples() -> List[str]:
    """Get all available example names from the examples directory."""
    project_root = Path(__file__).parent.parent.resolve()
    examples_dir = project_root / "examples"

    if not examples_dir.exists():
        return []

    # Find all .ino files recursively
    ino_files = list(examples_dir.rglob("*.ino"))

    examples: List[str] = []
    for ino_file in ino_files:
        # Get the parent directory relative to examples/
        # For examples/Blink/Blink.ino -> "Blink"
        # For examples/Fx/FxWave2d/FxWave2d.ino -> "Fx/FxWave2d"
        example_dir = ino_file.parent.relative_to(examples_dir)
        example_name = str(example_dir).replace("\\", "/")  # Normalize path separators
        examples.append(example_name)

    # Sort for consistent ordering
    examples.sort()
    return examples


def parse_args(args: Optional[list[str]] = None) -> argparse.Namespace:
    """Parse command line arguments."""
    print(f"DEBUG parse_args: input args = {args}")
    print(f"DEBUG parse_args: sys.argv = {sys.argv}")
    parser = argparse.ArgumentParser(
        description="Compile FastLED examples for various boards using PioCompiler"
    )
    parser.add_argument(
        "boards",
        type=str,
        help="Comma-separated list of boards to compile for",
        nargs="?",
    )
    parser.add_argument(
        "positional_examples",
        type=str,
        help="Examples to compile (positional arguments after board name)",
        nargs="*",
    )
    parser.add_argument(
        "--examples", type=str, help="Comma-separated list of examples to compile"
    )
    parser.add_argument(
        "--exclude-examples", type=str, help="Examples that should be excluded"
    )
    parser.add_argument(
        "--defines", type=str, help="Comma-separated list of compiler definitions"
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Enable verbose output"
    )
    parser.add_argument(
        "--no-cache",
        action="store_true",
        help="Disable sccache for faster compilation (default is already disabled)",
    )
    parser.add_argument(
        "--enable-cache",
        action="store_true",
        help="Enable sccache for faster compilation",
    )
    parser.add_argument(
        "--cache",
        action="store_true",
        help="(Deprecated) Enable sccache for faster compilation - use --enable-cache instead",
    )
    parser.add_argument(
        "--supported-boards",
        action="store_true",
        help="Print the list of supported boards and exit",
    )
    parser.add_argument(
        "--no-interactive",
        action="store_true",
        help="Disables interactive mode (deprecated)",
    )
    parser.add_argument(
        "--log-failures",
        type=str,
        help="Directory to write per-example failure logs (created on first failure)",
    )
    parser.add_argument(
        "--global-cache",
        type=str,
        help="Override global PlatformIO cache directory path (for testing)",
    )
    parser.add_argument(
        "-o",
        "--out",
        type=str,
        help="Output path for build artifact. Requires exactly one sketch. "
        "If path ends with '/', treated as directory. If has suffix, treated as file. "
        "Use '-o .' to save in current directory with sketch name.",
    )
    parser.add_argument(
        "--merged-bin",
        action="store_true",
        help="Generate merged binary for QEMU/flashing (ESP32/ESP8266 only). "
        "Produces a single flash image instead of separate bootloader/firmware/partition files.",
    )
    parser.add_argument(
        "--run",
        action="store_true",
        help="For WASM: Run Playwright tests after compilation (default is compile-only)",
    )
    parser.add_argument(
        "--docker",
        action="store_true",
        help="Run compilation inside Docker container with pre-cached dependencies",
    )
    parser.add_argument(
        "--extra-packages",
        type=str,
        help="Comma-separated list of extra PlatformIO library packages to install (e.g., 'OctoWS2811')",
    )

    try:
        parsed_args = parser.parse_intermixed_args(args)
        unknown = []
    except SystemExit:
        # If parse_intermixed_args fails, fall back to parse_known_args
        parsed_args, unknown = parser.parse_known_args(args)

    # Handle unknown arguments intelligently - treat non-flag arguments as examples
    unknown_examples: List[str] = []
    for arg in unknown:
        if not arg.startswith("-"):
            unknown_examples.append(arg)

    # Add unknown examples to positional_examples
    if unknown_examples:
        if (
            not hasattr(parsed_args, "positional_examples")
            or parsed_args.positional_examples is None
        ):
            parsed_args.positional_examples = []
        # Type assertion to help the type checker - argparse returns Any but we know it's List[str]
        positional_examples: List[str] = cast(
            List[str], getattr(parsed_args, "positional_examples", [])
        )
        positional_examples.extend(unknown_examples)
        parsed_args.positional_examples = positional_examples

    print(f"DEBUG parse_args: parsed_args.docker = {parsed_args.docker}")
    print(f"DEBUG parse_args: parsed_args.boards = {parsed_args.boards}")
    print(
        f"DEBUG parse_args: parsed_args.positional_examples = {parsed_args.positional_examples}"
    )

    return parsed_args


def resolve_example_path(example: str) -> str:
    """Resolve example name to path, ensuring it exists."""
    project_root = Path(__file__).parent.parent.resolve()
    examples_dir = project_root / "examples"

    # Handle both "Blink" and "examples/Blink" formats
    if example.startswith("examples/"):
        example = example[len("examples/") :]

    example_path = examples_dir / example
    if not example_path.exists():
        raise FileNotFoundError(f"Example not found: {example_path}")

    return example


@typechecked
@dataclass
class BoardCompilationResult:
    """Aggregated result for compiling a set of examples on a single board."""

    ok: bool
    sketch_results: List[SketchResult]


def compile_board_examples(
    board: Board,
    examples: List[str],
    defines: List[str],
    verbose: bool,
    enable_cache: bool,
    global_cache_dir: Optional[Path] = None,
    merged_bin: bool = False,
    merged_bin_output: Optional[Path] = None,
    extra_packages: Optional[List[str]] = None,
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
    print(f"GLOBAL CACHE: {resolved_cache_dir}")
    if merged_bin:
        print(f"MERGED-BIN MODE: Enabled")
        if merged_bin_output:
            print(f"MERGED-BIN OUTPUT: {merged_bin_output}")

    # Show other cache directories
    from ci.compiler.pio import FastLEDPaths

    paths = FastLEDPaths(board.board_name)

    print(f"BUILD CACHE: {paths.build_cache_dir}")
    print(f"CORE DIR: {paths.core_dir}")
    print(f"PACKAGES DIR: {paths.packages_dir}")

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
            futures: List[Future[SketchResult]] = []

            # Wrap the result in a completed future for consistency
            from concurrent.futures import Future as ConcurrentFuture

            future: ConcurrentFuture[SketchResult] = ConcurrentFuture()
            future.set_result(result)
            futures.append(future)
        else:
            # Build all examples normally
            futures = compiler.build(examples)

        # Wait for completion and collect results
        results: List[SketchResult] = []

        for future in futures:
            try:
                result = future.result()
                results.append(result)
                # SUCCESS/FAILED messages are printed by worker threads
            except KeyboardInterrupt:
                print("Keyboard interrupt detected, cancelling builds")
                compiler.cancel_all()
                import _thread

                for future in futures:
                    future.cancel()

                _thread.interrupt_main()
                raise
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
                print(f"EXCEPTION during build: {e}")
                # Cleanup
                compiler.cancel_all()

        # Show compiler statistics after all builds complete
        try:
            stats = compiler.get_cache_stats()
            if stats:
                print("\n" + "=" * 60)
                print("COMPILER STATISTICS")
                print("=" * 60)
                print(stats)
                print("=" * 60)
        except Exception as e:
            print(f"Warning: Could not retrieve compiler statistics: {e}")

        any_failures = False
        for r in results:
            if not r.success:
                any_failures = True
                break
        return BoardCompilationResult(ok=not any_failures, sketch_results=results)
    except KeyboardInterrupt:
        print("Keyboard interrupt detected, cancelling builds")
        import _thread

        _thread.interrupt_main()
        raise
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
        )


def get_board_artifact_extension(board: Board) -> str:
    """Get the primary artifact extension for a board."""
    # ESP32/ESP8266 boards always produce .bin files
    if board.board_name.startswith("esp"):
        return ".bin"

    # Most Arduino-based boards produce .hex files
    if board.framework and "arduino" in board.framework.lower():
        return ".hex"

    # Default to .hex for most microcontroller boards
    return ".hex"


def validate_output_path(
    output_path: str, sketch_name: str, board: Board
) -> tuple[bool, str, str]:
    """Validate output path and return (is_valid, resolved_path, error_message).

    Args:
        output_path: The user-specified output path
        sketch_name: Name of the sketch being built
        board: Board configuration

    Returns:
        Tuple of (is_valid, resolved_output_path, error_message)
    """
    import os

    expected_ext = get_board_artifact_extension(board)

    # Handle special case: -o .
    if output_path == ".":
        resolved_path = f"{sketch_name}{expected_ext}"
        return True, resolved_path, ""

    # If path ends with /, it's a directory
    if output_path.endswith("/") or output_path.endswith("\\"):
        resolved_path = os.path.join(output_path, f"{sketch_name}{expected_ext}")
        return True, resolved_path, ""

    # If path has an extension, it's a file - validate the extension
    if "." in os.path.basename(output_path):
        _, ext = os.path.splitext(output_path)
        if ext != expected_ext:
            return (
                False,
                "",
                f"Output file extension '{ext}' doesn't match expected '{expected_ext}' for board '{board.board_name}'",
            )
        return True, output_path, ""

    # Path doesn't end with / and has no extension - treat as directory
    resolved_path = os.path.join(output_path, f"{sketch_name}{expected_ext}")
    return True, resolved_path, ""


def validate_esp32_flash_mode_for_qemu(build_dir: Path, board: Board) -> bool:
    """Validate ESP32 flash mode for QEMU compatibility.

    QEMU requires DIO/80MHz flash mode, not QIO mode.

    Args:
        build_dir: Build directory path
        board: Board configuration

    Returns:
        True if flash mode is compatible or successfully validated
    """
    if not board.board_name.startswith("esp32"):
        return True  # Not an ESP32 board, no validation needed

    try:
        # Check platformio.ini for flash mode settings
        platformio_ini = build_dir / "platformio.ini"
        if platformio_ini.exists():
            content = platformio_ini.read_text()

            # Look for flash mode settings in build_flags
            if "FLASH_MODE=qio" in content.upper():
                print(f"⚠️  WARNING: QIO flash mode detected in platformio.ini")
                print(f"   QEMU requires DIO flash mode for compatibility")
                print(f"   Consider using DIO mode for QEMU builds")
                return True  # Warning but not blocking

            if "FLASH_MODE=dio" in content.upper():
                print(f"✅ DIO flash mode detected - compatible with QEMU")
                return True

        # Check if build artifacts exist to examine flash settings
        artifact_dir = build_dir / ".pio" / "build" / board.board_name
        if artifact_dir.exists():
            # Look for flash_args file which contains esptool flash arguments
            flash_args_file = artifact_dir / "flash_args"
            if flash_args_file.exists():
                flash_args = flash_args_file.read_text()
                if "--flash_mode qio" in flash_args:
                    print(f"⚠️  WARNING: QIO flash mode detected in build artifacts")
                    print(f"   QEMU may not boot properly with QIO mode")
                elif "--flash_mode dio" in flash_args:
                    print(
                        f"✅ DIO flash mode detected in build artifacts - QEMU compatible"
                    )

        print(f"ℹ️  Flash mode validation complete for {board.board_name}")
        return True

    except Exception as e:
        print(f"WARNING: Could not validate ESP32 flash mode: {e}")
        return True  # Don't fail build for validation issues


def create_esp32_flash_image(
    firmware_path: Path, output_path: Path, flash_size_mb: int = 4
) -> bool:
    """Create a properly formatted flash image for ESP32 QEMU.

    Args:
        firmware_path: Path to the firmware.bin file
        output_path: Path where to write the flash.bin
        flash_size_mb: Flash size in MB (must be 2, 4, 8, or 16)

    Returns:
        True if successful, False otherwise
    """
    try:
        if flash_size_mb not in [2, 4, 8, 16]:
            print(
                f"ERROR: Invalid flash size: {flash_size_mb}MB. Must be 2, 4, 8, or 16"
            )
            return False

        flash_size = flash_size_mb * 1024 * 1024

        # Read firmware content
        firmware_data = firmware_path.read_bytes()

        # Create flash image: firmware at beginning, rest filled with 0xFF
        flash_data = firmware_data + b"\xff" * (flash_size - len(firmware_data))

        # Ensure we have exactly the right size
        if len(flash_data) > flash_size:
            print(
                f"ERROR: Firmware size ({len(firmware_data)} bytes) exceeds flash size ({flash_size} bytes)"
            )
            return False

        flash_data = flash_data[:flash_size]  # Truncate to exact size

        output_path.write_bytes(flash_data)
        print(f"✅ Created flash image: {output_path} ({len(flash_data)} bytes)")
        return True

    except Exception as e:
        print(f"ERROR: Failed to create flash image: {e}")
        return False


def _create_flash_manual_merge(
    build_dir: Path, board_name: str, flash_size_mb: int
) -> bool:
    """Create a manually merged flash image from individual binary files.

    This function is used for testing and creates a properly formatted flash image
    by merging bootloader, partitions, boot_app0, and firmware at the correct offsets.

    Args:
        build_dir: Directory containing the binary files
        board_name: Name of the board (e.g., "esp32dev", "esp32c3", "esp32s3")
        flash_size_mb: Flash size in MB (must be 2, 4, 8, or 16)

    Returns:
        True if successful, False otherwise
    """
    try:
        if flash_size_mb not in [2, 4, 8, 16]:
            print(
                f"ERROR: Invalid flash size: {flash_size_mb}MB. Must be 2, 4, 8, or 16"
            )
            return False

        flash_size = flash_size_mb * 1024 * 1024

        # Determine bootloader offset based on chip type
        # ESP32 uses 0x1000, newer chips (C3, S3, C2, C5, C6, etc.) use 0x0
        if board_name.startswith("esp32") and not any(
            board_name.startswith(f"esp32{x}")
            for x in ["c3", "s3", "c2", "c5", "c6", "h2", "p4"]
        ):
            bootloader_offset = 0x1000  # ESP32 classic
        else:
            bootloader_offset = 0x0  # ESP32-C3, ESP32-S3, and newer

        # Standard offsets for partition table, boot_app0, and firmware
        partitions_offset = 0x8000
        boot_app0_offset = 0xE000
        firmware_offset = 0x10000

        # Create flash image filled with 0xFF
        flash_data = bytearray(b"\xff" * flash_size)

        # Required files and their offsets
        files_to_merge = [
            ("bootloader.bin", bootloader_offset),
            ("partitions.bin", partitions_offset),
            ("boot_app0.bin", boot_app0_offset),
            ("firmware.bin", firmware_offset),
        ]

        # Merge each file at its offset
        for filename, offset in files_to_merge:
            file_path = build_dir / filename
            if file_path.exists():
                file_data = file_path.read_bytes()
                # Check if file fits
                if offset + len(file_data) > flash_size:
                    print(
                        f"WARNING: {filename} at offset 0x{offset:x} exceeds flash size"
                    )
                    continue
                # Write file data at the specified offset
                flash_data[offset : offset + len(file_data)] = file_data
            else:
                # File not found - this is acceptable for optional files
                pass

        # Write the merged flash image
        output_path = build_dir / "flash.bin"
        output_path.write_bytes(flash_data)

        return True

    except Exception as e:
        print(f"ERROR: Failed to create manual flash merge: {e}")
        return False


def copy_esp32_qemu_artifacts(
    build_dir: Path, board: Board, sketch_name: str, output_path: str
) -> bool:
    """Copy all ESP32 QEMU artifacts to the specified output directory.

    Args:
        build_dir: Build directory path
        board: Board configuration
        sketch_name: Name of the sketch
        output_path: Target output path (directory or firmware.bin file)

    Returns:
        True if successful, False otherwise
    """
    import os
    import shutil

    print(f"🔧 ESP32 QEMU mode detected - collecting all required artifacts")

    # Validate flash mode for QEMU compatibility
    validate_esp32_flash_mode_for_qemu(build_dir, board)

    # Determine output directory
    output_path_obj = Path(output_path)
    if output_path.endswith(".bin"):
        # Output path is a file, use its parent directory
        output_dir = output_path_obj.parent
        firmware_name = output_path_obj.name
    else:
        # Output path is a directory
        output_dir = output_path_obj
        firmware_name = "firmware.bin"

    # Ensure output directory exists
    output_dir.mkdir(parents=True, exist_ok=True)

    # Find the PlatformIO build directory
    artifact_dir = build_dir / ".pio" / "build" / board.board_name

    if not artifact_dir.exists():
        print(f"ERROR: PlatformIO build directory not found: {artifact_dir}")
        return False

    print(f"📁 Searching for ESP32 artifacts in: {artifact_dir}")

    # Check if merged.bin exists (from --merged-bin build)
    merged_bin_src = artifact_dir / "merged.bin"
    if merged_bin_src.exists() and firmware_name == "merged.bin":
        # Use the already-created merged binary instead of copying firmware.bin
        print(f"ℹ️  Using pre-built merged.bin from build artifacts")
        required_files = {
            "merged.bin": firmware_name,
            "bootloader.bin": "bootloader.bin",
            "partitions.bin": "partitions.bin",
            "boot_app0.bin": "boot_app0.bin",
        }
    else:
        # Required files for ESP32 QEMU
        required_files = {
            "firmware.bin": firmware_name,
            "bootloader.bin": "bootloader.bin",
            "partitions.bin": "partitions.bin",
            "boot_app0.bin": "boot_app0.bin",
        }

    # Optional files
    optional_files = {"spiffs.bin": "spiffs.bin"}

    success = True
    copied_files: List[str] = []

    # Copy required files
    for source_name, dest_name in required_files.items():
        source_file = artifact_dir / source_name
        dest_file = output_dir / dest_name

        if source_file.exists():
            try:
                shutil.copy2(source_file, dest_file)
                file_size = source_file.stat().st_size
                print(f"✅ Copied {source_name}: {file_size} bytes")
                copied_files.append(dest_name)
            except Exception as e:
                print(f"ERROR: Failed to copy {source_name}: {e}")
                success = False
        else:
            print(f"⚠️  {source_name} not found at {source_file}")
            if source_name == "firmware.bin":
                success = False
            elif source_name == "partitions.bin":
                # Try to generate partitions.bin from partitions.csv if available
                partitions_csv = artifact_dir / "partitions.csv"
                if partitions_csv.exists():
                    print(
                        f"ℹ️  Found partitions.csv, you may need to generate partitions.bin manually"
                    )
                # For now, we'll create a default one in the fallback section

    # Copy optional files
    for source_name, dest_name in optional_files.items():
        source_file = artifact_dir / source_name
        dest_file = output_dir / dest_name

        if source_file.exists():
            try:
                shutil.copy2(source_file, dest_file)
                file_size = source_file.stat().st_size
                print(f"✅ Copied {source_name}: {file_size} bytes (optional)")
                copied_files.append(dest_name)
            except Exception as e:
                print(f"WARNING: Failed to copy optional file {source_name}: {e}")

    # Handle partitions.csv - required by tobozo QEMU
    partitions_csv_src = artifact_dir / "partitions.csv"
    partitions_csv_dest = output_dir / "partitions.csv"

    if partitions_csv_src.exists():
        try:
            shutil.copy2(partitions_csv_src, partitions_csv_dest)
            print(f"✅ Copied partitions.csv")
            copied_files.append("partitions.csv")
        except Exception as e:
            print(f"ERROR: Failed to copy partitions.csv: {e}")
            success = False
    else:
        # Create default partitions.csv
        print(f"⚠️  partitions.csv not found, creating default")
        default_partitions = """# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
otadata,  data, ota,     0xf000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x140000,
spiffs,   data, spiffs,  0x150000,0xb0000,
"""
        try:
            partitions_csv_dest.write_text(default_partitions)
            print(f"✅ Created default partitions.csv")
            copied_files.append("partitions.csv")
        except Exception as e:
            print(f"ERROR: Failed to create default partitions.csv: {e}")
            success = False

    # Generate missing critical files with defaults
    if not (output_dir / "boot_app0.bin").exists():
        print(f"⚠️  boot_app0.bin not found, creating default")
        try:
            # Create a minimal boot_app0.bin (8KB of 0xFF)
            boot_app0_data = b"\xff" * 8192
            (output_dir / "boot_app0.bin").write_bytes(boot_app0_data)
            print(f"✅ Created default boot_app0.bin (8192 bytes)")
            copied_files.append("boot_app0.bin")
        except Exception as e:
            print(f"ERROR: Failed to create default boot_app0.bin: {e}")
            success = False

    # Create proper flash image for QEMU
    firmware_bin = output_dir / firmware_name
    if firmware_bin.exists() and success:
        flash_bin = output_dir / "flash.bin"
        if create_esp32_flash_image(firmware_bin, flash_bin):
            copied_files.append("flash.bin")
        else:
            print(f"WARNING: Failed to create flash.bin - QEMU may not work properly")

    # Final verification
    print(f"\n📋 ESP32 QEMU artifacts summary:")
    print(f"   Output directory: {output_dir}")
    print(f"   Files copied: {len(copied_files)}")
    for filename in sorted(copied_files):
        file_path = output_dir / filename
        if file_path.exists():
            size = file_path.stat().st_size
            print(f"     ✅ {filename}: {size} bytes")

    if not success:
        print(f"❌ Some required files could not be copied or created")
        return False

    print(f"✅ ESP32 QEMU artifacts ready for use with tobozo/esp32-qemu-sim")
    return True


def copy_build_artifact(
    build_dir: Path, board: Board, sketch_name: str, output_path: str
) -> bool:
    """Copy the build artifact to the specified output path.

    For ESP32 boards with QEMU-style output paths, this will copy all required
    QEMU artifacts. For other cases, copies only the main firmware file.

    Args:
        build_dir: Build directory path
        board: Board configuration
        sketch_name: Name of the sketch
        output_path: Target output path

    Returns:
        True if successful, False otherwise
    """
    import os
    import shutil

    # Detect ESP32 QEMU mode
    is_esp32 = board.board_name.startswith("esp32")
    is_qemu_mode = (
        "qemu" in output_path.lower()
        or output_path.endswith("/")
        or output_path.endswith("\\")
    )

    if is_esp32 and is_qemu_mode:
        return copy_esp32_qemu_artifacts(build_dir, board, sketch_name, output_path)

    # Original single-file copy logic for non-QEMU cases
    expected_ext = get_board_artifact_extension(board)

    # Find the source artifact
    # PlatformIO builds are in .build/pio/{board}/.pio/build/{board}/firmware.{ext}
    artifact_dir = build_dir / ".pio" / "build" / board.board_name
    source_artifact = artifact_dir / f"firmware{expected_ext}"

    if not source_artifact.exists():
        print(f"ERROR: Build artifact not found: {source_artifact}")
        return False

    # Ensure output directory exists
    output_path_obj = Path(output_path)
    output_path_obj.parent.mkdir(parents=True, exist_ok=True)

    try:
        print(f"Copying {source_artifact} to {output_path}")
        shutil.copy2(source_artifact, output_path)
        print(f"✅ Build artifact saved to: {output_path}")
        return True
    except Exception as e:
        print(f"ERROR: Failed to copy build artifact: {e}")
        return False


def main() -> int:
    """Main function."""
    args = parse_args()
    if args.verbose:
        os.environ["VERBOSE"] = "1"

    if args.supported_boards:
        print(",".join(get_default_boards()))
        return 0

    # Handle Docker compilation mode
    # Skip if already running inside Docker (FASTLED_DOCKER env var is set)
    if args.docker and not os.environ.get("FASTLED_DOCKER"):
        print(f"DEBUG: args.docker={args.docker}, entering Docker mode")
        return handle_docker_compilation(args)
    elif args.docker and os.environ.get("FASTLED_DOCKER"):
        print(
            f"DEBUG: Already in Docker (FASTLED_DOCKER={os.environ.get('FASTLED_DOCKER')}), skipping nested Docker"
        )
    else:
        print(f"DEBUG: args.docker={args.docker}, running native compilation")

    # Determine which boards to compile for
    if args.boards:
        boards_names = args.boards.split(",")
    else:
        boards_names = get_default_boards()

    # Handle WASM compilation by delegating to wasm_compile.py
    if "wasm" in boards_names:
        if len(boards_names) > 1:
            print("ERROR: WASM compilation cannot be combined with other boards")
            return 1

        # Determine which examples to compile
        if args.positional_examples:
            examples: List[str] = []
            for example in args.positional_examples:
                if example.startswith("examples/"):
                    example = example[len("examples/") :]
                examples.append(example)
        elif args.examples:
            examples = args.examples.split(",")
        else:
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
        if args.run:
            cmd.append("--run")

        result = subprocess.run(cmd)
        return result.returncode

    # Get board objects
    boards: List[Board] = []
    for board_name in boards_names:
        try:
            board = create_board(board_name, no_project_options=False)
            boards.append(board)
        except Exception as e:
            print(f"ERROR: Failed to get board '{board_name}': {e}")
            return 1

    # Determine which examples to compile
    if args.positional_examples:
        # Convert positional examples, handling both "examples/Blink" and "Blink" formats
        examples: List[str] = []
        for example in args.positional_examples:
            # Remove "examples/" prefix if present
            if example.startswith("examples/"):
                example = example[len("examples/") :]
            examples.append(example)
    elif args.examples:
        examples = args.examples.split(",")
    else:
        # Compile all available examples since builds are fast now!
        examples = get_all_examples()

    # Process example exclusions
    if args.exclude_examples:
        exclude_examples = args.exclude_examples.split(",")
        examples = [ex for ex in examples if ex not in exclude_examples]

    # Validate examples exist
    for example in examples:
        try:
            resolve_example_path(example)
        except FileNotFoundError as e:
            print(f"ERROR: {e}")
            return 1

    # Validate --merged-bin option requirements
    if args.merged_bin:
        if len(examples) != 1:
            print(
                f"ERROR: --merged-bin option requires exactly one sketch, got {len(examples)}: {examples}"
            )
            return 1

        if len(boards) != 1:
            print(
                f"ERROR: --merged-bin option requires exactly one board, got {len(boards)}: {[b.board_name for b in boards]}"
            )
            return 1

        # Check if board supports merged binary
        board = boards[0]
        if not board.board_name.startswith("esp"):
            print(
                f"ERROR: --merged-bin only supports ESP32/ESP8266 boards, got: {board.board_name}"
            )
            return 1

    # Validate -o/--out option requirements
    if args.out:
        if len(examples) != 1:
            print(
                f"ERROR: -o/--out option requires exactly one sketch, got {len(examples)}: {examples}"
            )
            return 1

        if len(boards) != 1:
            print(
                f"ERROR: -o/--out option requires exactly one board, got {len(boards)}: {[b.board_name for b in boards]}"
            )
            return 1

        # Validate the output path
        sketch_name = examples[0]
        board = boards[0]
        is_valid, resolved_output_path, error_msg = validate_output_path(
            args.out, sketch_name, board
        )
        if not is_valid:
            print(f"ERROR: {error_msg}")
            return 1

    # Set up defines
    defines: List[str] = []
    if args.defines:
        defines.extend(args.defines.split(","))

    # Set up extra packages
    extra_packages: Optional[List[str]] = None
    if args.extra_packages:
        extra_packages = [pkg.strip() for pkg in args.extra_packages.split(",")]

    # Start compilation
    start_time = time.time()
    print(
        f"Starting compilation for {len(boards)} boards with {len(examples)} examples"
    )

    compilation_errors: List[str] = []
    failed_example_names: List[str] = []
    failure_logs_dir: Optional[Path] = (
        Path(args.log_failures) if getattr(args, "log_failures", None) else None
    )

    # Compile for each board
    for board in boards:
        # Parse global cache directory if provided
        global_cache_dir = None
        if args.global_cache:
            global_cache_dir = Path(args.global_cache)

        # Determine merged-bin output path if requested
        merged_bin_output = None
        if args.merged_bin and args.out:
            merged_bin_output = Path(args.out)

        result = compile_board_examples(
            board=board,
            examples=examples,
            defines=defines,
            verbose=args.verbose,
            enable_cache=(args.enable_cache or args.cache) and not args.no_cache,
            global_cache_dir=global_cache_dir,
            merged_bin=args.merged_bin,
            merged_bin_output=merged_bin_output,
            extra_packages=extra_packages,
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
            # Continue with other boards instead of stopping
        else:
            # Compilation succeeded - handle -o/--out option if specified
            if args.out:
                sketch_name = examples[0]  # We already validated there's exactly one
                is_valid, resolved_output_path, _ = validate_output_path(
                    args.out, sketch_name, board
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
    time_str = time.strftime("%Mm:%Ss", time.gmtime(elapsed_time))

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
        return 1
    else:
        print(f"\nAll compilations completed successfully in {time_str}")
        return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        sys.exit(1)
