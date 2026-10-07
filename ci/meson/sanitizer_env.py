"""Shared sanitizer environment setup for unit and example runners."""

import os
import sys
from pathlib import Path

from ci.util.timestamp_print import ts_print as _ts_print


def setup_sanitizer_env(source_dir: Path, verbose: bool) -> None:
    """Configure platform-compatible ASan/LSan options for debug builds."""
    from clang_tool_chain import (
        prepare_sanitizer_environment,  # noqa: PLC0415 - lazy: ~60ms on non-debug
    )

    sanitizer_env = prepare_sanitizer_environment(
        base_env=os.environ.copy(),
        compiler_flags=["-fsanitize=address"],
    )

    if sys.platform == "darwin":
        # Apple's Xcode ASan runtime on the macos-26 runner rejects
        # detect_leaks=1 before main() with "not supported on this platform."
        # LeakSanitizer is therefore unavailable on this runtime; keep all ASan
        # and UBSan instrumentation active while disabling only that option.
        asan_options = sanitizer_env.get("ASAN_OPTIONS", "")
        options = [
            option
            for option in asan_options.split(":")
            if option and not option.startswith("detect_leaks=")
        ]
        options.append("detect_leaks=0")
        sanitizer_env["ASAN_OPTIONS"] = ":".join(options)

    os.environ.update(sanitizer_env)

    lsan_suppressions = source_dir / "tests" / "lsan_suppressions.txt"
    if sys.platform != "darwin" and lsan_suppressions.exists():
        existing_lsan = os.environ.get("LSAN_OPTIONS", "")
        suppression_opt = f"suppressions={lsan_suppressions}"
        if existing_lsan:
            os.environ["LSAN_OPTIONS"] = f"{existing_lsan}:{suppression_opt}"
        else:
            os.environ["LSAN_OPTIONS"] = suppression_opt

    if verbose:
        symbolizer_path = sanitizer_env.get("ASAN_SYMBOLIZER_PATH")
        if symbolizer_path:
            _ts_print(f"[MESON] Set ASAN_SYMBOLIZER_PATH={symbolizer_path}")
