#!/usr/bin/env python3
"""
Claude Code Stop hook that runs lint and C++ tests concurrently.

Smart mode: Only runs if:
1. Repo has actual changes (git status --porcelain not empty)
2. Changes differ from last run (fingerprint mismatch)

Both lint and tests run in parallel. If lint fails first, the test process
is cancelled and only lint errors are shown. If lint passes, test results
are reported.

Usage: Receives JSON on stdin from Claude Code Stop hook.
Exit codes:
  0 - Both passed or skipped (no changes)
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
FINGERPRINT_FILE = PROJECT_ROOT / ".cache" / "last_agent_changes_fingerprint"


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


def get_current_fingerprint() -> str | None:
    """Get MD5 fingerprint of current git status."""
    result = run_cmd(["git", "status", "--porcelain"])
    if result.returncode != 0:
        return None
    status_output = result.stdout
    if not status_output.strip():
        return None
    return hashlib.md5(status_output.encode()).hexdigest()


def get_last_fingerprint() -> str | None:
    """Read stored fingerprint from .cache/."""
    if FINGERPRINT_FILE.exists():
        try:
            data = json.loads(FINGERPRINT_FILE.read_text())
            return data.get("fingerprint")
        except KeyboardInterrupt:
            import _thread

            _thread.interrupt_main()
            return None
        except Exception:
            return None
    return None


def save_fingerprint(fingerprint: str) -> None:
    """Save current fingerprint to .cache/."""
    FINGERPRINT_FILE.parent.mkdir(parents=True, exist_ok=True)
    FINGERPRINT_FILE.write_text(json.dumps({"fingerprint": fingerprint}))


def should_skip_hook() -> bool:
    """Check if hook should skip based on fingerprints."""
    current_fp = get_current_fingerprint()

    # No changes - skip
    if current_fp is None:
        return True

    # First run - save and run
    last_fp = get_last_fingerprint()
    if last_fp is None:
        save_fingerprint(current_fp)
        return False

    # Same fingerprint - skip
    if current_fp == last_fp:
        return True

    # Different fingerprint - save and run
    save_fingerprint(current_fp)
    return False


def main() -> int:
    if repo_is_clean():
        return 0

    if should_skip_hook():
        print("⏭️  Skipping lint+tests (no new changes)", file=sys.stderr)
        return 0

    print("🔧 Running lint and tests (new changes detected)", file=sys.stderr)

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
