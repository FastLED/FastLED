"""Global interrupt handler for coordinating KeyboardInterrupt across threads."""

import _thread
import os
import signal
import threading
import time
from typing import Optional


class GlobalInterruptHandler:
    """Handles KeyboardInterrupt across threads."""

    _instance: Optional["GlobalInterruptHandler"] = None
    _lock = threading.Lock()

    def __init__(self, grace_period: float = 3.0, force_exit_timeout: float = 5.0):
        self.grace_period = grace_period
        self.force_exit_timeout = force_exit_timeout
        self.interrupted = threading.Event()
        self.interrupt_time: Optional[float] = None
        self._interrupt_count = 0
        self._in_cleanup = False
        self._signal_handler_installed = False

    @classmethod
    def get_instance(cls) -> "GlobalInterruptHandler":
        if cls._instance is None:
            with cls._lock:
                if cls._instance is None:
                    cls._instance = cls()
        return cls._instance

    def is_interrupted(self) -> bool:
        return self.interrupted.is_set()

    def signal_interrupt(self, from_thread: Optional[str] = None) -> None:
        if not self.interrupted.is_set():
            self.interrupt_time = time.time()
            self.interrupted.set()
            thread_info = f" from {from_thread}" if from_thread else ""
            print(
                f"\n⚠️  Interrupt signal received{thread_info}, stopping all operations..."
            )

        self._interrupt_count += 1
        if self._interrupt_count >= 2:
            print("\n⚠️  Double Ctrl+C detected - forcing immediate exit")
            os._exit(2)

    def notify_main_thread(self) -> None:
        if not self.is_interrupted():
            self.signal_interrupt(from_thread=threading.current_thread().name)
        _thread.interrupt_main()

    def install_signal_handler(self) -> None:
        """Install SIGINT handler to catch Ctrl-C and set interrupt flag."""
        if self._signal_handler_installed:
            return

        def sigint_handler(signum: int, frame: object) -> None:
            """Handle SIGINT (Ctrl-C) by setting the interrupt flag."""
            self.signal_interrupt()
            # Raise KeyboardInterrupt to allow exception handlers to run
            raise KeyboardInterrupt()

        signal.signal(signal.SIGINT, sigint_handler)
        self._signal_handler_installed = True

    def wait_for_cleanup(self) -> None:
        if self._in_cleanup:
            return

        self._in_cleanup = True
        start_time = time.time()
        elapsed = 0

        print(
            f"\n⏳ Waiting up to {self.grace_period}s for worker threads to cleanup..."
        )

        while elapsed < self.grace_period:
            elapsed = time.time() - start_time
            remaining = self.grace_period - elapsed

            if remaining > 0:
                time.sleep(min(0.1, remaining))
            else:
                break

        total_elapsed = time.time() - start_time
        print(f"⏳ Cleanup wait complete ({total_elapsed:.1f}s)")

        if self.interrupt_time is not None:
            total_interrupt_time = time.time() - self.interrupt_time
            if total_interrupt_time > self.force_exit_timeout:
                print(
                    f"\n⚠️  Force exit timeout reached ({total_interrupt_time:.1f}s) - exiting"
                )
                os._exit(2)


_handler = GlobalInterruptHandler.get_instance()


def signal_interrupt() -> None:
    _handler.signal_interrupt()


def is_interrupted() -> bool:
    return _handler.is_interrupted()


def notify_main_thread() -> None:
    _handler.notify_main_thread()


# Backward compatibility alias for old linter
def handle_keyboard_interrupt_properly() -> None:
    """Alias for notify_main_thread() for backward compatibility with old flake8 plugin."""
    notify_main_thread()


def wait_for_cleanup() -> None:
    _handler.wait_for_cleanup()


def install_signal_handler() -> None:
    """Install SIGINT (Ctrl-C) signal handler."""
    _handler.install_signal_handler()
