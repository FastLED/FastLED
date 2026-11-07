#!/usr/bin/env python3
"""
Claude Code Stop hook that runs lint and C++ tests concurrently.

Smart mode: Only runs if files were actually changed during THIS session.
Session fingerprint is captured at session start (check-on-start.py) and
compared here. If nothing changed during the session, lint and tests are
skipped.

Path-filtered mode (issue #2609):
  - `bash test --cpp` is skipped unless changes touched src/, tests/,
    examples/, or top-level build files (CMakeLists.txt, meson.build,
    meson_options.txt).
  - `bash lint` is invoked with `--skip-meson` unless a meson.build or
    meson_options.txt file changed this session.
  - Per-stage timings are emitted to stderr on every real run so future
    regressions are easy to triage.

Usage: Receives JSON on stdin from Claude Code Stop hook.
Exit codes:
  0 - Both passed, skipped (no changes during session), or released after
      repeated identical blocks on an unchanged tree
  2 - Lint or test failures (stderr fed back to Claude)
"""

import hashlib
import json
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import cast

from running_process import PIPE, CompletedProcess, RunningProcess


SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent
SESSION_FINGERPRINT_FILE = PROJECT_ROOT / ".cache" / "session_fingerprint.json"
BLOCK_STATE_DIR = PROJECT_ROOT / ".cache" / "stop_hook_blocks"
MAX_IDENTICAL_BLOCKS = 2
FIX_DIRECTIVE = """
REMINDER - FIX ALL ERRORS FROM STOP HOOK, MAKE AT LEAST TWO ATTEMPTS!
Your next action must be a tool call that edits the failing code, not a status reply.
Fix these errors even if you did not cause them or the file has uncommitted edits
that may be another session's: edit in place, which keeps those edits, and leave
them uncommitted if they are not yours. Lint failures are never a reason to wait.
"""
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


@dataclass
class ChangeBuckets:
    """Categorization of files changed during the session."""

    src: bool = False
    tests: bool = False
    examples: bool = False
    build: bool = False  # CMakeLists.txt, meson.build, meson_options.txt
    meson: bool = False  # meson.build / meson_options.txt only
    other: bool = False  # ci/, agents/, docs/, .github/, anything else
    files: list[str] = field(default_factory=lambda: list[str]())

    @property
    def needs_cpp_test(self) -> bool:
        """C++ tests must run when source/test/example/build files changed."""
        return self.src or self.tests or self.examples or self.build

    @property
    def needs_meson_lint(self) -> bool:
        """Meson lint stage only needs to run when meson files changed."""
        return self.meson

    @property
    def has_any(self) -> bool:
        return bool(self.files)


def run_cmd(cmd: list[str]) -> CompletedProcess[str]:
    return RunningProcess.run(
        cmd,
        stdout=PIPE,
        stderr=PIPE,
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


def report_failure(label: str, result: CompletedProcess[str]) -> None:
    print(f"{label}:", file=sys.stderr)
    if result.stdout.strip():
        print(result.stdout.strip(), file=sys.stderr)
    if result.stderr.strip():
        print(result.stderr.strip(), file=sys.stderr)


def _parse_porcelain_paths(porcelain: str) -> list[str]:
    """Extract file paths from `git status --porcelain` output.

    Handles rename entries ("R  old -> new") by returning the new path, and
    strips the leading 3-character status field.
    """
    paths: list[str] = []
    for line in porcelain.splitlines():
        if len(line) < 4:
            continue
        # Format: XY <path>  (or "R  old -> new" for renames)
        rest = line[3:]
        if " -> " in rest:
            rest = rest.split(" -> ", 1)[1]
        # Strip optional surrounding quotes that git uses for paths with
        # special characters
        rest = rest.strip()
        if rest.startswith('"') and rest.endswith('"'):
            rest = rest[1:-1]
        if rest:
            paths.append(rest)
    return paths


def classify_changes(porcelain: str) -> ChangeBuckets:
    """Bucket the paths from `git status --porcelain` by directory/role.

    A path may contribute to multiple buckets (e.g. tests/meson.build sets
    tests, build, and meson). Anything outside src/, tests/, examples/, or a
    recognized build file goes into `other` (ci/, agents/, docs/, .github/, ...).
    """
    buckets = ChangeBuckets()
    for path in _parse_porcelain_paths(porcelain):
        buckets.files.append(path)

        # Normalize Windows-style backslashes that may appear in `git status`
        # output on some configurations.
        norm = path.replace("\\", "/")
        base = norm.rsplit("/", 1)[-1]

        is_src = norm.startswith("src/")
        is_tests = norm.startswith("tests/")
        is_examples = norm.startswith("examples/")
        is_meson_file = base == "meson.build" or base == "meson_options.txt"
        is_cmake_file = base == "CMakeLists.txt"
        # `is_build` only flips for TOP-LEVEL build files. A nested
        # `docs/meson.build` (or similar) must NOT force the expensive
        # C++ test phase to run. We keep `is_meson_file` broad so the
        # cheaper meson-lint stage still runs whenever any meson file
        # changes, but C++ tests are gated on root-only build files.
        is_top_level = "/" not in norm
        is_build = is_top_level and (is_meson_file or is_cmake_file)

        if is_src:
            buckets.src = True
        if is_tests:
            buckets.tests = True
        if is_examples:
            buckets.examples = True
        if is_build:
            buckets.build = True
        if is_meson_file:
            buckets.meson = True
        if not (is_src or is_tests or is_examples or is_build):
            buckets.other = True
    return buckets


class GitPorcelainError(RuntimeError):
    """`git status --porcelain` exited non-zero."""


def get_porcelain() -> str | None:
    """Return `git status --porcelain` output, or None if the tree is clean.

    Raises:
        GitPorcelainError: if the underlying ``git status`` invocation fails.
            Per coding guidelines, we never silently fall back on missing
            critical resources; a broken Git invocation must surface as a
            hook failure rather than being collapsed into the clean-tree
            "skip" path.
    """
    result = run_cmd(["git", "status", "--porcelain"])
    if result.returncode != 0:
        raise GitPorcelainError(
            f"git status --porcelain failed (exit {result.returncode}): "
            f"{(result.stderr or '').strip() or '(no stderr)'}"
        )
    if not result.stdout.strip():
        return None
    return result.stdout


def get_current_fingerprint(porcelain: str | None = None) -> str | None:
    """Get MD5 fingerprint of current git status.

    Returns None only when the tree is genuinely clean. Git failures
    propagate as ``GitPorcelainError`` from ``get_porcelain()``.
    """
    if porcelain is None:
        porcelain = get_porcelain()
    if porcelain is None:
        return None
    return hashlib.md5(porcelain.encode()).hexdigest()


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


def should_skip_hook(porcelain: str | None) -> bool:
    """Check if hook should skip based on session fingerprints.

    ``porcelain is None`` means the working tree is genuinely clean (callers
    must distinguish a clean tree from a Git failure before reaching here -
    Git failures are raised as ``GitPorcelainError`` by ``get_porcelain()``).
    """
    # No changes at all right now - skip
    if porcelain is None:
        return True

    current_fp = get_current_fingerprint(porcelain)
    # current_fp is only None when porcelain is None, which is handled above.
    # Belt-and-braces: skip if for any reason the fingerprint is missing.
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
    """Count directories under .claude/worktrees/.

    Iterates with an explicit loop so failures surface a descriptive error
    on stderr (per coding-standards "no silent fallbacks") while still
    keeping the stop-hook itself non-fatal — the loop body propagates the
    underlying OSError up.
    """
    if not WORKTREES_DIR.exists():
        return 0
    try:
        iterator = WORKTREES_DIR.iterdir()
    except KeyboardInterrupt:
        import _thread

        _thread.interrupt_main()
        raise
    except OSError as exc:
        raise OSError(
            f"count_stale_worktrees: cannot iterate {WORKTREES_DIR}: {exc}"
        ) from exc
    count = 0
    for p in iterator:
        if p.is_dir() and not p.is_symlink():
            count += 1
    return count


def warn_stale_worktrees() -> None:
    try:
        n = count_stale_worktrees()
    except KeyboardInterrupt:
        import _thread

        _thread.interrupt_main()
        raise
    except OSError as exc:
        # Don't crash the stop hook on a directory-scan failure, but do
        # surface the descriptive error so it isn't silently swallowed.
        print(f"warn_stale_worktrees: {exc}", file=sys.stderr)
        return
    if n > STALE_WORKTREE_THRESHOLD:
        print(
            f"Note: {n} stale agent worktrees in .claude/worktrees/, "
            "run `bash clean-worktrees` to reclaim space",
            file=sys.stderr,
        )


def _fmt_seconds(seconds: float) -> str:
    return f"{seconds:.1f}s"


def _read_stop_payload() -> dict[str, object]:
    """Parse the Stop hook JSON from stdin; empty when run by hand."""
    if sys.stdin is None or sys.stdin.isatty():
        return {}
    try:
        payload = json.loads(sys.stdin.read() or "{}")
    except (OSError, ValueError):
        return {}
    return payload if isinstance(payload, dict) else {}


def _tree_digest() -> str | None:
    """Hash tracked content and the status list, so any edit changes it."""
    status = run_cmd(["git", "status", "--porcelain"])
    diff = run_cmd(["git", "diff", "HEAD"])
    untracked = run_cmd(["git", "ls-files", "--others", "--exclude-standard", "-z"])
    if status.returncode != 0 or diff.returncode != 0 or untracked.returncode != 0:
        return None
    digest = hashlib.sha256((status.stdout + "\0" + diff.stdout).encode())
    for name in sorted(filter(None, untracked.stdout.split("\0"))):
        file_hash = hashlib.sha256()
        try:
            with (PROJECT_ROOT / name).open("rb") as handle:
                while chunk := handle.read(1024 * 1024):
                    file_hash.update(chunk)
            record = file_hash.hexdigest()
        except OSError:
            record = "unreadable"
        digest.update(f"{name}\0{record}\0".encode())
    return digest.hexdigest()


def _block_state_file(session_id: object) -> Path:
    """One file per session, so sessions sharing a checkout keep separate counts."""
    name = hashlib.sha256(str(session_id).encode()).hexdigest()[:16]
    return BLOCK_STATE_DIR / f"{name}.json"


def _load_block_state(session_id: object) -> dict[str, object]:
    try:
        state = json.loads(_block_state_file(session_id).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}
    return state if isinstance(state, dict) else {}


def should_release(
    active: object, session_id: object, digest: str | None, state: dict[str, object]
) -> bool:
    """Release only a repeat stop on a tree already blocked MAX_IDENTICAL_BLOCKS times.

    Without this, a failure the agent will not or cannot change blocks forever.
    Any edit changes the digest and earns the full checks again.
    """
    if active is not True or not session_id or digest is None:
        return False
    count = state.get("count")
    return (
        state.get("session_id") == session_id
        and state.get("digest") == digest
        and isinstance(count, int)
        and count >= MAX_IDENTICAL_BLOCKS
    )


def next_block_state(
    session_id: object, digest: str | None, state: dict[str, object]
) -> dict[str, object]:
    count = state.get("count")
    repeat = (
        state.get("session_id") == session_id
        and state.get("digest") == digest
        and isinstance(count, int)
    )
    return {
        "session_id": session_id,
        "digest": digest,
        "count": count + 1 if repeat else 1,
    }


def main() -> int:
    payload = _read_stop_payload()
    session_id = payload.get("session_id")
    state = _load_block_state(session_id)
    # Count blocks against the tree that was checked, not one lint reformatted.
    digest = _tree_digest()
    if should_release(payload.get("stop_hook_active"), session_id, digest, state):
        print(
            json.dumps(
                {
                    "systemMessage": (
                        "UNRESOLVED stop-hook failures: the working tree is "
                        f"unchanged after {MAX_IDENTICAL_BLOCKS} identical blocks, "
                        "so this stop was released. The failures remain; "
                        "report them to the user."
                    )
                }
            )
        )
        return 0
    code = run_checks()
    state_file = _block_state_file(session_id)
    try:
        if code == 2:
            state_file.parent.mkdir(parents=True, exist_ok=True)
            state_file.write_text(
                json.dumps(next_block_state(session_id, digest, state)),
                encoding="utf-8",
            )
        else:
            state_file.unlink(missing_ok=True)
    except OSError as exc:
        # Bookkeeping must never turn a blocking result into a hook crash.
        print(f"[stop-hook] could not update block state: {exc}", file=sys.stderr)
    return code


def run_checks() -> int:
    try:
        porcelain = get_porcelain()
    except GitPorcelainError as exc:
        # Fail fast with a descriptive error instead of silently treating a
        # broken git invocation as "clean tree, nothing to do". The hook
        # depends on `git status --porcelain` to know what changed; if it
        # cannot run, we cannot safely decide whether to skip lint/tests.
        print(f"[stop-hook] FATAL: {exc}", file=sys.stderr)
        return 2
    if should_skip_hook(porcelain):
        print("Skipping lint+tests (no changes during this session)", file=sys.stderr)
        warn_stale_worktrees()
        return 0

    # porcelain is guaranteed non-None here (should_skip_hook returned False)
    assert porcelain is not None
    buckets = classify_changes(porcelain)

    run_tests_phase = buckets.needs_cpp_test
    run_meson_stage = buckets.needs_meson_lint

    test_skip_reason = ""
    if not run_tests_phase:
        # Describe why we are skipping tests so the timing line is self-explanatory
        if buckets.has_any:
            test_skip_reason = "no src/tests/examples/build changes"
        else:
            test_skip_reason = "no changes detected"

    print(
        f"Running lint{' + C++ tests' if run_tests_phase else ''} "
        f"(session changes: "
        f"src={'Y' if buckets.src else 'N'} "
        f"tests={'Y' if buckets.tests else 'N'} "
        f"examples={'Y' if buckets.examples else 'N'} "
        f"build={'Y' if buckets.build else 'N'} "
        f"other={'Y' if buckets.other else 'N'})",
        file=sys.stderr,
    )

    lint_done = threading.Event()
    lint_results: list[CompletedProcess[str]] = []
    test_proc_holder: list[RunningProcess] = []
    test_results: list[CompletedProcess[str]] = []
    lint_duration: list[float] = []
    test_duration: list[float] = []
    # Signalled once the test process has either been spawned and registered
    # in test_proc_holder OR the thread has exited without spawning (e.g. the
    # spawn raised). The lint-failure cancellation path waits on this so it cannot
    # race past the start of run_tests() and miss the spawned child.
    test_ready = threading.Event()

    def run_lint() -> None:
        lint_cmd: list[str] = ["uv", "run", "ci/lint.py"]
        if not run_meson_stage:
            lint_cmd.append("--skip-meson")
        t0 = time.perf_counter()
        result = run_cmd(lint_cmd)
        lint_duration.append(time.perf_counter() - t0)
        lint_results.append(result)
        lint_done.set()

    def run_tests() -> None:
        t0 = time.perf_counter()
        try:
            test_cmd = ["uv", "run", "test.py", "--cpp"]
            proc = RunningProcess(
                test_cmd,
                capture=True,
                stderr=PIPE,
                encoding="utf-8",
                errors="replace",
                cwd=PROJECT_ROOT,
            )
            test_proc_holder.append(proc)
        finally:
            # Always signal readiness so the lint-failure cancellation path
            # never blocks forever on a spawn that raised before producing a
            # process handle.
            test_ready.set()
        returncode = cast(int, proc.wait())
        stdout = str(proc.stdout)
        stderr = str(proc.stderr)
        test_duration.append(time.perf_counter() - t0)
        test_results.append(CompletedProcess(test_cmd, returncode, stdout, stderr))

    overall_start = time.perf_counter()

    lint_thread = threading.Thread(target=run_lint)
    lint_thread.start()

    test_thread: threading.Thread | None = None
    if run_tests_phase:
        test_thread = threading.Thread(target=run_tests)
        test_thread.start()

    # Wait for lint first
    lint_thread.join()
    lint_result = lint_results[0]
    lint_secs = lint_duration[0] if lint_duration else 0.0

    def _emit_timing(test_phase_secs: float | None, test_phase_note: str = "") -> None:
        total = time.perf_counter() - overall_start
        if test_phase_secs is not None:
            test_part = _fmt_seconds(test_phase_secs)
        else:
            test_part = "0s"
        suffix = f" ({test_phase_note})" if test_phase_note else ""
        meson_note = "" if run_meson_stage else " (meson stage skipped)"
        print(
            f"[stop-hook] lint={_fmt_seconds(lint_secs)}{meson_note} "
            f"test={test_part}{suffix} total={_fmt_seconds(total)}",
            file=sys.stderr,
        )

    if lint_result.returncode != 0:
        # Lint failed - cancel tests, report lint errors only.
        # We must wait for run_tests() to publish the process handle (or finish
        # without one) before killing, otherwise we can race past the start
        # of the thread and leak a `uv run test.py --cpp` child that keeps
        # running after the hook exits.
        if test_thread is not None:
            # Bounded wait so a hung spawn cannot block the hook forever.
            test_ready.wait(timeout=10)
        for proc in test_proc_holder:
            proc.kill()
        if test_thread is not None:
            test_thread.join(timeout=5)
        report_failure("Lint failed", lint_result)
        _emit_timing(
            test_phase_secs=test_duration[0] if test_duration else None,
            test_phase_note="cancelled (lint failed)"
            if test_thread
            else test_skip_reason,
        )
        print(FIX_DIRECTIVE, file=sys.stderr)
        warn_stale_worktrees()
        return 2

    # Lint passed - wait for tests if we ran them
    if test_thread is None:
        _emit_timing(
            test_phase_secs=None, test_phase_note=f"skipped, {test_skip_reason}"
        )
        return 0

    test_thread.join()
    test_result = test_results[0]
    test_secs = test_duration[0] if test_duration else 0.0

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
        _emit_timing(test_phase_secs=test_secs, test_phase_note="failed")
        print(FIX_DIRECTIVE, file=sys.stderr)
        warn_stale_worktrees()
        return 2

    _emit_timing(test_phase_secs=test_secs)
    warn_stale_worktrees()
    return 0


if __name__ == "__main__":
    sys.exit(main())
