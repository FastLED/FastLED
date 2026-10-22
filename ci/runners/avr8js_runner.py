import os
import sys
from pathlib import Path
from typing import cast

from ci.util.test_types import TestArgs
from ci.util.timestamp_print import ts_print
from ci.util.zccache_config import show_zccache_stats


def avr8js_run_succeeded(example: str, returncode: int, output: str) -> bool:
    """Require Test's serial markers; other sketches are bounded smoke runs."""
    if example == "Test":
        return (
            returncode == 0
            and "Test setup starting" in output
            and "Test loop!" in output
        )
    if returncode == 0:
        return True
    # fbuild reports reaching an emulator duration as a failure. For sketches
    # without a completion protocol that is the intended smoke-run endpoint.
    # Accept only its terminal timeout verdict, never a build error or an
    # outer process timeout (124).
    return returncode == 1 and output.rstrip().endswith("test-emu failed: timed out")


def run_avr8js_tests(args: TestArgs) -> None:
    """Stage AVR examples and let fbuild build and emulate each project."""
    from running_process import PIPE, RunningProcess

    from ci.stage_fbuild_project import stage_fbuild_project
    from ci.util.fbuild_runner import fbuild_child_env, get_fbuild_executable
    from ci.util.global_interrupt_handler import handle_keyboard_interrupt

    if not args.run:
        ts_print("Error: --run requires a board (e.g., uno)")
        sys.exit(1)

    board = args.run[0].lower()
    supported_boards = ["uno"]
    if board not in supported_boards:
        ts_print(f"Unsupported AVR8JS board: {board}")
        sys.exit(1)

    examples = args.run[1:] or ["Test"]
    fbuild_executable = get_fbuild_executable()
    if fbuild_executable is None:
        ts_print("Error: fbuild is not installed; run uv sync")
        sys.exit(1)
    child_env = fbuild_child_env()
    ts_print(f"Running {board.upper()} AVR8JS tests through fbuild: {examples}")

    if os.getenv("FASTLED_AVR8JS_QUICK_TEST") == "true":
        ts_print("Quick setup check: fbuild version only; firmware is not emulated")
        result = RunningProcess.run(
            [fbuild_executable, "--version"], timeout=30, env=child_env
        )
        if result.returncode != 0:
            sys.exit(result.returncode)
        return

    failures = 0
    for example in examples:
        try:
            staged = stage_fbuild_project(board_name=board, example=example)
            if not staged.success:
                ts_print(staged.output)
                failures += 1
                continue

            command = [
                fbuild_executable,
                "test-emu",
                "--emulator",
                "avr8js",
                "--environment",
                board,
                "--timeout",
                "15",
            ]
            if example == "Test":
                command.extend(
                    [
                        "--expect",
                        "Test setup starting",
                        "--halt-on-success",
                        "Test loop!",
                        "--halt-on-error",
                        "legacy libc copy mismatch",
                    ]
                )
            command.append(str(staged.build_dir))
            process = RunningProcess(
                command,
                timeout=900,
                capture=True,
                stderr=PIPE,
                encoding="utf-8",
                errors="replace",
                env=child_env,
            )
            # No idle detector is passed, so wait() returns the exit code; the
            # utf-8 encoding makes both streams text.
            returncode = cast(int, process.wait(echo=ts_print, timeout=900))
            stdout = cast(str, process.stdout)
            output = stdout + cast(str, process.stderr)
            Path("avr8js_output.txt").write_text(output, encoding="utf-8")
            # Judge stdout separately: the emulator's stderr diagnostics may
            # be delivered after fbuild's terminal verdict.
            if avr8js_run_succeeded(example, returncode, stdout):
                ts_print(f"SUCCESS: {example} ran in native fbuild AVR8JS")
            else:
                ts_print(f"FAILED: {example} (fbuild exit code {returncode})")
                failures += 1
        except KeyboardInterrupt as interrupt:
            handle_keyboard_interrupt(interrupt)
            raise
        except Exception as exc:
            ts_print(f"ERROR: {example}: {exc}")
            failures += 1

    ts_print(f"AVR8JS: {len(examples) - failures} passed, {failures} failed")
    show_zccache_stats()
    if failures:
        sys.exit(1)
