from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""
SQLite-backed Lock Database for Inter-Process Reader-Writer Locking.

Provides centralized lock management using SQLite WAL mode for:
- True reader-writer lock semantics (multiple concurrent readers, exclusive writer)
- Atomic lock operations via SQLite transactions
- PID-based stale detection and lock breaking
- Re-entrancy: same PID re-acquiring succeeds
- Read-to-write lock upgrade for same PID
"""

import logging
import os
import sqlite3
import time
from pathlib import Path
from typing import Any

from ci.util.file_lock_rw_util import is_process_alive


logger = logging.getLogger(__name__)


class LockDatabase:
    """SQLite-backed lock database with reader-writer lock semantics.

    Uses WAL journal mode for concurrent read access and BEGIN IMMEDIATE
    transactions for atomic lock acquisition.
    """

    def __init__(self, db_path: Path) -> None:
        self.db_path = db_path
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        self._init_db()

    def _get_connection(self) -> sqlite3.Connection:
        """Get a new database connection with WAL mode and busy timeout."""
        conn = sqlite3.connect(str(self.db_path), timeout=10.0)
        conn.execute("PRAGMA journal_mode=WAL")
        conn.execute("PRAGMA busy_timeout=5000")
        return conn

    def _init_db(self) -> None:
        """Initialize database schema."""
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                """
                CREATE TABLE IF NOT EXISTS lock_holders (
                    lock_name TEXT NOT NULL,
                    owner_pid INTEGER NOT NULL,
                    lock_mode TEXT NOT NULL CHECK(lock_mode IN ('read', 'write')),
                    operation TEXT NOT NULL DEFAULT 'lock',
                    hostname TEXT NOT NULL DEFAULT '',
                    acquired_at REAL NOT NULL,
                    PRIMARY KEY (lock_name, owner_pid)
                )
                """
            )
            cursor.execute(
                "CREATE INDEX IF NOT EXISTS idx_lock_name ON lock_holders(lock_name)"
            )
            conn.commit()
        finally:
            conn.close()

    def try_acquire(
        self,
        lock_name: str,
        owner_pid: int,
        hostname: str,
        operation: str,
        mode: str = "write",
    ) -> bool:
        """Atomic lock acquisition via BEGIN IMMEDIATE.

        Read lock: succeeds if no OTHER PID holds write lock.
        Write lock: succeeds if no OTHER PID holds any lock.
        Re-entrancy: same PID always succeeds (upgrade read->write if sole holder).

        Args:
            lock_name: Name identifying the lock
            owner_pid: PID of the process acquiring the lock
            hostname: Hostname of the machine
            operation: Description of the operation
            mode: Lock mode - 'read' or 'write'

        Returns:
            True if lock was acquired, False if blocked
        """
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute("BEGIN IMMEDIATE")

            cursor.execute(
                "SELECT owner_pid, lock_mode FROM lock_holders WHERE lock_name = ?",
                (lock_name,),
            )
            holders = cursor.fetchall()

            my_rows = [h for h in holders if h[0] == owner_pid]
            other_rows = [h for h in holders if h[0] != owner_pid]

            if mode == "read":
                # Blocked by other writer
                if any(h[1] == "write" for h in other_rows):
                    conn.rollback()
                    return False
                # Already hold read or write - re-entrant
                if my_rows:
                    conn.commit()
                    return True
                # Insert new read lock
                cursor.execute(
                    "INSERT INTO lock_holders (lock_name, owner_pid, lock_mode, operation, hostname, acquired_at) "
                    "VALUES (?, ?, 'read', ?, ?, ?)",
                    (lock_name, owner_pid, operation, hostname, time.time()),
                )
                conn.commit()
                return True

            if mode == "write":
                # Blocked by other holders
                if other_rows:
                    conn.rollback()
                    return False
                # Already hold write - re-entrant
                if my_rows and my_rows[0][1] == "write":
                    conn.commit()
                    return True
                # Upgrade read->write or fresh write
                cursor.execute(
                    "INSERT OR REPLACE INTO lock_holders (lock_name, owner_pid, lock_mode, operation, hostname, acquired_at) "
                    "VALUES (?, ?, 'write', ?, ?, ?)",
                    (lock_name, owner_pid, operation, hostname, time.time()),
                )
                conn.commit()
                return True

            conn.rollback()
            return False

        except KeyboardInterrupt:
            try:
                conn.rollback()
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception:
                pass
            handle_keyboard_interrupt_properly()
            raise
        except sqlite3.OperationalError as e:
            # Database is locked by another process
            try:
                conn.rollback()
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception:
                pass
            logger.debug(f"Lock acquisition failed due to DB contention: {e}")
            return False
        except Exception:
            try:
                conn.rollback()
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception:
                pass
            raise
        finally:
            conn.close()

    def release(self, lock_name: str, owner_pid: int) -> bool:
        """Release lock for a specific PID.

        Args:
            lock_name: Name identifying the lock
            owner_pid: PID of the process releasing the lock

        Returns:
            True if a lock was released, False if no lock was held
        """
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                "DELETE FROM lock_holders WHERE lock_name = ? AND owner_pid = ?",
                (lock_name, owner_pid),
            )
            deleted = cursor.rowcount > 0
            conn.commit()
            return deleted
        finally:
            conn.close()

    def is_held(self, lock_name: str) -> bool:
        """Check if any holder exists for this lock.

        Args:
            lock_name: Name identifying the lock

        Returns:
            True if any process holds this lock
        """
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                "SELECT COUNT(*) FROM lock_holders WHERE lock_name = ?",
                (lock_name,),
            )
            row = cursor.fetchone()
            return row is not None and row[0] > 0
        finally:
            conn.close()

    def get_lock_info(self, lock_name: str) -> list[dict[str, Any]]:
        """Get all holders for a lock with metadata.

        Args:
            lock_name: Name identifying the lock

        Returns:
            List of dicts with holder information
        """
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                "SELECT lock_name, owner_pid, lock_mode, operation, hostname, acquired_at "
                "FROM lock_holders WHERE lock_name = ?",
                (lock_name,),
            )
            rows = cursor.fetchall()
            return [
                {
                    "lock_name": r[0],
                    "owner_pid": r[1],
                    "lock_mode": r[2],
                    "operation": r[3],
                    "hostname": r[4],
                    "acquired_at": r[5],
                }
                for r in rows
            ]
        finally:
            conn.close()

    def is_lock_stale(self, lock_name: str) -> bool:
        """Check if ALL holders have dead PIDs.

        Args:
            lock_name: Name identifying the lock

        Returns:
            True if lock is stale (all holders dead), False otherwise
        """
        holders = self.get_lock_info(lock_name)
        if not holders:
            return False

        for holder in holders:
            pid = holder["owner_pid"]
            if is_process_alive(pid):
                return False

        return True

    def break_stale_lock(self, lock_name: str) -> bool:
        """Remove rows where owner PID is dead.

        Args:
            lock_name: Name identifying the lock

        Returns:
            True if any stale locks were removed
        """
        holders = self.get_lock_info(lock_name)
        if not holders:
            return False

        removed = False
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            for holder in holders:
                pid = holder["owner_pid"]
                if not is_process_alive(pid):
                    cursor.execute(
                        "DELETE FROM lock_holders WHERE lock_name = ? AND owner_pid = ?",
                        (lock_name, pid),
                    )
                    if cursor.rowcount > 0:
                        removed = True
                        logger.info(f"Broke stale lock: {lock_name} (dead PID {pid})")
            conn.commit()
        finally:
            conn.close()

        return removed

    def force_break(self, lock_name: str) -> bool:
        """Unconditionally remove all holders for a lock.

        Args:
            lock_name: Name identifying the lock

        Returns:
            True if any locks were removed
        """
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                "DELETE FROM lock_holders WHERE lock_name = ?",
                (lock_name,),
            )
            deleted = cursor.rowcount > 0
            conn.commit()
            return deleted
        finally:
            conn.close()

    def list_all_locks(self) -> list[dict[str, Any]]:
        """List all lock records.

        Returns:
            List of dicts with all lock holder information
        """
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                "SELECT lock_name, owner_pid, lock_mode, operation, hostname, acquired_at "
                "FROM lock_holders ORDER BY lock_name, acquired_at"
            )
            rows = cursor.fetchall()
            return [
                {
                    "lock_name": r[0],
                    "owner_pid": r[1],
                    "lock_mode": r[2],
                    "operation": r[3],
                    "hostname": r[4],
                    "acquired_at": r[5],
                }
                for r in rows
            ]
        finally:
            conn.close()

    def cleanup_stale_locks(self) -> int:
        """Remove all lock rows where owner PID is dead.

        Returns:
            Number of stale lock rows removed
        """
        all_locks = self.list_all_locks()
        if not all_locks:
            return 0

        removed_count = 0
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            for lock in all_locks:
                pid = lock["owner_pid"]
                if not is_process_alive(pid):
                    cursor.execute(
                        "DELETE FROM lock_holders WHERE lock_name = ? AND owner_pid = ?",
                        (lock["lock_name"], pid),
                    )
                    if cursor.rowcount > 0:
                        removed_count += 1
                        logger.info(
                            f"Cleaned stale lock: {lock['lock_name']} (dead PID {pid})"
                        )
            conn.commit()
        finally:
            conn.close()

        return removed_count

    def force_break_all(self) -> int:
        """Unconditionally remove all lock records.

        Returns:
            Number of lock rows removed
        """
        conn = self._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM lock_holders")
            row = cursor.fetchone()
            count = row[0] if row else 0
            cursor.execute("DELETE FROM lock_holders")
            conn.commit()
            return count
        finally:
            conn.close()


# Module-level cache for LockDatabase instances
_db_cache: dict[str, LockDatabase] = {}


def get_lock_database(lock_path: Path | None = None) -> LockDatabase:
    """Resolve which DB file to use based on the lock path.

    - Paths under ~/.fastled/ or ~/.platformio/ -> ~/.fastled/locks.db
    - Everything else -> <project_root>/.cache/locks.db

    Env override: FASTLED_LOCK_DB_PATH (for test isolation)

    Args:
        lock_path: Optional path hint for choosing DB location

    Returns:
        LockDatabase instance (cached per DB path)
    """
    # Check env override first
    env_path = os.environ.get("FASTLED_LOCK_DB_PATH")
    if env_path:
        db_path_str = env_path
    else:
        home = Path.home()
        fastled_dir = home / ".fastled"
        platformio_dir = home / ".platformio"

        use_global = False
        if lock_path is not None:
            try:
                lock_str = str(lock_path.resolve())
                if str(fastled_dir) in lock_str or str(platformio_dir) in lock_str:
                    use_global = True
            except (OSError, ValueError):
                pass

        if use_global:
            db_path = fastled_dir / "locks.db"
        else:
            project_root = Path(__file__).parent.parent.parent
            db_path = project_root / ".cache" / "locks.db"

        db_path_str = str(db_path)

    if db_path_str not in _db_cache:
        _db_cache[db_path_str] = LockDatabase(Path(db_path_str))

    return _db_cache[db_path_str]
