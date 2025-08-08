from __future__ import annotations

import threading
from typing import List

# Import at runtime since this module is part of util package and used broadly
from ci.util.running_process import RunningProcess


class RunningProcessManager:
    """Thread-safe registry of currently running processes for diagnostics."""

    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._processes: list[RunningProcess] = []

    def register(self, proc: RunningProcess) -> None:
        with self._lock:
            if proc not in self._processes:
                self._processes.append(proc)

    def unregister(self, proc: RunningProcess) -> None:
        with self._lock:
            try:
                self._processes.remove(proc)
            except ValueError:
                pass

    def list_active(self) -> list[RunningProcess]:
        with self._lock:
            return [p for p in self._processes if not p.finished]

    def dump_active(self) -> None:
        active: list[RunningProcess] = self.list_active()
        if not active:
            print("\nNO ACTIVE SUBPROCESSES DETECTED - MAIN PROCESS LIKELY HUNG")
            return

        print("\nSTUCK SUBPROCESS COMMANDS:")
        import time

        now = time.time()
        for idx, p in enumerate(active, 1):
            pid: int | None = None
            try:
                if p.proc is not None:
                    pid = p.proc.pid
            except Exception:
                pid = None

            start = p.start_time
            last_out = p.time_last_stdout_line()
            duration_str = f"{(now - start):.1f}s" if start is not None else "?"
            since_out_str = (
                f"{(now - last_out):.1f}s" if last_out is not None else "no-output"
            )

            print(
                f"  {idx}. cmd={p.command} pid={pid} duration={duration_str} last_output={since_out_str}"
            )


# Global singleton instance for convenient access
RunningProcessManagerSingleton = RunningProcessManager()
