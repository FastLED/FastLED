from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""Docker management for FastLED compilation.

This module provides clean abstraction layers for Docker operations including
container lifecycle management, image building, and compilation orchestration.
"""

import hashlib
import os
import subprocess
import sys
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Optional

from ci.docker_utils.container_db import (
    ContainerDatabase,
    prepare_container,
)
from ci.util.docker_command import get_docker_command


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
    output_dir: Optional[Path] = None  # Optional host directory for build artifacts
    platform_type: str = ""  # Platform family (e.g., "avr", "esp-32s3") for GC

    def calculate_config_hash(self) -> str:
        """Calculate hash of container configuration for reuse detection.

        Hash is based on:
        - project_root path
        - output_dir path (if set)
        - image_name
        - mount_target

        Returns:
            SHA256 hash (first 16 chars) of configuration
        """
        hash_components = [
            str(self.project_root.resolve()),
            str(self.output_dir.resolve()) if self.output_dir else "",
            self.image_name,
            self.mount_target,
        ]

        hash_input = "|".join(hash_components)
        full_hash = hashlib.sha256(hash_input.encode()).hexdigest()

        # Return first 16 characters for shorter container names
        return full_hash[:16]

    @staticmethod
    def _validate_output_dir(output_dir: Path, project_root: Path) -> Path:
        """Validate and prepare output directory.

        Args:
            output_dir: Path to output directory (can be relative or absolute)
            project_root: Project root path for validation

        Returns:
            Absolute path to validated output directory

        Raises:
            ValueError: If output directory is outside project root or parent directory
        """
        # Resolve to absolute path
        if not output_dir.is_absolute():
            output_dir = (project_root / output_dir).resolve()
        else:
            output_dir = output_dir.resolve()

        # Security check: only allow directories within or relative to project root
        cwd = Path.cwd().resolve()
        try:
            # Check if output_dir is relative to project_root or current working directory
            try:
                output_dir.relative_to(project_root)
                # Output dir is under project root - OK
            except ValueError:
                # Not under project root, check if under current working directory
                output_dir.relative_to(cwd)
                # Output dir is under cwd - OK
        except ValueError:
            raise ValueError(
                f"Output directory must be within project root ({project_root}) "
                f"or current working directory ({cwd}), got: {output_dir}"
            )

        # Create directory if it doesn't exist
        output_dir.mkdir(parents=True, exist_ok=True)

        return output_dir

    @classmethod
    def from_board(
        cls, board_name: str, project_root: Path, output_dir: Optional[Path] = None
    ) -> "DockerConfig":
        """Factory method to create config from board name.

        Maps boards to their platform Docker images and containers.
        Platform strategy varies:
        - Grouped platforms (AVR, Teensy, etc.): ONE image contains ALL boards in that family
        - Flat platforms (ESP): ONE image per board to prevent build artifact accumulation

        All platform images use the :latest tag since they contain all necessary
        toolchains for their boards.

        Examples (board -> image, container):
            uno -> niteris/fastled-compiler-avr:latest, container: fastled-compiler-avr
            esp32s3 -> niteris/fastled-compiler-esp-32s3:latest, container: fastled-compiler-esp-32s3
            esp32c3 -> niteris/fastled-compiler-esp-32c3:latest, container: fastled-compiler-esp-32c3
            esp8266 -> niteris/fastled-compiler-esp-8266:latest, container: fastled-compiler-esp-8266
            teensy41 -> niteris/fastled-compiler-teensy:latest, container: fastled-compiler-teensy
        """
        from ci.docker_utils.build_platforms import (
            get_docker_image_name,
            get_platform_for_board,
        )

        platform: str | None = None
        try:
            # Look up which platform family this board belongs to
            platform = get_platform_for_board(board_name)

            if platform:
                # Get the platform image name (e.g., niteris/fastled-compiler-avr, niteris/fastled-compiler-esp-32s3)
                # For grouped platforms (avr, teensy): one image for multiple boards
                # For flat platforms (esp-32s3, esp-8266): one image per board
                platform_image = get_docker_image_name(platform)
                # Keep the full registry prefix (niteris/) for Docker Hub compatibility
                # Docker will automatically pull from the registry if image is not found locally
                image_name = f"{platform_image}:latest"
            else:
                # Board not in platform mapping - generate ad-hoc name with registry prefix
                from ci.docker_utils.build_image import extract_architecture

                arch = extract_architecture(board_name)
                image_name = f"niteris/fastled-compiler-{arch}-{board_name}:latest"

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            # Fallback if lookups fail
            print(
                f"Warning: Could not map board to platform image for {board_name}: {e}"
            )
            image_name = f"niteris/fastled-compiler-{board_name}:latest"

        # Determine platform_type for garbage collection
        # This is the same as platform, but stored separately for GC tracking
        platform_type_value = platform if platform else board_name

        # Use git-bash compatible mount target on Windows
        mount_target = "//host" if sys.platform == "win32" else "/host"

        # Validate and resolve output directory if provided
        validated_output_dir = None
        if output_dir is not None:
            validated_output_dir = cls._validate_output_dir(output_dir, project_root)

        # Create temporary config to calculate hash
        temp_config = cls(
            board_name=board_name,
            image_name=image_name,
            project_root=project_root,
            container_name="",  # Will be set below
            mount_target=mount_target,
            output_dir=validated_output_dir,
            platform_type=platform_type_value,
        )

        # Calculate configuration hash
        config_hash = temp_config.calculate_config_hash()

        # Container name includes hash suffix for unique identification
        # Format: fastled-compiler-{platform}-{hash[:8]}
        # This allows multiple containers with different configurations
        if platform:
            container_name = f"fastled-compiler-{platform}-{config_hash[:8]}"
        else:
            # Fallback for unmapped boards
            container_name = f"fastled-compiler-{board_name}-{config_hash[:8]}"

        return cls(
            board_name=board_name,
            image_name=image_name,
            project_root=project_root,
            container_name=container_name,
            mount_target=mount_target,
            output_dir=validated_output_dir,
            platform_type=platform_type_value,
        )


class DockerContainerManager:
    """Manages Docker container lifecycle with minimal side effects."""

    def __init__(self, config: DockerConfig):
        self.config = config
        self._image_name_override: Optional[str] = None
        self._db = ContainerDatabase()
        self._container_id: Optional[str] = None

    def set_image_name(self, image_name: str) -> None:
        """Override the image name to use (for cases where local image differs from config)."""
        self._image_name_override = image_name

    def _get_image_name(self) -> str:
        """Get the image name to use for this container."""
        return self._image_name_override or self.config.image_name

    def get_container_state(self) -> ContainerState:
        """Query current container state without side effects."""
        try:
            # Check if container exists
            result = subprocess.run(
                [
                    get_docker_command(),
                    "container",
                    "inspect",
                    self.config.container_name,
                ],
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode != 0:
                return ContainerState.NOT_EXISTS

            # Parse state from output
            state_result = subprocess.run(
                [
                    get_docker_command(),
                    "inspect",
                    "-f",
                    "{{.State.Running}} {{.State.Paused}}",
                    self.config.container_name,
                ],
                capture_output=True,
                text=True,
                timeout=10,
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
        except (FileNotFoundError, subprocess.TimeoutExpired):
            # If Docker is not available, we can't check state
            return ContainerState.NOT_EXISTS

    def _create_container_internal(self) -> str:
        """Internal method to create a Docker container.

        Returns:
            Container ID

        Raises:
            RuntimeError: If container creation fails
        """
        env = os.environ.copy()
        env["MSYS_NO_PATHCONV"] = "1"

        cmd = [
            get_docker_command(),
            "create",
            # NOTE: Removed --rm flag to enable container reuse
            # Containers are now managed via database and garbage collection
            "--name",
            self.config.container_name,
            "-v",
            f"{self.config.project_root}:{self.config.mount_target}:ro",
        ]

        # Add optional output directory mount
        if self.config.output_dir is not None:
            cmd.extend(["-v", f"{self.config.output_dir}:/fastled/output:rw"])

        # Add anonymous volume for PlatformIO build cache
        # Cache persists during container lifetime, auto-cleaned when container is garbage collected
        cmd.extend(["-v", "/fastled/.pio/build_cache"])

        cmd.extend(
            [
                "--stop-timeout",
                str(self.config.stop_timeout),
                self._get_image_name(),
                "tail",
                "-f",
                "/dev/null",
            ]
        )

        result = subprocess.run(cmd, env=env, capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(f"Failed to create container: {result.stderr}")

        # Get the container ID from docker create output
        container_id = result.stdout.strip()

        # Start the container
        start_result = subprocess.run(
            [get_docker_command(), "start", container_id],
            capture_output=True,
            text=True,
            timeout=30,
        )
        if start_result.returncode != 0:
            raise RuntimeError(f"Failed to start container: {start_result.stderr}")

        return container_id

    def transition_to_running(self) -> bool:
        """Ensure container is in running state using database tracking.

        This method implements container reuse strategy:
        1. Calculate config_hash from current configuration
        2. Check for existing containers with matching config_hash
        3. Reuse container if found and owner PID is dead
        4. Run garbage collection before creating new containers
        5. Create fresh container with database tracking
        6. Register container ownership by current PID

        Returns:
            True if container is running, False on failure
        """
        try:
            # Calculate configuration hash for reuse detection
            config_hash = self.config.calculate_config_hash()

            # Check if we have a container with matching config_hash (container reuse)
            existing_by_hash = self._db.get_by_config_hash(config_hash)
            if existing_by_hash:
                # Container with matching config exists - check if we can reuse it
                from ci.docker_utils.container_db import (
                    docker_container_exists,
                    process_exists,
                )

                if not process_exists(existing_by_hash.owner_pid):
                    # Owner is dead - we can reuse this container
                    actual_container_id = docker_container_exists(
                        existing_by_hash.container_name
                    )
                    if actual_container_id:
                        print(
                            f"♻️  Reusing existing container: {existing_by_hash.container_name}"
                        )
                        print(f"   Config hash: {config_hash}")

                        # Update ownership in database
                        self._db.delete_by_id(existing_by_hash.container_id)
                        self._db.insert(
                            actual_container_id,
                            existing_by_hash.container_name,
                            self._get_image_name(),
                            os.getpid(),
                            config_hash,
                            self.config.platform_type,
                        )

                        # Start the container if it's stopped
                        start_result = subprocess.run(
                            [get_docker_command(), "start", actual_container_id],
                            capture_output=True,
                            text=True,
                            timeout=30,
                        )
                        if start_result.returncode != 0:
                            print(
                                "⚠️  Failed to start existing container, will create new one"
                            )
                        else:
                            self._container_id = actual_container_id
                            return True

            # No reusable container found - run garbage collection before creating new one
            from ci.docker_utils.container_db import garbage_collect_platform_containers

            cleaned = garbage_collect_platform_containers(
                self.config.platform_type, max_containers=2, db=self._db
            )
            if cleaned > 0:
                print(f"♻️  Garbage collected {cleaned} old container(s)")

            # Use prepare_container to handle all the lifecycle management
            container_id, was_cleaned = prepare_container(
                container_name=self.config.container_name,
                image_name=self._get_image_name(),
                create_container_fn=self._create_container_internal,
                db=self._db,
                config_hash=config_hash,
                platform_type=self.config.platform_type,
            )

            self._container_id = container_id

            if was_cleaned:
                print(
                    f"✓ Container cleaned and recreated: {self.config.container_name}"
                )
            else:
                print(f"✓ Fresh container created: {self.config.container_name}")
            print(f"   Config hash: {config_hash}")

            return True
        except RuntimeError as e:
            print(f"❌ {e}")
            return False
        except FileNotFoundError:
            print("❌ Docker is not available. Please ensure Docker is running.")
            return False
        except subprocess.TimeoutExpired:
            print("❌ Docker command timed out. Docker may not be responding.")
            return False

    def cleanup(self) -> None:
        """Release container ownership while keeping it tracked for reuse.

        This method keeps the container in the database tracking (for reuse on next build)
        and leaves it running for debugging. The next build will reuse this container
        if the owner process is dead, preserving the PlatformIO build cache.
        """
        # Don't remove from database - keep tracked for reuse
        # The container reuse logic (lines 352-396) will handle reusing
        # containers from dead processes on the next build
        if self._container_id:
            print(
                f"Container {self._container_id[:12]} released. "
                f"Container will be reused on next build to preserve build cache."
            )
            self._container_id = None

    def pause(self) -> bool:
        """Pause the container to free resources."""
        try:
            result = subprocess.run(
                [get_docker_command(), "pause", self.config.container_name],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=30,
            )
            return result.returncode == 0
        except (FileNotFoundError, subprocess.TimeoutExpired):
            # Silently fail if Docker is not available
            return False


class DockerEntrypointBuilder:
    """Builds Docker entrypoint scripts with proper syncing logic."""

    @staticmethod
    def build_sync_script() -> str:
        """Generate the rsync script for directory syncing.

        Key rsync flags explained:
        - --checksum: Compare file contents (MD5) instead of timestamps/size
                     Detects real changes even when timestamps are unreliable
        - --no-t:    Do NOT update destination timestamps when content is unchanged
                     Critical for preventing spurious rebuilds in PlatformIO
        - --delete:  Remove files in destination that don't exist in source

        Line ending protection:
        - .gitattributes enforces eol=lf for all text files
        - .editorconfig provides editor-level LF enforcement
        - rsync compares byte-for-byte, so CRLF vs LF would trigger sync
        - Run ci/docker_utils/diagnose_sync.sh inside container to debug sync issues
        """
        return """
# Sync host directories to container working directory if they exist
if command -v rsync &> /dev/null; then
    echo "Syncing directories from host..."

    # Define rsync exclude patterns for build artifacts
    # Note: .pio/build_cache is on an anonymous Docker volume (persists per container lifetime)
    RSYNC_EXCLUDES="--exclude=**/__pycache__ --exclude=.build/ --exclude=.pio/ --exclude=*.pyc"

    # rsync flags:
    # --checksum: Compare MD5 checksums instead of timestamps (detects real changes)
    # --no-t: Prevent timestamp updates on unchanged files (avoids spurious rebuilds)
    # --delete: Remove destination files that don't exist in source

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
            # --no-t prevents timestamp updates when content is unchanged (avoids spurious rebuilds).
            rsync -a --no-t --checksum /host/pyproject.toml /fastled/
            END=$(date +%s.%N)
            echo "$END - $START" | bc > "$TMPDIR/pyproject.time"
        ) &
    fi

    # Sync project-root library.json if present
    if [ -f "/host/library.json" ]; then
        echo "  → syncing library.json"
        (
            START=$(date +%s.%N)
            # Copy single file to /fastled root. No --delete needed for single-file.
            # --checksum ensures content-based update even with skewed timestamps.
            # --no-t prevents timestamp updates when content is unchanged (avoids spurious rebuilds).
            rsync -a --no-t --checksum /host/library.json /fastled/
            END=$(date +%s.%N)
            echo "$END - $START" | bc > "$TMPDIR/library.time"
        ) &
    fi

    # Run all syncs in parallel, each writing its timing to a temp file
    if [ -d "/host/src" ]; then
        echo "  → syncing src/..."
        (
            START=$(date +%s.%N)
            # --no-t prevents timestamp updates when content is unchanged (avoids spurious rebuilds)
            rsync -a --no-t --checksum --delete $RSYNC_EXCLUDES /host/src/ /fastled/src/
            END=$(date +%s.%N)
            echo "$END - $START" | bc > "$TMPDIR/src.time"
        ) &
    fi

    if [ -d "/host/examples" ]; then
        echo "  → syncing examples/..."
        (
            START=$(date +%s.%N)
            # --no-t prevents timestamp updates when content is unchanged (avoids spurious rebuilds)
            rsync -a --no-t --checksum --delete $RSYNC_EXCLUDES /host/examples/ /fastled/examples/
            END=$(date +%s.%N)
            echo "$END - $START" | bc > "$TMPDIR/examples.time"
        ) &
    fi

    if [ -d "/host/ci" ]; then
        echo "  → syncing ci/..."
        (
            START=$(date +%s.%N)
            # --no-t prevents timestamp updates when content is unchanged (avoids spurious rebuilds)
            rsync -a --no-t --checksum --delete $RSYNC_EXCLUDES /host/ci/ /fastled/ci/
            END=$(date +%s.%N)
            echo "$END - $START" | bc > "$TMPDIR/ci.time"
        ) &
    fi

    if [ -d "/host/lint_plugins" ]; then
        echo "  → syncing lint_plugins/..."
        (
            START=$(date +%s.%N)
            # --no-t prevents timestamp updates when content is unchanged (avoids spurious rebuilds)
            rsync -a --no-t --checksum --delete $RSYNC_EXCLUDES /host/lint_plugins/ /fastled/lint_plugins/
            END=$(date +%s.%N)
            echo "$END - $START" | bc > "$TMPDIR/lint_plugins.time"
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

    if [ -f "$TMPDIR/library.time" ]; then
        printf "  ✓ library.json synced in %.3fs\\n" $(cat "$TMPDIR/library.time")
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
    if [ -f "$TMPDIR/lint_plugins.time" ]; then
        printf "  ✓ lint_plugins/ synced in %.3fs\\n" $(cat "$TMPDIR/lint_plugins.time")
    fi

    printf "  ─────────────────────────────\\n"
    printf "  Total sync time: %.3fs\\n" $SYNC_TIME

    # Cleanup temp files
    rm -rf "$TMPDIR"

    echo ""
    echo "Directory sync complete"

    # Verify critical files and recover from rsync failures
    echo "Verifying critical files..."
    RECOVERY_NEEDED=0
    CRITICAL_FILES=(
        "src/FastLED.h"
    )

    # Create diagnostic log directory
    DIAG_DIR="/fastled/.docker_diag"
    mkdir -p "$DIAG_DIR"
    DIAG_LOG="$DIAG_DIR/rsync_recovery.log"

    echo "=== RSYNC Recovery Diagnostics $(date) ===" > "$DIAG_LOG"
    echo "Container: $(hostname)" >> "$DIAG_LOG"
    echo "" >> "$DIAG_LOG"

    for file in "${CRITICAL_FILES[@]}"; do
        echo "Checking: $file" >> "$DIAG_LOG"

        # Check if file exists in /fastled
        if [ ! -f "/fastled/$file" ]; then
            echo "  ✗ Missing: $file"
            echo "  STATUS: MISSING in /fastled/$file" >> "$DIAG_LOG"

            # Check if source file exists in /host
            if [ -f "/host/$file" ]; then
                echo "  SOURCE: EXISTS in /host/$file" >> "$DIAG_LOG"
                ls -la "/host/$file" >> "$DIAG_LOG" 2>&1

                echo "    → Recovering from /host/$file..."
                echo "  RECOVERY: Attempting rsync..." >> "$DIAG_LOG"

                # Attempt recovery with verbose logging
                rsync -avh "/host/$file" "/fastled/$file" >> "$DIAG_LOG" 2>&1
                RSYNC_EXIT=$?
                echo "  RSYNC_EXIT_CODE: $RSYNC_EXIT" >> "$DIAG_LOG"

                # Verify recovery
                if [ -f "/fastled/$file" ]; then
                    echo "    ✓ Recovered successfully"
                    echo "  RECOVERY: SUCCESS" >> "$DIAG_LOG"
                    ls -la "/fastled/$file" >> "$DIAG_LOG" 2>&1
                else
                    echo "    ✗ Recovery failed!"
                    echo "  RECOVERY: FAILED - file still missing after rsync" >> "$DIAG_LOG"
                    RECOVERY_NEEDED=1
                fi
            else
                echo "    ✗ Source file missing in /host/$file"
                echo "  SOURCE: MISSING in /host/$file" >> "$DIAG_LOG"
                ls -la "/host/src/" >> "$DIAG_LOG" 2>&1
                RECOVERY_NEEDED=1
            fi
        else
            echo "  ✓ $file exists"
            echo "  STATUS: OK" >> "$DIAG_LOG"
            ls -la "/fastled/$file" >> "$DIAG_LOG" 2>&1
        fi
        echo "" >> "$DIAG_LOG"
    done

    # Log filesystem and mount information
    echo "=== Filesystem Info ===" >> "$DIAG_LOG"
    df -h /fastled /host >> "$DIAG_LOG" 2>&1
    echo "" >> "$DIAG_LOG"
    mount | grep -E "(fastled|host)" >> "$DIAG_LOG" 2>&1
    echo "" >> "$DIAG_LOG"

    if [ $RECOVERY_NEEDED -eq 1 ]; then
        echo ""
        echo "ERROR: Critical file recovery failed. Container may be unusable."
        echo "This suggests an rsync or filesystem issue."
        echo "Diagnostic log saved to: $DIAG_LOG"
        cat "$DIAG_LOG"
    else
        echo "  ✓ All critical files verified"
        echo "Diagnostic log saved to: $DIAG_LOG"
    fi
    echo ""

    # Clean Python bytecode cache to ensure latest code is used
    echo "Cleaning Python cache..."
    find /fastled/ci -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
    find /fastled/ci -type f -name "*.pyc" -delete 2>/dev/null || true

    # TODO: Remove this from the docker image.
    # Remove stale /tmp/ci directory to prevent import conflicts
    # The Dockerfile copies ci/ to /tmp/ci for build-time use, but at runtime
    # we want to use the synced version at /fastled/ci instead
    if [ -d "/tmp/ci" ]; then
        rm -rf /tmp/ci
    fi

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
        self._actual_image_name: Optional[str] = (
            None  # Tracks the actual image name found
        )

    def _get_image_name(self) -> str:
        """Get the actual image name to use (may be different from config if local)."""
        return self._actual_image_name or self.config.image_name

    def ensure_image_exists(self, build_if_missing: bool = False) -> bool:
        """Ensure Docker image exists, optionally building it.

        Attempts to:
        1. Ensure Docker is running (auto-start if needed)
        2. Use existing local image (with or without registry prefix)
        3. Pull from Docker registry (if not building)
        4. Build locally (if build_if_missing=True)
        """
        # First, ensure Docker is running
        from ci.util.docker_command import is_docker_available

        if not is_docker_available():
            print("Docker is not running. Attempting to start Docker...")
            print()

            from ci.util.docker_helper import attempt_start_docker

            success, message = attempt_start_docker()

            if success:
                print(f"✓ {message}")
                print()
            else:
                print(f"❌ {message}")
                print()
                print("Please ensure Docker is installed and running, then try again.")
                print("Docker Desktop: https://www.docker.com/products/docker-desktop")
                return False

        try:
            result = subprocess.run(
                [get_docker_command(), "image", "inspect", self.config.image_name],
                capture_output=True,
                text=True,
                timeout=10,
            )

            if result.returncode == 0:
                print(f"✓ Using existing Docker image: {self.config.image_name}")
                self._actual_image_name = self.config.image_name
                return True  # Image exists locally

            # Image not found - try without registry prefix (e.g., niteris/fastled-compiler-avr-uno -> fastled-compiler-avr-uno)
            if "/" in self.config.image_name:
                image_without_registry = self.config.image_name.split("/", 1)[1]
                result = subprocess.run(
                    [get_docker_command(), "image", "inspect", image_without_registry],
                    capture_output=True,
                    text=True,
                    timeout=10,
                )
                if result.returncode == 0:
                    print(
                        f"✓ Using existing Docker image (local): {image_without_registry}"
                    )
                    self._actual_image_name = image_without_registry
                    return True

            # Image not found locally
            print(f"Docker image {self.config.image_name} not found locally.")

            # If build_if_missing is True, skip pull and go straight to build
            if build_if_missing:
                print("Skipping pull (--build specified), building locally...")
                print()
            else:
                # Try to pull from registry with streaming output
                print("Attempting to pull from registry...")
                print()

                sys.stdout.flush()
                sys.stderr.flush()
                pull_result = subprocess.run(
                    [get_docker_command(), "pull", self.config.image_name],
                    timeout=600,  # 10 minute timeout for pull
                )

                if pull_result.returncode == 0:
                    print(
                        f"✓ Successfully pulled Docker image: {self.config.image_name}"
                    )
                    return True

            # Pull failed or was skipped - check if we should build
            if not build_if_missing:
                print("❌ Could not pull Docker image from registry.")
                print(f"   Image: {self.config.image_name}")
                print("")
                print("Ensure the image exists on Docker Hub:")
                print("  https://hub.docker.com/r/niteris")
                print("")
                print("Or build locally with:")
                print(
                    f"  bash compile --docker --build {self.config.board_name} <example>"
                )
                print("")
                return False

            # Build the image locally
            print(f"Building Docker image: {self.config.image_name}")
            print("This may take 15-30 minutes depending on the platform...")

            # Parse image name to extract base name (without tag)
            image_parts = self.config.image_name.split(":")
            image_base = image_parts[0]
            image_tag = image_parts[1] if len(image_parts) > 1 else "latest"

            build_cmd = [
                sys.executable,
                "ci/build_docker_image_pio.py",
                "--platform",
                self.config.board_name,
                "--image-name",
                image_base,
                "--tag",
                image_tag,
            ]

            result = subprocess.run(build_cmd)
            return result.returncode == 0
        except (FileNotFoundError, subprocess.TimeoutExpired):
            # Docker command not found or timed out - likely Docker is not running
            return self._handle_docker_not_found()
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"❌ Unexpected error checking Docker image: {e}")
            return False

    def _handle_docker_not_found(self) -> bool:
        """Handle the case where Docker is not installed or running."""
        from ci.util.docker_helper import attempt_start_docker

        print("❌ Docker is not running or not installed")
        print("")
        print("Attempting to start Docker...")
        print("")

        success, message = attempt_start_docker()

        if success:
            print(f"✓ {message}")
            print("")
            return True
        else:
            print(f"ℹ  {message}")
            print("")
            print("Please ensure Docker is installed and running, then try again.")
            print("Docker Desktop: https://www.docker.com/products/docker-desktop")
            return False

    def run_compilation(
        self, compile_cmd: str, stream_output: bool = True
    ) -> "subprocess.CompletedProcess[str]":
        """Execute compilation inside container with proper setup."""
        try:
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
                get_docker_command(),
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
        except RuntimeError as e:
            print(f"❌ {e}")
            raise
        except FileNotFoundError:
            print("❌ Docker is not available. Please ensure Docker is running.")
            raise RuntimeError("Docker not available") from None

    def _stream_execution(self, cmd: list[str]) -> "subprocess.CompletedProcess[str]":
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
        print("Copying build artifacts from container to host...")

        copy_cmd = [
            get_docker_command(),
            "cp",
            f"{self.config.container_name}:{container_path}/.",
            str(host_path),
        ]

        result = subprocess.run(copy_cmd, capture_output=True, text=True)

        if result.returncode == 0:
            print(f"✅ Build artifacts copied to {host_path}")
            return True
        else:
            print("⚠️  Warning: Could not copy artifacts from container")
            if result.stderr:
                print(f"   Error: {result.stderr}")
            return False
