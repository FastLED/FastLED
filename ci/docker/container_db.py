"""Docker container lifecycle database management.

This module provides a robust container lifecycle management system that:
- Keeps containers alive after stopping (for debugging)
- Deletes and recreates containers before each run
- Prevents multiple processes from using the same container
- Tracks container ownership by PID
- Cleans up orphaned containers when parent process dies

Database location: ~/.fastled/docker-compiler/compiler.db
"""

import os
import sqlite3
import subprocess
import time
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from ci.util.docker_command import get_docker_command


@dataclass
class ContainerRecord:
    """Container tracking record."""

    container_id: str
    container_name: str
    image_name: str
    owner_pid: int
    created_at: int


class ContainerDatabase:
    """Manages container lifecycle tracking database."""

    def __init__(self, db_path: Optional[Path] = None):
        """Initialize database connection.

        Args:
            db_path: Path to database file. Defaults to ~/.fastled/docker-compiler/compiler.db
        """
        if db_path is None:
            home = Path.home()
            db_dir = home / ".fastled" / "docker-compiler"
            db_dir.mkdir(parents=True, exist_ok=True)
            db_path = db_dir / "compiler.db"

        self.db_path = db_path
        self._init_db()

    def _init_db(self) -> None:
        """Initialize database schema."""
        conn = sqlite3.connect(self.db_path)
        try:
            cursor = conn.cursor()
            cursor.execute(
                """
                CREATE TABLE IF NOT EXISTS containers (
                    container_id TEXT PRIMARY KEY,
                    container_name TEXT UNIQUE NOT NULL,
                    image_name TEXT NOT NULL,
                    owner_pid INTEGER NOT NULL,
                    created_at INTEGER NOT NULL
                )
                """
            )
            cursor.execute(
                "CREATE INDEX IF NOT EXISTS idx_owner_pid ON containers(owner_pid)"
            )
            cursor.execute(
                "CREATE INDEX IF NOT EXISTS idx_container_name ON containers(container_name)"
            )
            conn.commit()
        finally:
            conn.close()

    def _get_connection(self) -> sqlite3.Connection:
        """Get a new database connection."""
        return sqlite3.connect(self.db_path, timeout=10.0)

    def get_by_name(self, container_name: str) -> Optional[ContainerRecord]:
        """Get container record by name.

        Args:
            container_name: Container name to look up

        Returns:
            ContainerRecord if found, None otherwise
        """
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                "SELECT container_id, container_name, image_name, owner_pid, created_at FROM containers WHERE container_name = ?",
                (container_name,),
            )
            row = cursor.fetchone()
            if row:
                return ContainerRecord(
                    container_id=row[0],
                    container_name=row[1],
                    image_name=row[2],
                    owner_pid=row[3],
                    created_at=row[4],
                )
            return None
        finally:
            conn.close()

    def get_by_id(self, container_id: str) -> Optional[ContainerRecord]:
        """Get container record by ID.

        Args:
            container_id: Container ID to look up

        Returns:
            ContainerRecord if found, None otherwise
        """
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                "SELECT container_id, container_name, image_name, owner_pid, created_at FROM containers WHERE container_id = ?",
                (container_id,),
            )
            row = cursor.fetchone()
            if row:
                return ContainerRecord(
                    container_id=row[0],
                    container_name=row[1],
                    image_name=row[2],
                    owner_pid=row[3],
                    created_at=row[4],
                )
            return None
        finally:
            conn.close()

    def insert(
        self,
        container_id: str,
        container_name: str,
        image_name: str,
        owner_pid: int,
    ) -> None:
        """Insert a new container record.

        Args:
            container_id: Docker container ID
            container_name: Container name
            image_name: Docker image name
            owner_pid: Process ID of owner
        """
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                """
                INSERT INTO containers (container_id, container_name, image_name, owner_pid, created_at)
                VALUES (?, ?, ?, ?, ?)
                """,
                (container_id, container_name, image_name, owner_pid, int(time.time())),
            )
            conn.commit()
        finally:
            conn.close()

    def delete_by_id(self, container_id: str) -> None:
        """Delete container record by ID.

        Args:
            container_id: Container ID to delete
        """
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                "DELETE FROM containers WHERE container_id = ?", (container_id,)
            )
            conn.commit()
        finally:
            conn.close()

    def delete_by_name(self, container_name: str) -> None:
        """Delete container record by name.

        Args:
            container_name: Container name to delete
        """
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                "DELETE FROM containers WHERE container_name = ?", (container_name,)
            )
            conn.commit()
        finally:
            conn.close()

    def get_all(self) -> list[ContainerRecord]:
        """Get all container records.

        Returns:
            List of ContainerRecord objects
        """
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                "SELECT container_id, container_name, image_name, owner_pid, created_at FROM containers"
            )
            rows = cursor.fetchall()
            return [
                ContainerRecord(
                    container_id=row[0],
                    container_name=row[1],
                    image_name=row[2],
                    owner_pid=row[3],
                    created_at=row[4],
                )
                for row in rows
            ]
        finally:
            conn.close()


def process_exists(pid: int) -> bool:
    """Check if a process with given PID is running.

    Args:
        pid: Process ID to check

    Returns:
        True if process exists, False otherwise
    """
    try:
        # os.kill with signal 0 doesn't actually kill the process,
        # it just checks if we can send a signal to it
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        # Process exists but we don't have permission to signal it
        return True
    except Exception:
        return False


def docker_container_exists(container_name: str) -> Optional[str]:
    """Check if a Docker container exists (by name).

    Args:
        container_name: Container name to check

    Returns:
        Container ID if exists, None otherwise
    """
    try:
        result = subprocess.run(
            [
                get_docker_command(),
                "ps",
                "-a",
                "--filter",
                f"name=^{container_name}$",
                "--format",
                "{{.ID}}",
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout.strip()
        return None
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return None


def docker_stop_container(container_id: str) -> bool:
    """Stop a Docker container.

    Args:
        container_id: Container ID to stop

    Returns:
        True if successful, False otherwise
    """
    try:
        result = subprocess.run(
            [get_docker_command(), "stop", container_id],
            capture_output=True,
            text=True,
            timeout=30,
        )
        return result.returncode == 0
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return False


def docker_remove_container(container_id: str) -> bool:
    """Remove a Docker container.

    Args:
        container_id: Container ID to remove

    Returns:
        True if successful, False otherwise
    """
    try:
        result = subprocess.run(
            [get_docker_command(), "rm", container_id],
            capture_output=True,
            text=True,
            timeout=30,
        )
        return result.returncode == 0
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return False


def docker_force_remove_container(
    container_id: str, max_retries: int = 3
) -> tuple[bool, str]:
    """Force remove a Docker container with retry logic.

    Uses 'docker rm -f' to forcefully remove a container, retrying with
    exponential backoff if it fails due to timing issues.

    Args:
        container_id: Container ID to remove
        max_retries: Maximum number of retry attempts (default: 3)

    Returns:
        Tuple of (success: bool, error_message: str)
    """
    import time

    for attempt in range(max_retries):
        try:
            result = subprocess.run(
                [get_docker_command(), "rm", "-f", container_id],
                capture_output=True,
                text=True,
                timeout=30,
            )
            if result.returncode == 0:
                return True, ""

            # Failed - capture error message
            error_msg = (
                result.stderr.strip() if result.stderr else result.stdout.strip()
            )

            # If this is the last attempt, return the error
            if attempt == max_retries - 1:
                return False, error_msg

            # Retry with exponential backoff: 0.5s, 1s, 2s
            wait_time = 0.5 * (2**attempt)
            time.sleep(wait_time)

        except FileNotFoundError:
            return False, "Docker command not found"
        except subprocess.TimeoutExpired:
            if attempt == max_retries - 1:
                return False, "Docker command timed out after 30 seconds"
            # Retry on timeout
            time.sleep(0.5 * (2**attempt))

    return False, "Max retries exceeded"


def prepare_container(
    container_name: str,
    image_name: str,
    create_container_fn: Callable[[], str],
    db: Optional[ContainerDatabase] = None,
) -> tuple[str, bool]:
    """Prepare a container for use, ensuring clean state.

    This function implements the pre-run phase:
    1. Checks if container name already exists in DB
    2. If owner process is alive, raises error (container in use)
    3. If owner process is dead, cleans up orphaned container
    4. Removes any untracked Docker containers with same name
    5. Creates fresh container using provided function
    6. Registers container in database

    Args:
        container_name: Name for the container
        image_name: Docker image name to use
        create_container_fn: Callable that creates and starts container, returns container ID
        db: Database instance (creates new if None)

    Returns:
        Tuple of (container_id, was_cleaned) where was_cleaned indicates if cleanup occurred

    Raises:
        RuntimeError: If container is already in use by another process
    """
    if db is None:
        db = ContainerDatabase()

    current_pid = os.getpid()
    was_cleaned = False

    # Check if container name already exists in DB
    existing = db.get_by_name(container_name)

    if existing:
        # Check if owner process is still alive
        if process_exists(existing.owner_pid):
            # Owner is alive - cannot proceed
            raise RuntimeError(
                f"Container '{container_name}' is already in use by PID {existing.owner_pid}. "
                f"Stop that process first or wait for it to complete.\n\n"
                f"To force cleanup:\n"
                f"  kill {existing.owner_pid}\n"
                f"  OR\n"
                f"  uv run python -m ci.docker.cleanup_orphans"
            )
        else:
            # Owner is dead - orphaned container, clean it up
            print(
                f"Cleaning up orphaned container from dead process {existing.owner_pid}"
            )
            success, error_msg = docker_force_remove_container(existing.container_id)
            if not success:
                print(f"⚠️  Warning: Failed to remove orphaned container: {error_msg}")
                # Continue anyway - database cleanup is more important
            db.delete_by_id(existing.container_id)
            was_cleaned = True

    # Check if Docker container actually exists (outside DB tracking)
    actual_container_id = docker_container_exists(container_name)
    if actual_container_id:
        print(f"Found untracked container '{container_name}', removing it")
        success, error_msg = docker_force_remove_container(actual_container_id)
        if not success:
            raise RuntimeError(
                f"Failed to remove untracked container '{container_name}': {error_msg}"
            )
        was_cleaned = True

    # Create fresh container using provided function
    container_id = create_container_fn()

    # Register in DB
    db.insert(container_id, container_name, image_name, current_pid)

    return container_id, was_cleaned


def cleanup_container(
    container_id: str, db: Optional[ContainerDatabase] = None
) -> None:
    """Clean up container registration but keep container alive.

    This function implements the cleanup phase:
    - Removes container from database
    - Leaves container running for debugging

    Args:
        container_id: Container ID to clean up
        db: Database instance (creates new if None)
    """
    if db is None:
        db = ContainerDatabase()

    # Remove from DB (container stays alive)
    db.delete_by_id(container_id)

    print(
        f"Container {container_id} released. Container still running for debugging. "
        f"Use 'docker stop {container_id}' to stop it."
    )


def cleanup_orphaned_containers(db: Optional[ContainerDatabase] = None) -> int:
    """Clean up containers from dead processes.

    This function implements the orphan cleanup phase:
    - Scans all tracked containers
    - Identifies containers whose owner PIDs are dead
    - Stops and removes orphaned containers
    - Cleans up database records

    Args:
        db: Database instance (creates new if None)

    Returns:
        Number of containers cleaned up
    """
    if db is None:
        db = ContainerDatabase()

    all_tracked = db.get_all()
    cleaned_count = 0

    for container in all_tracked:
        if not process_exists(container.owner_pid):
            print(
                f"Found orphaned container {container.container_id} from dead PID {container.owner_pid}"
            )

            # Force remove the container with retry logic
            success, error_msg = docker_force_remove_container(container.container_id)
            if not success:
                print(
                    f"⚠️  Warning: Failed to remove container {container.container_id}: {error_msg}"
                )
                print(f"   Database record will still be cleaned up")

            # Remove from DB (even if container removal failed)
            db.delete_by_id(container.container_id)
            cleaned_count += 1

    return cleaned_count
