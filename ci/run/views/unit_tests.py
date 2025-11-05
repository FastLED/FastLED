import asyncio
from typing import Any, List

from textual.app import ComposeResult
from textual.containers import Vertical
from textual.screen import Screen
from textual.widgets import Static

from ci.run.core.runner import run_task
from ci.run.core.task import TaskState
from ci.run.core.updater import TaskBlock


# NOTE: These are C++ unit test executables, NOT Python pytest suites
# The actual test discovery happens in test.py
# For the TUI, we can either:
# 1. Run all C++ tests: ["uv", "run", "test.py", "--cpp"]
# 2. Run specific tests: ["uv", "run", "test.py", "test_name"]
EXAMPLE_TESTS = [
    "xypath",
    "color_math",
    "rmt_timing",
]


class UnitTestsView(Screen[Any]):
    BINDINGS = [("q", "app.pop_screen", "Back")]

    def __init__(self) -> None:
        super().__init__()
        self._tasks: list[TaskState] = []
        self._runner_task: Any = None

    def compose(self) -> ComposeResult:
        yield Static("[b]C++ Unit Tests[/b]")
        self._container = Vertical()
        yield self._container
        self.call_after_refresh(self._start)

    async def _start(self) -> None:
        blocks: list[tuple[TaskState, TaskBlock]] = []
        for test_name in EXAMPLE_TESTS:
            tb = TaskBlock(f"test_{test_name}")
            self._container.mount(tb)
            # CORRECT: C++ unit tests run via test.py, not pytest
            t = TaskState(
                id=test_name,
                name=test_name,
                cmd=["uv", "run", "test.py", test_name],
            )
            self._tasks.append(t)
            blocks.append((t, tb))

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

        async def run_all() -> None:
            await asyncio.gather(*(run_one(ts, tb) for ts, tb in blocks))

        self._runner_task = asyncio.create_task(run_all())

    async def on_unmount(self) -> None:
        """Cancel all running tasks when screen is dismissed."""
        if self._runner_task and not self._runner_task.done():
            self._runner_task.cancel()
        for task in self._tasks:
            if task.status == "running":
                await task.cancel()
