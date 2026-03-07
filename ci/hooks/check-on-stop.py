#!/usr/bin/env python3
"""
Claude Code Stop hook that runs lint and C++ tests concurrently.

Smart mode: Only runs if files were actually changed during THIS session.
Session fingerprint is captured at session start (check-on-start.py) and
compared here. If nothing changed during the session, lint and tests are
skipped.

Usage: Receives JSON on stdin from Claude Code Stop hook.
Exit codes:
  0 - Both passed or skipped (no changes during session)
  2 - Lint or test failures (stderr fed back to Claude)
"""

import hashlib
import json
import subprocess
import sys
import threading
from pathlib import Path


SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent
SESSION_FINGERPRINT_FILE = PROJECT_ROOT / ".cache" / "session_fingerprint.json"


def run_cmd(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        cwd=str(PROJECT_ROOT),
    )


def report_failure(label: str, result: subprocess.CompletedProcess[str]) -> None:
    print(f"{label}:", file=sys.stderr)
    if result.stdout.strip():
        print(result.stdout.strip(), file=sys.stderr)
    if result.stderr.strip():
        print(result.stderr.strip(), file=sys.stderr)


def get_current_fingerprint() -> str | None:
    """Get MD5 fingerprint of current git status."""
    result = run_cmd(["git", "status", "--porcelain"])
    if result.returncode != 0:
        return None
    status_output = result.stdout
    if not status_output.strip():
        return None
    return hashlib.md5(status_output.encode()).hexdigest()


def get_session_fingerprint() -> str | None:
    """Read fingerprint captured at session start."""
    if SESSION_FINGERPRINT_FILE.exists():
        try:
            data = json.loads(SESSION_FINGERPRINT_FILE.read_text())
            return data.get("fingerprint")
        except KeyboardInterrupt:
            import _thread

            _thread.interrupt_main()
            return None
        except Exception:
            return None
    return None


def should_skip_hook() -> bool:
    """Check if hook should skip based on session fingerprints."""
    current_fp = get_current_fingerprint()

    # No changes at all right now - skip
    if current_fp is None:
        return True

    # Check if we have a session fingerprint (captured at session start)
    session_fp = get_session_fingerprint()
    if session_fp is None:
        # No session fingerprint means repo was clean when session started,
        # and if we have changes now, they were made during this session
        return False

    # Same fingerprint as session start - no changes this session - skip
    if current_fp == session_fp:
        return True

    # Different fingerprint - changes made this session - run hook
    return False


def main() -> int:
    if should_skip_hook():
        print(
            "⏭️  Skipping lint+tests (no changes during this session)", file=sys.stderr
        )
        return 0

    print("🔧 Running lint and tests (changes detected this session)", file=sys.stderr)

    lint_done = threading.Event()
    lint_results: list[subprocess.CompletedProcess[str]] = []
    test_proc_holder: list[subprocess.Popen[str]] = []
    test_results: list[subprocess.CompletedProcess[str]] = []

    def run_lint() -> None:
        result = run_cmd(["uv", "run", "ci/lint.py"])
        lint_results.append(result)
        lint_done.set()

    def run_tests() -> None:
        proc = subprocess.Popen(
            ["uv", "run", "test.py", "--cpp"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            cwd=str(PROJECT_ROOT),
        )
        test_proc_holder.append(proc)
        stdout, stderr = proc.communicate()
        test_results.append(
            subprocess.CompletedProcess(proc.args, proc.returncode, stdout, stderr)
        )

    lint_thread = threading.Thread(target=run_lint)
    test_thread = threading.Thread(target=run_tests)
    lint_thread.start()
    test_thread.start()

    # Wait for lint first
    lint_thread.join()
    lint_result = lint_results[0]

    if lint_result.returncode != 0:
        # Lint failed — cancel tests, report lint errors only
        for proc in test_proc_holder:
            proc.kill()
        report_failure("Lint failed", lint_result)
        return 2

    # Lint passed — wait for tests
    test_thread.join()
    test_result = test_results[0]

    if test_result.returncode != 0:
        report_failure("C++ tests failed", test_result)
        return 2

    return 0


if __name__ == "__main__":
    sys.exit(main())
