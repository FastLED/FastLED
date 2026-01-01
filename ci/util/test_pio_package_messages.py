#!/usr/bin/env python3
"""
Unit tests for PlatformIO package message protocol.

Tests the typed dataclasses for client-daemon communication to ensure
proper serialization, deserialization, and backward compatibility.
"""

import time

import pytest

from ci.util.pio_package_messages import DaemonState, DaemonStatus, PackageRequest


class TestPackageRequest:
    """Test PackageRequest dataclass."""

    def test_create_request(self):
        """Test creating a PackageRequest object."""
        req = PackageRequest(
            project_dir="/path/to/project",
            environment="esp32c6",
            timestamp=1234567890.0,
            caller_pid=12345,
            caller_cwd="/path/to/cwd",
            request_id="req_test_123",
        )

        assert req.project_dir == "/path/to/project"
        assert req.environment == "esp32c6"
        assert req.timestamp == 1234567890.0
        assert req.caller_pid == 12345
        assert req.caller_cwd == "/path/to/cwd"
        assert req.request_id == "req_test_123"

    def test_to_dict(self):
        """Test converting PackageRequest to dictionary."""
        req = PackageRequest(
            project_dir="/path/to/project",
            environment="esp32c6",
            timestamp=1234567890.0,
            caller_pid=12345,
            caller_cwd="/path/to/cwd",
            request_id="req_test_123",
        )

        req_dict = req.to_dict()

        assert req_dict["project_dir"] == "/path/to/project"
        assert req_dict["environment"] == "esp32c6"
        assert req_dict["timestamp"] == 1234567890.0
        assert req_dict["caller_pid"] == 12345
        assert req_dict["caller_cwd"] == "/path/to/cwd"
        assert req_dict["request_id"] == "req_test_123"

    def test_from_dict(self):
        """Test creating PackageRequest from dictionary."""
        data = {
            "project_dir": "/path/to/project",
            "environment": "esp32c6",
            "timestamp": 1234567890.0,
            "caller_pid": 12345,
            "caller_cwd": "/path/to/cwd",
            "request_id": "req_test_123",
        }

        req = PackageRequest.from_dict(data)

        assert req.project_dir == "/path/to/project"
        assert req.environment == "esp32c6"
        assert req.timestamp == 1234567890.0
        assert req.caller_pid == 12345
        assert req.caller_cwd == "/path/to/cwd"
        assert req.request_id == "req_test_123"

    def test_roundtrip(self):
        """Test round-trip conversion: object -> dict -> object."""
        original = PackageRequest(
            project_dir="/path/to/project",
            environment=None,  # Test None value
            timestamp=1234567890.0,
            caller_pid=12345,
            caller_cwd="/path/to/cwd",
        )

        # Convert to dict and back
        data = original.to_dict()
        restored = PackageRequest.from_dict(data)

        assert restored.project_dir == original.project_dir
        assert restored.environment == original.environment
        assert restored.timestamp == original.timestamp
        assert restored.caller_pid == original.caller_pid
        assert restored.caller_cwd == original.caller_cwd

    def test_auto_generated_request_id(self):
        """Test that request_id is auto-generated if not provided."""
        req1 = PackageRequest(
            project_dir="/path/to/project",
            environment="esp32c6",
            timestamp=time.time(),
            caller_pid=12345,
            caller_cwd="/path/to/cwd",
        )

        # Small delay to ensure different millisecond timestamp
        time.sleep(0.002)

        req2 = PackageRequest(
            project_dir="/path/to/project",
            environment="esp32c6",
            timestamp=time.time(),
            caller_pid=12345,
            caller_cwd="/path/to/cwd",
        )

        # Each request should have a unique ID
        assert req1.request_id.startswith("req_")
        assert req2.request_id.startswith("req_")
        # Request IDs should be different (generated from timestamp)
        assert req1.request_id != req2.request_id


class TestDaemonStatus:
    """Test DaemonStatus dataclass."""

    def test_create_status(self):
        """Test creating a DaemonStatus object."""
        status = DaemonStatus(
            state=DaemonState.INSTALLING,
            message="Installing packages",
            updated_at=1234567890.0,
            installation_in_progress=True,
            daemon_pid=9999,
            caller_pid=12345,
        )

        assert status.state == DaemonState.INSTALLING
        assert status.message == "Installing packages"
        assert status.updated_at == 1234567890.0
        assert status.installation_in_progress is True
        assert status.daemon_pid == 9999
        assert status.caller_pid == 12345

    def test_to_dict(self):
        """Test converting DaemonStatus to dictionary."""
        status = DaemonStatus(
            state=DaemonState.COMPLETED,
            message="Installation complete",
            updated_at=1234567890.0,
            installation_in_progress=False,
        )

        status_dict = status.to_dict()

        # State enum should be converted to string value
        assert status_dict["state"] == "completed"
        assert status_dict["message"] == "Installation complete"
        assert status_dict["updated_at"] == 1234567890.0
        assert status_dict["installation_in_progress"] is False

    def test_from_dict(self):
        """Test creating DaemonStatus from dictionary."""
        data = {
            "state": "installing",
            "message": "Installing packages",
            "updated_at": 1234567890.0,
            "installation_in_progress": True,
            "daemon_pid": 9999,
            "caller_pid": 12345,
            "current_operation": "Downloading toolchain",
        }

        status = DaemonStatus.from_dict(data)

        assert status.state == DaemonState.INSTALLING
        assert status.message == "Installing packages"
        assert status.updated_at == 1234567890.0
        assert status.installation_in_progress is True
        assert status.daemon_pid == 9999
        assert status.caller_pid == 12345
        assert status.current_operation == "Downloading toolchain"

    def test_roundtrip(self):
        """Test round-trip conversion: object -> dict -> object."""
        original = DaemonStatus(
            state=DaemonState.FAILED,
            message="Installation failed",
            updated_at=1234567890.0,
            installation_in_progress=False,
            daemon_pid=9999,
            current_operation=None,  # Test None value
        )

        # Convert to dict and back
        data = original.to_dict()
        restored = DaemonStatus.from_dict(data)

        assert restored.state == original.state
        assert restored.message == original.message
        assert restored.updated_at == original.updated_at
        assert restored.installation_in_progress == original.installation_in_progress
        assert restored.daemon_pid == original.daemon_pid
        assert restored.current_operation == original.current_operation

    def test_is_stale(self):
        """Test stale detection."""
        # Fresh status (just created)
        fresh_status = DaemonStatus(
            state=DaemonState.INSTALLING,
            message="Installing",
            updated_at=time.time(),
        )

        assert not fresh_status.is_stale(timeout_seconds=30)

        # Stale status (1 minute old)
        stale_status = DaemonStatus(
            state=DaemonState.INSTALLING,
            message="Installing",
            updated_at=time.time() - 60,
        )

        assert stale_status.is_stale(timeout_seconds=30)

    def test_get_age_seconds(self):
        """Test getting age of status."""
        now = time.time()
        status = DaemonStatus(
            state=DaemonState.IDLE,
            message="Ready",
            updated_at=now - 10,  # 10 seconds ago
        )

        age = status.get_age_seconds()
        assert age >= 10.0  # Should be at least 10 seconds


class TestDaemonState:
    """Test DaemonState enum."""

    def test_from_string_valid(self):
        """Test creating DaemonState from valid string."""
        assert DaemonState.from_string("idle") == DaemonState.IDLE
        assert DaemonState.from_string("installing") == DaemonState.INSTALLING
        assert DaemonState.from_string("completed") == DaemonState.COMPLETED
        assert DaemonState.from_string("failed") == DaemonState.FAILED

    def test_from_string_invalid(self):
        """Test creating DaemonState from invalid string."""
        # Invalid strings should return UNKNOWN
        assert DaemonState.from_string("invalid") == DaemonState.UNKNOWN
        assert DaemonState.from_string("") == DaemonState.UNKNOWN
        assert DaemonState.from_string("IDLE") == DaemonState.UNKNOWN  # Case sensitive

    def test_enum_values(self):
        """Test enum string values."""
        assert DaemonState.IDLE.value == "idle"
        assert DaemonState.INSTALLING.value == "installing"
        assert DaemonState.COMPLETED.value == "completed"
        assert DaemonState.FAILED.value == "failed"
        assert DaemonState.UNKNOWN.value == "unknown"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
