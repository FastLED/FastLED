from threading import Lock


PRINT_LOCK = Lock()


def locked_print(string: str):
    """Print with a lock to prevent garbled output for multiple threads."""
    with PRINT_LOCK:
        # print only prints so much, break up the string into lines
        for line in string.splitlines():
            # Build output is commonly piped through ``tee`` in CI.  Without
            # an explicit flush, Python block-buffers stdout and hides live
            # compiler progress until the buffer fills or the process exits.
            print(line, flush=True)
