from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Build Process Registration Client

This module provides a simple API for registering build processes with the daemon
so they can be cleaned up if the client dies unexpectedly.

Usage:
    from ci.util.build_process_client import register_build_process

    # Start a build process
    proc = subprocess.Popen([...])

    # Register it with the daemon for orphan cleanup
    register_build_process(proc.pid, example="Blink", project_dir="/path/to/project")

    # If this client crashes, daemon will kill proc.pid and all its children
"""

import json
import logging
import os
from pathlib import Path


# Daemon configuration (must match daemon settings)
DAEMON_DIR = Path.home() / ".fastled" / "daemon"
BUILD_REGISTRY_FILE = DAEMON_DIR / "build_processes.json"


def register_build_process(
    root_pid: int,
    example: str = "",
    project_dir: str = "",
) -> None:
    """Register a build process with the daemon for orphan cleanup.

    This function adds the current client PID and the build process PID to the
    daemon's registry. If the client dies, the daemon will automatically kill
    the build process and all its children.

    Args:
        root_pid: PID of the root build process (PlatformIO/scons)
        example: Example name being built (for logging)
        project_dir: Project directory (for logging)
    """
    client_pid = os.getpid()

    try:
        # Ensure daemon directory exists
        DAEMON_DIR.mkdir(parents=True, exist_ok=True)

        # Read existing registry
        registry = {}
        if BUILD_REGISTRY_FILE.exists():
            try:
                with open(
                    BUILD_REGISTRY_FILE, "r", encoding="utf-8", errors="replace"
                ) as f:
                    registry = json.load(f)
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception:
                pass  # Start with empty registry if corrupted

        # Add this process
        registry[str(client_pid)] = {
            "client_pid": client_pid,
            "root_pid": root_pid,
            "child_pids": [],  # Will be populated by daemon
            "request_id": f"build_{root_pid}",
            "project_dir": project_dir,
            "started_at": 0,  # Daemon will set this
            "last_updated": 0,  # Daemon will set this
        }

        # Write atomically
        temp_file = BUILD_REGISTRY_FILE.with_suffix(".tmp")
        with open(temp_file, "w", encoding="utf-8", errors="replace") as f:
            json.dump(registry, f, indent=2)

        temp_file.replace(BUILD_REGISTRY_FILE)

        logging.debug(
            f"Registered build process: client={client_pid}, root={root_pid}, example={example}"
        )

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        # Non-fatal: log but don't fail the build
        logging.warning(f"Failed to register build process: {e}")


def unregister_build_process() -> None:
    """Unregister the current client's build process.

    This should be called when the build completes successfully to prevent
    the daemon from unnecessarily tracking the process.
    """
    client_pid = os.getpid()

    try:
        if not BUILD_REGISTRY_FILE.exists():
            return

        # Read registry
        with open(BUILD_REGISTRY_FILE, "r", encoding="utf-8", errors="replace") as f:
            registry = json.load(f)

        # Remove this client
        if str(client_pid) in registry:
            del registry[str(client_pid)]

            # Write atomically
            temp_file = BUILD_REGISTRY_FILE.with_suffix(".tmp")
            with open(temp_file, "w", encoding="utf-8", errors="replace") as f:
                json.dump(registry, f, indent=2)

            temp_file.replace(BUILD_REGISTRY_FILE)

            logging.debug(f"Unregistered build process for client={client_pid}")

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        # Non-fatal: log but don't fail
        logging.warning(f"Failed to unregister build process: {e}")
