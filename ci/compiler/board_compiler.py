"""Board build driver for FastLED: stages an example as an fbuild project and
builds it with fbuild.

The staged project is ``.build/fbuild/<board>/`` (see
``ci/compiler/path_manager.py``): a ``platformio.ini`` (the project-file
format fbuild reads), ``src/`` with the sketch, and ``lib/FastLED``. fbuild
resolves platforms and toolchains itself; nothing here installs packages.
"""

import gc
import hashlib
import shutil
import sys
import time
import warnings
from concurrent.futures import Future, ThreadPoolExecutor
from pathlib import Path
from typing import Any, Optional

from ci.boards import Board, create_board
from ci.compiler.build_config import (
    apply_board_specific_config,
    generate_build_info_json_from_existing_build,
)
from ci.compiler.build_utils import create_building_banner, get_example_error_message
from ci.compiler.compiler import Compiler, InitResult, SketchResult
from ci.compiler.lock_manager import PlatformLock
from ci.compiler.path_manager import FastLEDPaths, board_build_dir, resolve_project_root
from ci.compiler.project_ini import PROJECT_INI_NAME, ProjectIni
from ci.compiler.source_manager import (
    CopyExampleResult,
    copy_boards_directory,
    copy_example_source,
    copy_fastled_library,
)
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


def _update_generated_build_defines(
    project_ini_path: Path,
    board_name: str,
    previous_defines: list[str],
    copy_result: CopyExampleResult,
) -> None:
    """Replace per-sketch defines in an already-generated project ini."""
    project_ini = ProjectIni.parseFile(project_ini_path)
    previous_flags = {f"-D{define}" for define in previous_defines}
    build_flags = [
        flag
        for flag in project_ini.get_build_flags(board_name)
        if flag not in previous_flags
    ]
    for define in copy_result.build_defines:
        flag = f"-D{define}"
        if flag not in build_flags:
            build_flags.append(flag)
    project_ini.set_build_flags(board_name, build_flags)
    project_ini.dump(project_ini_path)


def _owned_sketch_build_defines(
    discovered_defines: list[str], persistent_defines: list[str] | None
) -> list[str]:
    """Return asset defines that the compiler may remove on sketch changes."""
    persistent = set(persistent_defines or [])
    owned: list[str] = []
    for define in discovered_defines:
        if define not in persistent:
            owned.append(define)
    return owned


def init_fbuild_project(
    board: Board,
    verbose: bool,
    example: str,
    paths: FastLEDPaths,
    build_dir: Optional[Path] = None,
    additional_defines: Optional[list[str]] = None,
    additional_include_dirs: Optional[list[str]] = None,
    additional_libs: Optional[list[str]] = None,
) -> InitResult:
    """Stage ``example`` for ``board`` as an fbuild project in ``build_dir``.

    Writes the project ini, copies the sketch, the FastLED library and the
    ``boards/`` directory. Does not compile. Assumes any lock is already
    held by the caller.

    Per-board flags come from ``ci/boards.py`` only; the repo-root
    ``platformio.ini`` is owned by ``bash autoresearch`` / ``bash debug``
    and is never merged (#3274, #3278, #3279).
    """
    project_root = resolve_project_root()
    build_dir = build_dir or board_build_dir(board.board_name, project_root)

    build_dir.mkdir(parents=True, exist_ok=True)
    project_ini = build_dir / PROJECT_INI_NAME

    board_with_sketch_include = board.clone()
    if board_with_sketch_include.build_flags is None:
        board_with_sketch_include.build_flags = []

    # Make the staged source roots available as include roots so
    # sketch-relative includes resolve consistently.
    default_include_dirs = [
        (build_dir / "src").resolve().as_posix(),
        (build_dir / "src" / "sketch").resolve().as_posix(),
    ]
    merged_include_dirs = list(default_include_dirs)
    if additional_include_dirs:
        merged_include_dirs.extend(additional_include_dirs)

    # Optimization report (`-fopt-info-all`) is OPT-IN per board because the
    # report file accumulates across every example in the matrix and can
    # exceed 100 MB on no-LTO platforms — that overwhelmed the GHA log
    # buffer and shut down the nrf52840 runners (see PR #2658 for the
    # downstream cap and PR opening this gate). Set
    # `generate_optimization_report=True` on the Board to opt in.
    #
    # The linker map (`-Wl,-Map,firmware.map`) stays unconditional; it's a
    # small per-example file and is consumed by size-analysis tooling.
    try:
        map_path = (build_dir / "firmware.map").resolve()
        board_with_sketch_include.build_flags.append(f"-Wl,-Map,{map_path.as_posix()}")

        # ESP32-C2 and AVR platforms cannot work with -fopt-info-all even when
        # opted in — preserve the original suppression list.
        if (
            getattr(board, "generate_optimization_report", False)
            and board.board_name != "esp32c2"
            and board.platform != "atmelavr"
        ):
            opt_report_path = (build_dir / "optimization_report.txt").resolve()
            board_with_sketch_include.build_flags.append(
                f"-fopt-info-all={opt_report_path.as_posix()}"
            )
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        # Non-fatal: continue without optimization artifacts if path resolution fails
        pass

    # Stage first because asset declarations contribute per-sketch build defines.
    print(create_building_banner(example))
    copy_result = copy_example_source(project_root, build_dir, example)
    if not copy_result.success:
        error_msg = get_example_error_message(project_root, example)
        warnings.warn(error_msg)
        return InitResult(
            success=False,
            output=error_msg,
            build_dir=build_dir,
        )

    merged_defines = list(additional_defines or [])
    merged_defines.extend(
        define for define in copy_result.build_defines if define not in merged_defines
    )

    if not apply_board_specific_config(
        board_with_sketch_include,
        project_ini,
        example,
        paths,
        merged_defines,
        merged_include_dirs,
        additional_libs,
    ):
        return InitResult(
            success=False,
            output=f"Failed to apply board configuration for {board.board_name}",
            build_dir=build_dir,
        )

    ok_copy_fastled = copy_fastled_library(project_root, build_dir)
    if not ok_copy_fastled:
        warnings.warn("Failed to copy FastLED library")
        return InitResult(
            success=False, output="Failed to copy FastLED library", build_dir=build_dir
        )

    ok_copy_boards = copy_boards_directory(project_root, build_dir)
    if not ok_copy_boards:
        warnings.warn("Failed to copy boards directory")
        return InitResult(
            success=False,
            output="Failed to copy boards directory",
            build_dir=build_dir,
        )

    # On Windows, force garbage collection and small delay to release all file
    # handles before fbuild scans the staged tree.
    if sys.platform == "win32":
        gc.collect()
        time.sleep(0.1)

    # Create sdkconfig.defaults when the board builds through the ESP-IDF
    # component system (framework = "arduino, espidf").
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
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            warnings.warn(f"Failed to write sdkconfig: {e}")

    if verbose and project_ini.exists():
        print()
        print("=" * 60)
        print("PROJECT INI (fbuild):")
        print("=" * 60)
        print(project_ini.read_text())
        print("=" * 60)
        print()

    return InitResult(
        success=True,
        output="",
        build_dir=build_dir,
        sketch_build_defines=copy_result.build_defines,
    )


class BoardCompiler(Compiler):
    """Builds FastLED examples for one board with fbuild."""

    def __init__(
        self,
        board: Board | str,
        verbose: bool,
        additional_defines: Optional[list[str]] = None,
        additional_include_dirs: Optional[list[str]] = None,
        additional_libs: Optional[list[str]] = None,
    ) -> None:
        super().__init__()

        if isinstance(board, str):
            self.board = create_board(board)
        else:
            self.board = board
        self.verbose = verbose
        self.additional_defines = additional_defines
        self.additional_include_dirs = additional_include_dirs
        self.additional_libs = additional_libs

        self.paths = FastLEDPaths(self.board.board_name)
        self.platform_lock = PlatformLock(self.paths.platform_lock_file)
        self.build_dir: Path = self.paths.build_dir
        self.paths.ensure_directories_exist()

        self.initialized = False
        self._sketch_build_defines: list[str] = []
        self.executor = ThreadPoolExecutor(max_workers=1)

    def _internal_init_build_no_lock(self, example: str) -> InitResult:
        """Stage the project once with the first example."""
        if self.initialized:
            return InitResult(
                success=True, output="Already initialized", build_dir=self.build_dir
            )

        result = init_fbuild_project(
            self.board,
            self.verbose,
            example,
            self.paths,
            build_dir=self.build_dir,
            additional_defines=self.additional_defines,
            additional_include_dirs=self.additional_include_dirs,
            additional_libs=self.additional_libs,
        )
        if result.success:
            self._sketch_build_defines = _owned_sketch_build_defines(
                result.sketch_build_defines,
                self.additional_defines,
            )
            self.initialized = True
        return result

    def cancel_all(self) -> None:
        """Cancel all builds."""
        sys.stdout.write("      → Shutting down build executor...\n")
        sys.stdout.flush()
        self.executor.shutdown(wait=False, cancel_futures=True)
        sys.stdout.write("      → Executor shutdown complete\n")
        sys.stdout.flush()

    def build(self, examples: list[str]) -> list[Future[SketchResult]]:
        """Build a list of examples.

        Runs synchronously on the main thread so that Ctrl+C / SIGINT is
        delivered immediately (the Rust native extension can hold the GIL in
        a worker thread, preventing signal delivery on Windows). Returns
        completed futures so callers can treat it like an async API.
        """
        if not examples:
            return []
        return self._build_fbuild(examples)

    def _build_fbuild(self, examples: list[str]) -> list[Future[SketchResult]]:
        """Build examples using fbuild, preferring ``ci`` when available.

        compile-many / ``fbuild ci`` are gated behind ``FASTLED_USE_FBUILD_CI``
        because today they re-build the framework from scratch in every
        stage-2 sketch dir (FastLED/fbuild#335). On uno that's a ~7 min run
        vs. ~3.5 min serial; on teensy41/esp32s3 (larger framework) it
        blows past the 30 min batch timeout — see the runs that landed on
        master commits d2a025441e / 63e0241808 / fa147ae894. Default off
        until fbuild#335 ships a shared framework cache; set
        ``FASTLED_USE_FBUILD_CI=1`` to opt in (e.g. for local timing
        experiments against the architectural fix once it lands).
        """
        import os

        from ci.util.fbuild_runner import (
            fbuild_supports_ci,
            fbuild_supports_compile_many,
        )

        opt_in = os.environ.get("FASTLED_USE_FBUILD_CI", "").lower() in (
            "1",
            "true",
            "yes",
            "on",
        )
        if opt_in:
            if fbuild_supports_ci():
                return self._build_fbuild_ci(examples)
            if fbuild_supports_compile_many():
                print(
                    "fbuild ci is unavailable in the installed fbuild; "
                    "falling back to fbuild compile-many."
                )
                return self._build_fbuild_compile_many(examples)
            print(
                "FASTLED_USE_FBUILD_CI=1 was set but fbuild ci/compile-many "
                "are unavailable in the installed fbuild; falling back to "
                "the serial loop."
            )
        return self._build_fbuild_sync(examples)

    def _compile_many_project_dir(self, example: str, index: int) -> Path:
        """Return the staged project directory for one compile-many sketch."""
        if index == 0:
            return self.build_dir

        example_path = Path(example)
        if example_path.is_absolute():
            digest = hashlib.sha1(str(example_path).encode("utf-8")).hexdigest()[:12]
            safe_name = (
                "".join(
                    ch if ch.isalnum() or ch in "._-" else "_"
                    for ch in example_path.name
                )
                or f"example_{index}"
            )
            return self.build_dir / "compile_many" / f"{safe_name}_{digest}"
        return self.build_dir / "compile_many" / example_path

    def _stage_fbuild_compile_many_projects(
        self, examples: list[str]
    ) -> list[tuple[str, Path]]:
        """Stage one fbuild project directory per example."""
        staged_projects: list[tuple[str, Path]] = []
        for index, example in enumerate(examples):
            project_dir = self._compile_many_project_dir(example, index)
            init_result = init_fbuild_project(
                self.board,
                self.verbose,
                example,
                self.paths,
                build_dir=project_dir,
                additional_defines=self.additional_defines,
                additional_include_dirs=self.additional_include_dirs,
                additional_libs=self.additional_libs,
            )
            if not init_result.success:
                raise RuntimeError(init_result.output)
            staged_projects.append((example, project_dir))
        return staged_projects

    def _build_fbuild_compile_many(
        self, examples: list[str]
    ) -> list[Future[SketchResult]]:
        """Build examples with one ``fbuild compile-many`` invocation."""
        from ci.util.fbuild_runner import run_fbuild_compile_many

        return self._build_fbuild_batch(
            examples=examples,
            command_label="compile-many",
            run_batch=run_fbuild_compile_many,
        )

    def _build_fbuild_ci(self, examples: list[str]) -> list[Future[SketchResult]]:
        """Build examples with one ``fbuild ci`` invocation."""
        from ci.util.fbuild_runner import run_fbuild_ci

        return self._build_fbuild_batch(
            examples=examples,
            command_label="ci",
            run_batch=run_fbuild_ci,
        )

    def _build_fbuild_batch(
        self,
        examples: list[str],
        command_label: str,
        run_batch: Any,
    ) -> list[Future[SketchResult]]:
        """Build examples with one batched ``fbuild`` invocation."""

        futures: list[Future[SketchResult]] = []

        try:
            staged_projects = self._stage_fbuild_compile_many_projects(examples)
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            for example in examples:
                future: Future[SketchResult] = Future()
                future.set_result(
                    SketchResult(
                        success=False,
                        output=f"Failed to stage fbuild {command_label} project: {e}",
                        build_dir=self.build_dir,
                        example=example,
                    )
                )
                futures.append(future)
            print(f"Releasing platform lock: {self.platform_lock.lock_file_path}\n")
            self.platform_lock.release()
            return futures

        from ci.util.fbuild_runner import _parse_size_info_from_log

        batch_start = time.monotonic()
        try:
            compile_many_result = run_batch(
                board=self.board.board_name,
                sketch_project_dirs=[project_dir for _, project_dir in staged_projects],
                verbose=self.verbose,
                timeout=1800,
                quiet=False,
                log_file=None,
            )
            reported = {
                sketch_result.sketch_dir.resolve(): sketch_result
                for sketch_result in compile_many_result.sketch_results
            }

            success_count = 0
            fail_count = 0
            per_sketch_secs: list[float] = []
            for example, project_dir in staged_projects:
                sketch_result = reported.get(project_dir.resolve())
                if sketch_result is None:
                    output = (
                        f"fbuild {command_label} did not report a result for "
                        f"{project_dir}\n\n{compile_many_result.output}"
                    )
                    success = False
                    stage = "?"
                    build_secs = 0.0
                    flash_str: str | None = None
                    ram_str: str | None = None
                else:
                    output = sketch_result.message
                    if (
                        sketch_result.log_path is not None
                        and sketch_result.log_path.exists()
                    ):
                        output = sketch_result.log_path.read_text(
                            encoding="utf-8", errors="ignore"
                        )
                    success = sketch_result.success
                    stage = sketch_result.stage
                    build_secs = sketch_result.build_time_secs
                    flash_str, ram_str = _parse_size_info_from_log(
                        sketch_result.log_path
                    )

                # Normalized per-sketch summary line. The serial path emits
                # `build succeeded in Xs (flash: N bytes, ram: M bytes)` from
                # fbuild + `Compilation succeeded (fbuild) [Xs]` from FastLED.
                # Compile-many never sees those per-sketch lines (fbuild only
                # prints one aggregate table), so reconstruct an equivalent
                # line per sketch from the parsed result + per-sketch log.
                # Sizes come from the same fbuild `Flash:`/`RAM:` lines the
                # serial path renders, just kept in fbuild's display units
                # (e.g. "2.75KB") rather than re-converted to bytes — the
                # signal is "did flash/ram change?", not the literal value.
                size_part = ""
                if flash_str is not None and ram_str is not None:
                    size_part = f" (flash: {flash_str}, ram: {ram_str})"
                elif flash_str is not None:
                    size_part = f" (flash: {flash_str})"
                stage_tag = f"[{stage}]" if stage != "?" else "[?]"
                if success:
                    success_count += 1
                    green_color = "\033[32m"
                    reset_color = "\033[0m"
                    print(
                        f"{green_color}SUCCESS: {example}  "
                        f"build succeeded in {build_secs:.1f}s{size_part} {stage_tag}"
                        f"{reset_color}"
                    )
                    if generate_build_info_json_from_existing_build(
                        project_dir, self.board, example
                    ):
                        build_info_path = project_dir / f"build_info_{example}.json"
                        if project_dir != self.build_dir and build_info_path.exists():
                            shutil.copy2(
                                build_info_path,
                                self.build_dir / build_info_path.name,
                            )
                else:
                    fail_count += 1
                    red_color = "\033[31m"
                    reset_color = "\033[0m"
                    print(
                        f"{red_color}FAILED:  {example}  "
                        f"build failed in {build_secs:.1f}s {stage_tag}"
                        f"{reset_color}"
                    )

                per_sketch_secs.append(build_secs)

                future: Future[SketchResult] = Future()
                future.set_result(
                    SketchResult(
                        success=success,
                        output=output or f"fbuild {command_label}",
                        build_dir=project_dir,
                        example=example,
                    )
                )
                futures.append(future)

            # Total wall-clock and parallelism factor. The fbuild_runner step
            # already prints `Compilation succeeded (fbuild ci) [Xs]` with
            # the same wall time, but it lands BEFORE the per-sketch lines
            # (fbuild prints its summary as soon as the subprocess exits).
            # Re-emit a final-line total here so the bottom-of-log reader
            # sees the wall clock right after the per-sketch summary, with
            # an explicit parallelism factor so it's obvious when stage 2
            # actually fanned out vs. ran serially.
            batch_wall = time.monotonic() - batch_start
            sum_per_sketch = sum(per_sketch_secs)
            sketches_n = max(1, len(staged_projects))
            speedup = sum_per_sketch / batch_wall if batch_wall > 0 else 1.0
            print(
                f"\nfbuild {command_label} total wall-clock: {batch_wall:.1f}s "
                f"across {sketches_n} sketches "
                f"(sum-of-per-sketch={sum_per_sketch:.1f}s, "
                f"parallelism={speedup:.2f}x, "
                f"avg-wall={batch_wall / sketches_n:.2f}s/sketch, "
                f"ok={success_count}, fail={fail_count})\n"
            )
        finally:
            print(f"Releasing platform lock: {self.platform_lock.lock_file_path}\n")
            self.platform_lock.release()

        return futures

    def _build_fbuild_sync(self, examples: list[str]) -> list[Future[SketchResult]]:
        """Build examples one at a time with fbuild on the main thread."""
        futures: list[Future[SketchResult]] = []

        for example in examples:
            if not self.initialized:
                init_result = self._internal_init_build_no_lock(example)
                if not init_result.success:
                    red_color = "\033[31m"
                    reset_color = "\033[0m"
                    print(f"{red_color}FAILED: {example}{reset_color}")
                    result = SketchResult(
                        success=False,
                        output=init_result.output,
                        build_dir=init_result.build_dir,
                        example=example,
                    )
                    future: Future[SketchResult] = Future()
                    future.set_result(result)
                    futures.append(future)
                    continue
            else:
                # Re-stage the sketch for subsequent examples
                print(create_building_banner(example))
                copy_result = self._restage_example(example)
                if not copy_result.success:
                    project_root = resolve_project_root()
                    error_msg = get_example_error_message(project_root, example)
                    result = SketchResult(
                        success=False,
                        output=error_msg,
                        build_dir=self.build_dir,
                        example=example,
                    )
                    future = Future()
                    future.set_result(result)
                    futures.append(future)
                    continue

            result = self._build_with_fbuild(example)
            future = Future()
            future.set_result(result)
            futures.append(future)

        print(f"Releasing platform lock: {self.platform_lock.lock_file_path}\n")
        self.platform_lock.release()

        return futures

    def _restage_example(self, example: str) -> CopyExampleResult:
        """Copy ``example`` into the staged project and refresh its defines."""
        project_root = resolve_project_root()
        copy_result = copy_example_source(project_root, self.build_dir, example)
        if not copy_result.success:
            return copy_result
        _update_generated_build_defines(
            self.build_dir / PROJECT_INI_NAME,
            self.board.board_name,
            self._sketch_build_defines,
            copy_result,
        )
        self._sketch_build_defines = _owned_sketch_build_defines(
            copy_result.build_defines,
            self.additional_defines,
        )
        return copy_result

    def _build_with_fbuild(self, example: str) -> SketchResult:
        """Compile the staged project with fbuild and write build_info."""
        from ci.util.fbuild_runner import run_fbuild_compile

        environment = self.board.board_name
        result = run_fbuild_compile(
            self.build_dir,
            environment=environment,
            verbose=self.verbose,
        )
        success = result.success

        if success:
            green_color = "\033[32m"
            reset_color = "\033[0m"
            print(f"{green_color}SUCCESS: {example}{reset_color}")
            generate_build_info_json_from_existing_build(
                self.build_dir, self.board, example
            )
        else:
            red_color = "\033[31m"
            reset_color = "\033[0m"
            print(f"{red_color}FAILED: {example}{reset_color}")

        return SketchResult(
            success=success,
            output=result.output or "fbuild compilation",
            build_dir=self.build_dir,
            example=example,
        )

    def clean(self) -> None:
        """Remove this board's staged project and build outputs."""
        print(f"Cleaning build artifacts for platform {self.board.board_name}...")
        if self.build_dir.exists():
            print(f"Removing build directory: {self.build_dir}")
            shutil.rmtree(self.build_dir)
            print(f"✅ Cleaned local build artifacts for {self.board.board_name}")
        else:
            print(
                f"✅ No build directory found for {self.board.board_name} (already clean)"
            )

    def clean_all(self) -> None:
        """Remove the staged project under the platform lock.

        fbuild owns the platform/toolchain cache (``~/.fbuild``); use
        ``fbuild clean-all`` to drop that.
        """
        with self.platform_lock:
            self.clean()

    def deploy(
        self, example: str, upload_port: Optional[str] = None, monitor: bool = False
    ) -> SketchResult:
        """Build ``example`` and flash it with ``fbuild deploy``.

        Args:
            example: Name of the example to deploy
            upload_port: Optional specific port for upload (e.g., "/dev/ttyUSB0", "COM3")
            monitor: If True, attach the fbuild monitor after a successful upload
        """
        from ci.util.fbuild_runner import run_fbuild_deploy

        print(f"Deploying {example} to {self.board.board_name}...")

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
            copy_result = self._restage_example(example)
            if not copy_result.success:
                return SketchResult(
                    success=False,
                    output=get_example_error_message(resolve_project_root(), example),
                    build_dir=self.build_dir,
                    example=example,
                )

        result = run_fbuild_deploy(
            self.build_dir,
            environment=self.board.board_name,
            upload_port=upload_port,
            verbose=self.verbose,
            monitor_after=monitor,
        )
        if result.success:
            print(f"✅ Upload completed successfully for {example}")
        return SketchResult(
            success=result.success,
            output=result.output or "fbuild deploy",
            build_dir=self.build_dir,
            example=example,
        )


def run_board_build(
    board: Board | str,
    examples: list[str],
    verbose: bool = False,
    additional_defines: Optional[list[str]] = None,
    additional_include_dirs: Optional[list[str]] = None,
) -> list[Future[SketchResult]]:
    """Build ``examples`` for ``board`` with fbuild.

    Args:
        board: Board class instance or board name string (resolved via create_board())
        examples: List of example names to build
        verbose: Enable verbose output
        additional_defines: Additional defines to add to build flags (e.g., ["FASTLED_DEFINE=0", "DEBUG=1"])
        additional_include_dirs: Additional include directories to add to build flags (e.g., ["src/platforms/sub", "external/libs"])
    """
    compiler = BoardCompiler(
        board,
        verbose,
        additional_defines,
        additional_include_dirs,
        None,
    )
    return compiler.build(examples)
