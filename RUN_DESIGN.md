# FastLED CI TUI — Design & Scaffold (Textual/Rich)

**Author:** Zach Vorhies
**Date:** 2025-10-11
**Status:** Draft v0.3 (updated paths)
**Target Platforms:** Windows, macOS, Linux
**Tech Stack:** Python 3.11+, `textual`, `rich`, `uv`

> **Structure correction:** All runtime code now lives under `ci/run/**`, with `ci/run.py` as the top-level entrypoint for `uv run -m ci.run`.

---

## 1) Overview

A cross-platform, retro-styled **terminal UI** for FastLED code-quality and builds. It orchestrates:

* C++ unit tests (with multi-line live updater)
* Single-platform embedded compile (`bash compile <platform> [--docker]`)
* Python QA: `pytest`, `ruff`, `black --check`, `isort --check-only`
* "Run all" orchestration (Python QA → Build current platform → Unit tests)

---

## 2) Goals

* **Cross-platform** Windows/macOS/Linux terminals
* **Fast feedback** (async subprocess streaming)
* **Readable** two-line task blocks (status + stdout tail)
* **Simple entrypoint** `bash run` → `uv run -m ci.run`
* **Optional** `--docker` via compile script param (not coupled in TUI)
* **Retro aesthetic** (ANSI borders, bubbles, spinners)

---

## 3) Architecture

### 3.1 File Tree

```
ci/
 ├─ run.py                 # entrypoint (module: ci.run)
 └─ run/
     ├─ __init__.py
     ├─ core/
     │   ├─ __init__.py
     │   ├─ task.py
     │   ├─ runner.py
     │   ├─ updater.py
     │   └─ config.py
     ├─ views/
     │   ├─ __init__.py
     │   ├─ main_menu.py
     │   ├─ unit_tests.py
     │   ├─ build.py
     │   ├─ python_qa.py
     │   └─ all_tests.py
     └─ assets/
         └─ theme.css
```

### 3.2 Data Flow

```
User Input → MainMenu → TaskRunner → TaskEvents → LiveUpdater → Screen Render
```

* **TaskRunner** spawns subprocesses via `asyncio.create_subprocess_exec`
* Streams stdout/stderr lines into task state; **Updater** shows latest line
* **Textual** `Reactive` fields trigger view updates at ~8 Hz

---

## 4) UI Design

### 4.1 Layout

* **Header**: bubble "FastLED"; **Footer**: keybinds
* **Main panel**: menu or live task blocks

### 4.2 Task Block Format

```
<name>   [⠙] running  0:05
  ➜ "last stdout line"
```

Done:

```
<name>   ✅ done (0:06)
  ➜ "PASS (12/12)"
```

### 4.3 Views

* **MainMenu**: 1) Unit test  2) Compile platform  3) Python QA  4) Run all
* **UnitTestsView**: run selected/all C++ tests
* **BuildView**: compile single platform only
* **PythonQAView**: run pytest/ruff/black/isort
* **AllTestsView**: orchestrated run (QA → Build → Unit)

---

## 5) Cross-Platform Strategy

* **ANSI** handled by Rich/Textual; modern terminals recommended
* **Subprocess** via asyncio; CRLF/LF normalized during decode
* **No native extensions**; pure Python deps
* **Windows compile**: ensure `bash` is discoverable or supply a PowerShell wrapper and switch command accordingly

---

## 6) Error Handling

* Command-not-found → inline ❌ with hint
* Timeout → advise higher timeout or fewer parallel jobs
* Non-zero exit → red task with tail
* Ctrl-C → graceful cancellation

---

## 7) Dependencies

`textual`, `rich`, `pytest`, `ruff`, `black`, `isort`, `uv` (+ optional `platformio`)

---

## 8) Code Scaffold (imports updated to `ci.run.*`)

### `ci/run.py`

```python
from textual.app import App, ComposeResult
from textual.widgets import Header, Footer
from textual.containers import Vertical

from ci.run.views.main_menu import MainMenu


class FastLEDCI(App):
    CSS_PATH = "run/assets/theme.css"
    TITLE = "FastLED CI"

    def on_mount(self) -> None:
        # Start on main menu view
        self.push_view(MainMenu())

    def compose(self) -> ComposeResult:
        yield Header(show_clock=False)
        yield Vertical(id="root")
        yield Footer()


def main() -> None:
    FastLEDCI().run()


if __name__ == "__main__":
    main()
```

### `ci/run/__init__.py`

```python
# Package root for TUI runtime under ci.run
```

### `ci/run/core/__init__.py`

```python
# Core runtime for FastLED CI TUI
```

### `ci/run/core/task.py`

```python
from __future__ import annotations
from dataclasses import dataclass, field
from typing import Optional, Literal
import time

Status = Literal["queued", "running", "done", "failed", "canceled"]


@dataclass
class TaskState:
    id: str
    name: str
    cmd: list[str]
    cwd: Optional[str] = None
    status: Status = "queued"
    start_ts: float = field(default_factory=time.time)
    end_ts: Optional[float] = None
    last_line: str = ""
    returncode: Optional[int] = None

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
```

### `ci/run/core/runner.py`

```python
from __future__ import annotations
import asyncio
from asyncio.subprocess import PIPE
from typing import AsyncIterator, Iterable

from ci.run.core.task import TaskState


async def _read_lines(stream: asyncio.StreamReader) -> AsyncIterator[str]:
    while not stream.at_eof():
        line = await stream.readline()
        if not line:
            break
        yield line.decode(errors="replace").rstrip("\r\n")


async def run_task(task: TaskState, *, env: dict | None = None) -> TaskState:
    task.mark_running()
    proc = await asyncio.create_subprocess_exec(
        *task.cmd, cwd=task.cwd, env=env, stdout=PIPE, stderr=PIPE
    )

    async def pump(reader: asyncio.StreamReader):
        async for line in _read_lines(reader):
            task.last_line = line
            # publish event/callback hook here if needed

    await asyncio.gather(pump(proc.stdout), pump(proc.stderr))
    rc = await proc.wait()
    task.mark_done(rc)
    return task


async def run_many(tasks: Iterable[TaskState]) -> list[TaskState]:
    coros = [run_task(t) for t in tasks]
    return await asyncio.gather(*coros)
```

### `ci/run/core/updater.py`

```python
from __future__ import annotations
from textual.reactive import reactive
from textual.widget import Widget
from textual.containers import Horizontal
from textual.widgets import Static


class TaskBlock(Widget):
    """Two-line live block: title/status/elapsed + tail line."""

    title = reactive("")
    status = reactive("queued")
    elapsed = reactive(0.0)
    tail = reactive("")

    def __init__(self, title: str) -> None:
        super().__init__()
        self.title = title
        self._line1 = Static(id="tb-line1")
        self._line2 = Static(id="tb-line2")

    def compose(self):
        yield Horizontal(self._line1)
        yield Horizontal(self._line2)

    def render_line1(self) -> str:
        badge = {
            "queued": "[dim]queued[/]",
            "running": "[yellow]⠙ running[/]",
            "done": "[green]✅ done[/]",
            "failed": "[red]❌ failed[/]",
            "canceled": "[magenta]✖ canceled[/]",
        }.get(self.status, "")
        mm, ss = divmod(int(self.elapsed), 60)
        return f"[bold]{self.title}[/]  {badge}  [dim]{mm}:{ss:02d}[/]"

    def on_mount(self) -> None:
        self.set_interval(0.125, self._tick)  # ~8 Hz

    def _tick(self) -> None:
        self._line1.update(self.render_line1())
        self._line2.update(f"  ➜ {self.tail}" if self.tail else "  ")
```

### `ci/run/core/config.py`

```python
from __future__ import annotations
import json
import os
from dataclasses import dataclass, asdict
from pathlib import Path

CONFIG_DIR = Path(os.path.expanduser("~/.fastledci"))
CONFIG_PATH = CONFIG_DIR / "config.json"


@dataclass
class Config:
    default_platform: str = "esp32s3"
    parallel_jobs: int = 4
    docker_default: bool = False

    @classmethod
    def load(cls) -> "Config":
        try:
            data = json.loads(CONFIG_PATH.read_text())
            return cls(**data)
        except Exception:
            return cls()

    def save(self) -> None:
        CONFIG_DIR.mkdir(parents=True, exist_ok=True)
        CONFIG_PATH.write_text(json.dumps(asdict(self), indent=2))
```

### `ci/run/views/__init__.py`

```python
# Views package
```

### `ci/run/views/main_menu.py`

```python
from textual.app import ComposeResult
from textual.screen import Screen
from textual.widgets import Static, SelectionList

from ci.run.views.unit_tests import UnitTestsView
from ci.run.views.build import BuildView
from ci.run.views.python_qa import PythonQAView
from ci.run.views.all_tests import AllTestsView


class MainMenu(Screen):
    BINDINGS = [
        ("q", "app.pop_screen", "Back"),
        ("1", "unit", "Run unit test"),
        ("2", "build", "Compile platform"),
        ("3", "qa", "Run Python tests/lints"),
        ("4", "all", "Run all tests"),
    ]

    def compose(self) -> ComposeResult:
        yield Static("[b]FASTLED CI[/b]", id="title")
        sel = SelectionList[str]()
        sel.add_option("Run unit test", "unit")
        sel.add_option("Compile platform", "build")
        sel.add_option("Run Python tests / lints", "qa")
        sel.add_option("Run all tests", "all")
        yield sel

    def on_selection_list_selected(self, event: SelectionList.Selected) -> None:
        self._route(event.value)

    def action_unit(self) -> None: self._route("unit")
    def action_build(self) -> None: self._route("build")
    def action_qa(self) -> None: self._route("qa")
    def action_all(self) -> None: self._route("all")

    def _route(self, key: str) -> None:
        match key:
            case "unit":
                self.app.push_view(UnitTestsView())
            case "build":
                self.app.push_view(BuildView())
            case "qa":
                self.app.push_view(PythonQAView())
            case "all":
                self.app.push_view(AllTestsView())
```

### `ci/run/views/unit_tests.py`

```python
from __future__ import annotations
import asyncio
from textual.app import ComposeResult
from textual.screen import Screen
from textual.containers import Vertical
from textual.widgets import Static

from ci.run.core.task import TaskState
from ci.run.core.runner import run_task
from ci.run.core.updater import TaskBlock


EXAMPLE_SUITES = [
    "tests/test_color_math.py",
    "tests/test_rmt_timing.py",
    "tests/test_spi_bus.py",
]


class UnitTestsView(Screen):
    BINDINGS = [("q", "app.pop_view", "Back")]

    def compose(self) -> ComposeResult:
        yield Static("[b]Unit Tests[/b]")
        self._container = Vertical()
        yield self._container
        self.call_after_refresh(self._start)

    async def _start(self) -> None:
        tasks = []
        for suite in EXAMPLE_SUITES:
            tb = TaskBlock(suite)
            self._container.mount(tb)
            t = TaskState(id=suite, name=suite, cmd=["uv", "run", "pytest", suite])
            tasks.append((t, tb))

        async def run_one(ts: TaskState, tb: TaskBlock):
            async def tick():
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

        await asyncio.gather(*(run_one(ts, tb) for ts, tb in tasks))
```

### `ci/run/views/build.py`

```python
from __future__ import annotations
import asyncio
from textual.app import ComposeResult
from textual.screen import Screen
from textual.widgets import Static, SelectionList
from textual.containers import Vertical

from ci.run.core.task import TaskState
from ci.run.core.runner import run_task
from ci.run.core.updater import TaskBlock

PLATFORMS = ["uno", "esp32dev", "esp32s3", "esp32p4", "teensy41"]


class BuildView(Screen):
    BINDINGS = [("q", "app.pop_view", "Back")]

    def compose(self) -> ComposeResult:
        yield Static("[b]Compile Platform[/b]")
        self.sel = SelectionList[str]()
        for p in PLATFORMS:
            self.sel.add_option(p, p)
        yield self.sel
        self.container = Vertical()
        yield self.container

    def on_selection_list_selected(self, event: SelectionList.Selected) -> None:
        platform = event.value
        self._start_build(platform)

    def _start_build(self, platform: str) -> None:
        tb = TaskBlock(f"build:{platform}")
        self.container.mount(tb)
        cmd = ["bash", "compile", platform]  # add "--docker" via UI option if desired
        ts = TaskState(id=f"build:{platform}", name=platform, cmd=cmd)
        self.run_worker(self._run(ts, tb), exclusive=False)

    async def _run(self, ts: TaskState, tb: TaskBlock):
        async def tick():
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
```

### `ci/run/views/python_qa.py`

```python
from __future__ import annotations
import asyncio
from textual.app import ComposeResult
from textual.screen import Screen
from textual.containers import Vertical
from textual.widgets import Static

from ci.run.core.task import TaskState
from ci.run.core.runner import run_task
from ci.run.core.updater import TaskBlock

QA_COMMANDS = [
    ("pytest", ["uv", "run", "pytest", "-q", "-n", "4"]),
    ("ruff",   ["uv", "run", "ruff", "check", "."]),
    ("black",  ["uv", "run", "black", "--check", "."]),
    ("isort",  ["uv", "run", "isort", "--check-only", "."]),
]


class PythonQAView(Screen):
    BINDINGS = [("q", "app.pop_view", "Back")]

    def compose(self) -> ComposeResult:
        yield Static("[b]Python QA[/b]")
        self.container = Vertical()
        yield self.container
        self.call_after_refresh(self._start)

    async def _start(self) -> None:
        tasks = []
        for name, cmd in QA_COMMANDS:
            tb = TaskBlock(name)
            self.container.mount(tb)
            ts = TaskState(id=name, name=name, cmd=cmd)
            tasks.append((ts, tb))

        async def run_one(ts: TaskState, tb: TaskBlock):
            async def tick():
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

        await asyncio.gather(*(run_one(ts, tb) for ts, tb in tasks))
```

### `ci/run/views/all_tests.py`

```python
from __future__ import annotations
import asyncio
from textual.app import ComposeResult
from textual.screen import Screen
from textual.containers import Vertical
from textual.widgets import Static

from ci.run.views.python_qa import QA_COMMANDS
from ci.run.views.unit_tests import EXAMPLE_SUITES
from ci.run.core.task import TaskState
from ci.run.core.runner import run_task
from ci.run.core.updater import TaskBlock


class AllTestsView(Screen):
    BINDINGS = [("q", "app.pop_view", "Back")]

    def compose(self) -> ComposeResult:
        yield Static("[b]Run All Tests[/b]")
        self.container = Vertical()
        yield self.container
        self.call_after_refresh(self._start)

    async def _run_block(self, items: list[TaskState], title_prefix: str):
        blocks = []
        for ts in items:
            tb = TaskBlock(f"{title_prefix}:{ts.name}")
            self.container.mount(tb)
            blocks.append((ts, tb))

        async def run_one(ts: TaskState, tb: TaskBlock):
            async def tick():
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
        build = TaskState(id="build:esp32s3", name="esp32s3", cmd=["bash", "compile", "esp32s3"])  # add --docker via flag if needed
        await self._run_block([build], "build")

        # 3) Unit tests (concurrent lanes)
        unit_tasks = [TaskState(id=s, name=s, cmd=["uv", "run", "pytest", s]) for s in EXAMPLE_SUITES]
        await self._run_block(unit_tasks, "unit")
```

### `ci/run/assets/theme.css`

```css
/* Minimal retro look */
#title { text-align: center; color: cyan; }
#tb-line1 { color: white; }
#tb-line2 { color: grey66; }
Screen { background: #000000; }
Footer { background: #111111; color: #cccccc; }
Header { background: #111111; color: #00ffff; }
```

---

## 9) Dev Notes

* Hot-reload: `textual run --dev ci/run.py`
* Pin deps in `pyproject.toml`; add `uv` scripts
* Logs: `.ci/logs/` (to be wired in runner as needed)

---

## 10) Milestones

* **M1**: Scaffold + MainMenu
* **M2**: Async TaskRunner + TaskBlock updater
* **M3**: UnitTests + Build wired to real commands
* **M4**: Python QA view + summary actions
* **M5**: Cross-platform polish
