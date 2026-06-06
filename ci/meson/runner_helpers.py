#!/usr/bin/env python3
"""Helper functions for the Meson build runner.

Extracted from ``ci/meson/runner.py`` to keep the main orchestrator under
1000 LOC. Includes timing aggregation, stale-build recovery, build-dir
cleanup with locked-file handling, failure-log writers, and zccache session
bootstrap.
"""

import os
import re
import shutil
import subprocess
import sys
import time
import uuid
from pathlib import Path
from typing import Optional

from ci.meson.build_config import cleanup_build_artifacts, setup_meson_build
from ci.meson.build_timer import BuildTimer
from ci.meson.native_launchers import _find_zccache_binary
from ci.meson.test_execution import MesonTestResult
from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.timestamp_print import ts_print as _ts_print


def _apply_phase_timing(
    result: MesonTestResult, build_timer: BuildTimer
) -> MesonTestResult:
    """Apply phase timing data from BuildTimer to MesonTestResult."""
    build_timer.calculate_phases()
    result.meson_setup_time = build_timer.meson_setup_time
    result.ninja_maintenance_time = build_timer.ninja_maintenance_time
    result.compile_time = build_timer.compile_time
    result.test_execution_time = build_timer.test_execution_time
    return result


def _apply_compile_sub_phases(
    result: MesonTestResult, sub_phases: dict[str, float]
) -> None:
    """Calculate compile sub-phase durations from timestamps and set on result."""
    if not sub_phases:
        return

    compile_start = sub_phases.get("compile_start")
    core_done = sub_phases.get("core_done")
    example_link_start = sub_phases.get("example_link_start")
    test_link_start = sub_phases.get("test_link_start")
    compile_done = sub_phases.get("compile_done")

    if compile_start and core_done:
        result.compile_core_time = core_done - compile_start
    if core_done and compile_done:
        link_start = None
        if example_link_start and test_link_start:
            link_start = min(example_link_start, test_link_start)
        elif example_link_start:
            link_start = example_link_start
        elif test_link_start:
            link_start = test_link_start

        if link_start:
            result.compile_objects_time = link_start - core_done
        else:
            result.compile_objects_time = compile_done - core_done

    if example_link_start and compile_done:
        end = test_link_start if test_link_start else compile_done
        result.compile_example_link_time = end - example_link_start
    if test_link_start and compile_done:
        result.compile_test_link_time = compile_done - test_link_start


def _recover_stale_build(
    source_dir: Path,
    build_dir: Path,
    debug: bool,
    check: bool,
    build_mode: str,
    verbose: bool,
    enable_examples: bool = True,
) -> bool:
    """
    Attempt to recover from a stale build state.

    This is called when compilation fails due to missing/renamed/deleted files.
    It cleans stale Ninja deps, clears metadata caches, and forces Meson
    reconfiguration so the next compilation attempt has a fresh build graph.

    Args:
        source_dir: Project root directory
        build_dir: Meson build directory
        debug: Debug mode flag
        check: IWYU check flag
        build_mode: Build mode string
        verbose: Verbose output flag

    Returns:
        True if recovery setup succeeded (caller should retry compilation),
        False if recovery failed
    """
    _ts_print("=" * 80)
    _ts_print("[MESON] 🔧 SELF-HEALING: Stale build state detected - auto-recovering")
    _ts_print("=" * 80)
    _ts_print("[MESON] This typically happens when files are renamed or deleted.")
    _ts_print("[MESON] Cleaning stale dependencies and reconfiguring...")

    try:
        # Step 1: Clean stale ninja deps using ninja -t cleandead
        ninja_exe = shutil.which("ninja") or str(
            Path(sys.prefix) / "Scripts" / "ninja.EXE"
        )
        try:
            result = subprocess.run(
                [ninja_exe, "-C", str(build_dir), "-t", "cleandead"],
                capture_output=True,
                text=True,
                timeout=60,
            )
            if result.returncode == 0:
                _ts_print(
                    f"[MESON] 🗑️  Cleaned stale Ninja outputs: {result.stdout.strip()}"
                )
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            _ts_print(f"[MESON] Warning: ninja cleandead failed: {e}")

        # Step 2: Delete .ninja_deps to force full dependency re-scan
        ninja_deps = build_dir / ".ninja_deps"
        if ninja_deps.exists():
            try:
                ninja_deps.unlink()
                _ts_print("[MESON] 🗑️  Deleted stale .ninja_deps")
            except OSError as e:
                _ts_print(f"[MESON] Warning: Could not delete .ninja_deps: {e}")

        # Step 3: Clear metadata caches
        cache_files = [
            build_dir / "src_metadata.cache",
            build_dir / "test_list_cache.txt",
            build_dir / ".source_files_hash",
            build_dir / "tests" / "test_metadata.cache",
            build_dir / "example_metadata.cache",
        ]
        for cache_file in cache_files:
            if cache_file.exists():
                try:
                    cache_file.unlink()
                    _ts_print(f"[MESON] 🗑️  Deleted cache: {cache_file.name}")
                except OSError as e:
                    _ts_print(
                        f"[MESON] Warning: Could not delete {cache_file.name}: {e}"
                    )

        # Step 4: Clean build artifacts (stale .obj files referencing old headers)
        cleanup_build_artifacts(build_dir, "Stale build recovery")

        # Step 5: Force Meson reconfiguration
        _ts_print("[MESON] 🔄 Forcing Meson reconfiguration...")
        success = setup_meson_build(
            source_dir,
            build_dir,
            reconfigure=True,
            debug=debug,
            check=check,
            build_mode=build_mode,
            verbose=verbose,
            enable_examples=enable_examples,
            enable_unit_tests=True,
        )

        if success:
            _ts_print("[MESON] ✅ Self-healing reconfiguration complete")
            _ts_print("[MESON] 🔄 Retrying compilation...")
        else:
            _ts_print("[MESON] ❌ Self-healing reconfiguration failed")

        return success

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        _ts_print(f"[MESON] ❌ Self-healing failed with exception: {e}")
        return False


def _purge_trash(trash_dir: Path) -> None:
    """Purge unlocked files from .trash directory."""
    if not trash_dir.exists():
        return
    for item in list(trash_dir.iterdir()):
        try:
            if item.is_file():
                item.unlink()
            elif item.is_dir():
                shutil.rmtree(item)
        except PermissionError:
            pass  # Still locked, skip


def _kill_zombie_zccache(build_dir: Path) -> None:
    """Kill zccache processes whose CWD is inside build_dir.

    When ninja is interrupted, zccache.exe wrappers can survive because they
    communicate through a daemon and are not direct children of ninja. Their
    CWD remains set to the build directory, which on Windows holds a kernel
    handle that prevents directory deletion.
    """
    if sys.platform != "win32":
        return
    try:
        import psutil  # noqa: PLC0415 - lazy import, only needed on Windows cleanup
    except ImportError:
        return
    build_str = str(build_dir.resolve())
    killed = 0
    for proc in psutil.process_iter(["pid", "name"]):
        try:
            name = proc.info["name"] or ""
            if "zccache" not in name.lower() or "daemon" in name.lower():
                continue
            cwd = proc.cwd()
            if cwd and (cwd == build_str or cwd.startswith(build_str + os.sep)):
                proc.kill()
                killed += 1
        except (psutil.NoSuchProcess, psutil.AccessDenied, OSError):
            continue
    if killed:
        _ts_print(
            f"[MESON] 🧹 Killed {killed} zombie zccache process(es) holding build directory"
        )
        time.sleep(0.5)  # Brief wait for Windows to release handles


def _safe_rmtree(build_dir: Path) -> None:
    """Remove build directory, relocating locked files to .trash/ for later cleanup."""
    _kill_zombie_zccache(build_dir)
    trash_dir = build_dir.parent / ".trash"
    _purge_trash(trash_dir)

    locked_files: list[str] = []

    def _on_exc(func, path, exc):  # noqa: ARG001
        """Handle locked files by renaming them into .trash/."""
        p = Path(path)
        trash_dir.mkdir(parents=True, exist_ok=True)
        trash_name = f"{uuid.uuid4().hex[:8]}-{p.name}"
        trash_path = trash_dir / trash_name
        try:
            p.rename(trash_path)
            locked_files.append(p.name)
        except (PermissionError, OSError):
            locked_files.append(f"{p.name} (unmovable)")

    if sys.version_info >= (3, 12):
        shutil.rmtree(build_dir, onexc=_on_exc)
    else:
        shutil.rmtree(build_dir, onerror=lambda f, p, e: _on_exc(f, p, e[1]))

    if build_dir.exists():
        try:
            shutil.rmtree(build_dir)
        except OSError:
            _ts_print(f"[MESON] ⚠️  Some locked files moved to {trash_dir}")
            for name in locked_files:
                _ts_print(f"  - {name}")


def _write_failure_log(
    log_dir: Optional[Path], name: str, suffix: str, content: str
) -> None:
    """Write a failure log file to the log_failures directory.

    Args:
        log_dir: Directory to write to (None = no-op)
        name: Test name (will be sanitized for filename)
        suffix: 'compile' or 'run'
        content: Log content
    """
    if log_dir is None:
        return
    log_dir.mkdir(parents=True, exist_ok=True)
    safe_name = name.replace(":", "_").replace("/", "_").replace("\\", "_")
    log_path = log_dir / f"{safe_name}_{suffix}.log"
    with open(log_path, "w", encoding="utf-8", errors="replace") as f:
        f.write(content)


def _extract_compile_failures(compile_output: str) -> dict[str, str]:
    """Extract per-target compilation failures from ninja output.

    Parses FAILED: lines to identify which targets had errors and collects
    the associated error output. Handles timestamped output (e.g., "7.59 FAILED: ...")
    and [code=N] prefixes.

    Returns:
        Dict mapping target name to error output text.
    """
    failures: dict[str, str] = {}
    lines = compile_output.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        if "FAILED:" in line:
            target = line.split("FAILED:", 1)[1].strip()
            target = re.sub(r"^\[code=\d+\]\s*", "", target)
            m = re.match(r"tests[/\\]([^.]+)\.", target)
            test_name = m.group(1) if m else "unknown"

            error_lines = [line]
            i += 1
            while i < len(lines):
                if "FAILED:" in lines[i] or (
                    "ninja:" in lines[i] and "build stopped" in lines[i]
                ):
                    break
                error_lines.append(lines[i])
                i += 1

            existing = failures.get(test_name, "")
            failures[test_name] = existing + "\n".join(error_lines) + "\n"
        else:
            i += 1

    return failures


def _write_testlog_failures(build_dir: Path, log_dir: Path) -> None:
    """Parse testlog.txt and write per-test failure logs.

    Args:
        build_dir: Meson build directory (contains meson-logs/testlog.txt)
        log_dir: Directory to write failure logs to
    """
    from ci.meson.testlog_parser import parse_testlog

    testlog = build_dir / "meson-logs" / "testlog.txt"
    entries = parse_testlog(testlog)
    for entry in entries:
        if "exit status 0" in entry.result:
            continue
        content = f"# Test: {entry.test_name}\n"
        content += f"# Result: {entry.result}\n"
        content += f"# Duration: {entry.duration}\n\n"
        if entry.stdout:
            content += "--- stdout ---\n" + entry.stdout + "\n\n"
        if entry.stderr:
            content += "--- stderr ---\n" + entry.stderr + "\n"
        safe_name = entry.test_name.replace(":", "_").replace("/", "_")
        _write_failure_log(log_dir, safe_name, "run", content)


def _start_zccache_session(build_dir: Path) -> None:
    """Start a zccache session with logging to the build directory.

    Sets ZCCACHE_SESSION_ID in os.environ so all subsequent compiler
    invocations through zccache use this session and get logged.
    The session auto-cleans up via PID monitoring when the process exits.

    Also sets ZCCACHE_LINK_DEPLOY_CMD so zccache invokes
    `clang-tool-chain-libdeploy` after each cache-miss link. This
    materializes runtime DLLs next to the linked binary (on Windows
    these otherwise don't get deployed because the native ctc-clang++
    trampoline skips Python post-link hooks) and lets zccache's
    side-effect scanner bundle them into the cached artifact set —
    fixing flaky error-126 DLL load failures under parallel test
    execution. See https://github.com/FastLED/FastLED/issues/2329.
    """
    if "ZCCACHE_LINK_DEPLOY_CMD" not in os.environ:
        os.environ["ZCCACHE_LINK_DEPLOY_CMD"] = "clang-tool-chain-libdeploy"
    os.environ.setdefault("ZCCACHE_STRICT_PATHS", "absolute")
    # Opt in to zccache's CLI-side probe-bypass fast-path for meson
    # configure-phase try-compiles (zackees/zccache#625, #633, #636 — bench
    # in #636 shows 7-10s saved per configure run). Not a disable: per-call
    # routing, only sub-4 KiB single-source no-PCH no-@rsp invocations
    # bypass; production TUs continue to use the cache. setdefault keeps any
    # user override intact; older zccache without this env var ignores it.
    os.environ.setdefault("ZCCACHE_PROBE_BYPASS", "1")

    zccache_bin = _find_zccache_binary()
    if not zccache_bin:
        return

    build_dir.mkdir(parents=True, exist_ok=True)
    log_path = build_dir / "zccache-session.log"
    journal_path = build_dir / "zccache-session.jsonl"

    try:
        result = subprocess.run(
            [
                zccache_bin,
                "session-start",
                "--stats",
                "--log",
                str(log_path),
                "--journal",
                str(journal_path),
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )
        if result.returncode == 0:
            import json

            output = result.stdout.strip()
            try:
                data = json.loads(output)
                session_id = str(data["session_id"])
            except (json.JSONDecodeError, KeyError):
                # Windows paths with backslashes break json.loads.
                # Extract session_id with regex as fallback.
                m = re.search(r'"session_id"\s*:\s*"([^"]+)"', output)
                if m:
                    session_id = m.group(1)
                else:
                    session_id = output
            os.environ["ZCCACHE_SESSION_ID"] = session_id
            _ts_print(f"[ZCCACHE] Session {session_id} started, log: {log_path}")
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        pass  # Non-fatal — build proceeds without session logging
