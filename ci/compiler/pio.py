from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
PlatformIO Builder for FastLED

Provides a clean interface for building FastLED projects with PlatformIO.
"""

import gc
import platform
import shutil
import subprocess
import sys
import threading
import time
import urllib.request
import warnings
from concurrent.futures import Future, ThreadPoolExecutor
from pathlib import Path
from typing import Any, Optional

from running_process import RunningProcess
from running_process.process_output_reader import EndOfStream

from ci.boards import Board, create_board

# Import from new modules
from ci.compiler.build_config import (
    apply_board_specific_config,
    copy_cache_build_script,
    generate_build_info_json_from_existing_build,
    get_cache_build_flags,
)
from ci.compiler.build_utils import (
    create_building_banner,
    get_example_error_message,
    get_utf8_env,
)
from ci.compiler.compiler import CacheType, Compiler, InitResult, SketchResult
from ci.compiler.lock_manager import PlatformLock
from ci.compiler.package_manager import (
    aggressive_clean_pio_packages,
    detect_and_fix_corrupted_packages_dynamic,
    detect_package_exception_in_output,
    ensure_platform_installed,
    find_platform_path_from_board,
)
from ci.compiler.path_manager import FastLEDPaths, resolve_project_root
from ci.compiler.source_manager import (
    copy_boards_directory,
    copy_example_source,
    copy_fastled_library,
)
from ci.util.output_formatter import create_sketch_path_formatter


def _init_platformio_build(
    board: Board,
    verbose: bool,
    example: str,
    paths: FastLEDPaths,
    additional_defines: Optional[list[str]] = None,
    additional_include_dirs: Optional[list[str]] = None,
    additional_libs: Optional[list[str]] = None,
    cache_type: CacheType = CacheType.NO_CACHE,
) -> InitResult:
    """Initialize the PlatformIO build directory. Assumes lock is already held by caller."""
    project_root = resolve_project_root()
    build_dir = project_root / ".build" / "pio" / board.board_name

    # Check for and fix corrupted packages before building
    # Find platform path based on board's platform URL (works for any platform)
    platform_path = find_platform_path_from_board(board, paths)
    detect_and_fix_corrupted_packages_dynamic(paths, board.board_name, platform_path)

    # Lock is already held by build() - no need to acquire again

    # Setup the build directory.
    build_dir.mkdir(parents=True, exist_ok=True)
    platformio_ini = build_dir / "platformio.ini"

    # Ensure platform is installed if needed
    if not ensure_platform_installed(board):
        return InitResult(
            success=False,
            output=f"Failed to install platform for {board.board_name}",
            build_dir=build_dir,
        )

    # Clone board and add sketch directory include path (enables "shared/file.h" style includes)
    board_with_sketch_include = board.clone()
    if board_with_sketch_include.build_flags is None:
        board_with_sketch_include.build_flags = []
    else:
        board_with_sketch_include.build_flags = list(
            board_with_sketch_include.build_flags
        )
    board_with_sketch_include.build_flags.append("-Isrc/sketch")

    # Set up compiler cache through build_flags if enabled and available
    cache_config = get_cache_build_flags(board.board_name, cache_type)
    if cache_config:
        print(f"Applied cache configuration: {list(cache_config.keys())}")

        # Add compiler launcher build flags directly
        sccache_path = cache_config.get("SCCACHE_PATH")
        if sccache_path:
            # Add build flags to use sccache as compiler launcher
            launcher_flags = [f'-DCACHE_LAUNCHER="{sccache_path}"']
            board_with_sketch_include.build_flags.extend(launcher_flags)
            print(f"Added cache launcher flags: {launcher_flags}")

        # Create build script that will set up cache environment
        copy_cache_build_script(build_dir, cache_config)

    # Optimization report generation is available but OFF by default
    # To enable optimization reports, add these flags to your board configuration:
    # - "-fopt-info-all=optimization_report.txt" for detailed optimization info
    # - "-Wl,-Map,firmware.map" for memory map analysis
    #
    # Note: The infrastructure is in place to support optimization reports when needed

    # Always generate optimization artifacts into the board build directory
    # Use absolute paths to ensure GCC/LD write into a known location even when the
    # working directory changes inside PlatformIO builds.
    try:
        opt_report_path = (build_dir / "optimization_report.txt").resolve()
        # GCC writes reports relative to the current working directory; provide absolute path

        # ESP32-C2 and AVR platforms cannot work with -fopt-info-all, suppress it for these platforms
        if board.board_name != "esp32c2" and board.platform != "atmelavr":
            board_with_sketch_include.build_flags.append(
                f"-fopt-info-all={opt_report_path.as_posix()}"
            )

        # Generate linker map in the board directory (file name is sufficient; PIO writes here)
        board_with_sketch_include.build_flags.append("-Wl,-Map,firmware.map")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        # Non-fatal: continue without optimization artifacts if path resolution fails
        pass

    # Apply board-specific configuration
    if not apply_board_specific_config(
        board_with_sketch_include,
        platformio_ini,
        example,
        paths,
        additional_defines,
        additional_include_dirs,
        additional_libs,
        cache_type,
    ):
        return InitResult(
            success=False,
            output=f"Failed to apply board configuration for {board.board_name}",
            build_dir=build_dir,
        )

    # Print building banner first
    print(create_building_banner(example))

    ok_copy_src = copy_example_source(project_root, build_dir, example)
    if not ok_copy_src:
        error_msg = get_example_error_message(project_root, example)
        warnings.warn(error_msg)
        return InitResult(
            success=False,
            output=error_msg,
            build_dir=build_dir,
        )

    # Copy FastLED library
    ok_copy_fastled = copy_fastled_library(project_root, build_dir)
    if not ok_copy_fastled:
        warnings.warn("Failed to copy FastLED library")
        return InitResult(
            success=False, output="Failed to copy FastLED library", build_dir=build_dir
        )

    # Copy boards directory
    ok_copy_boards = copy_boards_directory(project_root, build_dir)
    if not ok_copy_boards:
        warnings.warn("Failed to copy boards directory")
        return InitResult(
            success=False,
            output="Failed to copy boards directory",
            build_dir=build_dir,
        )

    # On Windows, force garbage collection and small delay to release all file handles
    # This prevents file access errors when PlatformIO scans the libraries
    if sys.platform == "win32":
        gc.collect()
        time.sleep(0.1)  # Give filesystem time to release handles

    # Create sdkconfig.defaults if framework has "espidf" in it for esp32c2 board
    frameworks = {f.strip() for f in (board.framework or "").split(",")}
    if {"arduino", "espidf"}.issubset(frameworks):
        sdkconfig_path = build_dir / "sdkconfig.defaults"
        print("Creating sdkconfig.defaults file")
        try:
            sdkconfig_path.write_text(
                "CONFIG_FREERTOS_HZ=1000\r\nCONFIG_AUTOSTART_ARDUINO=y"
            )
            with open(sdkconfig_path, "r", encoding="utf-8", errors="ignore") as f:
                for line in f:
                    print(line, end="")
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            warnings.warn(f"Failed to write sdkconfig: {e}")

    # Final platformio.ini is already written by apply_board_specific_config
    # No need to write it again

    # Run initial build with LDF enabled to set up the environment
    run_cmd: list[str] = ["pio", "run", "--project-dir", str(build_dir)]
    if verbose:
        run_cmd.append("--verbose")

    # Print platformio.ini content for the initialization build (verbose only)
    if verbose:
        platformio_ini_path = build_dir / "platformio.ini"
        if platformio_ini_path.exists():
            print()  # Add newline before configuration section
            print("=" * 60)
            print("PLATFORMIO.INI CONFIGURATION:")
            print("=" * 60)
            ini_content = platformio_ini_path.read_text()
            print(ini_content)
            print("=" * 60)
            print()  # Add newline after configuration section

        print(f"Running initial build command: {subprocess.list2cmdline(run_cmd)}")

    # Start timer for this example
    time.time()

    # Create formatter for path substitution and timestamping
    formatter = create_sketch_path_formatter(example)

    running_process = RunningProcess(
        run_cmd,
        cwd=build_dir,
        auto_run=True,
        output_formatter=formatter,
        env=get_utf8_env(),
    )
    # Output is transformed by the formatter, but we need to print it
    while line := running_process.get_next_line():
        if isinstance(line, EndOfStream):
            break
        # Print the transformed line
        print(line)

    running_process.wait()

    if running_process.returncode != 0:
        return InitResult(
            success=False,
            output=f"Initial build failed: {running_process.stdout}",
            build_dir=build_dir,
        )

    # After successful build, configuration is already properly set up
    # Board configuration includes all necessary settings

    # Generate build_info.json after successful initialization build
    # Pass example name to generate example-specific build_info_{example}.json
    generate_build_info_json_from_existing_build(build_dir, board, example)

    return InitResult(success=True, output="", build_dir=build_dir)


class PioCompiler(Compiler):
    def __init__(
        self,
        board: Board | str,
        verbose: bool,
        global_cache_dir: Path,
        additional_defines: Optional[list[str]] = None,
        additional_include_dirs: Optional[list[str]] = None,
        additional_libs: Optional[list[str]] = None,
        cache_type: CacheType = CacheType.NO_CACHE,
    ) -> None:
        # Call parent constructor
        super().__init__()

        # Convert string to Board object if needed
        if isinstance(board, str):
            self.board = create_board(board)
        else:
            self.board = board
        self.verbose = verbose
        self.additional_defines = additional_defines
        self.additional_include_dirs = additional_include_dirs
        self.additional_libs = additional_libs
        self.cache_type = cache_type

        # Global cache directory is already resolved by caller
        self.global_cache_dir = global_cache_dir

        # Use centralized path management
        self.paths = FastLEDPaths(self.board.board_name)
        self.platform_lock = PlatformLock(self.paths.platform_lock_file)

        # Always override the cache directory with our resolved path
        self.paths._global_platformio_cache_dir = self.global_cache_dir
        self.build_dir: Path = self.paths.build_dir

        # Ensure all directories exist
        self.paths.ensure_directories_exist()

        # Package installation lock is now handled by daemon (system-wide singleton)
        # No per-board lock needed

        self.initialized = False
        self.executor = ThreadPoolExecutor(max_workers=1)

    def _internal_init_build_no_lock(self, example: str) -> InitResult:
        """Initialize the PlatformIO build directory once with the first example."""
        if self.initialized:
            return InitResult(
                success=True, output="Already initialized", build_dir=self.build_dir
            )

        # Initialize with the actual first example being built
        result = _init_platformio_build(
            self.board,
            self.verbose,
            example,
            self.paths,
            self.additional_defines,
            self.additional_include_dirs,
            self.additional_libs,
            self.cache_type,
        )
        if result.success:
            self.initialized = True
        return result

    def cancel_all(self) -> None:
        """Cancel all builds."""
        # Provide immediate feedback that cancellation is starting
        sys.stdout.write("      → Shutting down build executor...\n")
        sys.stdout.flush()
        self.executor.shutdown(wait=False, cancel_futures=True)
        sys.stdout.write("      → Executor shutdown complete\n")
        sys.stdout.flush()

    def build(self, examples: list[str]) -> list[Future[SketchResult]]:
        """Build a list of examples with proper lock management."""
        if not examples:
            return []

        # Acquire the global package lock for the first build (package installation)

        count = len(examples)

        def release_platform_lock(fut: Future[SketchResult]) -> None:
            """Release the platform lock when all builds complete."""
            nonlocal count
            count -= 1
            if count == 0:
                print(f"Releasing platform lock: {self.platform_lock.lock_file_path}\n")
                self.platform_lock.release()

        futures: list[Future[SketchResult]] = []

        # Package installation lock is now handled by daemon (in _internal_build_no_lock)
        # No global lock acquire/release needed here
        cancelled = threading.Event()
        try:
            for example in examples:
                future = self.executor.submit(
                    self._internal_build_no_lock, example, cancelled
                )
                future.add_done_callback(release_platform_lock)
                futures.append(future)
        except KeyboardInterrupt:
            print("KeyboardInterrupt: Cancelling all builds")
            cancelled.set()
            for future in futures:
                future.cancel()
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Exception: {e}")
            for future in futures:
                future.cancel()
            raise

        return futures

    def _build_internal(self, example: str) -> SketchResult:
        """Internal build method without lock management."""
        # Print building banner first
        print(create_building_banner(example))

        # Copy example source to build directory
        project_root = resolve_project_root()
        ok_copy_src = copy_example_source(project_root, self.build_dir, example)
        if not ok_copy_src:
            error_msg = get_example_error_message(project_root, example)
            return SketchResult(
                success=False,
                output=error_msg,
                build_dir=self.build_dir,
                example=example,
            )

        # Cache configuration is handled through build flags during initialization

        # Attempt build with retry on PackageException
        max_attempts = 2
        for attempt in range(max_attempts):
            if attempt > 0:
                print(f"\n{'=' * 60}")
                print(f"RETRY ATTEMPT {attempt}/{max_attempts - 1}")
                print(f"{'=' * 60}\n")

            # Run PlatformIO build
            run_cmd: list[str] = [
                "pio",
                "run",
                "--project-dir",
                str(self.build_dir),
                "--disable-auto-clean",
            ]
            if self.verbose:
                run_cmd.append("--verbose")

            print(f"Running command: {subprocess.list2cmdline(run_cmd)}")

            # Create formatter for path substitution and timestamping
            formatter = create_sketch_path_formatter(example)

            running_process = RunningProcess(
                run_cmd,
                cwd=self.build_dir,
                auto_run=True,
                output_formatter=formatter,
                env=get_utf8_env(),
            )
            try:
                # Output is transformed by the formatter, but we need to print it
                while line := running_process.get_next_line(
                    timeout=900
                ):  # 15 minutes for platform builds
                    if isinstance(line, EndOfStream):
                        break
                    # Print the transformed line
                    print(line)
            except KeyboardInterrupt:
                print("KeyboardInterrupt: Cancelling build")
                running_process.terminate()
                handle_keyboard_interrupt_properly()
                raise
            except OSError as e:
                # Handle output encoding issues on Windows
                print(f"Output encoding issue: {e}")
                pass

            running_process.wait()

            success = running_process.returncode == 0
            output = running_process.stdout

            # Check for PackageException in output
            has_package_exception, package_info = detect_package_exception_in_output(
                output
            )

            # Print SUCCESS/FAILED message immediately in worker thread to avoid race conditions
            if success:
                green_color = "\033[32m"
                reset_color = "\033[0m"
                print(f"{green_color}SUCCESS: {example}{reset_color}")
            else:
                red_color = "\033[31m"
                reset_color = "\033[0m"
                print(f"{red_color}FAILED: {example}{reset_color}")

            # If build failed with PackageException and we have retries left, attempt recovery
            if not success and has_package_exception and attempt < max_attempts - 1:
                print("\nPackageException detected in build output")
                if package_info:
                    print(f"Package info: {package_info}")

                # Attempt aggressive cleanup and retry
                paths = FastLEDPaths(self.board.board_name)
                if aggressive_clean_pio_packages(paths, self.board.board_name):
                    print("Retrying build after package cleanup...")
                    continue  # Retry the build
                else:
                    print("Package cleanup did not remove any packages")
                    # Still try again - maybe something else will help
                    print("Retrying build anyway...")
                    continue

            # Build succeeded or no PackageException, return result
            # Generate build_info.json after successful build
            if success:
                generate_build_info_json_from_existing_build(
                    self.build_dir, self.board, example
                )

            return SketchResult(
                success=success,
                output=output,
                build_dir=self.build_dir,
                example=example,
            )

        # Should not reach here, but just in case
        return SketchResult(
            success=False,
            output="Build failed after all retry attempts",
            build_dir=self.build_dir,
            example=example,
        )

    def get_cache_stats(self) -> str:
        """Get compiler statistics as a formatted string.

        For PIO compiler, this includes cache statistics (sccache/ccache).
        """
        if self.cache_type == CacheType.NO_CACHE:
            return ""

        cache_name = "sccache" if self.cache_type == CacheType.SCCACHE else "ccache"
        cache_path = shutil.which(cache_name)

        if not cache_path:
            return f"{cache_name.upper()} not found in PATH"

        try:
            result = subprocess.run(
                [cache_path, "--show-stats"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                check=False,
            )

            if result.returncode == 0:
                stats_lines: list[str] = []
                for line in result.stdout.strip().split("\n"):
                    if line.strip():
                        stats_lines.append(line)

                # Add header with cache type
                stats_output = f"{cache_name.upper()} STATISTICS:\n"
                stats_output += "\n".join(stats_lines)
                return stats_output
            else:
                return f"Failed to get {cache_name.upper()} statistics: {result.stderr}"

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            return f"Error retrieving {cache_name.upper()} statistics: {e}"

    def _internal_build_no_lock(
        self, example: str, cancelled: threading.Event
    ) -> SketchResult:
        """Build a specific example without lock management. Only call from build()."""
        if cancelled.is_set():
            return SketchResult(
                success=False,
                output="Cancelled",
                build_dir=self.build_dir,
                example=example,
            )
        try:
            # ============================================================
            # PHASE 0: Initialize build directory and create platformio.ini
            # ============================================================
            if not self.initialized:
                init_result = self._internal_init_build_no_lock(example)
                if not init_result.success:
                    # Print FAILED message immediately in worker thread
                    red_color = "\033[31m"
                    reset_color = "\033[0m"
                    print(f"{red_color}FAILED: {example}{reset_color}")

                    return SketchResult(
                        success=False,
                        output=init_result.output,
                        build_dir=init_result.build_dir,
                        example=example,
                    )

                # ============================================================
                # PHASE 1: Ensure packages installed via daemon (ONCE per compiler instance)
                # ============================================================
                # NOW platformio.ini exists at self.build_dir/platformio.ini
                print("=" * 60)
                print("ENSURING PLATFORMIO PACKAGES INSTALLED")
                print("=" * 60)

                from ci.util.pio_package_client import ensure_packages_installed

                if not ensure_packages_installed(
                    self.build_dir,  # Project directory with platformio.ini
                    self.board.board_name,  # Environment name
                    timeout=1800,  # 30 minute timeout
                ):
                    print("\n❌ Package installation failed or timed out")
                    # Print FAILED message immediately in worker thread
                    red_color = "\033[31m"
                    reset_color = "\033[0m"
                    print(f"{red_color}FAILED: {example}{reset_color}")

                    return SketchResult(
                        success=False,
                        output="Package installation failed or timed out",
                        build_dir=self.build_dir,
                        example=example,
                    )

                print()  # Blank line separator

                # If initialization succeeded and we just built the example, return success
                # Print SUCCESS message immediately in worker thread
                green_color = "\033[32m"
                reset_color = "\033[0m"
                print(f"{green_color}SUCCESS: {example}{reset_color}")

                return SketchResult(
                    success=True,
                    output="Built during initialization",
                    build_dir=self.build_dir,
                    example=example,
                )

            # No lock management - caller (build) handles locks
            return self._build_internal(example)
        except KeyboardInterrupt:
            print("KeyboardInterrupt: Cancelling build")
            cancelled.set()
            handle_keyboard_interrupt_properly()
            return SketchResult(
                success=False,
                output="Build cancelled by user",
                build_dir=self.build_dir,
                example=example,
            )

    def clean(self) -> None:
        """Clean build artifacts for this platform (acquire platform lock)."""
        print(f"Cleaning build artifacts for platform {self.board.board_name}...")

        try:
            # Remove the local build directory
            if self.build_dir.exists():
                print(f"Removing build directory: {self.build_dir}")
                shutil.rmtree(self.build_dir)
                print(f"✅ Cleaned local build artifacts for {self.board.board_name}")
            else:
                print(
                    f"✅ No build directory found for {self.board.board_name} (already clean)"
                )
        finally:
            pass  # we used to release the platform lock here, but we disabled it

    def clean_all(self) -> None:
        """Clean all build artifacts (local and global packages) for this platform."""
        print(f"Cleaning all artifacts for platform {self.board.board_name}...")

        # Use platform lock for automatic lock release
        with self.platform_lock:
            # Clean local build artifacts first
            if self.build_dir.exists():
                print(f"Removing build directory: {self.build_dir}")
                shutil.rmtree(self.build_dir)
                print(f"✅ Cleaned local build artifacts for {self.board.board_name}")
            else:
                print(f"✅ No build directory found for {self.board.board_name}")

            # Clean global packages directory
            if self.paths.packages_dir.exists():
                print(f"Removing global packages directory: {self.paths.packages_dir}")
                shutil.rmtree(self.paths.packages_dir)
                print(f"✅ Cleaned global packages for {self.board.board_name}")
            else:
                print(
                    f"✅ No global packages directory found for {self.board.board_name}"
                )

            # Clean global core directory (build cache, platforms)
            if self.paths.core_dir.exists():
                print(f"Removing global core directory: {self.paths.core_dir}")
                shutil.rmtree(self.paths.core_dir)
                print(f"✅ Cleaned global core cache for {self.board.board_name}")
            else:
                print(f"✅ No global core directory found for {self.board.board_name}")

    def deploy(
        self, example: str, upload_port: Optional[str] = None, monitor: bool = False
    ) -> SketchResult:
        """Deploy (upload) a specific example to the target device.

        Args:
            example: Name of the example to deploy
            upload_port: Optional specific port for upload (e.g., "/dev/ttyUSB0", "COM3")
            monitor: If True, attach to device monitor after successful upload
        """
        print(f"Deploying {example} to {self.board.board_name}...")

        try:
            # Ensure the build is initialized and the example is built
            if not self.initialized:
                init_result = self._internal_init_build_no_lock(example)
                if not init_result.success:
                    return SketchResult(
                        success=False,
                        output=init_result.output,
                        build_dir=init_result.build_dir,
                        example=example,
                    )
            else:
                # Build the example first (ensures it's up to date)
                build_result = self._build_internal(example)
                if not build_result.success:
                    return build_result

            # Run PlatformIO upload command
            upload_cmd: list[str] = [
                "pio",
                "run",
                "--project-dir",
                str(self.build_dir),
                "--target",
                "upload",
            ]

            if upload_port:
                upload_cmd.extend(["--upload-port", upload_port])

            if self.verbose:
                upload_cmd.append("--verbose")

            print(f"Running upload command: {subprocess.list2cmdline(upload_cmd)}")

            # Create formatter for upload output
            formatter = create_sketch_path_formatter(example)

            running_process = RunningProcess(
                upload_cmd,
                cwd=self.build_dir,
                auto_run=True,
                output_formatter=formatter,
                env=get_utf8_env(),
            )
            try:
                # Output is transformed by the formatter, but we need to print it
                while line := running_process.get_next_line(
                    timeout=900
                ):  # 15 minutes for platform builds
                    if isinstance(line, EndOfStream):
                        break
                    # Print the transformed line
                    print(line)
            except OSError as e:
                # Handle output encoding issues on Windows
                print(f"Upload encoding issue: {e}")
                pass

            running_process.wait()

            # Check if upload was successful
            upload_success = running_process.returncode == 0
            upload_output = running_process.stdout

            if not upload_success:
                return SketchResult(
                    success=False,
                    output=upload_output,
                    build_dir=self.build_dir,
                    example=example,
                )

            # Upload completed successfully - release the lock before monitor
            print(f"✅ Upload completed successfully for {example}")

            # If monitor is requested and upload was successful, start monitor
            if monitor:
                print(
                    f"📡 Starting monitor for {example} on {self.board.board_name}..."
                )
                print("📝 Press Ctrl+C to exit monitor")

                monitor_cmd: list[str] = [
                    "pio",
                    "device",
                    "monitor",
                    "--project-dir",
                    str(self.build_dir),
                ]

                if upload_port:
                    monitor_cmd.extend(["--port", upload_port])

                print(
                    f"Running monitor command: {subprocess.list2cmdline(monitor_cmd)}"
                )

                # Start monitor process (no lock needed for monitoring)
                monitor_process = RunningProcess(
                    monitor_cmd, cwd=self.build_dir, auto_run=True, env=get_utf8_env()
                )
                try:
                    while line := monitor_process.get_next_line(
                        timeout=None
                    ):  # No timeout for monitor
                        if isinstance(line, EndOfStream):
                            break
                        print(line)  # No timestamp for monitor output
                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise
                    print("\n📡 Monitor stopped by user")
                    monitor_process.terminate()
                except OSError as e:
                    print(f"Monitor encoding issue: {e}")
                    pass

                monitor_process.wait()

            return SketchResult(
                success=True,
                output=upload_output,
                build_dir=self.build_dir,
                example=example,
            )

        finally:
            # Only decrement the lock if it hasn't been released yet
            pass  # we used to release the platform lock here, but we disabled it

    def check_usb_permissions(self) -> tuple[bool, str]:
        """Check if USB device access is properly configured on Linux.

        Checks multiple methods for USB device access:
        1. PlatformIO udev rules
        2. User group membership (dialout, uucp, plugdev)
        3. Alternative udev rules files

        Returns:
            Tuple of (has_access, status_message)
        """
        if platform.system() != "Linux":
            return True, "Not applicable on non-Linux systems"

        access_methods: list[str] = []

        # Check 1: PlatformIO udev rules
        udev_rules_path = Path("/etc/udev/rules.d/99-platformio-udev.rules")
        if udev_rules_path.exists():
            access_methods.append("PlatformIO udev rules")

        # Check 2: User group membership
        user_groups = self._get_user_groups()
        usb_groups = ["dialout", "uucp", "plugdev", "tty"]
        user_usb_groups = [group for group in usb_groups if group in user_groups]
        if user_usb_groups:
            access_methods.append(f"Group membership: {', '.join(user_usb_groups)}")

        # Check 3: Alternative udev rules
        alt_udev_files = [
            "/etc/udev/rules.d/99-arduino.rules",
            "/etc/udev/rules.d/50-platformio-udev.rules",
            "/lib/udev/rules.d/99-platformio-udev.rules",
        ]
        for alt_file in alt_udev_files:
            if Path(alt_file).exists():
                access_methods.append(f"Alternative udev rules: {Path(alt_file).name}")

        # Check 4: Root user (always has access)
        if self._is_root_user():
            access_methods.append("Root user privileges")

        if access_methods:
            status = f"USB access available via: {'; '.join(access_methods)}"
            return True, status
        else:
            return False, "No USB device access methods detected"

    def _get_user_groups(self) -> list[str]:
        """Get list of groups the current user belongs to."""
        try:
            result = subprocess.run(
                ["groups"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
            if result.returncode == 0:
                return result.stdout.strip().split()
            return []
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception:
            return []

    def _is_root_user(self) -> bool:
        """Check if running as root user."""
        try:
            import os

            return os.geteuid() == 0
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception:
            return False

    def check_udev_rules(self) -> bool:
        """Check if PlatformIO udev rules are installed on Linux.

        DEPRECATED: Use check_usb_permissions() instead for comprehensive checking.

        Returns:
            True if any USB access method is available, False otherwise
        """
        has_access, _ = self.check_usb_permissions()
        return has_access

    def install_usb_permissions(self) -> bool:
        """Install platform-specific USB permissions (e.g., udev rules on Linux).

        Returns:
            True if installation succeeded, False otherwise
        """
        import os

        if platform.system() != "Linux":
            print("INFO: udev rules are only needed on Linux systems")
            return True

        udev_url = "https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules"
        udev_rules_path = "/etc/udev/rules.d/99-platformio-udev.rules"

        print("📡 Downloading PlatformIO udev rules...")

        try:
            # Download the udev rules
            with urllib.request.urlopen(udev_url) as response:
                udev_content = response.read().decode("utf-8")

            # Write to temporary file first
            temp_file = "/tmp/99-platformio-udev.rules"
            with open(temp_file, "w") as f:
                f.write(udev_content)

            print("💾 Installing udev rules (requires sudo)...")

            # Use sudo to copy to system location
            result = subprocess.run(
                ["sudo", "cp", temp_file, udev_rules_path],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
            )

            if result.returncode != 0:
                print(f"ERROR: Failed to install udev rules: {result.stderr}")
                return False

            # Clean up temp file
            os.unlink(temp_file)

            print("✅ PlatformIO udev rules installed successfully!")
            print("⚠️  To complete the installation, run one of the following:")
            print("   sudo service udev restart")
            print("   # or")
            print("   sudo udevadm control --reload-rules")
            print("   sudo udevadm trigger")
            print(
                "⚠️  You may also need to restart your system for the changes to take effect."
            )

            return True

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"ERROR: Failed to install udev rules: {e}")
            return False

    def supports_merged_bin(self) -> bool:
        """Check if board supports merged binary generation.

        Returns:
            True if the board platform supports merged binary generation
        """
        supported_platforms = ["espressif32", "espressif8266"]
        return any(
            plat in str(self.board.platform).lower() for plat in supported_platforms
        )

    def get_merged_bin_path(self, example: str) -> Optional[Path]:
        """Get path to merged binary if it exists.

        Args:
            example: Example name (unused but kept for API consistency)

        Returns:
            Path to merged.bin if it exists, None otherwise
        """
        env_name = self.board.board_name
        merged_bin = self.build_dir / ".pio" / "build" / env_name / "merged.bin"
        return merged_bin if merged_bin.exists() else None

    def build_with_merged_bin(
        self, example: str, output_path: Optional[Path] = None
    ) -> SketchResult:
        """Build example and generate merged binary.

        Args:
            example: Example name to build
            output_path: Optional custom output path for merged binary

        Returns:
            SketchResult with merged_bin_path populated
        """
        # 1. Validate board support
        if not self.supports_merged_bin():
            return SketchResult(
                success=False,
                output=f"Board {self.board.board_name} does not support merged binary. "
                f"Supported platforms: ESP32, ESP8266",
                build_dir=self.build_dir,
                example=example,
            )

        # 2. Build normally first
        cancelled = threading.Event()
        result = self._internal_build_no_lock(example, cancelled)
        if not result.success:
            return result

        # 3. Use esptool to merge binaries
        # PlatformIO doesn't have a merge_bin target - we must use esptool directly
        env_name = self.board.board_name
        artifacts_dir = self.build_dir / ".pio" / "build" / env_name

        # Find required binary components
        bootloader_bin = artifacts_dir / "bootloader.bin"
        partitions_bin = artifacts_dir / "partitions.bin"
        boot_app0_bin = artifacts_dir / "boot_app0.bin"
        firmware_bin = artifacts_dir / "firmware.bin"

        # Verify all required files exist
        missing_files: list[str] = []
        for bin_file in [bootloader_bin, partitions_bin, firmware_bin]:
            if not bin_file.exists():
                missing_files.append(str(bin_file))

        if missing_files:
            return SketchResult(
                success=False,
                output=f"Required binary files not found after build: {', '.join(missing_files)}",
                build_dir=self.build_dir,
                example=example,
            )

        # Create boot_app0.bin if it doesn't exist (it's optional)
        if not boot_app0_bin.exists():
            # Create default 8KB boot_app0.bin (all 0xFF)
            boot_app0_bin.write_bytes(b"\xff" * 8192)
            print("Created default boot_app0.bin")

        # Determine chip type and offsets based on board
        # ESP32 (original) uses 0x1000 for bootloader
        # ESP32-S3/C3 use 0x0 for bootloader
        platform_str = str(self.board.platform).lower()
        if "esp32s3" in env_name.lower() or "esp32s3" in platform_str:
            chip = "esp32s3"
            bootloader_offset = "0x0"
        elif "esp32c3" in env_name.lower() or "esp32c3" in platform_str:
            chip = "esp32c3"
            bootloader_offset = "0x0"
        elif "esp8266" in platform_str:
            chip = "esp8266"
            bootloader_offset = "0x0"
        else:
            # Default ESP32
            chip = "esp32"
            bootloader_offset = "0x1000"

        # Create merged binary using esptool
        merged_bin = artifacts_dir / "merged.bin"

        merge_cmd = [
            "uv",
            "run",
            "esptool",
            "--chip",
            chip,
            "merge-bin",
            "-o",
            str(merged_bin),
            "--flash-mode",
            "dio",
            "--flash-freq",
            "80m",
            "--flash-size",
            "4MB",
            "--pad-to-size",
            "4MB",
            bootloader_offset,
            str(bootloader_bin),
            "0x8000",
            str(partitions_bin),
            "0xe000",
            str(boot_app0_bin),
            "0x10000",
            str(firmware_bin),
        ]

        print(f"Running esptool merge_bin: {' '.join(merge_cmd)}")

        try:
            merge_proc = subprocess.run(
                merge_cmd,
                capture_output=True,
                text=True,
                timeout=300,
                env=get_utf8_env(),
            )
        except subprocess.TimeoutExpired:
            return SketchResult(
                success=False,
                output="esptool merge_bin timed out after 300 seconds",
                build_dir=self.build_dir,
                example=example,
            )
        except FileNotFoundError:
            return SketchResult(
                success=False,
                output="esptool not found. Please install: uv pip install esptool",
                build_dir=self.build_dir,
                example=example,
            )

        if merge_proc.returncode != 0:
            return SketchResult(
                success=False,
                output=f"esptool merge_bin failed: {merge_proc.stderr}\n{merge_proc.stdout}",
                build_dir=self.build_dir,
                example=example,
            )

        # 4. Verify merged binary was created
        if not merged_bin.exists():
            return SketchResult(
                success=False,
                output=f"Merged binary not created at {merged_bin}",
                build_dir=self.build_dir,
                example=example,
            )

        # 5. Copy to output path if specified
        if output_path:
            output_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(merged_bin, output_path)
            merged_bin = output_path

        # 6. Return enhanced result
        return SketchResult(
            success=True,
            output=result.output,
            build_dir=self.build_dir,
            example=example,
            merged_bin_path=merged_bin,
        )


def run_pio_build(
    board: Board | str,
    examples: list[str],
    verbose: bool = False,
    additional_defines: Optional[list[str]] = None,
    additional_include_dirs: Optional[list[str]] = None,
    cache_type: CacheType = CacheType.NO_CACHE,
) -> list[Future[SketchResult]]:
    """Run build for specified examples and platform using new PlatformIO system.

    Args:
        board: Board class instance or board name string (resolved via create_board())
        examples: List of example names to build
        verbose: Enable verbose output
        additional_defines: Additional defines to add to build flags (e.g., ["FASTLED_DEFINE=0", "DEBUG=1"])
        additional_include_dirs: Additional include directories to add to build flags (e.g., ["src/platforms/sub", "external/libs"])
    """
    pio = PioCompiler(
        board,
        verbose,
        Path.home() / ".fastled" / "platformio_cache",
        additional_defines,
        additional_include_dirs,
        None,
        cache_type,
    )
    futures = pio.build(examples)

    # Show cache statistics after all builds complete
    if cache_type != CacheType.NO_CACHE:
        # Create a callback to show stats when all builds complete
        def add_stats_callback():
            completed_count = 0
            total_count = len(futures)

            def on_future_complete(future: Any) -> None:
                nonlocal completed_count
                completed_count += 1

                # When all futures complete, show compiler statistics
                if completed_count == total_count:
                    try:
                        stats = pio.get_cache_stats()
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

            # Add callback to all futures
            for future in futures:
                future.add_done_callback(on_future_complete)

        add_stats_callback()

    return futures
