import atexit
import sys
from queue import Empty, Queue
from threading import Lock, Semaphore, Thread


PRINT_LOCK = Lock()
_QUEUE_CAPACITY = 1024
_PRINT_QUEUE: Queue[str | None] = Queue(maxsize=_QUEUE_CAPACITY)
_PRINT_READY = Semaphore(0)
_WRITER: Thread | None = None
_WRITER_START_LOCK = Lock()
_QUEUE_STATE_LOCK = Lock()
_NOTIFIED = False
_SHUTTING_DOWN = False


def _write_batches() -> None:
    """Drain queued lines in batches and flush each batch to the pipe."""
    while True:
        _PRINT_READY.acquire()
        stopping = False
        while not stopping:
            batch: list[str] = []
            try:
                while True:
                    line = _PRINT_QUEUE.get_nowait()
                    try:
                        if line is None:
                            stopping = True
                        else:
                            batch.append(line)
                    finally:
                        _PRINT_QUEUE.task_done()
                    if stopping:
                        break
            except Empty:
                pass
            if batch:
                sys.stdout.write("\n".join(batch) + "\n")
                sys.stdout.flush()
            if stopping:
                return
            # A producer may have enqueued while the batch was being written.
            # Keep draining without another wakeup until the queue is empty.
            with _QUEUE_STATE_LOCK:
                if _PRINT_QUEUE.empty():
                    global _NOTIFIED
                    _NOTIFIED = False
                    break


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
        _signal_writer()
    _PRINT_QUEUE.join()
    _WRITER.join()


atexit.register(_shutdown_writer)


def _signal_writer() -> None:
    """Wake the writer once when the queue transitions from idle to active."""
    global _NOTIFIED
    with _QUEUE_STATE_LOCK:
        if not _NOTIFIED:
            _NOTIFIED = True
            _PRINT_READY.release()


def locked_print(string: str):
    """Queue lines for ordered, flushed, batched output with backpressure."""
    lines = string.splitlines()
    if not lines:
        return
    _ensure_writer()
    with PRINT_LOCK:
        if _SHUTTING_DOWN:
            return
        for line in lines:
            _PRINT_QUEUE.put(line)
        _signal_writer()
