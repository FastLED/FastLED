#!/usr/bin/env python3
"""
Claude Code Stop hook that runs lint and C++ tests concurrently.

Both run in parallel. If lint fails first, the test process is cancelled
and only lint errors are shown. If lint passes, test results are reported.

Usage: Receives JSON on stdin from Claude Code Stop hook.
Exit codes:
  0 - Both passed
  2 - Lint or test failures (stderr fed back to Claude)
"""

import subprocess
import sys
import threading
from pathlib import Path


SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent


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


def repo_is_clean() -> bool:
    """Return True if the working tree has no staged, unstaged, or untracked changes."""
    result = run_cmd(["git", "status", "--porcelain"])
    return result.returncode == 0 and not result.stdout.strip()


def main() -> int:
    if repo_is_clean():
        return 0

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
