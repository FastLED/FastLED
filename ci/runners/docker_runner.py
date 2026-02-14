#!/usr/bin/env python3
"""Docker runner for FastLED unit tests.

Builds and runs C++ unit tests inside a Docker container to provide
a consistent Linux environment with sanitizers (ASAN/LSAN) for detecting
memory issues.

Optimized for fast incremental builds:
- Container and volume names are unique per project folder (via path hash)
- Named volumes persist .venv and .build across runs
- Source code is mounted read-only (no COPY in Dockerfile)
- Garbage collection removes old containers/volumes when limit exceeded
"""

import hashlib
import subprocess
from pathlib import Path

from ci.docker_utils.container_db import (
    ContainerDatabase,
    cleanup_orphaned_containers,
    docker_container_exists,
    docker_force_remove_container,
    garbage_collect_platform_containers,
)
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.test_types import TestArgs
from ci.util.timestamp_print import ts_print


# Maximum containers to keep per project path (for garbage collection)
MAX_CONTAINERS_PER_PROJECT = 2


def _get_path_hash(project_root: Path) -> str:
    """Generate a short hash from the project path for unique naming.

    Args:
        project_root: Path to the project root

    Returns:
        8-character hex hash of the path
    """
    # Normalize path (resolve symlinks, convert to absolute)
    normalized_path = str(project_root.resolve()).lower()
    # Use SHA256 and take first 8 characters
    return hashlib.sha256(normalized_path.encode()).hexdigest()[:8]


def _get_container_name(project_root: Path) -> str:
    """Get unique container name based on project path.

    Args:
        project_root: Path to the project root

    Returns:
        Container name in format: fastled-unit-tests-{hash}
    """
    path_hash = _get_path_hash(project_root)
    return f"fastled-unit-tests-{path_hash}"


def _get_volume_names(project_root: Path) -> tuple[str, str, str]:
    """Get unique volume names based on project path.

    Args:
        project_root: Path to the project root

    Returns:
        Tuple of (venv_volume_name, build_volume_name, cache_volume_name)
    """
    path_hash = _get_path_hash(project_root)
    return (
        f"fastled-docker-venv-{path_hash}",
        f"fastled-docker-build-{path_hash}",
        f"fastled-docker-cache-{path_hash}",
    )


def _check_docker_available() -> bool:
    """Check if Docker is available and running."""
    try:
        # Use 30s timeout - Docker Desktop on Windows can be slow to respond
        result = subprocess.run(
            ["docker", "info"],
            capture_output=True,
            timeout=30,
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
                str(project_root),  # Build context is project root (for pyproject.toml)
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


def _run_garbage_collection() -> None:
    """Run garbage collection to clean up orphaned containers and old volumes.

    This is best-effort and non-critical - don't let GC errors block the build.
    Runs in a subprocess with timeout to prevent hanging.
    """
    import concurrent.futures

    def _gc_task() -> tuple[int, int]:
        """Run GC in a separate thread with timeout protection."""
        db = ContainerDatabase()
        orphans = cleanup_orphaned_containers(db)
        old = garbage_collect_platform_containers(
            "unit-tests", max_containers=MAX_CONTAINERS_PER_PROJECT, db=db
        )
        return orphans, old

    try:
        # Run GC with a 30-second timeout to prevent hanging on Docker API errors
        with concurrent.futures.ThreadPoolExecutor(max_workers=1) as executor:
            future = executor.submit(_gc_task)
            try:
                orphans_cleaned, old_cleaned = future.result(timeout=30)
                if orphans_cleaned > 0:
                    ts_print(f"Cleaned up {orphans_cleaned} orphaned container(s)")
                if old_cleaned > 0:
                    ts_print(
                        f"Garbage collected {old_cleaned} old unit-test container(s)"
                    )
            except concurrent.futures.TimeoutError:
                ts_print("Warning: Garbage collection timed out (30s) - skipping")
                future.cancel()
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        # Don't fail the build if garbage collection fails
        ts_print(f"Warning: Garbage collection failed: {e}")


def _build_sync_script() -> str:
    """Build rsync script for syncing source files from /host to /fastled.

    Uses content-based comparison (MD5) and preserves source timestamps to avoid
    spurious rebuilds when files haven't actually changed.
    """
    return """
# Sync host directories to container working directory
if command -v rsync &> /dev/null; then
    echo "Syncing source files from /host to /fastled..."

    # rsync flags:
    # -a: Archive mode (includes -rlptgoD - recursive, links, perms, times, group, owner, devices)
    # --checksum: Compare MD5 checksums instead of size+mtime (detects real changes)
    # --delete: Remove destination files that don't exist in source
    #
    # By preserving timestamps (-t in -a), Meson won't see files as modified
    # when they haven't actually changed (avoids spurious reconfiguration).

    # Sync directories in parallel for speed
    if [ -d "/host/src" ]; then
        echo "  → syncing src/..."
        rsync -a --checksum --delete /host/src/ /fastled/src/ &
    fi

    if [ -d "/host/tests" ]; then
        echo "  → syncing tests/..."
        rsync -a --checksum --delete /host/tests/ /fastled/tests/ &
    fi

    if [ -d "/host/examples" ]; then
        echo "  → syncing examples/..."
        rsync -a --checksum --delete /host/examples/ /fastled/examples/ &
    fi

    if [ -d "/host/ci" ]; then
        echo "  → syncing ci/..."
        rsync -a --checksum --delete /host/ci/ /fastled/ci/ &
    fi

    if [ -d "/host/lint_plugins" ]; then
        echo "  → syncing lint_plugins/..."
        rsync -a --checksum --delete /host/lint_plugins/ /fastled/lint_plugins/ &
    fi

    # Sync individual files (preserving timestamps prevents Meson reconfiguration)
    if [ -f "/host/pyproject.toml" ]; then
        rsync -a --checksum /host/pyproject.toml /fastled/ &
    fi

    if [ -f "/host/uv.lock" ]; then
        rsync -a --checksum /host/uv.lock /fastled/ &
    fi

    if [ -f "/host/test.py" ]; then
        rsync -a --checksum /host/test.py /fastled/ &
    fi

    if [ -f "/host/meson.build" ]; then
        rsync -a --checksum /host/meson.build /fastled/ &
    fi

    if [ -f "/host/meson.options" ]; then
        rsync -a --checksum /host/meson.options /fastled/ &
    fi

    if [ -f "/host/README.md" ]; then
        rsync -a --checksum /host/README.md /fastled/ &
    fi

    # Wait for all parallel syncs to complete
    wait

    echo "  ✓ Source sync complete"

    # Clean Python bytecode cache to ensure latest code is used
    find /fastled/ci -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
    find /fastled/ci -type f -name "*.pyc" -delete 2>/dev/null || true
else
    echo "Warning: rsync not available, skipping directory sync"
fi
"""


def run_docker_tests(args: TestArgs) -> int:
    """Run tests inside Docker container.

    Uses path-based unique naming for containers and volumes to support
    multiple project folders and enable efficient caching.

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

    # Get unique container and volume names based on project path
    container_name = _get_container_name(project_root)
    venv_volume, build_volume, cache_volume = _get_volume_names(project_root)
    path_hash = _get_path_hash(project_root)

    ts_print(f"Project path hash: {path_hash}")
    ts_print(f"Container: {container_name}")
    ts_print(f"Volumes: {venv_volume}, {build_volume}, {cache_volume}")

    # Run garbage collection (non-blocking, best-effort)
    _run_garbage_collection()

    # Build the test command
    test_cmd = _build_test_command(args)
    test_cmd_str = " ".join(test_cmd)

    ts_print("=== Running Tests in Docker ===")
    ts_print(f"Command: {test_cmd_str}")
    ts_print("")

    # Remove any existing container with same name (in case of previous crash)
    existing_container_id = docker_container_exists(container_name)
    if existing_container_id:
        ts_print(f"Removing existing container: {container_name}")
        success, error_msg = docker_force_remove_container(existing_container_id)
        if not success:
            ts_print(f"Warning: Failed to remove existing container: {error_msg}")

    # Build rsync script for syncing host files to container
    sync_script = _build_sync_script()

    # Run tests in Docker
    # Use named volumes unique to this project path for .venv and .build
    # Mount source root as read-only at /host, then rsync into /fastled
    docker_cmd = [
        "docker",
        "run",
        "--rm",
        "--name",
        container_name,
        # Mount entire project root as read-only at /host
        "-v",
        f"{project_root}:/host:ro",
        # Use named volumes unique to this project path (persistent across runs)
        "-v",
        f"{venv_volume}:/fastled/.venv",
        "-v",
        f"{build_volume}:/fastled/.build",
        "-v",
        f"{cache_volume}:/root/.cache",  # Persist compiler/tool caches (uv, sccache, clang modules)
        "-w",
        "/fastled",
        # Pass through terminal for colors
        "-t",
        # Set environment to indicate Docker execution
        "-e",
        "FASTLED_DOCKER=1",
        "-e",
        "DOCKER_CONTAINER=1",  # Enables rsync --checksum mode
        "fastled-unit-tests",
        "bash",
        "-c",
        # Sync files from /host to /fastled, then run tests
        f"{sync_script}\n\n"
        f"export PATH=/fastled/.venv/bin:$PATH && "
        f"export PYTHONPATH=/fastled:$PYTHONPATH && "
        f"{test_cmd_str}",
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
