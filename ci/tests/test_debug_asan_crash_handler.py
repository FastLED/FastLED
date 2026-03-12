"""Test that debug builds with ASAN don't hang on Windows."""

import os
import sys
from pathlib import Path

import pytest


@pytest.mark.skipif(sys.platform != "win32", reason="Windows-only test")
def test_debug_runner_no_hang():
    """Verify runner.exe doesn't hang with ASAN-instrumented DLLs."""
    from running_process import RunningProcess

    build_dir = Path(".build/meson-debug")
    runner = build_dir / "tests" / "runner.exe"
    # Use a simple/fast test DLL
    test_dll = build_dir / "tests" / "fl_align.dll"

    if not runner.exists() or not test_dll.exists():
        pytest.skip("Debug build not available")

    # Set up environment with crash handler disabled (belt-and-suspenders)
    env = os.environ.copy()
    env["FASTLED_DISABLE_CRASH_HANDLER"] = "1"

    # Should complete in <30s, not hang for 600s
    proc = RunningProcess(
        [str(runner), str(test_dll)],
        timeout=30,
        auto_run=True,
        check=False,
        env=env,
    )
    rc = proc.wait(echo=True)
    assert rc == 0, f"Debug test failed with exit code {rc}"
