from threading import Lock


PRINT_LOCK = Lock()


def locked_print(string: str):
    """Print with a lock to prevent garbled output for multiple threads."""
    with PRINT_LOCK:
        # print only prints so much, break up the string into lines
        for line in string.splitlines():
            print(line)
