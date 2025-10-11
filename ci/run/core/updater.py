from __future__ import annotations

from textual.app import ComposeResult
from textual.containers import Horizontal
from textual.reactive import reactive
from textual.widget import Widget
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

    def compose(self) -> ComposeResult:
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
