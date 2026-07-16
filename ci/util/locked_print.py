import atexit
import sys
from queue import Empty, Queue
from threading import Lock, Thread


PRINT_LOCK = Lock()
_QUEUE_CAPACITY = 1024
_PRINT_QUEUE: Queue[str | None] = Queue(maxsize=_QUEUE_CAPACITY)
_WRITER: Thread | None = None
_WRITER_START_LOCK = Lock()
_SHUTTING_DOWN = False


def _write_batches() -> None:
    """Drain queued lines in batches and flush each batch to the pipe."""
    while True:
        line = _PRINT_QUEUE.get()
        batch: list[str] = []
        stopping = line is None
        _PRINT_QUEUE.task_done()
        if line is not None:
            batch.append(line)
        while not stopping:
            try:
                line = _PRINT_QUEUE.get_nowait()
            except Empty:
                break
            try:
                if line is None:
                    stopping = True
                else:
                    batch.append(line)
            finally:
                _PRINT_QUEUE.task_done()
        if batch:
            sys.stdout.write("\n".join(batch) + "\n")
            sys.stdout.flush()
        if stopping:
            return


def _ensure_writer() -> None:
    global _WRITER
    if _WRITER is not None:
        return
    with _WRITER_START_LOCK:
        if _WRITER is None:
            _WRITER = Thread(
                target=_write_batches,
                name="fastled-build-output",
                daemon=True,
            )
            _WRITER.start()


def _shutdown_writer() -> None:
    """Drain all accepted output before normal interpreter shutdown."""
    global _SHUTTING_DOWN
    if _WRITER is None:
        return
    with PRINT_LOCK:
        if _SHUTTING_DOWN:
            return
        _SHUTTING_DOWN = True
        _PRINT_QUEUE.put(None)
    _PRINT_QUEUE.join()
    _WRITER.join()


atexit.register(_shutdown_writer)


def locked_print(string: str):
    """Queue lines for ordered, flushed, batched output with backpressure."""
    _ensure_writer()
    with PRINT_LOCK:
        if _SHUTTING_DOWN:
            return
        for line in string.splitlines():
            _PRINT_QUEUE.put(line)
