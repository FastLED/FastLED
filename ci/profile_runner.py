#!/usr/bin/env python3
"""
Profile runner - orchestrates profiler generation, building, and execution.

Usage:
    python ci/profile_runner.py sincos16
    python ci/profile_runner.py sincos16 --iterations 50 --build-mode release
    python ci/profile_runner.py 'fl::Perlin::pnoise2d' --callgrind

The Docker-based profile path (`bash profile --docker`, `--docker+--callgrind`) was
retired along with the rest of the host-Docker infrastructure. Profiling is native
only — Meson builds a local binary and callgrind (when installed) runs against it
directly.
"""

import argparse
import json
import os
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from running_process import EndOfStream, RunningProcess

from ci.util.deadlock_detector import handle_hung_test
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


@dataclass(slots=True)
class RunWithTimeoutResult:
    success: bool
    output: str
    pid_if_timeout: int | None


class ProfileRunner:
    def __init__(
        self,
        target: str,
        iterations: int = 20,
        build_mode: str = "release",
        skip_generate: bool = False,
        force_regenerate: bool = False,
        use_callgrind: bool = False,
        debuggable: bool = False,
    ):
        self.target = target
        self.iterations = iterations
        # Auto-select build mode based on flags
        # Priority: callgrind > debuggable > user-specified
        if use_callgrind and build_mode == "release":
            # Callgrind needs DWARF4 debug symbols (Valgrind 3.19 compatibility)
            self.build_mode = "profile"
            print(
                "ℹ️  Callgrind mode: Using 'profile' build mode (DWARF4 for Valgrind 3.19)"
            )
        elif debuggable and build_mode == "release":
            # Debuggable mode needs stack traces
            self.build_mode = "quick"
            print(
                "ℹ️  Debuggable mode: Using 'quick' build mode for stack trace preservation"
            )
        else:
            self.build_mode = build_mode
        self.skip_generate = skip_generate
        self.force_regenerate = force_regenerate
        self.use_callgrind = use_callgrind
        self.debuggable = debuggable
        self.test_name = self._sanitize_name(target)
        self.test_path = Path(f"tests/profile/{self.test_name}.cpp")
        self.results_file = Path(f"{self.test_name}_results.json")

    def _sanitize_name(self, name: str) -> str:
        """Convert 'fl::sincos16' → 'sincos16'"""
        return (
            name.replace("::", "_")
            .replace("<", "_")
            .replace(">", "_")
            .replace(" ", "_")
            .lower()
        )

    def generate_profiler(self) -> bool:
        """Generate profiler test with smart defaults (no interactive prompts)"""
        # Case 1: Profiler exists and we're not forcing regeneration
        if self.test_path.exists() and not self.force_regenerate:
            if self.skip_generate:
                print(f"Using existing profiler: {self.test_path}")
            else:
                print(f"Using existing profiler: {self.test_path}")
                print("  (Use --regenerate to recreate from template)")
            return True

        # Case 2: Profiler doesn't exist but generation is disabled
        if not self.test_path.exists() and self.skip_generate:
            print(f"❌ Profiler not found: {self.test_path}")
            print("  Remove --no-generate flag to auto-generate, or create it manually")
            return False

        # Case 3: Generate profiler (either doesn't exist or --regenerate was used)
        if self.force_regenerate:
            print(f"Regenerating profiler for {self.target}...")
        else:
            print(f"Generating profiler for {self.target}...")

        try:
            subprocess.run(
                [
                    "uv",
                    "run",
                    "python",
                    "ci/profile/generate_profile_test.py",
                    self.target,
                ],
                check=True,
            )
            return True
        except subprocess.CalledProcessError as e:
            print(f"Error generating profiler: {e}")
            return False

    def build_profiler(self) -> bool:
        """Build profiler binary using the local test system."""
        print(f"Building profiler (mode: {self.build_mode})...")

        cmd = [
            "uv",
            "run",
            "python",
            "test.py",
            self.test_name,
            "--cpp",
            "--build",  # Build-only mode
            "--build-mode",
            self.build_mode,
        ]

        try:
            proc = RunningProcess(
                cmd,
                auto_run=True,
                timeout=600,
            )

            while line := proc.get_next_line(timeout=600):
                if isinstance(line, EndOfStream):
                    break
                print(line, end="")

            exit_code = proc.wait()
            if exit_code == 0:
                print(f"✓ Built {self.test_name}")
                return True
            else:
                print(f"Error: Build failed with exit code {exit_code}")
                return False

        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            print(f"Error building profiler: {e}")
            return False

    def get_binary_path(self) -> Path:
        """Get path to built profiler binary"""
        base_path = f".build/meson-{self.build_mode}/tests/profile/{self.test_name}"

        # Try without extension first (Linux/macOS)
        binary_path = Path(base_path)
        if binary_path.exists():
            return binary_path

        # Try with .exe extension (Windows)
        binary_path_exe = Path(f"{base_path}.exe")
        if binary_path_exe.exists():
            return binary_path_exe

        # Return the expected path even if it doesn't exist (for error messages)
        return Path(base_path)

    def _run_with_timeout(
        self, cmd: list[str], timeout_seconds: int = 120
    ) -> RunWithTimeoutResult:
        """
        Run command with timeout and deadlock detection.

        Returns:
            Tuple of (success, output, pid_if_timeout)
        """
        env = os.environ.copy()
        dll_dir = str(Path(f".build/meson-{self.build_mode}/ci/meson/native").resolve())
        if "PATH" in env:
            env["PATH"] = dll_dir + os.pathsep + env["PATH"]
        else:
            env["PATH"] = dll_dir
        if "LD_LIBRARY_PATH" in env:
            env["LD_LIBRARY_PATH"] = dll_dir + os.pathsep + env["LD_LIBRARY_PATH"]
        else:
            env["LD_LIBRARY_PATH"] = dll_dir
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env,
        )

        start_time = time.time()

        while True:
            retcode = proc.poll()

            if retcode is not None:
                stdout, stderr = proc.communicate()
                output = stdout + stderr

                if retcode == 0:
                    return RunWithTimeoutResult(
                        success=True, output=output, pid_if_timeout=None
                    )
                else:
                    return RunWithTimeoutResult(
                        success=False, output=output, pid_if_timeout=None
                    )

            elapsed = time.time() - start_time
            if elapsed > timeout_seconds:
                pid = proc.pid
                print(f"\n🚨 TIMEOUT EXCEEDED after {elapsed:.1f}s!")
                print(f"📍 Attaching debugger to PID {pid}...")

                handle_hung_test(pid, self.test_name, timeout_seconds)

                proc.kill()
                proc.wait()

                return RunWithTimeoutResult(
                    success=False, output="TIMEOUT", pid_if_timeout=pid
                )

            time.sleep(0.1)

    def run_benchmark(self) -> bool:
        """Run benchmark iterations and collect results"""
        print(f"Running {self.iterations} iterations...")
        print("⏱️  Timeout: 120 seconds per iteration (deadlock detector enabled)")
        if self.debuggable or self.build_mode in ["debug", "quick"]:
            print(f"🐛 Stack traces enabled (build mode: {self.build_mode})")
        else:
            print(
                f"⚡ Performance mode (build mode: {self.build_mode}) - limited stack traces"
            )

        binary_path = self.get_binary_path()
        if not binary_path.exists():
            print(f"Error: Binary not found at {binary_path}")
            return False

        results: list[dict[str, Any]] = []

        for i in range(1, self.iterations + 1):
            print(f"  Iteration {i}/{self.iterations}...", end=" ", flush=True)

            try:
                cmd = [str(binary_path), "baseline"]
                result = self._run_with_timeout(cmd, timeout_seconds=120)
                success = result.success
                output = result.output
                pid = result.pid_if_timeout

                if not success:
                    if pid is not None:
                        return False
                    print("ERROR: Process exited with error")
                    print(f"Output: {output}")
                    return False

                iteration_results: list[dict[str, Any]] = []
                for line in output.splitlines():
                    if "PROFILE_RESULT:" in line:
                        json_start = line.find("{")
                        json_end = line.rfind("}") + 1
                        if json_start != -1 and json_end > json_start:
                            profile_data: dict[str, Any] = json.loads(
                                line[json_start:json_end]
                            )
                            iteration_results.append(profile_data)

                if not iteration_results:
                    print("ERROR: No PROFILE_RESULT in output")
                    print(f"Output: {output[:500]}...")
                    return False

                results.extend(iteration_results)

                if len(iteration_results) == 1:
                    print(f"{iteration_results[0]['ns_per_call']:.2f} ns/call")
                else:
                    summary = ", ".join(
                        f"{r['variant']}: {r['ns_per_call']:.2f} ns"
                        for r in iteration_results
                    )
                    print(summary)

            except subprocess.TimeoutExpired:
                print("\n🚨 TIMEOUT EXCEEDED!")
                return False

            except subprocess.CalledProcessError as e:
                print(f"ERROR: {e}")
                return False

        with open(self.results_file, "w") as f:
            json.dump(results, f, indent=2)

        print(f"\nResults saved to: {self.results_file}")
        return True

    def analyze_results(self) -> bool:
        """Parse and display results"""
        print("\nAnalyzing results...")

        try:
            subprocess.run(
                [
                    "uv",
                    "run",
                    "python",
                    "ci/profile/parse_results.py",
                    str(self.results_file),
                ],
                check=True,
            )
            return True
        except subprocess.CalledProcessError as e:
            print(f"Error analyzing results: {e}")
            return False

    def run_callgrind(self) -> bool:
        """Run callgrind analysis against the local binary."""
        print("\nRunning callgrind analysis...")

        binary_path = self.get_binary_path()
        if not binary_path.exists():
            print(f"Error: Binary not found at {binary_path}")
            return False

        try:
            subprocess.run(
                [
                    "uv",
                    "run",
                    "python",
                    "ci/profile/callgrind_analyze.py",
                    str(binary_path),
                ],
                check=True,
            )
            return True
        except subprocess.CalledProcessError as e:
            print(f"Error running callgrind: {e}")
            return False

    def run(self) -> int:
        """Run complete profiling workflow"""
        # Step 1: Generate profiler
        if not self.generate_profiler():
            return 1

        # Step 2: Build profiler
        if not self.build_profiler():
            return 1

        # Step 3: Run benchmarks
        if not self.run_benchmark():
            return 1

        # Step 4: Analyze results
        if not self.analyze_results():
            return 1

        # Step 5: Optional callgrind analysis
        if self.use_callgrind:
            if not self.run_callgrind():
                print("Warning: Callgrind analysis failed (continuing anyway)")

        print("\n✅ Profiling complete!")
        return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Profile function performance",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python ci/profile_runner.py sincos16
  python ci/profile_runner.py sincos16 --iterations 50 --build-mode release
  python ci/profile_runner.py 'fl::Perlin::pnoise2d' --callgrind
        """,
    )
    parser.add_argument("target", help="Function/module to profile")
    parser.add_argument(
        "--iterations",
        type=int,
        default=20,
        help="Number of benchmark iterations (default: 20)",
    )
    parser.add_argument(
        "--build-mode",
        choices=["quick", "debug", "release", "profile"],
        default="release",
        help="Build mode (default: release)",
    )
    parser.add_argument(
        "--no-generate",
        action="store_true",
        help="Skip test generation (use existing profiler)",
    )
    parser.add_argument(
        "--regenerate",
        action="store_true",
        help="Force regeneration of profiler (overwrite existing)",
    )
    parser.add_argument(
        "--callgrind",
        action="store_true",
        help="Run callgrind analysis (slower)",
    )
    parser.add_argument(
        "--debuggable",
        action="store_true",
        help="Enable stack trace preservation for deadlock debugging (uses 'quick' mode, ~1-5%% slower)",
    )

    args = parser.parse_args()

    runner = ProfileRunner(
        target=args.target,
        iterations=args.iterations,
        build_mode=args.build_mode,
        skip_generate=args.no_generate,
        force_regenerate=args.regenerate,
        use_callgrind=args.callgrind,
        debuggable=args.debuggable,
    )

    return runner.run()


if __name__ == "__main__":
    sys.exit(main())
