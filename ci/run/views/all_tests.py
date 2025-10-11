from __future__ import annotations

import asyncio
from typing import Any, List

from textual.app import ComposeResult
from textual.containers import Vertical
from textual.screen import Screen
from textual.widgets import Static

from ci.run.core.runner import run_task
from ci.run.core.task import TaskState
from ci.run.core.updater import TaskBlock
from ci.run.views.python_qa import QA_COMMANDS
from ci.run.views.unit_tests import EXAMPLE_TESTS


class AllTestsView(Screen[Any]):
    BINDINGS = [("q", "app.pop_screen", "Back")]

    def __init__(self) -> None:
        super().__init__()
        self._tasks: List[TaskState] = []
        self._runner_task: Any = None

    def compose(self) -> ComposeResult:
        yield Static("[b]Run All Tests[/b]")
        self.container = Vertical()
        yield self.container
        self.call_after_refresh(self._start)

    async def _run_block(self, items: List[TaskState], title_prefix: str) -> None:
        blocks: List[tuple[TaskState, TaskBlock]] = []
        for ts in items:
            tb = TaskBlock(f"{title_prefix}:{ts.name}")
            self.container.mount(tb)
            self._tasks.append(ts)
            blocks.append((ts, tb))

        async def run_one(ts: TaskState, tb: TaskBlock) -> None:
            async def tick() -> None:
                while ts.status in ("queued", "running"):
                    tb.elapsed = ts.elapsed
                    tb.status = ts.status
                    tb.tail = ts.last_line
                    await asyncio.sleep(0.1)
                tb.elapsed = ts.elapsed
                tb.status = ts.status
                tb.tail = ts.last_line

            ticker = asyncio.create_task(tick())
            try:
                await run_task(ts)
            finally:
                ticker.cancel()

        await asyncio.gather(*(run_one(ts, tb) for ts, tb in blocks))

    async def _start(self) -> None:
        # 1) Python QA (concurrent lanes)
        qa_tasks = [TaskState(id=n, name=n, cmd=cmd) for n, cmd in QA_COMMANDS]
        await self._run_block(qa_tasks, "qa")

        # 2) Build current platform (single task) — change platform as needed
        build = TaskState(
            id="build:esp32s3",
            name="esp32s3",
            cmd=["bash", "compile", "esp32s3"],
        )  # add --docker via flag if needed
        await self._run_block([build], "build")

        # 3) C++ Unit tests (concurrent lanes) - CORRECT: use test.py not pytest
        unit_tasks = [
            TaskState(id=t, name=t, cmd=["uv", "run", "test.py", t])
            for t in EXAMPLE_TESTS
        ]
        await self._run_block(unit_tasks, "unit")

    async def on_unmount(self) -> None:
        """Cancel all running tasks when screen is dismissed."""
        if self._runner_task and not self._runner_task.done():
            self._runner_task.cancel()
        for task in self._tasks:
            if task.status == "running":
                await task.cancel()
