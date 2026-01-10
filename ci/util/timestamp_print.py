"""Global timestamped print function for consistent output formatting."""

import time
from typing import Any

# Global timestamp for consistent timing across all output
_GLOBAL_START_TIME: float | None = None


def ts_print(*args: Any, **kwargs: Any) -> None:
    """Print with timestamp prefix showing elapsed time since first call."""
    global _GLOBAL_START_TIME
    if _GLOBAL_START_TIME is None:
        _GLOBAL_START_TIME = time.time()

    elapsed = time.time() - _GLOBAL_START_TIME
    # Convert args to string and add timestamp
    message = " ".join(str(arg) for arg in args)
    print(f"{elapsed:.2f} {message}", **kwargs)
