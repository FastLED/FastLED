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
from dataclasses import dataclass
from pathlib import Path


SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent
SESSION_FINGERPRINT_FILE = PROJECT_ROOT / ".cache" / "session_fingerprint.json"
WORKTREES_DIR = PROJECT_ROOT / ".claude" / "worktrees"
STALE_WORKTREE_THRESHOLD = 5


@dataclass
class FailedTest:
    """A test that failed during the on-stop hook run."""

    name: str
    category: str  # "unit" or "example"

    @property
    def debug_cmd(self) -> str:
        if self.category == "example":
            return f"bash test {self.name} --examples --debug"
        return f"bash test {self.name} --debug"


def run_cmd(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        cwd=str(PROJECT_ROOT),
    )


def extract_failed_tests(output: str) -> list[FailedTest]:
    """Extract failed test names from test runner output."""
    import re

    failed: list[FailedTest] = []
    # Match lines like "  test_name | unit     | bash test test_name --cpp"
    # from the FAILED TESTS SUMMARY table
    for line in output.splitlines():
        m = re.match(r"^\s+(\S+)\s+\|\s+(unit|example)\s+\|", line)
        if m:
            failed.append(FailedTest(name=m.group(1), category=m.group(2)))
    return failed


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


def count_stale_worktrees() -> int:
    """Count directories under .claude/worktrees/ (best-effort, never raises)."""
    if not WORKTREES_DIR.exists():
        return 0
    try:
        return sum(
            1 for p in WORKTREES_DIR.iterdir() if p.is_dir() and not p.is_symlink()
        )
    except KeyboardInterrupt:
        raise
    except OSError:
        return 0


def warn_stale_worktrees() -> None:
    n = count_stale_worktrees()
    if n > STALE_WORKTREE_THRESHOLD:
        print(
            f"Note: {n} stale agent worktrees in .claude/worktrees/, "
            "run `bash clean-worktrees` to reclaim space",
            file=sys.stderr,
        )


def main() -> int:
    if should_skip_hook():
        print(
            "⏭️  Skipping lint+tests (no changes during this session)", file=sys.stderr
        )
        warn_stale_worktrees()
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
        print(
            "\nREMINDER - FIX ALL ERRORS FROM STOP HOOK, MAKE AT LEAST TWO ATTEMPTS!",
            file=sys.stderr,
        )
        warn_stale_worktrees()
        return 2

    # Lint passed — wait for tests
    test_thread.join()
    test_result = test_results[0]

    if test_result.returncode != 0:
        report_failure("C++ tests failed", test_result)
        # Extract failed test names and emit debug rerun directive
        combined_output = (test_result.stdout or "") + (test_result.stderr or "")
        failed = extract_failed_tests(combined_output)
        if failed:
            max_show = 3
            print("", file=sys.stderr)
            print("!" * 60, file=sys.stderr)
            print(
                "!!  AGENT ACTION REQUIRED: RE-RUN IN DEBUG MODE  !!", file=sys.stderr
            )
            print("!" * 60, file=sys.stderr)
            print("", file=sys.stderr)
            print("Quick mode does NOT enable sanitizers.", file=sys.stderr)
            print(
                "You MUST re-run failed tests with --debug for full diagnostics.",
                file=sys.stderr,
            )
            print("", file=sys.stderr)
            print("Run these commands NOW:", file=sys.stderr)
            for ft in failed[:max_show]:
                print(f"  {ft.debug_cmd}", file=sys.stderr)
            if len(failed) > max_show:
                print(
                    f"  ... and {len(failed) - max_show} more failed tests",
                    file=sys.stderr,
                )
            print("", file=sys.stderr)
            print("!" * 60, file=sys.stderr)
        print(
            "\nREMINDER - FIX ALL ERRORS FROM STOP HOOK, MAKE AT LEAST TWO ATTEMPTS!",
            file=sys.stderr,
        )
        warn_stale_worktrees()
        return 2

    warn_stale_worktrees()
    return 0


if __name__ == "__main__":
    sys.exit(main())
