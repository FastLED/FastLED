#!/usr/bin/env python3
"""
Profile runner - orchestrates profiler generation, building, and execution.

Usage:
    python ci/profile_runner.py sincos16 --docker
    python ci/profile_runner.py sincos16 --iterations 50 --build-mode release
"""

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

from running_process import RunningProcess
from running_process.process_output_reader import EndOfStream

from ci.util.deadlock_detector import handle_hung_test
from ci.util.docker_helper import attempt_start_docker
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


class ProfileRunner:
    def __init__(
        self,
        target: str,
        iterations: int = 20,
        build_mode: str = "release",
        use_docker: bool = False,
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
        self.use_docker = use_docker
        self.skip_generate = skip_generate
        self.force_regenerate = force_regenerate
        self.use_callgrind = use_callgrind
        self.debuggable = debuggable
        self.test_name = "profile_" + self._sanitize_name(
            target
        )  # Include "profile_" prefix
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
        """Build profiler binary using test system (supports Docker)"""
        print(f"Building profiler (mode: {self.build_mode})...")

        # Use test system which properly handles Docker builds
        # test.py wraps the Meson build system and supports Docker
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

        if self.use_docker:
            cmd.append("--docker")

        try:
            # Use RunningProcess to stream output and avoid buffer deadlock
            # test.py can produce lots of output (multiple MB), so we must stream
            proc = RunningProcess(
                cmd,
                auto_run=True,
                timeout=600,  # 10 minute timeout for Docker builds
            )

            # Stream all output to console
            while line := proc.get_next_line(timeout=600):
                if isinstance(line, EndOfStream):
                    break
                # Print build output in real-time
                print(line, end="")

            # Check exit code
            exit_code = proc.wait()
            if exit_code == 0:
                print(f"✓ Built {self.test_name}")
                return True
            else:
                print(f"Error: Build failed with exit code {exit_code}")
                return False

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Error building profiler: {e}")
            return False

    def get_binary_path(self) -> Path:
        """Get path to built profiler binary"""
        # Both Docker and local builds use the same .build/meson-<mode>/ directory
        # Docker mounts the host's .build directory, so binaries are in the same location
        base_path = f".build/meson-{self.build_mode}/tests/profile/{self.test_name}"

        # Try without extension first (Linux/Docker)
        binary_path = Path(base_path)
        if binary_path.exists():
            return binary_path

        # Try with .exe extension (Windows local builds)
        binary_path_exe = Path(f"{base_path}.exe")
        if binary_path_exe.exists():
            return binary_path_exe

        # Return the expected path even if it doesn't exist (for error messages)
        return Path(base_path)

    def _run_with_timeout(
        self, cmd: list[str], timeout_seconds: int = 120
    ) -> tuple[bool, str, int | None]:
        """
        Run command with timeout and deadlock detection.

        Returns:
            Tuple of (success, output, pid_if_timeout)
        """
        # Start process without built-in timeout (we monitor manually)
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        start_time = time.time()

        # Poll process with timeout monitoring
        while True:
            retcode = proc.poll()

            # Process finished normally
            if retcode is not None:
                stdout, stderr = proc.communicate()
                output = stdout + stderr

                if retcode == 0:
                    return (True, output, None)
                else:
                    return (False, output, None)

            # Check timeout
            elapsed = time.time() - start_time
            if elapsed > timeout_seconds:
                # Timeout! Attach debugger before killing
                pid = proc.pid
                print(f"\n🚨 TIMEOUT EXCEEDED after {elapsed:.1f}s!")
                print(f"📍 Attaching debugger to PID {pid}...")

                # Dump stack trace before killing
                handle_hung_test(pid, self.test_name, timeout_seconds)

                # Now kill the process
                proc.kill()
                proc.wait()

                return (False, "TIMEOUT", pid)

            # Sleep briefly before next check
            time.sleep(0.1)

    def run_benchmark(self) -> bool:
        """Run benchmark iterations and collect results"""
        # Docker mode: Binary is in Docker volume, not accessible from host
        # For Docker+callgrind, we skip iterations and rely on callgrind analysis
        if self.use_docker and self.use_callgrind:
            print("🐳 Docker + Callgrind mode: Skipping benchmark iterations")
            print("   (Callgrind provides deterministic instruction counts)")
            # Create minimal results file for callgrind
            minimal_result = {
                "variant": "docker_build",
                "target": self.target,
                "total_calls": 1,
                "total_time_ns": 0,
                "ns_per_call": 0,
                "calls_per_sec": 0,
            }
            with open(self.results_file, "w") as f:
                json.dump([minimal_result], f, indent=2)
            return True

        print(f"Running {self.iterations} iterations...")
        print(f"⏱️  Timeout: 120 seconds per iteration (deadlock detector enabled)")
        if self.debuggable or self.build_mode in ["debug", "quick"]:
            print(f"🐛 Stack traces enabled (build mode: {self.build_mode})")
        else:
            print(
                f"⚡ Performance mode (build mode: {self.build_mode}) - limited stack traces"
            )

        if self.use_docker:
            print(
                f"🐳 Running in Docker (volume: fastled-docker-build-{self._get_project_hash()})"
            )

        binary_path = self.get_binary_path()
        if not self.use_docker and not binary_path.exists():
            print(f"Error: Binary not found at {binary_path}")
            return False

        results: list[dict[str, Any]] = []

        for i in range(1, self.iterations + 1):
            print(f"  Iteration {i}/{self.iterations}...", end=" ", flush=True)

            try:
                if self.use_docker:
                    # Docker: Run binary inside container with access to build volume
                    docker_cmd = [
                        "docker",
                        "run",
                        "--rm",
                        "-v",
                        f"{Path.cwd().as_posix()}:/fastled",
                        "-v",
                        f"fastled-docker-build-{self._get_project_hash()}:/fastled/.build",
                        "fastled-unit-tests",
                        f"/fastled/.build/meson-{self.build_mode}/tests/profile/{self.test_name}",
                        "baseline",
                    ]
                    result = subprocess.run(
                        docker_cmd,
                        capture_output=True,
                        text=True,
                        check=True,
                        timeout=120,
                    )
                    output = result.stdout + result.stderr
                else:
                    # Local: Use custom timeout with debugger attachment
                    cmd = [str(binary_path), "baseline"]
                    success, output, pid = self._run_with_timeout(
                        cmd, timeout_seconds=120
                    )

                    if not success:
                        if pid is not None:
                            # Timeout occurred - stack trace already dumped
                            return False
                        else:
                            # Process failed normally
                            print(f"ERROR: Process exited with error")
                            print(f"Output: {output}")
                            return False

                # Parse structured output - handle multiple PROFILE_RESULT lines
                # (comparison tests output multiple results per run)
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
                    print(f"Output: {output[:500]}...")  # Show first 500 chars
                    return False

                # Add all results from this iteration
                results.extend(iteration_results)

                # Display summary for this iteration
                if len(iteration_results) == 1:
                    print(f"{iteration_results[0]['ns_per_call']:.2f} ns/call")
                else:
                    # Multiple variants - show all
                    summary = ", ".join(
                        f"{r['variant']}: {r['ns_per_call']:.2f} ns"
                        for r in iteration_results
                    )
                    print(summary)

            except subprocess.TimeoutExpired:
                if self.use_docker:
                    print("\n🚨 DOCKER TIMEOUT EXCEEDED!")
                    print("⚠️  Cannot attach debugger inside Docker container")
                else:
                    print("\n🚨 TIMEOUT EXCEEDED!")
                return False

            except subprocess.CalledProcessError as e:
                print(f"ERROR: {e}")
                if self.use_docker:
                    print("Docker stderr:", e.stderr if hasattr(e, "stderr") else "N/A")
                return False

        # Save results
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

    def _get_project_hash(self) -> str:
        """Generate a short hash from the project path for Docker volume naming.

        Returns:
            8-character hex hash of the path
        """
        # Normalize path (resolve symlinks, convert to absolute)
        normalized_path = str(Path.cwd().resolve()).lower()
        # Use SHA256 and take first 8 characters
        return hashlib.sha256(normalized_path.encode()).hexdigest()[:8]

    def ensure_docker_running(self) -> bool:
        """Ensure Docker is running if Docker mode is enabled.

        Returns:
            True if Docker is ready (or not needed), False if Docker failed to start
        """
        if not self.use_docker:
            return True  # Docker not needed

        print("🐳 Checking Docker availability...")
        success, message = attempt_start_docker()

        if success:
            print(f"✓ {message}")
            return True
        else:
            print(f"❌ Docker Error: {message}")
            print()
            print("Unable to start Docker automatically.")
            print("Please start Docker Desktop manually and try again.")
            return False

    def run_callgrind(self) -> bool:
        """Run callgrind analysis"""
        print("\nRunning callgrind analysis...")

        if self.use_docker:
            # Docker mode: Run callgrind using test system infrastructure
            # The test system uses Docker volumes, so we need to use test.py to access them
            print("🐳 Running callgrind inside Docker container...")
            try:
                # Run a custom callgrind wrapper script inside Docker
                # Use test.py's Docker infrastructure to ensure volume access
                callgrind_script = f"""
#!/bin/bash
set -e
cd /fastled
BINARY=.build/meson-{self.build_mode}/tests/profile/{self.test_name}
if [ ! -f "$BINARY" ]; then
    echo "Error: Binary not found at $BINARY"
    ls -la .build/meson-{self.build_mode}/tests/profile/ || true
    exit 1
fi
echo "Running callgrind on $BINARY..."
echo "Build mode: {self.build_mode} (DWARF4 debug symbols for Valgrind 3.19 compatibility)"
# Run callgrind directly on binary with DWARF4 symbols
valgrind --tool=callgrind \\
    --callgrind-out-file=/fastled/callgrind.out \\
    "$BINARY" baseline
echo "Callgrind complete! Output: callgrind.out"
ls -lh /fastled/callgrind.out
"""
                # Write script to temp file with Unix line endings (LF only)
                script_path = Path("_callgrind_temp.sh")
                script_path.write_text(callgrind_script, newline="\n")

                # Run via Docker with same volumes as test system
                print(
                    f"   Docker volume: fastled-docker-build-{self._get_project_hash()}"
                )
                result = subprocess.run(
                    [
                        "docker",
                        "run",
                        "--rm",
                        "-v",
                        f"{Path.cwd().as_posix()}:/fastled",
                        "-v",
                        f"fastled-docker-build-{self._get_project_hash()}:/fastled/.build",
                        "fastled-unit-tests",
                        "bash",
                        "/fastled/_callgrind_temp.sh",
                    ],
                    capture_output=False,  # Show output in real-time
                    check=False,
                )

                # Cleanup temp script (keep on error for debugging)
                if result.returncode == 0:
                    script_path.unlink(missing_ok=True)

                if result.returncode == 0:
                    print("\n✓ Callgrind analysis complete")
                    print("📊 Callgrind output: callgrind.out")
                    print("   View with: callgrind_annotate callgrind.out")
                    return True
                else:
                    print(f"\n⚠️  Callgrind exited with code {result.returncode}")
                    print(f"   Debug script saved: _callgrind_temp.sh")
                    print("   Check output above for details")
                    return False

            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                print(f"Error running callgrind in Docker: {e}")
                return False
        else:
            # Local mode: Run callgrind directly
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
        # Step 0: Ensure Docker is running (if needed)
        if not self.ensure_docker_running():
            return 1

        # Step 1: Generate profiler
        if not self.generate_profiler():
            return 1

        # Step 2: Build profiler
        if not self.build_profiler():
            return 1

        # Step 3: Run benchmarks (skipped for Docker+callgrind)
        if not self.run_benchmark():
            return 1

        # Step 4: Analyze results (skip for Docker+callgrind mode)
        if not (self.use_docker and self.use_callgrind):
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
  python ci/profile_runner.py sincos16 --docker
  python ci/profile_runner.py sincos16 --iterations 50 --build-mode release
  python ci/profile_runner.py 'fl::Perlin::pnoise2d' --docker --callgrind
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
        "--docker",
        action="store_true",
        help="Run in Docker (consistent environment)",
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
        use_docker=args.docker,
        skip_generate=args.no_generate,
        force_regenerate=args.regenerate,
        use_callgrind=args.callgrind,
        debuggable=args.debuggable,
    )

    return runner.run()


if __name__ == "__main__":
    sys.exit(main())
