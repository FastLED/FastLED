#!/usr/bin/env python3
"""
Profile runner - orchestrates profiler generation, building, and execution.

Usage:
    python ci/profile_runner.py sincos16 --docker
    python ci/profile_runner.py sincos16 --iterations 50 --build-mode release
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path


class ProfileRunner:
    def __init__(
        self,
        target: str,
        iterations: int = 20,
        build_mode: str = "release",
        use_docker: bool = False,
        skip_generate: bool = False,
        use_callgrind: bool = False,
    ):
        self.target = target
        self.iterations = iterations
        self.build_mode = build_mode
        self.use_docker = use_docker
        self.skip_generate = skip_generate
        self.use_callgrind = use_callgrind
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
        """Generate profiler test if it doesn't exist"""
        if self.skip_generate and self.test_path.exists():
            print(f"Using existing profiler: {self.test_path}")
            return True

        if self.test_path.exists() and not self.skip_generate:
            print(f"Profiler already exists: {self.test_path}")
            response = input("Regenerate? [y/N]: ").strip().lower()
            if response != "y":
                return True

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
        """Build profiler binary directly using meson/ninja"""
        print(f"Building profiler (mode: {self.build_mode})...")

        # Use ci/meson/compile.py to build the specific target
        # This handles meson setup, configuration, and building
        cmd = [
            "uv",
            "run",
            "python",
            "ci/meson/compile.py",
            self.test_name,  # Build only this specific target
            "--build-mode",
            self.build_mode,
        ]

        if self.use_docker:
            cmd.append("--docker")

        try:
            subprocess.run(cmd, check=True, capture_output=True)
            print(f"✓ Built {self.test_name}")
            return True
        except subprocess.CalledProcessError as e:
            print(f"Error building profiler: {e}")
            if e.stderr:
                print(
                    f"stderr: {e.stderr.decode() if isinstance(e.stderr, bytes) else e.stderr}"
                )
            return False

    def get_binary_path(self) -> Path:
        """Get path to built profiler binary"""
        if self.use_docker:
            # Docker builds go to .build/meson-docker-<mode>/tests/profile/
            binary_path = Path(
                f".build/meson-docker-{self.build_mode}/tests/profile/{self.test_name}"
            )
        else:
            # Local builds go to .build/meson-<mode>/tests/profile/
            binary_path = Path(
                f".build/meson-{self.build_mode}/tests/profile/{self.test_name}"
            )
            if not binary_path.exists():
                # Windows adds .exe extension
                binary_path = Path(
                    f".build/meson-{self.build_mode}/tests/profile/{self.test_name}.exe"
                )

        return binary_path

    def run_benchmark(self) -> bool:
        """Run benchmark iterations and collect results"""
        print(f"Running {self.iterations} iterations...")

        binary_path = self.get_binary_path()
        if not binary_path.exists():
            print(f"Error: Binary not found at {binary_path}")
            return False

        results = []

        for i in range(1, self.iterations + 1):
            print(f"  Iteration {i}/{self.iterations}...", end=" ", flush=True)

            try:
                if self.use_docker:
                    # Run in Docker container
                    result = subprocess.run(
                        [
                            "docker",
                            "run",
                            "-it",
                            "--rm",
                            "-v",
                            f"{Path.cwd()}:/workspace",
                            "fastled-test",
                            f"/workspace/{binary_path}",
                            "baseline",
                        ],
                        capture_output=True,
                        text=True,
                        check=True,
                    )
                else:
                    # Run locally
                    result = subprocess.run(
                        [str(binary_path), "baseline"],
                        capture_output=True,
                        text=True,
                        check=True,
                    )

                # Parse structured output (check both stdout and stderr)
                output = result.stdout + result.stderr
                if "PROFILE_RESULT:" in output:
                    json_start = output.find("{")
                    json_end = output.rfind("}") + 1
                    if json_start != -1 and json_end > json_start:
                        profile_data = json.loads(output[json_start:json_end])
                        results.append(profile_data)
                        print(f"{profile_data['ns_per_call']:.2f} ns/call")
                    else:
                        print("ERROR: Could not parse JSON output")
                        return False
                else:
                    print("ERROR: No PROFILE_RESULT in output")
                    return False

            except subprocess.CalledProcessError as e:
                print(f"ERROR: {e}")
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

    def run_callgrind(self) -> bool:
        """Run callgrind analysis"""
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
        "--callgrind",
        action="store_true",
        help="Run callgrind analysis (slower)",
    )

    args = parser.parse_args()

    runner = ProfileRunner(
        target=args.target,
        iterations=args.iterations,
        build_mode=args.build_mode,
        use_docker=args.docker,
        skip_generate=args.no_generate,
        use_callgrind=args.callgrind,
    )

    return runner.run()


if __name__ == "__main__":
    sys.exit(main())
