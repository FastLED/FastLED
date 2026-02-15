"""
PhaseTracker utility for tracking build and test execution phases.

This module provides thread-safe tracking of build/test phases to disambiguate
failures. When a build fails, we can determine if it failed during CONFIGURE,
COMPILE, LINK, or EXECUTE phase.
"""

import atexit
import json
import os
import threading
from datetime import datetime
from pathlib import Path
from typing import Any, Literal, Optional


class PhaseTracker:
    """
    Thread-safe phase state tracker for build and test execution.

    Tracks the current phase (CONFIGURE/COMPILE/LINK/EXECUTE) to help
    disambiguate build and test failures. Uses atomic file writes and
    stale detection to handle process crashes gracefully.

    Example:
        tracker = PhaseTracker(build_dir, "quick")
        tracker.set_phase("CONFIGURE")
        # ... meson setup ...
        tracker.set_phase("COMPILE", target="test_async")
        # ... compilation ...
        tracker.set_phase("EXECUTE", test_name="test_async")
        # ... test execution ...
        tracker.clear()  # Success
    """

    def __init__(self, build_dir: Path, build_mode: str):
        """
        Initialize PhaseTracker.

        Args:
            build_dir: Meson build directory (e.g., .build/meson-quick)
            build_mode: Build mode name (e.g., "quick", "debug", "release")
        """
        self.build_dir = Path(build_dir)
        self.build_mode = build_mode
        self.state_dir = self.build_dir.parent / "meson-state"
        self.state_file = self.state_dir / f"current-phase-{build_mode}.json"
        self._lock = threading.Lock()

        # Ensure state directory exists
        self.state_dir.mkdir(parents=True, exist_ok=True)

        # Register cleanup handler for process exit
        self._register_cleanup()

    def set_phase(
        self,
        phase: Literal["CONFIGURE", "COMPILE", "LINK", "EXECUTE"],
        test_name: Optional[str] = None,
        target: Optional[str] = None,
        path: Literal["sequential", "streaming"] = "sequential",
    ) -> None:
        """
        Update phase state (thread-safe, atomic write).

        Args:
            phase: Current build/test phase
            test_name: Name of test being executed (optional)
            target: Compilation target (optional)
            path: Execution path ("sequential" or "streaming")
        """
        with self._lock:
            state = {
                "phase": phase,
                "test_name": test_name,
                "target": target,
                "timestamp": datetime.now().isoformat(),
                "pid": os.getpid(),
                "build_mode": self.build_mode,
                "path": path,
            }

            # Atomic write: write to temp file, then rename
            temp_file = self.state_file.with_suffix(".tmp")
            with open(temp_file, "w", encoding="utf-8") as f:
                json.dump(state, f, indent=2)

            # Atomic rename (replaces existing file)
            temp_file.replace(self.state_file)

    def get_phase(self) -> Optional[dict[str, Any]]:
        """
        Read current phase state.

        Returns:
            Phase state dict, or None if file doesn't exist or is stale
        """
        if not self.state_file.exists():
            return None

        try:
            with open(self.state_file, "r", encoding="utf-8") as f:
                state = json.load(f)

            # Check if stale (process no longer exists)
            if self._is_stale(state):
                return None

            return state
        except (json.JSONDecodeError, KeyError, OSError):
            # Corrupted or unreadable state file
            return None

    def clear(self) -> None:
        """Remove phase state file (call on successful completion)."""
        with self._lock:
            if self.state_file.exists():
                self.state_file.unlink()

    def _is_stale(self, state: dict[str, Any]) -> bool:
        """
        Check if phase file is from a dead process.

        Args:
            state: Phase state dict

        Returns:
            True if process no longer exists
        """
        pid: Any = state.get("pid")
        if not pid:
            return True

        # Check if process exists (cross-platform)
        try:
            # Signal 0 checks existence without killing process
            os.kill(pid, 0)
            return False
        except (OSError, ProcessLookupError):
            # Process doesn't exist
            return True

    def _register_cleanup(self) -> None:
        """Register cleanup handler for process exit."""
        # Note: atexit handlers run even on normal exit, but we only
        # want to clear on success. Failures will leave the phase file
        # for debugging. The clear() method should be called explicitly
        # on success.
        atexit.register(self._cleanup_if_needed)

    def _cleanup_if_needed(self) -> None:
        """
        Cleanup handler called on process exit.

        Only clears the phase file if it's from our PID (prevents
        clearing state from other concurrent builds).
        """
        if not self.state_file.exists():
            return

        try:
            with open(self.state_file, "r", encoding="utf-8") as f:
                state = json.load(f)

            # Only clear if this is our phase file
            if state.get("pid") == os.getpid():
                # Don't actually clear here - let explicit clear() handle it
                # This prevents clearing on abnormal exit (which we want to debug)
                pass
        except (json.JSONDecodeError, KeyError, OSError):
            pass
