#!/usr/bin/env python3
"""Sequential compile + direct test execution paths.

Extracted from ``ci/meson/runner.py``. The sequential path is used when a
specific test is requested by name: we compile that one target (with fuzzy
fallback resolution), then either skip to IWYU-check exit or invoke the
resulting test executable directly. ``meson test`` is bypassed to avoid
its target-discovery layer for single-test runs.
"""

import os
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, cast

from running_process import RunningProcess

from ci.meson.build_timer import BuildTimer
from ci.meson.cache_utils import save_test_result_state
from ci.meson.compile import CompileResult, _is_compilation_error, compile_meson
from ci.meson.output import print_banner, print_error, print_success
from ci.meson.phase_tracker import PhaseTracker
from ci.meson.runner_helpers import _apply_phase_timing, _write_failure_log
from ci.meson.test_discovery import get_fuzzy_test_candidates
from ci.meson.test_execution import MesonTestResult
from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.output_formatter import TimestampFormatter
from ci.util.timestamp_print import ts_print as _ts_print


@dataclass
class SequentialCompileContext:
    """Parameters for the sequential compile path."""

    source_dir: Path
    build_dir: Path
    build_mode: str
    verbose: bool
    test_name: Optional[str]
    meson_test_name: Optional[str]
    fallback_test_name: Optional[str]
    fuzzy_candidates: list[str]
    log_failures: Optional[Path]
    start_time: float
    build_timer: BuildTimer
    phase_tracker: PhaseTracker


@dataclass
class SequentialCompileResult:
    """Outcome of the sequential compile attempt.

    On success, ``meson_test_name`` is the resolved target name to launch.
    On failure, ``error_result`` is the MesonTestResult to return immediately.
    """

    success: bool
    meson_test_name: Optional[str] = None
    error_result: Optional[MesonTestResult] = None


def _resolve_compile_candidates(ctx: SequentialCompileContext) -> list[str]:
    """Compose the ordered candidate list to try compiling."""
    compile_target = ctx.meson_test_name
    targets_to_try: list[str] = [compile_target] if compile_target else []
    if ctx.fallback_test_name and ctx.fallback_test_name != compile_target:
        targets_to_try.append(ctx.fallback_test_name)

    fuzzy = ctx.fuzzy_candidates
    if ctx.test_name and not fuzzy:
        fuzzy = get_fuzzy_test_candidates(ctx.build_dir, ctx.test_name)
    for c in fuzzy:
        if c not in targets_to_try:
            targets_to_try.append(c)

    # Path-qualified variants for disambiguation: "tests/<name>", "tests/profile/<name>"
    path_qualified: list[str] = []
    for candidate in targets_to_try:
        if candidate and "/" not in candidate:
            for prefix in ["tests/", "tests/profile/"]:
                qualified = f"{prefix}{candidate}"
                if qualified not in targets_to_try and qualified not in path_qualified:
                    path_qualified.append(qualified)
    targets_to_try.extend(path_qualified)
    return targets_to_try


def _print_compile_failure(
    *,
    ctx: SequentialCompileContext,
    last_result: Optional[CompileResult],
    last_error_output: str,
    has_fallbacks: bool,
    targets_to_try: list[str],
    all_suppressed_errors: list[str],
) -> None:
    """Print the failure header + error excerpt + diagnostics for a failed compile."""
    last_error_file = (
        ctx.build_dir.parent / "meson-state" / "last-compilation-error.txt"
    )
    phase_data = ctx.phase_tracker.get_phase()
    phase_name = phase_data.get("phase", "COMPILE") if phase_data else "COMPILE"

    print_error(f"\n[MESON] ❌ Build FAILED during {phase_name} phase")
    print_error("=" * 80)

    if last_result and last_result.error_log_file:
        print_error(f"[MESON] 📄 Full compilation log:")
        print_error(f"[MESON]    {last_result.error_log_file}")
        print_error("")

    error_snippet_shown = False

    if last_error_file.exists():
        with open(last_error_file, "r", encoding="utf-8") as f:
            error_context = f.read()
            for line in error_context.splitlines():
                if _is_compilation_error(line):
                    print_error(f"\033[91m{line}\033[0m")
                else:
                    print_error(line)
        error_snippet_shown = True

    elif (
        last_result
        and last_result.error_log_file
        and last_result.error_log_file.exists()
    ):
        print_error("[MESON] 🔍 Error excerpt from compilation log:")
        print_error("")
        try:
            with open(last_result.error_log_file, "r", encoding="utf-8") as f:
                log_lines = f.readlines()
                error_lines = [
                    (i, line.rstrip())
                    for i, line in enumerate(log_lines)
                    if _is_compilation_error(line)
                ]
                if error_lines:
                    last_error_idx = error_lines[-1][0]
                    start_idx = max(0, last_error_idx - 5)
                    end_idx = min(len(log_lines), last_error_idx + 5)
                    for line in log_lines[start_idx:end_idx]:
                        line = line.rstrip()
                        if _is_compilation_error(line):
                            print_error(f"\033[91m{line}\033[0m")
                        else:
                            print_error(line)
                    error_snippet_shown = True
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            print_error(f"[MESON] ⚠️  Could not read error log: {e}")

    if error_snippet_shown:
        print_error("=" * 80)
    else:
        if all_suppressed_errors:
            print_error(
                "[MESON] Self-healing triggered - showing suppressed validation errors:"
            )
            for err in all_suppressed_errors[:5]:
                print_error(f"  {err}")
            print_error("")

        print_error(f"[MESON] Compilation failed for test '{ctx.test_name}'")
        if has_fallbacks:
            tried = ", ".join(str(t) for t in targets_to_try)
            print_error(f"[MESON] Tried targets: {tried}")
        if last_error_output:
            for line in last_error_output.splitlines():
                if "ERROR:" in line or "Can't invoke target" in line:
                    print_error(f"[MESON] {line.strip()}")
                    break
        print_error("=" * 80)


def run_sequential_compile(
    ctx: SequentialCompileContext,
) -> SequentialCompileResult:
    """Run the sequential compile path (target resolution + compile + diagnostics)."""
    targets_to_try = _resolve_compile_candidates(ctx)

    _ts_print(f"[BUILD] Building FastLED engine ({ctx.build_mode} mode)...")

    has_fallbacks = len(targets_to_try) > 1
    compilation_success = False
    successful_target: Optional[str] = None
    last_error_output = ""
    last_result: Optional[CompileResult] = None
    all_suppressed_errors: list[str] = []
    resolved_meson_test_name = ctx.meson_test_name

    for candidate in targets_to_try:
        ctx.phase_tracker.set_phase(
            "COMPILE",
            test_name=ctx.test_name,
            target=candidate,
            path="sequential",
        )
        result = compile_meson(
            ctx.build_dir,
            target=candidate,
            quiet=has_fallbacks,
            verbose=ctx.verbose,
        )
        last_result = result
        compilation_success = result.success
        if compilation_success:
            ctx.phase_tracker.set_phase(
                "LINK",
                test_name=ctx.test_name,
                target=candidate,
                path="sequential",
            )
            successful_target = candidate
            resolved_meson_test_name = (
                candidate.removeprefix("tests/") if candidate else candidate
            )
            break
        last_error_output = result.error_output
        all_suppressed_errors.extend(result.suppressed_errors)

    ctx.build_timer.checkpoint("compile_done")

    if compilation_success and has_fallbacks:
        print_banner("Compile", "📦", verbose=ctx.verbose)
        print(f"Compiling: {successful_target}")

    if compilation_success:
        return SequentialCompileResult(
            success=True, meson_test_name=resolved_meson_test_name
        )

    _print_compile_failure(
        ctx=ctx,
        last_result=last_result,
        last_error_output=last_error_output,
        has_fallbacks=has_fallbacks,
        targets_to_try=targets_to_try,
        all_suppressed_errors=all_suppressed_errors,
    )

    if ctx.log_failures is not None:
        compile_content = last_error_output
        if (
            last_result
            and last_result.error_log_file
            and last_result.error_log_file.exists()
        ):
            try:
                compile_content = last_result.error_log_file.read_text(
                    encoding="utf-8", errors="replace"
                )
            except OSError:
                pass
        _write_failure_log(
            ctx.log_failures,
            ctx.test_name or "unknown",
            "compile",
            compile_content,
        )

    return SequentialCompileResult(
        success=False,
        error_result=MesonTestResult(
            success=False,
            duration=time.time() - ctx.start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        ),
    )


@dataclass
class DirectTestContext:
    """Parameters for direct test executable invocation."""

    source_dir: Path
    build_dir: Path
    meson_test_name: str
    build_mode: str
    verbose: bool
    log_failures: Optional[Path]
    start_time: float
    build_timer: BuildTimer
    phase_tracker: PhaseTracker


def _resolve_test_command(
    build_dir: Path, meson_test_name: str, verbose: bool
) -> tuple[list[str], Optional[Path]]:
    """Find the test executable / DLL and return (cmd, artifact_path).

    Returns ([], None) if no executable could be located.
    """
    # Profile tests (tests/profile/) — any name (not just profile_* prefix)
    profile_exe_path = build_dir / "tests" / "profile" / f"{meson_test_name}.exe"
    if profile_exe_path.exists():
        return [str(profile_exe_path)], profile_exe_path
    profile_exe_unix = build_dir / "tests" / "profile" / meson_test_name
    if profile_exe_unix.exists():
        return [str(profile_exe_unix)], profile_exe_unix

    # Copied runner .exe (e.g. fl_async.exe) — DLL preferred as artifact
    test_exe_path = build_dir / "tests" / f"{meson_test_name}.exe"
    if test_exe_path.exists():
        dll_candidate = build_dir / "tests" / f"{meson_test_name}.dll"
        artifact = dll_candidate if dll_candidate.exists() else test_exe_path
        return [str(test_exe_path)], artifact

    # Unix bare-name variant
    test_exe_unix = build_dir / "tests" / meson_test_name
    if test_exe_unix.exists():
        return [str(test_exe_unix)], test_exe_unix

    # DLL-based architecture: shared runner + test.{dll,so,dylib}
    runner_name = "runner.exe" if os.name == "nt" else "runner"
    runner_exe = build_dir / "tests" / runner_name
    if not runner_exe.exists():
        compile_meson(build_dir, target="runner", quiet=True, verbose=verbose)
    if runner_exe.exists():
        for ext in (".dll", ".so", ".dylib"):
            candidate = build_dir / "tests" / f"{meson_test_name}{ext}"
            if candidate.exists():
                return [str(runner_exe), str(candidate)], candidate

    return [], None


def _make_direct_test_env(
    source_dir: Path, build_dir: Path, build_mode: str
) -> dict[str, str]:
    """Build env for the direct test subprocess (PATH/LD_LIBRARY_PATH + ASAN)."""
    test_env = os.environ.copy()
    fastled_lib_dir = str(build_dir / "ci" / "meson" / "native")
    if os.name == "nt":
        from clang_tool_chain import get_runtime_dll_paths

        extra = os.pathsep.join([fastled_lib_dir] + get_runtime_dll_paths())
        test_env["PATH"] = extra + os.pathsep + test_env.get("PATH", "")
    else:
        test_env["LD_LIBRARY_PATH"] = (
            fastled_lib_dir + os.pathsep + test_env.get("LD_LIBRARY_PATH", "")
        )
    if "FASTLED_TEST_TIMEOUT" not in test_env:
        test_env["FASTLED_TEST_TIMEOUT"] = "60"
    # ASAN's vectored exception handler conflicts with our crash handler on Windows.
    if build_mode == "debug" and os.name == "nt":
        test_env["FASTLED_DISABLE_CRASH_HANDLER"] = "1"
    # source_dir intentionally unused — kept for signature symmetry
    _ = source_dir
    return test_env


def _print_direct_test_failure(
    ctx: DirectTestContext,
    proc: RunningProcess,
    returncode: int,
) -> None:
    """Print phase-aware failure messages + write error log for a failed direct test."""
    phase_data = ctx.phase_tracker.get_phase()

    if phase_data and phase_data["phase"] == "EXECUTE":
        print_error(f"[MESON] ❌ Test FAILED during execution (exit code {returncode})")
        print_error(f"[MESON] Test: {ctx.meson_test_name}")

        compile_errors_dir = ctx.build_dir.parent / "compile-errors"
        compile_errors_dir.mkdir(exist_ok=True)
        test_name_slug = ctx.meson_test_name.replace("test_", "").replace("/", "_")
        error_log_path = compile_errors_dir / f"{test_name_slug}.log"

        with open(error_log_path, "w", encoding="utf-8") as f:
            f.write(f"# Test: {ctx.meson_test_name}\n")
            f.write(f"# Exit code: {returncode}\n")
            f.write(f"# Full test output below:\n\n")
            f.write("--- Test Output ---\n\n")
            f.write(str(proc.stdout))

        print_error(f"[MESON] 📄 Full test output: {error_log_path}")

        stdout_lower = str(proc.stdout).lower()
        has_doctest_output = any(
            pattern in stdout_lower
            for pattern in [
                "test cases:",
                "assertions:",
                "[doctest]",
                "test case failed",
            ]
        )
        has_watchdog_timeout = "internal timeout watchdog triggered" in stdout_lower

        if returncode == 1 and not proc.stdout.strip():
            print_error("[MESON] ⚠️  No output - possible crash at startup")
        elif returncode == 1 and has_watchdog_timeout and not has_doctest_output:
            print_error(
                "[MESON] ⚠️  Test killed by internal timeout watchdog (exceeded time limit)"
            )
            print_error(
                "[MESON] 💡 This is a timeout, not a crash. Consider adding to slow_tests in tests/meson.build"
            )
        elif returncode == 1 and not has_doctest_output:
            print_error(
                "[MESON] ⚠️  Test failed during initialization (before doctest ran)"
            )
            error_lines = [
                line.strip()
                for line in str(proc.stdout).splitlines()
                if "error" in line.lower() and line.strip()
            ]
            if error_lines:
                print_error("[MESON] 🔍 Error messages found in output:")
                for error_line in error_lines[:10]:
                    print_error(f"[MESON]    {error_line}")
                if len(error_lines) > 10:
                    print_error(
                        f"[MESON]    ... ({len(error_lines) - 10} more error(s))"
                    )
            print_error("[MESON] 💡 Possible causes:")
            print_error(
                "[MESON]    - Static initialization failure (global object constructor crash)"
            )
            print_error(
                "[MESON]    - Test registration issue (FL_TEST_CASE macro problem)"
            )
            print_error(
                "[MESON]    - DLL loading failure (missing dependency or symbol)"
            )
            print_error(
                "[MESON]    - All test cases disabled with #if 0 (check test file)"
            )
    else:
        phase_name = phase_data.get("phase", "UNKNOWN") if phase_data else "UNKNOWN"
        print_error(
            f"[MESON] ❌ Build FAILED during {phase_name} phase (exit code {returncode})"
        )

    if not ctx.verbose and proc.stdout:
        print_error("[MESON] Test output:")
        for line in str(proc.stdout).splitlines()[-50:]:
            _ts_print(f"  {line}")


def run_direct_test(ctx: DirectTestContext) -> MesonTestResult:
    """Locate and execute a single test executable; return its MesonTestResult."""
    test_cmd, artifact_path = _resolve_test_command(
        ctx.build_dir, ctx.meson_test_name, ctx.verbose
    )

    if not test_cmd:
        _ts_print(
            f"[MESON] Error: test executable not found for: {ctx.meson_test_name}",
            file=sys.stderr,
        )
        return MesonTestResult(
            success=False,
            duration=time.time() - ctx.start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    print_banner("Test", "▶️", verbose=ctx.verbose)
    print(f"Running: {ctx.meson_test_name}")
    ctx.phase_tracker.set_phase(
        "EXECUTE", test_name=ctx.meson_test_name, path="sequential"
    )

    try:
        test_env = _make_direct_test_env(ctx.source_dir, ctx.build_dir, ctx.build_mode)
        proc = RunningProcess(
            test_cmd,
            cwd=ctx.source_dir,
            timeout=600,
            auto_run=True,
            check=False,
            env=test_env,
            output_formatter=TimestampFormatter(),
        )
        is_profile_test = artifact_path and "profile" in str(artifact_path)
        echo_callback = ctx.verbose or is_profile_test
        test_start = time.time()
        returncode = cast(int, proc.wait(echo=bool(echo_callback)))
        test_duration = time.time() - test_start
        duration = time.time() - ctx.start_time
        ctx.build_timer.checkpoint("test_execution_done")

        if returncode != 0:
            _print_direct_test_failure(ctx, proc, returncode)
            if artifact_path is not None:
                save_test_result_state(
                    ctx.build_dir, ctx.meson_test_name, artifact_path, passed=False
                )
            if ctx.log_failures is not None and ctx.meson_test_name:
                _write_failure_log(
                    ctx.log_failures, ctx.meson_test_name, "run", str(proc.stdout)
                )
            return MesonTestResult(
                success=False,
                duration=duration,
                num_tests_run=1,
                num_tests_passed=0,
                num_tests_failed=1,
                failed_test_names=[ctx.meson_test_name] if ctx.meson_test_name else [],
            )

        ctx.phase_tracker.clear()
        if artifact_path is not None:
            save_test_result_state(
                ctx.build_dir, ctx.meson_test_name, artifact_path, passed=True
            )
        build_duration = duration - test_duration
        _ts_print(ctx.build_timer.format_table())
        print_success(
            f"✅ All tests passed (1/1 in {test_duration:.2f}s, build: {build_duration:.1f}s)"
        )
        result = MesonTestResult(
            success=True,
            duration=duration,
            num_tests_run=1,
            num_tests_passed=1,
            num_tests_failed=0,
        )
        return _apply_phase_timing(result, ctx.build_timer)

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        duration = time.time() - ctx.start_time
        _ts_print(
            f"[MESON] Test execution failed with exception: {e}",
            file=sys.stderr,
        )
        return MesonTestResult(
            success=False,
            duration=duration,
            num_tests_run=1,
            num_tests_passed=0,
            num_tests_failed=1,
        )
