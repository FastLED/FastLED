import asyncio
from typing import Any, List

from textual.app import ComposeResult
from textual.containers import Vertical
from textual.screen import Screen
from textual.widgets import OptionList, Static
from textual.widgets.option_list import Option

from ci.run.core.runner import run_task
from ci.run.core.task import TaskState
from ci.run.core.updater import TaskBlock


PLATFORMS: list[str] = ["uno", "esp32dev", "esp32s3", "esp32p4", "teensy41"]


class BuildView(Screen[Any]):
    BINDINGS = [("q", "app.pop_screen", "Back")]

    def __init__(self) -> None:
        super().__init__()
        self._tasks: list[TaskState] = []

    def compose(self) -> ComposeResult:
        yield Static("[b]Compile Platform[/b]")
        menu = OptionList()
        for p in PLATFORMS:
            menu.add_option(Option(p, id=p))
        yield menu
        self.container = Vertical()
        yield self.container

    def on_option_list_option_selected(self, event: OptionList.OptionSelected) -> None:
        # Get the selected platform ID
        if event.option_id:
            self._start_build(event.option_id)

    def _start_build(self, platform: str) -> None:
        tb = TaskBlock(f"build:{platform}")
        self.container.mount(tb)
        cmd = [
            "bash",
            "compile",
            platform,
        ]  # add "--docker" via UI option if desired
        ts = TaskState(id=f"build:{platform}", name=platform, cmd=cmd)
        self._tasks.append(ts)
        self.run_worker(self._run(ts, tb), exclusive=False)

    async def _run(self, ts: TaskState, tb: TaskBlock) -> None:
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

    async def on_unmount(self) -> None:
        """Cancel all running tasks when screen is dismissed."""
        for task in self._tasks:
            if task.status == "running":
                await task.cancel()
