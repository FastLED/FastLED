"""fbuild native-QEMU smoke test.

Runs `fbuild test-emu` on a tiny FastLED sketch under
``tests/fbuild_qemu_smoke/`` and asserts the ESP32 QEMU emulator boots far
enough to print a known marker string. This exercises fbuild's native
emulator path (not Docker) so we can catch regressions in the
fbuild ↔ FastLED-library integration without spinning up containers.

If fbuild is not installed or the test project is missing, the test is
skipped. If fbuild runs but fails to build or boot, the test fails with the
full stdout/stderr so the failure mode is diagnosable.
"""

from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
PROJECT_DIR = ROOT / "tests" / "fbuild_qemu_smoke"
SUCCESS_MARKER = "FBUILD-QEMU-TEST-OK"


pytestmark = pytest.mark.skipif(
    shutil.which("fbuild") is None,
    reason="fbuild CLI not on PATH — install with `uv pip install fbuild`",
)


@pytest.mark.skipif(
    not PROJECT_DIR.exists(),
    reason=f"smoke project missing at {PROJECT_DIR}",
)
@pytest.mark.skipif(
    os.environ.get("FASTLED_SKIP_FBUILD_QEMU") == "1",
    reason="FASTLED_SKIP_FBUILD_QEMU=1",
)
def test_fbuild_test_emu_esp32dev() -> None:
    """`fbuild test-emu` builds and boots the smoke sketch under ESP32 QEMU."""
    cmd = [
        "fbuild",
        "test-emu",
        str(PROJECT_DIR),
        "-e",
        "esp32dev",
        "--emulator",
        "qemu",
        "--timeout",
        "10",
    ]
    proc = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        timeout=600,
    )
    output = proc.stdout + "\n" + proc.stderr
    assert proc.returncode == 0, (
        f"fbuild test-emu exited with rc={proc.returncode}.\n"
        f"CMD: {subprocess.list2cmdline(cmd)}\n"
        f"--- STDOUT ---\n{proc.stdout}\n"
        f"--- STDERR ---\n{proc.stderr}"
    )
    assert SUCCESS_MARKER in output, (
        f"Expected marker {SUCCESS_MARKER!r} not found in emulator output.\n"
        f"--- STDOUT ---\n{proc.stdout}\n"
        f"--- STDERR ---\n{proc.stderr}"
    )
