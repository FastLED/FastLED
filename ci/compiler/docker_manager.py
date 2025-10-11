"""Docker management for FastLED compilation.

This module provides clean abstraction layers for Docker operations including
container lifecycle management, image building, and compilation orchestration.
"""

import os
import subprocess
import sys
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import List, Optional


class ContainerState(Enum):
    """Docker container states."""

    NOT_EXISTS = "not_exists"
    STOPPED = "stopped"
    PAUSED = "paused"
    RUNNING = "running"


@dataclass(frozen=True)
class DockerConfig:
    """Immutable Docker configuration."""

    board_name: str
    image_name: str
    project_root: Path
    container_name: str
    mount_target: str = "//host"  # Git-bash compatible path
    stop_timeout: int = 300

    @classmethod
    def from_board(cls, board_name: str, project_root: Path) -> "DockerConfig":
        """Factory method to create config from board name."""
        from ci.docker.build_image import extract_architecture, generate_config_hash

        architecture = extract_architecture(board_name)

        try:
            config_hash = generate_config_hash(board_name)
            image_name = f"fastled-platformio-{architecture}-{board_name}-{config_hash}"
        except Exception:
            # Fallback if hash generation fails
            image_name = f"fastled-platformio-{architecture}-{board_name}"

        container_name = f"fastled-{board_name}"

        # Use git-bash compatible mount target on Windows
        mount_target = "//host" if sys.platform == "win32" else "/host"

        return cls(
            board_name=board_name,
            image_name=image_name,
            project_root=project_root,
            container_name=container_name,
            mount_target=mount_target,
        )


class DockerContainerManager:
    """Manages Docker container lifecycle with minimal side effects."""

    def __init__(self, config: DockerConfig):
        self.config = config

    def get_container_state(self) -> ContainerState:
        """Query current container state without side effects."""
        # Check if container exists
        result = subprocess.run(
            ["docker", "container", "inspect", self.config.container_name],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            return ContainerState.NOT_EXISTS

        # Parse state from output
        state_result = subprocess.run(
            [
                "docker",
                "inspect",
                "-f",
                "{{.State.Running}} {{.State.Paused}}",
                self.config.container_name,
            ],
            capture_output=True,
            text=True,
        )
        parts = state_result.stdout.strip().split()
        is_running = parts[0] == "true"
        is_paused = parts[1] == "true" if len(parts) > 1 else False

        if is_paused:
            return ContainerState.PAUSED
        elif is_running:
            return ContainerState.RUNNING
        else:
            return ContainerState.STOPPED

    def create_container(self) -> bool:
        """Create a new container."""
        env = os.environ.copy()
        env["MSYS_NO_PATHCONV"] = "1"

        cmd = [
            "docker",
            "create",
            "--name",
            self.config.container_name,
            "-v",
            f"{self.config.project_root}:{self.config.mount_target}:ro",
            "--stop-timeout",
            str(self.config.stop_timeout),
            self.config.image_name,
            "tail",
            "-f",
            "/dev/null",
        ]

        result = subprocess.run(cmd, env=env)
        return result.returncode == 0

    def transition_to_running(self) -> bool:
        """Ensure container is in running state."""
        state = self.get_container_state()

        if state == ContainerState.NOT_EXISTS:
            print(f"Creating new container: {self.config.container_name}")
            if not self.create_container():
                return False
            state = ContainerState.STOPPED

        if state == ContainerState.STOPPED:
            print(f"Starting container: {self.config.container_name}")
            result = subprocess.run(["docker", "start", self.config.container_name])
            if result.returncode != 0:
                return False
        elif state == ContainerState.PAUSED:
            print(f"Unpausing container: {self.config.container_name}")
            result = subprocess.run(["docker", "unpause", self.config.container_name])
            if result.returncode != 0:
                return False
        else:
            print(f"✓ Using existing container: {self.config.container_name}")

        return True

    def pause(self) -> bool:
        """Pause the container to free resources."""
        result = subprocess.run(
            ["docker", "pause", self.config.container_name],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return result.returncode == 0


class DockerEntrypointBuilder:
    """Builds Docker entrypoint scripts with proper syncing logic."""

    @staticmethod
    def build_sync_script() -> str:
        """Generate the rsync script for directory syncing."""
        return """
# Sync host directories to container working directory if they exist
if command -v rsync &> /dev/null; then
    echo "Syncing directories from host..."

    # Define rsync exclude patterns for build artifacts
    RSYNC_EXCLUDES="--exclude=**/__pycache__ --exclude=.build/ --exclude=.pio/ --exclude=*.pyc"

    # Use --checksum to compare file contents instead of timestamps
    # This ensures changes are detected even when timestamps are unreliable across host/container filesystems

    # Start timing the entire sync phase
    SYNC_START=$(date +%s.%N)

    # Create temp files for timing results to avoid race conditions in output
    TMPDIR=$(mktemp -d)

    # Sync project-root pyproject.toml if present
    if [ -f "/host/pyproject.toml" ]; then
        echo "  → syncing pyproject.toml"
        (
            START=$(date +%s.%N)
            # Copy single file to /fastled root. No --delete needed for single-file.
            # --checksum ensures content-based update even with skewed timestamps.
            rsync -a --checksum /host/pyproject.toml /fastled/
            END=$(date +%s.%N)
            echo "$END - $START" | bc > "$TMPDIR/pyproject.time"
        ) &
    fi

    # Run all syncs in parallel, each writing its timing to a temp file
    if [ -d "/host/src" ]; then
        echo "  → syncing src/..."
        (
            START=$(date +%s.%N)
            rsync -a --checksum --delete $RSYNC_EXCLUDES /host/src/ /fastled/src/
            END=$(date +%s.%N)
            echo "$END - $START" | bc > "$TMPDIR/src.time"
        ) &
    fi

    if [ -d "/host/examples" ]; then
        echo "  → syncing examples/..."
        (
            START=$(date +%s.%N)
            rsync -a --checksum --delete $RSYNC_EXCLUDES /host/examples/ /fastled/examples/
            END=$(date +%s.%N)
            echo "$END - $START" | bc > "$TMPDIR/examples.time"
        ) &
    fi

    if [ -d "/host/ci" ]; then
        echo "  → syncing ci/..."
        (
            START=$(date +%s.%N)
            rsync -a --checksum --delete $RSYNC_EXCLUDES /host/ci/ /fastled/ci/
            END=$(date +%s.%N)
            echo "$END - $START" | bc > "$TMPDIR/ci.time"
        ) &
    fi

    # Wait for all parallel syncs to complete
    wait

    # Calculate and display total sync time
    SYNC_END=$(date +%s.%N)
    SYNC_TIME=$(echo "$SYNC_END - $SYNC_START" | bc)

    # Print individual times in deterministic order

    if [ -f "$TMPDIR/pyproject.time" ]; then
        printf "  ✓ pyproject.toml synced in %.3fs\\n" $(cat "$TMPDIR/pyproject.time")
    fi

    if [ -f "$TMPDIR/src.time" ]; then
        printf "  ✓ src/ synced in %.3fs\\n" $(cat "$TMPDIR/src.time")
    fi
    if [ -f "$TMPDIR/examples.time" ]; then
        printf "  ✓ examples/ synced in %.3fs\\n" $(cat "$TMPDIR/examples.time")
    fi
    if [ -f "$TMPDIR/ci.time" ]; then
        printf "  ✓ ci/ synced in %.3fs\\n" $(cat "$TMPDIR/ci.time")
    fi

    printf "  ─────────────────────────────\\n"
    printf "  Total sync time: %.3fs\\n" $SYNC_TIME

    # Cleanup temp files
    rm -rf "$TMPDIR"

    echo ""
    echo "Directory sync complete"

    # Clean Python bytecode cache to ensure latest code is used
    echo "Cleaning Python cache..."
    find /fastled/ci -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
    find /fastled/ci -type f -name "*.pyc" -delete 2>/dev/null || true
    echo "✓ Python cache cleaned"
else
    echo "Warning: rsync not available, skipping directory sync"
fi
"""

    @staticmethod
    def build_artifact_handling() -> str:
        """Generate artifact handling script."""
        return """
# If /fastled/output is mounted and compilation succeeded, copy build artifacts
if [ -d "/fastled/output" ] && [ $EXIT_CODE -eq 0 ]; then
    echo ""
    echo "Copying build artifacts to output directory..."

    # Find and copy all build artifacts
    if [ -d "/fastled/.pio/build" ]; then
        # Copy all binary files
        find /fastled/.pio/build -type f \\( -name "*.bin" -o -name "*.hex" -o -name "*.elf" -o -name "*.factory.bin" \\) \\
            -exec cp -v {} /fastled/output/ \\; 2>/dev/null || true

        echo "Build artifacts copied to output directory"
    fi
fi
"""

    @staticmethod
    def build_entrypoint(compile_cmd: str, handle_artifacts: bool = True) -> str:
        """Build complete entrypoint script."""
        sync_script = DockerEntrypointBuilder.build_sync_script()
        artifact_handling = (
            DockerEntrypointBuilder.build_artifact_handling()
            if handle_artifacts
            else ""
        )

        return f"""
set -e

{sync_script}

# Execute the compile command
{compile_cmd}
EXIT_CODE=$?

{artifact_handling}

exit $EXIT_CODE
"""


class DockerCompilationOrchestrator:
    """High-level orchestration of Docker compilation workflow."""

    def __init__(self, config: DockerConfig, container_mgr: DockerContainerManager):
        self.config = config
        self.container = container_mgr

    def ensure_image_exists(self, build_if_missing: bool = False) -> bool:
        """Ensure Docker image exists, optionally building it."""
        result = subprocess.run(
            ["docker", "image", "inspect", self.config.image_name],
            capture_output=True,
            text=True,
        )

        if result.returncode == 0:
            print(f"✓ Using existing Docker image: {self.config.image_name}")
            return True  # Image exists

        if not build_if_missing:
            print(f"❌ Docker image for {self.config.board_name} is not built yet.")
            print(f"")
            print(f"To build the image (can take up to 30 minutes):")
            print(f"  bash compile --docker --build {self.config.board_name} <example>")
            print(f"")
            return False

        # Build the image
        print(f"Building Docker image for platform: {self.config.board_name}")
        build_cmd = [
            sys.executable,
            "ci/build_docker_image_pio.py",
            "--platform",
            self.config.board_name,
            "--image-name",
            self.config.image_name,
        ]

        result = subprocess.run(build_cmd)
        return result.returncode == 0

    def run_compilation(
        self, compile_cmd: str, stream_output: bool = True
    ) -> "subprocess.CompletedProcess[str]":
        """Execute compilation inside container with proper setup."""
        if not self.container.transition_to_running():
            raise RuntimeError(
                f"Failed to start container: {self.config.container_name}"
            )

        print()
        print(f"Running compilation in container: {self.config.container_name}")
        print()

        # Build entrypoint script
        entrypoint = DockerEntrypointBuilder.build_entrypoint(
            compile_cmd, handle_artifacts=True
        )

        # Execute in container
        exec_cmd = [
            "docker",
            "exec",
            "-e",
            "FASTLED_DOCKER=1",
            self.config.container_name,
            "bash",
            "-c",
            entrypoint,
        ]

        print(f"Executing: {compile_cmd}")
        print()

        if stream_output:
            return self._stream_execution(exec_cmd)
        else:
            return subprocess.run(exec_cmd, capture_output=True, text=True)

    def _stream_execution(self, cmd: List[str]) -> "subprocess.CompletedProcess[str]":
        """Execute command with real-time output streaming."""
        sys.stdout.flush()
        sys.stderr.flush()
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=1,  # Line buffered
            universal_newlines=True,
            encoding="utf-8",
            errors="replace",  # Replace invalid UTF-8 with replacement character
        )

        if process.stdout:
            for line in process.stdout:
                print(line, end="", flush=True)

        returncode = process.wait()
        return subprocess.CompletedProcess[str](cmd, returncode, stdout="", stderr="")

    def copy_artifacts(self, container_path: str, host_path: Path) -> bool:
        """Copy artifacts from container to host."""
        host_path.mkdir(parents=True, exist_ok=True)

        print()
        print(f"Copying build artifacts from container to host...")

        copy_cmd = [
            "docker",
            "cp",
            f"{self.config.container_name}:{container_path}/.",
            str(host_path),
        ]

        result = subprocess.run(copy_cmd, capture_output=True, text=True)

        if result.returncode == 0:
            print(f"✅ Build artifacts copied to {host_path}")
            return True
        else:
            print(f"⚠️  Warning: Could not copy artifacts from container")
            if result.stderr:
                print(f"   Error: {result.stderr}")
            return False
