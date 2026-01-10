#!/usr/bin/env python3
"""
Typed message protocol for PlatformIO daemon operations.

This module defines typed dataclasses for all client-daemon communication,
replacing raw dictionaries with validated, type-safe message objects.

Supports:
- Package installation requests (PackageRequest/DaemonStatus)
- Build/upload/monitor requests (BuildRequest/BuildStatus)
"""

import time
from dataclasses import asdict, dataclass, field
from enum import Enum
from typing import Any

from typeguard import typechecked


class DaemonState(Enum):
    """Daemon state enumeration for package installation workflow."""

    IDLE = "idle"
    INSTALLING = "installing"
    COMPLETED = "completed"
    FAILED = "failed"
    UNKNOWN = "unknown"

    @classmethod
    def from_string(cls, value: str) -> "DaemonState":
        """Convert string to DaemonState, defaulting to UNKNOWN if invalid.

        Args:
            value: State string

        Returns:
            DaemonState enum value
        """
        try:
            return cls(value)
        except ValueError:
            return cls.UNKNOWN


@typechecked
@dataclass
class PackageRequest:
    """Client → Daemon: Package installation request message.

    This message is written to the request file by the client to initiate
    a package installation.

    Attributes:
        project_dir: Absolute path to PlatformIO project directory
        environment: PlatformIO environment name (None = use default)
        timestamp: Unix timestamp when request was created
        caller_pid: Process ID of the requesting client
        caller_cwd: Working directory of the requesting client
        request_id: Unique identifier for this request
    """

    project_dir: str
    environment: str | None
    timestamp: float
    caller_pid: int
    caller_cwd: str
    request_id: str = field(default_factory=lambda: f"req_{int(time.time() * 1000)}")

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization.

        Returns:
            Dictionary representation of the request
        """
        return asdict(self)

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "PackageRequest":
        """Create PackageRequest from dictionary.

        Args:
            data: Dictionary with request fields

        Returns:
            PackageRequest instance

        Raises:
            TypeError: If required fields are missing or have wrong types
        """
        return cls(
            project_dir=data["project_dir"],
            environment=data.get("environment"),
            timestamp=data["timestamp"],
            caller_pid=data["caller_pid"],
            caller_cwd=data["caller_cwd"],
            request_id=data.get("request_id", f"req_{int(time.time() * 1000)}"),
        )


@typechecked
@dataclass
class DaemonStatus:
    """Daemon → Client: Status update message.

    This message is written to the status file by the daemon to communicate
    current state and progress to clients.

    Attributes:
        state: Current daemon state
        message: Human-readable status message
        updated_at: Unix timestamp of last status update
        installation_in_progress: Whether installation is actively running
        daemon_pid: Process ID of the daemon
        daemon_started_at: Unix timestamp when daemon started
        caller_pid: Process ID of client whose request is being processed
        caller_cwd: Working directory of client whose request is being processed
        request_id: ID of the request currently being processed
        request_started_at: Unix timestamp when current request started
        environment: PlatformIO environment being installed
        project_dir: Project directory for current installation
        current_operation: Detailed description of current operation
    """

    state: DaemonState
    message: str
    updated_at: float
    installation_in_progress: bool = False
    daemon_pid: int | None = None
    daemon_started_at: float | None = None
    caller_pid: int | None = None
    caller_cwd: str | None = None
    request_id: str | None = None
    request_started_at: float | None = None
    environment: str | None = None
    project_dir: str | None = None
    current_operation: str | None = None

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization.

        Returns:
            Dictionary representation of the status
        """
        result = asdict(self)
        # Convert DaemonState enum to string value
        result["state"] = self.state.value
        return result

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "DaemonStatus":
        """Create DaemonStatus from dictionary.

        Args:
            data: Dictionary with status fields

        Returns:
            DaemonStatus instance

        Raises:
            TypeError: If required fields are missing or have wrong types
        """
        # Convert state string to enum
        state_str = data.get("state", "unknown")
        state = DaemonState.from_string(state_str)

        return cls(
            state=state,
            message=data.get("message", ""),
            updated_at=data.get("updated_at", time.time()),
            installation_in_progress=data.get("installation_in_progress", False),
            daemon_pid=data.get("daemon_pid"),
            daemon_started_at=data.get("daemon_started_at"),
            caller_pid=data.get("caller_pid"),
            caller_cwd=data.get("caller_cwd"),
            request_id=data.get("request_id"),
            request_started_at=data.get("request_started_at"),
            environment=data.get("environment"),
            project_dir=data.get("project_dir"),
            current_operation=data.get("current_operation"),
        )

    def is_stale(self, timeout_seconds: float = 30.0) -> bool:
        """Check if status hasn't been updated recently.

        Args:
            timeout_seconds: Maximum age in seconds before considering stale

        Returns:
            True if status is stale, False otherwise
        """
        return (time.time() - self.updated_at) > timeout_seconds

    def get_age_seconds(self) -> float:
        """Get age of this status update in seconds.

        Returns:
            Seconds since last update
        """
        return time.time() - self.updated_at


class BuildCommand(Enum):
    """Build command types for PlatformIO operations."""

    BUILD = "build"
    UPLOAD = "upload"
    MONITOR = "monitor"
    BUILD_AND_UPLOAD = "build_and_upload"

    @classmethod
    def from_string(cls, value: str) -> "BuildCommand":
        """Convert string to BuildCommand.

        Args:
            value: Command string

        Returns:
            BuildCommand enum value

        Raises:
            ValueError: If value is not a valid command
        """
        return cls(value)


class BuildState(Enum):
    """Build process state enumeration."""

    QUEUED = "queued"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    TERMINATED = "terminated"  # Killed by daemon cleanup

    @classmethod
    def from_string(cls, value: str) -> "BuildState":
        """Convert string to BuildState.

        Args:
            value: State string

        Returns:
            BuildState enum value
        """
        try:
            return cls(value)
        except ValueError:
            return cls.FAILED


@typechecked
@dataclass
class BuildRequest:
    """Client → Daemon: Build/upload/monitor request message.

    This message is written to the build request file by the client to initiate
    a PlatformIO build operation.

    Attributes:
        project_dir: Absolute path to PlatformIO project directory
        environment: PlatformIO environment name
        command: Build command to execute (build/upload/monitor)
        example: Example sketch name (for logging)
        caller_pid: Process ID of the requesting client
        caller_cwd: Working directory of the requesting client
        timestamp: Unix timestamp when request was created
        request_id: Unique identifier for this request
        upload_port: Serial port for upload/monitor (optional)
        additional_args: Additional PlatformIO arguments (optional)
    """

    project_dir: str
    environment: str
    command: BuildCommand
    example: str
    caller_pid: int
    caller_cwd: str
    timestamp: float = field(default_factory=time.time)
    request_id: str = field(default_factory=lambda: f"build_{int(time.time() * 1000)}")
    upload_port: str | None = None
    additional_args: list[str] = field(default_factory=lambda: [])

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization.

        Returns:
            Dictionary representation of the request
        """
        result = asdict(self)
        result["command"] = self.command.value
        return result

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "BuildRequest":
        """Create BuildRequest from dictionary.

        Args:
            data: Dictionary with request fields

        Returns:
            BuildRequest instance

        Raises:
            TypeError: If required fields are missing or have wrong types
        """
        command = BuildCommand.from_string(data["command"])

        return cls(
            project_dir=data["project_dir"],
            environment=data["environment"],
            command=command,
            example=data["example"],
            caller_pid=data["caller_pid"],
            caller_cwd=data["caller_cwd"],
            timestamp=data.get("timestamp", time.time()),
            request_id=data.get("request_id", f"build_{int(time.time() * 1000)}"),
            upload_port=data.get("upload_port"),
            additional_args=data.get("additional_args", []),
        )


@typechecked
@dataclass
class BuildStatus:
    """Daemon → Client: Build process status update message.

    This message is written to the build status file by the daemon to communicate
    build progress and output to clients.

    Attributes:
        request_id: ID of the build request being processed
        state: Current build state
        message: Human-readable status message
        updated_at: Unix timestamp of last status update
        output_lines: New output lines since last status update
        exit_code: Process exit code (None if still running)
        root_pid: Root PlatformIO process PID
        child_pids: List of all child process PIDs (recursive)
        started_at: Unix timestamp when build started
        completed_at: Unix timestamp when build completed (None if running)
    """

    request_id: str
    state: BuildState
    message: str
    updated_at: float
    output_lines: list[str] = field(default_factory=lambda: [])
    exit_code: int | None = None
    root_pid: int | None = None
    child_pids: list[int] = field(default_factory=lambda: [])
    started_at: float | None = None
    completed_at: float | None = None

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization.

        Returns:
            Dictionary representation of the status
        """
        result = asdict(self)
        result["state"] = self.state.value
        return result

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "BuildStatus":
        """Create BuildStatus from dictionary.

        Args:
            data: Dictionary with status fields

        Returns:
            BuildStatus instance

        Raises:
            TypeError: If required fields are missing or have wrong types
        """
        state = BuildState.from_string(data["state"])

        return cls(
            request_id=data["request_id"],
            state=state,
            message=data.get("message", ""),
            updated_at=data.get("updated_at", time.time()),
            output_lines=data.get("output_lines", []),
            exit_code=data.get("exit_code"),
            root_pid=data.get("root_pid"),
            child_pids=data.get("child_pids", []),
            started_at=data.get("started_at"),
            completed_at=data.get("completed_at"),
        )

    def get_age_seconds(self) -> float:
        """Get age of this status update in seconds.

        Returns:
            Seconds since last update
        """
        return time.time() - self.updated_at
