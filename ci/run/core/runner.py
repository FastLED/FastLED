from __future__ import annotations

import asyncio
from asyncio.subprocess import PIPE
from typing import AsyncIterator, Dict, Iterable, List, Optional

from ci.run.core.task import TaskState


async def _read_lines(stream: asyncio.StreamReader) -> AsyncIterator[str]:
    while not stream.at_eof():
        line = await stream.readline()
        if not line:
            break
        yield line.decode(errors="replace").rstrip("\r\n")


async def run_task(
    task: TaskState, *, env: Optional[Dict[str, str]] = None
) -> TaskState:
    task.mark_running()
    proc = await asyncio.create_subprocess_exec(
        *task.cmd, cwd=task.cwd, env=env, stdout=PIPE, stderr=PIPE
    )
    task.process = proc  # Store process handle for cancellation

    async def pump(reader: asyncio.StreamReader) -> None:
        async for line in _read_lines(reader):
            task.last_line = line
            # publish event/callback hook here if needed

    try:
        await asyncio.gather(pump(proc.stdout), pump(proc.stderr))  # type: ignore
        rc = await proc.wait()
        task.mark_done(rc)
    except asyncio.CancelledError:
        # Task was cancelled - terminate the subprocess
        await task.cancel()
        raise
    return task


async def run_many(tasks: Iterable[TaskState]) -> List[TaskState]:
    coros = [run_task(t) for t in tasks]
    return await asyncio.gather(*coros)
