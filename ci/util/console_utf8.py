from __future__ import annotations

import io
import os
import sys


def configure_utf8_console() -> None:
    """Ensure stdout/stderr use UTF-8 encoding on Windows consoles.

    Safe no-op on non-Windows platforms and on environments where
    reconfigure is unavailable.
    """
    os.environ.setdefault("PYTHONIOENCODING", "utf-8:replace")
    os.environ.setdefault("PYTHONUTF8", "1")

    if os.name != "nt":
        return

    try:
        # TextIOWrapper.reconfigure exists on stock 3.7+; redirected streams
        # (StringIO, ipykernel wrappers, etc.) won't be TextIOWrapper instances.
        if isinstance(sys.stdout, io.TextIOWrapper):
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        if isinstance(sys.stderr, io.TextIOWrapper):
            sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, OSError):
        # Older Python versions or redirected streams may not support reconfigure
        pass
