"""Host-load scaling for CI deadlines that must tell "hung" from "slow" (#5398)."""

import os


# Never stretch a deadline more than this, however overloaded the host is.
MAX_LOAD_SCALE = 8.0


def load_scale() -> float:
    """Return the 1-minute load average per CPU, clamped to [1, MAX_LOAD_SCALE].

    An idle or lightly loaded host returns 1.0, so deadlines keep their nominal
    value. Hosts without ``os.getloadavg`` (Windows) also return 1.0.
    """
    getloadavg = getattr(os, "getloadavg", None)
    if getloadavg is None:
        return 1.0
    try:
        load = float(getloadavg()[0])
    except OSError:
        return 1.0
    cpus = os.cpu_count() or 1
    return min(MAX_LOAD_SCALE, max(1.0, load / cpus))
