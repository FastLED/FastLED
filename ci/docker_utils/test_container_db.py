from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""Basic tests for container database operations.

This script performs basic validation of the container database:
1. Database initialization
2. Record insertion and retrieval
3. Cleanup operations
4. Orphan detection
"""

import os
import sqlite3
import tempfile
from pathlib import Path

from ci.docker_utils.container_db import (
    ContainerDatabase,
    process_exists,
)


def test_database_initialization():
    """Test database creation and schema initialization."""
    print("Testing database initialization...")

    with tempfile.TemporaryDirectory() as tmpdir:
        db_path = Path(tmpdir) / "test.db"
        ContainerDatabase(db_path)

        # Verify database file exists
        assert db_path.exists(), "Database file was not created"

        # Verify tables exist
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        cursor.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name='containers'"
        )
        result = cursor.fetchone()
        conn.close()

        assert result is not None, "containers table was not created"

    print("✓ Database initialization test passed")


def test_record_operations():
    """Test basic CRUD operations."""
    print("Testing record operations...")

    with tempfile.TemporaryDirectory() as tmpdir:
        db_path = Path(tmpdir) / "test.db"
        db = ContainerDatabase(db_path)

        # Insert a record
        container_id = "test_container_123"
        container_name = "test-container"
        image_name = "test-image:latest"
        owner_pid = os.getpid()

        db.insert(container_id, container_name, image_name, owner_pid)

        # Retrieve by name
        record = db.get_by_name(container_name)
        assert record is not None, "Failed to retrieve record by name"
        assert record.container_id == container_id
        assert record.container_name == container_name
        assert record.image_name == image_name
        assert record.owner_pid == owner_pid

        # Retrieve by ID
        record = db.get_by_id(container_id)
        assert record is not None, "Failed to retrieve record by ID"
        assert record.container_id == container_id

        # Delete by ID
        db.delete_by_id(container_id)
        record = db.get_by_id(container_id)
        assert record is None, "Record was not deleted"

    print("✓ Record operations test passed")


def test_orphan_detection():
    """Test orphan detection with dead PIDs."""
    print("Testing orphan detection...")

    with tempfile.TemporaryDirectory() as tmpdir:
        db_path = Path(tmpdir) / "test.db"
        db = ContainerDatabase(db_path)

        # Insert a record with a dead PID (99999 is unlikely to exist)
        dead_pid = 99999
        container_id = "orphan_container_456"
        container_name = "orphan-container"
        image_name = "test-image:latest"

        db.insert(container_id, container_name, image_name, dead_pid)

        # Verify process_exists returns False for dead PID
        assert not process_exists(dead_pid), "Dead PID incorrectly detected as alive"

        # Get all records - should include our orphan
        all_records = db.get_all()
        assert len(all_records) == 1, "Expected 1 record"
        assert all_records[0].owner_pid == dead_pid

        # Note: We can't test actual cleanup_orphaned_containers here because
        # it tries to interact with Docker. Just verify the logic detects orphans.

    print("✓ Orphan detection test passed")


def test_process_exists():
    """Test process existence checking."""
    print("Testing process existence checking...")

    # Current process should exist
    current_pid = os.getpid()
    assert process_exists(current_pid), "Current process not detected as alive"

    # PID 1 should exist on Unix systems (init/systemd)
    # On Windows, we'll skip this check
    if os.name != "nt":
        assert process_exists(1), "PID 1 not detected as alive"

    # Very high PID should not exist
    assert not process_exists(999999), "Non-existent PID incorrectly detected as alive"

    print("✓ Process existence test passed")


def main():
    """Run all tests."""
    print("Running container database tests...")
    print()

    try:
        test_database_initialization()
        test_record_operations()
        test_orphan_detection()
        test_process_exists()

        print()
        print("✅ All tests passed!")
        return 0
    except AssertionError as e:
        print()
        print(f"❌ Test failed: {e}")
        return 1
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print()
        print(f"❌ Unexpected error: {e}")
        import traceback

        traceback.print_exc()
        return 1


if __name__ == "__main__":
    import sys

    sys.exit(main())
