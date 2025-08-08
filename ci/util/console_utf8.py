from __future__ import annotations

import os
import sys


def configure_utf8_console() -> None:
    """Ensure stdout/stderr use UTF-8 encoding on Windows consoles.

    Safe no-op on non-Windows platforms and on environments where
    reconfigure is unavailable.
    """
    if os.name != "nt":
        return

    try:
        if hasattr(sys.stdout, "reconfigure") and callable(
            getattr(sys.stdout, "reconfigure", None)
        ):
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[attr-defined]
        if hasattr(sys.stderr, "reconfigure") and callable(
            getattr(sys.stderr, "reconfigure", None)
        ):
            sys.stderr.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[attr-defined]
    except (AttributeError, OSError):
        # Older Python versions or redirected streams may not support reconfigure
        pass
