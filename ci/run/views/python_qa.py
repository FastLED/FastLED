import asyncio
from typing import Any, List, Tuple

from textual.app import ComposeResult
from textual.containers import Vertical
from textual.screen import Screen
from textual.widgets import Static

from ci.run.core.runner import run_task
from ci.run.core.task import TaskState
from ci.run.core.updater import TaskBlock


QA_COMMANDS: list[tuple[str, list[str]]] = [
    (
        "pytest",
        ["uv", "run", "pytest", "ci/", "-q", "-n", "4"],
    ),  # Python tests only
    ("ruff-check", ["uv", "run", "ruff", "check", "."]),
    (
        "ruff-format",
        ["uv", "run", "ruff", "format", "--check", "."],
    ),  # Replaces black
    # NOTE: isort is NOT used - ruff handles import sorting
]


class PythonQAView(Screen[Any]):
    BINDINGS = [("q", "app.pop_screen", "Back")]

    def __init__(self) -> None:
        super().__init__()
        self._tasks: list[TaskState] = []
        self._runner_task: Any = None

    def compose(self) -> ComposeResult:
        yield Static("[b]Python QA[/b]")
        self.container = Vertical()
        yield self.container
        self.call_after_refresh(self._start)

    async def _start(self) -> None:
        blocks: list[tuple[TaskState, TaskBlock]] = []
        for name, cmd in QA_COMMANDS:
            tb = TaskBlock(name)
            self.container.mount(tb)
            ts = TaskState(id=name, name=name, cmd=cmd)
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
