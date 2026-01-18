"""Global print function for consistent output formatting.

Timestamps were removed in iteration 7 based on TUI specialist analysis:
- Inline timestamps (0.00, 0.03, etc.) add noise without providing value
- The final "Total: X.XXs" summary is sufficient for timing information
- Use --verbose flag for detailed timing if needed
"""

import time
from typing import Any


# Global timestamp for consistent timing across all output
# Kept for backward compatibility with modules that read _GLOBAL_START_TIME
_GLOBAL_START_TIME: float | None = None


def ts_print(*args: Any, **kwargs: Any) -> None:
    """Print message (timestamps removed for cleaner output).

    Note: Timestamps were removed in iteration 7. Use --verbose for detailed timing.
    The global start time is still tracked for modules that need it.
    """
    global _GLOBAL_START_TIME
    if _GLOBAL_START_TIME is None:
        _GLOBAL_START_TIME = time.time()

    # Convert args to string (no timestamp prefix)
    message = " ".join(str(arg) for arg in args)
    print(message, **kwargs)
