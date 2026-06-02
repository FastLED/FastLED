"""Autoresearch orchestrator — the clean top-level entry point.

This module wires together the phase pipeline:
  A. Parse args & build RPC commands  (pure computation)
  B. Resolve port & environment       (hardware I/O)
  C. Build & deploy                   (BuildDriver abstraction)
  D. Schema validation & pin setup    (RPC + GPIO)
  E. Execute tests or special mode    (RPC test loop)
"""

from __future__ import annotations

import asyncio
import sys
from typing import Any

from colorama import Fore, Style

from ci.autoresearch.args import Args
from ci.autoresearch.context import (
    _STDOUT_CRASH_PATTERNS,
    QuietContext,
    print_run_summary,
)
from ci.autoresearch.phases import (
    _is_native_platform,
    _parse_args_and_build_commands,
    _resolve_port_and_environment,
    _run_build_deploy,
    _run_native_autoresearch,
    _run_schema_and_pin_setup,
    _run_tests_or_special_mode,
)
from ci.util.global_interrupt_handler import (
    handle_keyboard_interrupt,
    install_signal_handler,
)
from ci.util.port_utils import auto_detect_upload_port


# ============================================================
# Crash Pattern Interceptor
# ============================================================


class CrashPatternInterceptor:
    """Wraps sys.stdout to detect crash patterns in ALL output.

    Any line matching a crash pattern is recorded. The caller checks
    ``crash_detected`` after the run completes.
    """

    def __init__(self, inner: Any) -> None:
        self._inner = inner
        self.crash_detected: bool = False
        self.crash_lines: list[str] = []

    def write(self, s: str) -> int:
        for line in s.splitlines():
            for pattern in _STDOUT_CRASH_PATTERNS:
                if pattern.search(line):
                    self.crash_detected = True
                    self.crash_lines.append(line.strip())
                    break
        return self._inner.write(s)

    def flush(self) -> None:
        self._inner.flush()

    def __getattr__(self, name: str) -> Any:
        return getattr(self._inner, name)


# ============================================================
# Orchestrator
# ============================================================


async def run(args: Args | None = None) -> int:
    """Main entry point — orchestrates build, deploy, pin setup, and test phases."""
    install_signal_handler()

    args = args or Args.parse_args()
    assert args is not None

    if args.quiet:
        args.skip_lint = True
        args.skip_schema = True

    # Phase A: Parse args, validate modes, build RPC commands (pure computation)
    result = _parse_args_and_build_commands(args)
    if isinstance(result, int):
        return result
    ctx = result

    # Early return: --decode mode
    if ctx.decode_mode:
        assert args.decode is not None
        final_environment = ctx.final_environment
        if final_environment:
            upload_port = args.upload_port
            if not upload_port:
                port_result = auto_detect_upload_port(final_environment)
                if not port_result.ok:
                    print(
                        f"{Fore.RED}\u274c Error: No serial port detected for device decode{Style.RESET_ALL}"
                    )
                    return 1
                upload_port = port_result.selected_port
            assert upload_port is not None
            from ci.autoresearch.decode import run_device_decode_autoresearch

            return await run_device_decode_autoresearch(args.decode, upload_port)
        from ci.autoresearch.decode import run_decode_autoresearch

        return await run_decode_autoresearch(args.decode)

    # Early return: native platform
    if _is_native_platform(ctx.final_environment):
        return await _run_native_autoresearch(args)

    # Phase B: Resolve port & environment
    rc = await _resolve_port_and_environment(ctx)
    if rc is not None:
        return rc

    # Print summary banner
    print_run_summary(ctx)

    with QuietContext(args.quiet) as qctx:
        try:
            # Phase C: Build & deploy
            rc = await _run_build_deploy(ctx, qctx)
            if rc is not None:
                return rc

            # Phase D: Schema validation, pin discovery, GPIO pre-test
            rc = await _run_schema_and_pin_setup(ctx)
            if rc is not None:
                return rc

            # Phase E: Execute tests or special mode
            return await _run_tests_or_special_mode(ctx, qctx)

        except KeyboardInterrupt as ki:
            print("\n\n\u26a0\ufe0f  Interrupted by user")
            handle_keyboard_interrupt(ki)
            return 130


def main() -> int:
    interceptor = CrashPatternInterceptor(sys.stdout)
    sys.stdout = interceptor  # type: ignore[assignment]
    try:
        rc = asyncio.run(run())
        if rc == 0 and interceptor.crash_detected:
            print()
            print(
                f"{Fore.RED}\U0001f6a8 CRASH DETECTED in device output:{Style.RESET_ALL}"
            )
            for line in interceptor.crash_lines:
                print(f"  {line}")
            return 1
        return rc
    finally:
        sys.stdout = interceptor._inner
