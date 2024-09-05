import os


def cpu_count() -> int:
    """Get the number of CPUs."""
    if "GITHUB_ACTIONS" in os.environ:
        return 4
    return os.cpu_count() or 1
