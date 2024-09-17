from threading import Lock

PRINT_LOCK = Lock()


def locked_print(*args, **kwargs):
    """Print with a lock to prevent garbled output for multiple threads."""
    with PRINT_LOCK:
        print(*args, **kwargs)
