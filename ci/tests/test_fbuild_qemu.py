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
from pathlib import Path

import pytest
from running_process import RunningProcess

from ci.stage_fbuild_project import _parse_args, stage_fbuild_project
from ci.util.test_runner import (
    _GLOBAL_TIMEOUT,
    _TOP_LEVEL_TEST_TIMEOUT,
    create_python_test_process,
)


ROOT = Path(__file__).resolve().parents[2]
PROJECT_DIR = ROOT / "tests" / "fbuild_qemu_smoke"
SUCCESS_MARKER = "FBUILD-QEMU-TEST-OK"
ERROR_PATTERN = (
    r"Guru Meditation|abort\(\)|Backtrace:|TEST_SUITE_COMPLETE: FAIL|"
    r"QEMU_LCD_CLOCKLESS_REGISTRATION: FAIL"
)
# fbuild 2.5.18 caps daemon-side long operations at 30 minutes. Keep the
# process wrapper slightly longer so fbuild can return its structured result.
FBUILD_DAEMON_LONG_OPERATION_TIMEOUT_SECONDS = 30 * 60
FBUILD_TEST_EMU_PROCESS_TIMEOUT_SECONDS = (
    FBUILD_DAEMON_LONG_OPERATION_TIMEOUT_SECONDS + 60
)


def test_stage_fbuild_argument_parser_accepts_explicit_argv() -> None:
    """The staging CLI can be tested without mutating process arguments."""
    args = _parse_args(["--board", "esp32dev", "--example", "Blink"])

    assert args.board == "esp32dev"
    assert args.example == "Blink"


def test_stage_fbuild_qemu_project_without_compiling(tmp_path: Path) -> None:
    """Staging prepares a native-fbuild project without building firmware."""
    staged_dir = tmp_path / "esp32dev"

    result = stage_fbuild_project(
        board_name="esp32dev",
        example="Blink",
        defines=["FASTLED_ESP32_IS_QEMU"],
        build_dir=staged_dir,
    )

    assert result.success
    assert result.build_dir == staged_dir
    assert (staged_dir / "platformio.ini").is_file()
    assert (staged_dir / "src" / "sketch" / "Blink.ino").is_file()
    assert (staged_dir / "lib" / "FastLED" / "FastLED.h").is_file()
    assert not (staged_dir / ".fbuild" / "build").exists()


def test_fbuild_process_timeout_allows_daemon_diagnostic_grace() -> None:
    """The outer wrapper must not mask fbuild's own long-operation result."""
    assert (
        FBUILD_TEST_EMU_PROCESS_TIMEOUT_SECONDS
        > FBUILD_DAEMON_LONG_OPERATION_TIMEOUT_SECONDS
    )


def test_python_worker_outlives_fbuild_process_timeout() -> None:
    """The parent pytest worker must not mask the child fbuild result."""
    worker = create_python_test_process(False)

    assert worker.timeout is not None
    assert worker.timeout > FBUILD_TEST_EMU_PROCESS_TIMEOUT_SECONDS


def test_stuck_output_threshold_outlives_fbuild_process_timeout() -> None:
    """A healthy quiet child must reach its explicit process deadline."""
    assert _GLOBAL_TIMEOUT > FBUILD_TEST_EMU_PROCESS_TIMEOUT_SECONDS


def test_top_level_watchdog_outlives_python_worker_and_deadman_grace() -> None:
    """The outermost watchdog must not preempt the process-group result."""
    worker = create_python_test_process(False)

    assert worker.timeout is not None
    assert _TOP_LEVEL_TEST_TIMEOUT > worker.timeout + 60


@pytest.mark.skipif(
    not PROJECT_DIR.exists(),
    reason=f"smoke project missing at {PROJECT_DIR}",
)
@pytest.mark.skipif(
    os.environ.get("FASTLED_SKIP_FBUILD_QEMU") == "1",
    reason="FASTLED_SKIP_FBUILD_QEMU=1",
)
@pytest.mark.skipif(
    shutil.which("fbuild") is None,
    reason="fbuild CLI not on PATH — install with `uv pip install fbuild`",
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
        "--halt-on-success",
        SUCCESS_MARKER,
        "--halt-on-error",
        ERROR_PATTERN,
    ]
    proc = RunningProcess.run(
        cmd,
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
        timeout=FBUILD_TEST_EMU_PROCESS_TIMEOUT_SECONDS,
    )
    output = proc.stdout or ""
    assert proc.returncode == 0, (
        f"fbuild test-emu exited with rc={proc.returncode}.\n"
        f"CMD: {cmd!r}\n"
        f"--- OUTPUT ---\n{output}"
    )
    assert SUCCESS_MARKER in output, (
        f"Expected marker {SUCCESS_MARKER!r} not found in emulator output.\n"
        f"--- OUTPUT ---\n{output}"
    )
