"""Parallel lint orchestration with dependency-aware scheduling."""

import _thread  # noqa: F401 - Required for KBI linter
import os
import tempfile
import time
from concurrent.futures import Future, ThreadPoolExecutor
from pathlib import Path

from ci.lint.duration_tracker import DurationTracker
from ci.lint.stages import LintStage
from ci.util.global_interrupt_handler import is_interrupted


class LintOrchestrator:
    """
    Orchestrates parallel execution of lint stages.

    Handles:
    - Parallel execution with ThreadPoolExecutor
    - Dependency-aware scheduling
    - Output capture and replay
    - Timeout handling
    - Ctrl-C cleanup
    """

    def __init__(
        self,
        stages: list[LintStage],
        tracker: DurationTracker,
        parallel: bool = True,
    ) -> None:
        self.stages = stages
        self.tracker = tracker
        self.parallel = parallel
        self.tmpdir: Path | None = None
        self.stage_futures: dict[str, Future[tuple[bool, str]]] = {}
        self.stage_metadata: dict[str, dict[str, str]] = {}

    def run(self) -> bool:
        """
        Run all lint stages.

        Returns:
            True if all stages passed, False otherwise
        """
        if not self.stages:
            return True

        # Create temp directory for output capture
        self.tmpdir = Path(tempfile.mkdtemp(prefix="fastled_lint_"))

        try:
            if self.parallel and len(self.stages) > 1:
                return self._run_parallel()
            else:
                return self._run_inline()
        finally:
            # Cleanup temp directory
            if self.tmpdir and self.tmpdir.exists():
                import shutil

                shutil.rmtree(self.tmpdir, ignore_errors=True)

    def _run_parallel(self) -> bool:
        """Run stages in parallel with ThreadPoolExecutor."""
        assert self.tmpdir is not None, "tmpdir must be initialized before running"

        print("")
        print(f"⚡ PARALLEL LINTING ({len(self.stages)} stages)")
        print("================================")

        parallel_start = time.time()
        failed_stages: list[str] = []

        # Use ThreadPoolExecutor for I/O-bound workloads
        with ThreadPoolExecutor(max_workers=len(self.stages)) as executor:
            # Submit all stages (respecting dependencies)
            for stage in self.stages:
                future = executor.submit(self._run_stage_parallel, stage)
                self.stage_futures[stage.name] = future
                print(f"  Started {stage.name} (PID {os.getpid()})")

            print("")

            # Wait for all stages to complete
            for stage in self.stages:
                if is_interrupted():
                    print("\n⚠️  Interrupted - stopping all stages")
                    failed_stages.append(stage.name)
                    continue

                future = self.stage_futures[stage.name]
                try:
                    success, _ = future.result(timeout=stage.timeout)
                    if success:
                        print(f"  ✅ {stage.name} completed")
                    else:
                        print(f"  ❌ {stage.name} FAILED")
                        failed_stages.append(stage.name)
                except TimeoutError:
                    print(f"  ❌ {stage.name} TIMEOUT (>{stage.timeout}s)")
                    failed_stages.append(stage.name)
                except KeyboardInterrupt:
                    _thread.interrupt_main()
                    raise
                except Exception as e:
                    print(f"  ❌ {stage.name} ERROR: {e}")
                    failed_stages.append(stage.name)

        parallel_duration = time.time() - parallel_start
        print("")
        print(f"  Parallel phase completed in {int(parallel_duration)}s")

        # Print captured output from each stage
        for stage in self.stages:
            logfile = self.tmpdir / f"{stage.name}.log"
            if logfile.exists():
                print("")
                print(f"--- {stage.name} output ---")
                print(logfile.read_text())

        # Load metadata and record durations
        for stage in self.stages:
            self._load_stage_metadata(stage.name)

        return len(failed_stages) == 0

    def _run_inline(self) -> bool:
        """Run stages sequentially (inline mode)."""
        for stage in self.stages:
            if is_interrupted():
                print("\n⚠️  Interrupted")
                return False

            self.tracker.start_stage(stage.name)

            # Run the stage
            success = stage.run_fn()

            # End the stage and record duration
            self.tracker.end_stage(stage.name, skipped=False)

            if not success:
                return False

        return True

    def _run_stage_parallel(self, stage: LintStage) -> tuple[bool, str]:
        """
        Run a single stage in parallel mode.

        Returns:
            (success, output) tuple
        """
        assert self.tmpdir is not None, "tmpdir must be initialized"
        metafile = self.tmpdir / f"{stage.name}.log.meta"

        self.tracker.start_stage(stage.name)

        try:
            # Execute the stage
            success = stage.run_fn()

            # Record duration in metadata
            duration = time.time() - self.tracker.start_times[stage.name]
            self._write_metadata(metafile, "DURATION", str(int(duration)))

            return success, ""
        except KeyboardInterrupt:
            _thread.interrupt_main()
            raise
        except Exception as e:
            # Record error
            self._write_metadata(metafile, "ERROR", str(e))
            return False, f"Exception: {e}"

    def _write_metadata(self, filepath: Path, key: str, value: str) -> None:
        """Write a key-value pair to a metadata file."""
        with open(filepath, "a") as f:
            f.write(f"{key}:{value}\n")

    def _load_stage_metadata(self, stage_name: str) -> None:
        """Load metadata for a stage and record in tracker."""
        assert self.tmpdir is not None, "tmpdir must be initialized"
        metafile = self.tmpdir / f"{stage_name}.log.meta"
        if not metafile.exists():
            return

        metadata: dict[str, str] = {}
        for line in metafile.read_text().splitlines():
            if ":" in line:
                key, value = line.split(":", 1)
                metadata[key] = value

        self.stage_metadata[stage_name] = metadata

        # Record in tracker
        duration = float(metadata.get("DURATION", "0"))
        skipped = metadata.get("SKIPPED", "false").lower() == "true"
        self.tracker.record_stage(stage_name, duration, skipped)
