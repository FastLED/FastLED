"""Regression tests for streaming fbuild deploy output (FastLED#3441).

Before this, run_fbuild_deploy() used subprocess.run(stdout=PIPE) and printed
everything only after the child exited, so a multi-minute board build looked
stalled and a failing build hid its error until the wait was over.

The interesting property is not "the output eventually appears" -- the buffered
version did that too. It is that a line becomes visible BEFORE the process
exits. These tests assert that directly, by having the fake fbuild block on a
sentinel file until the test observes its first line.
"""

from __future__ import annotations

import sys
import threading
import time
from pathlib import Path
from typing import IO, Any

import pytest

from ci.util import fbuild_runner


class _RecordingSink:
    """Output sink that timestamps every write, so ordering is checkable."""

    def __init__(self) -> None:
        self.lines: list[tuple[float, str]] = []
        self._lock = threading.Lock()

    def write(self, text: str) -> int:
        if text.strip():
            with self._lock:
                self.lines.append((time.monotonic(), text.strip()))
        return len(text)

    def flush(self) -> None:  # pragma: no cover - required by the IO protocol
        pass

    def texts(self) -> list[str]:
        with self._lock:
            return [line for _, line in self.lines]

    def first_time_containing(self, needle: str) -> float | None:
        with self._lock:
            for stamp, line in self.lines:
                if needle in line:
                    return stamp
        return None


def _install_fake_fbuild(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    script_body: str,
    sink: _RecordingSink,
) -> Path:
    """Point run_fbuild_deploy at a fake fbuild that runs `script_body`."""
    script = tmp_path / "fake_fbuild.py"
    script.write_text(script_body, encoding="utf-8")

    # The real code builds argv as [fbuild_exe, build_dir, "deploy", ...]. Using
    # the interpreter as the executable and the script as the first arg keeps
    # that shape intact while running our fake.
    monkeypatch.setattr(fbuild_runner, "get_fbuild_executable", lambda: sys.executable)

    real_ctor = fbuild_runner.RunningProcess

    def patched(command: Any, *args: Any, **kwargs: Any) -> Any:
        # command == [python, <build_dir>, "deploy", "-e", env, ...]
        # Replace the positional build_dir with our script path.
        cmd = list(command)
        cmd[1] = str(script)
        return real_ctor(cmd, *args, **kwargs)

    monkeypatch.setattr(fbuild_runner, "RunningProcess", patched)
    monkeypatch.setattr(fbuild_runner, "_get_output", lambda quiet, log_file: sink)
    return script


def test_deploy_output_is_visible_before_process_exits(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """The core guarantee: a line surfaces while the child is still running."""
    gate = tmp_path / "gate"
    sink = _RecordingSink()
    _install_fake_fbuild(
        monkeypatch,
        tmp_path,
        script_body=(
            "import sys, time, pathlib\n"
            "print('COMPILING early-line', flush=True)\n"
            f"gate = pathlib.Path(r'{gate}')\n"
            # Block until the test confirms it already saw the first line.
            "for _ in range(600):\n"
            "    if gate.exists():\n"
            "        break\n"
            "    time.sleep(0.05)\n"
            "print('DONE late-line', flush=True)\n"
        ),
        sink=sink,
    )

    result_box: dict[str, Any] = {}

    def run() -> None:
        result_box["result"] = fbuild_runner.run_fbuild_deploy(
            build_dir=tmp_path, environment="esp32s3", timeout=60
        )

    worker = threading.Thread(target=run, daemon=True)
    worker.start()

    # Wait for the early line WITHOUT letting the child finish. If output were
    # still buffered until exit, this would time out -- the child cannot exit
    # until we touch the gate, which we have not done yet.
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        if sink.first_time_containing("early-line") is not None:
            break
        time.sleep(0.05)

    saw_early = sink.first_time_containing("early-line")
    assert saw_early is not None, (
        "no output surfaced while the child was still running -- "
        f"output was buffered. Sink contained: {sink.texts()}"
    )
    assert sink.first_time_containing("late-line") is None, (
        "the child should still be blocked on the gate at this point"
    )

    gate.write_text("go", encoding="utf-8")
    worker.join(timeout=30)
    assert not worker.is_alive(), "run_fbuild_deploy did not return"

    result = result_box["result"]
    assert result.success is True
    # Accumulated output is still preserved for logs/diagnostics.
    assert "early-line" in result.output
    assert "late-line" in result.output


@pytest.mark.parametrize("port", ["COM42", "/dev/ttyACM0"])
def test_deploy_parses_port_marker_from_streamed_output(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path, port: str
) -> None:
    """FBUILD_DEPLOY_PORT= is still picked up when parsed line-by-line."""
    sink = _RecordingSink()
    _install_fake_fbuild(
        monkeypatch,
        tmp_path,
        script_body=(
            "print('starting', flush=True)\n"
            f"print('FBUILD_DEPLOY_PORT={port} extra', flush=True)\n"
            "print('finished', flush=True)\n"
        ),
        sink=sink,
    )

    result = fbuild_runner.run_fbuild_deploy(
        build_dir=tmp_path, environment="esp32s3", timeout=60
    )
    assert result.success is True
    assert result.port == port


def test_deploy_ignores_empty_port_marker_before_diagnostic(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """An empty marker must not turn the following semicolon into a port."""
    sink = _RecordingSink()
    _install_fake_fbuild(
        monkeypatch,
        tmp_path,
        script_body=(
            "print('deploy succeeded (full flash); FBUILD_DEPLOY_PORT=; "
            "firmware flashed but runtime CDC did not recover', flush=True)\n"
        ),
        sink=sink,
    )

    result = fbuild_runner.run_fbuild_deploy(
        build_dir=tmp_path, environment="rp2350w", timeout=60
    )
    assert result.success is True
    assert result.port is None
    assert "runtime CDC did not recover" in result.output


def test_deploy_reports_failure_exit_code(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """A failing build still reports failure, and its output is retained."""
    sink = _RecordingSink()
    _install_fake_fbuild(
        monkeypatch,
        tmp_path,
        script_body=(
            "import sys\n"
            "print('error: undefined reference to foo', flush=True)\n"
            "sys.exit(3)\n"
        ),
        sink=sink,
    )

    result = fbuild_runner.run_fbuild_deploy(
        build_dir=tmp_path, environment="esp32s3", timeout=60
    )
    assert result.success is False
    assert result.returncode == 3
    assert "undefined reference" in result.output


def test_deploy_fails_on_build_error_with_zero_returncode(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """A `build error:` marker outweighs a zero exit code."""
    sink = _RecordingSink()
    _install_fake_fbuild(
        monkeypatch,
        tmp_path,
        script_body=(
            "print('   0.00 =   BUILDING esp32s3   =', flush=True)\n"
            'print("build error: build failed: local library "\n'
            "      \"'FastLED' compilation failed\", flush=True)\n"
        ),
        sink=sink,
    )

    result = fbuild_runner.run_fbuild_deploy(
        build_dir=tmp_path, environment="esp32s3", timeout=60
    )
    assert result.success is False
    assert result.returncode == 0
    assert "build error:" in result.output


def test_deploy_writes_to_log_file_in_quiet_mode(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """Quiet mode routes the streamed lines to log_file, not stdout."""
    monkeypatch.setattr(fbuild_runner, "get_fbuild_executable", lambda: sys.executable)
    script = tmp_path / "fake_fbuild.py"
    script.write_text("print('quiet-mode-line', flush=True)\n", encoding="utf-8")

    real_ctor = fbuild_runner.RunningProcess

    def patched(command: Any, *args: Any, **kwargs: Any) -> Any:
        cmd = list(command)
        cmd[1] = str(script)
        return real_ctor(cmd, *args, **kwargs)

    monkeypatch.setattr(fbuild_runner, "RunningProcess", patched)

    log_path = tmp_path / "deploy.log"
    with open(log_path, "w", encoding="utf-8") as handle:
        log_io: IO[str] = handle
        result = fbuild_runner.run_fbuild_deploy(
            build_dir=tmp_path,
            environment="esp32s3",
            timeout=60,
            quiet=True,
            log_file=log_io,
        )

    assert result.success is True
    assert "quiet-mode-line" in log_path.read_text(encoding="utf-8")
