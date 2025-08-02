import os


def cpu_count() -> int:
    """Get the number of CPUs."""
    # Force sequential execution if NO_PARALLEL is set
    if "GITHUB_ACTIONS" in os.environ:
        return 1
    no_parallel = os.environ.get("NO_PARALLEL", "0") in ["1", "true", "True"]
    if no_parallel:
        return 1
    return os.cpu_count() or 1
