import asyncio
import time
from asyncio.subprocess import Process
from dataclasses import dataclass, field
from typing import Literal


Status = Literal["queued", "running", "done", "failed", "canceled"]


@dataclass
class TaskState:
    id: str
    name: str
    cmd: list[str]
    cwd: str | None = None
    status: Status = "queued"
    start_ts: float = field(default_factory=time.time)
    end_ts: float | None = None
    last_line: str = ""
    returncode: int | None = None
    process: Process | None = None

    @property
    def elapsed(self) -> float:
        end = self.end_ts if self.end_ts else time.time()
        return max(0.0, end - self.start_ts)

    def mark_running(self) -> None:
        self.status = "running"
        self.start_ts = time.time()

    def mark_done(self, rc: int) -> None:
        self.end_ts = time.time()
        self.returncode = rc
        self.status = "done" if rc == 0 else "failed"

    def mark_canceled(self) -> None:
        self.end_ts = time.time()
        self.status = "canceled"

    async def cancel(self) -> None:
        """Cancel the running task by terminating the subprocess."""
        if self.process and self.status == "running":
            try:
                self.process.terminate()
                # Give it 2 seconds to terminate gracefully
                await asyncio.wait_for(self.process.wait(), timeout=2.0)
            except asyncio.TimeoutError:
                # Force kill if it doesn't terminate
                self.process.kill()
                await self.process.wait()
            except ProcessLookupError:
                # Process already terminated
                pass
            self.mark_canceled()
