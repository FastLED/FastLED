"""Global print function for consistent output formatting.

All output lines are prefixed with global elapsed time (seconds since program start)
for consistent timing across all build phases: Meson setup, compilation, testing, etc.
"""

import time
from typing import Any


# Global timestamp captured at module load (program start)
_GLOBAL_START_TIME: float = time.time()


def get_elapsed() -> float:
    """Get elapsed seconds since program start.

    Returns:
        Elapsed time in seconds as a float.
    """
    return time.time() - _GLOBAL_START_TIME


def get_elapsed_str() -> str:
    """Get elapsed time formatted as a string.

    Returns:
        Elapsed time formatted as "{elapsed:.2f}" (e.g., "5.42").
    """
    return f"{get_elapsed():.2f}"


def ts_print(*args: Any, **kwargs: Any) -> None:
    """Print message with global elapsed time timestamp.

    Args:
        *args: Message parts (joined with spaces).
        **kwargs: Keyword arguments passed to print().
    """
    # Convert args to string and prepend elapsed time
    message = " ".join(str(arg) for arg in args)
    timestamped_message = f"{get_elapsed_str()} {message}"
    print(timestamped_message, **kwargs)
