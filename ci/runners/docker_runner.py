#!/usr/bin/env python3
"""Docker runner for FastLED unit tests.

Builds and runs C++ unit tests inside a Docker container to provide
a consistent Linux environment with sanitizers (ASAN/LSAN) for detecting
memory issues.
"""

import subprocess
from pathlib import Path

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.test_types import TestArgs
from ci.util.timestamp_print import ts_print


def _check_docker_available() -> bool:
    """Check if Docker is available and running."""
    try:
        result = subprocess.run(
            ["docker", "info"],
            capture_output=True,
            timeout=10,
        )
        return result.returncode == 0
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise


def _build_docker_image(project_root: Path, rebuild: bool = False) -> bool:
    """Build the Docker image for unit tests.

    Args:
        project_root: Path to FastLED project root
        rebuild: If True, force rebuild even if image exists

    Returns:
        True if build succeeded, False otherwise
    """
    image_name = "fastled-unit-tests"
    dockerfile_path = project_root / "docker" / "unit-tests" / "Dockerfile"

    if not dockerfile_path.exists():
        ts_print(f"Error: Dockerfile not found at {dockerfile_path}")
        return False

    # Check if image already exists (skip build if not rebuilding)
    if not rebuild:
        result = subprocess.run(
            ["docker", "images", "-q", image_name],
            capture_output=True,
            text=True,
        )
        if result.stdout.strip():
            ts_print(f"Using existing Docker image: {image_name}")
            return True

    ts_print(f"Building Docker image: {image_name}")
    ts_print("This may take a few minutes on first run...")
    ts_print("")

    try:
        result = subprocess.run(
            [
                "docker",
                "build",
                "--progress=plain",  # Show full build output (not BuildKit compact mode)
                "-f",
                str(dockerfile_path),
                "-t",
                image_name,
                str(project_root),
            ],
            cwd=project_root,
            timeout=600,  # 10 minute timeout for build
        )
        if result.returncode != 0:
            ts_print("Error: Docker build failed")
            return False
        ts_print("Docker image built successfully")
        return True
    except subprocess.TimeoutExpired:
        ts_print("Error: Docker build timed out")
        return False
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise


def _build_test_command(args: TestArgs) -> list[str]:
    """Build the test command to run inside Docker.

    Args:
        args: Test arguments

    Returns:
        List of command parts for the test invocation
    """
    cmd = ["uv", "run", "test.py"]

    # Pass through relevant flags
    if args.cpp:
        cmd.append("--cpp")
    if args.unit:
        cmd.append("--unit")
    if args.examples is not None:
        cmd.append("--examples")
        if args.examples:  # Non-empty list
            cmd.extend(args.examples)
    if args.test:
        cmd.append(args.test)
    if args.verbose:
        cmd.append("--verbose")
    if args.clean:
        cmd.append("--clean")
    if args.no_parallel:
        cmd.append("--no-parallel")
    if args.full:
        cmd.append("--full")
    if args.no_fingerprint or args.force:
        cmd.append("--no-fingerprint")

    # Handle build mode
    if args.build_mode:
        cmd.extend(["--build-mode", args.build_mode])
    elif args.debug:
        cmd.append("--debug")
    elif args.quick:
        cmd.append("--quick")

    return cmd


def run_docker_tests(args: TestArgs) -> int:
    """Run tests inside Docker container.

    Args:
        args: Test arguments

    Returns:
        Exit code (0 for success, non-zero for failure)
    """
    project_root = Path.cwd()

    # Check Docker availability
    if not _check_docker_available():
        ts_print("Error: Docker is not available or not running")
        ts_print("")
        ts_print("Please install Docker Desktop and ensure it's running:")
        ts_print("  https://www.docker.com/products/docker-desktop")
        return 1

    # Build the Docker image
    if not _build_docker_image(project_root, rebuild=args.clean):
        return 1

    # Build the test command
    test_cmd = _build_test_command(args)
    test_cmd_str = " ".join(test_cmd)

    ts_print("=== Running Tests in Docker ===")
    ts_print(f"Command: {test_cmd_str}")
    ts_print("")

    # Run tests in Docker
    # Use a named volume for .venv to avoid conflicts with Windows host
    # Mount source directories individually to avoid .venv conflicts
    docker_cmd = [
        "docker",
        "run",
        "--rm",
        # Mount source code (read-only where possible)
        "-v",
        f"{project_root}/src:/fastled/src:ro",
        "-v",
        f"{project_root}/tests:/fastled/tests:ro",
        "-v",
        f"{project_root}/examples:/fastled/examples:ro",
        "-v",
        f"{project_root}/ci:/fastled/ci:ro",
        "-v",
        f"{project_root}/pyproject.toml:/fastled/pyproject.toml:ro",
        "-v",
        f"{project_root}/uv.lock:/fastled/uv.lock:ro",
        "-v",
        f"{project_root}/test.py:/fastled/test.py:ro",
        "-v",
        f"{project_root}/meson.build:/fastled/meson.build:ro",
        # Use a named volume for build artifacts (persistent across runs)
        "-v",
        "fastled-docker-venv:/fastled/.venv",
        "-v",
        "fastled-docker-build:/fastled/.build",
        "-w",
        "/fastled",
        # Pass through terminal for colors
        "-t",
        # Set environment to indicate Docker execution
        "-e",
        "FASTLED_DOCKER=1",
        "fastled-unit-tests",
        "bash",
        "-c",
        f"uv sync && {test_cmd_str}",
    ]

    try:
        result = subprocess.run(
            docker_cmd,
            cwd=project_root,
            timeout=1800,  # 30 minute timeout
        )
        return result.returncode
    except subprocess.TimeoutExpired:
        ts_print("Error: Tests timed out after 30 minutes")
        return 1
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        ts_print("\nTest run interrupted")
        raise
