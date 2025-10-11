from typing import Any

from textual.app import App, ComposeResult
from textual.containers import Vertical
from textual.widgets import Footer, Header

from ci.run.views.main_menu import MainMenu


class FastLEDCI(App[Any]):
    CSS_PATH = "run/assets/theme.css"
    TITLE = "FastLED CI"
    BINDINGS = [
        ("ctrl+c", "quit", "Quit"),
        ("q", "quit", "Quit"),
    ]

    def on_mount(self) -> None:
        # Start on main menu view
        self.push_screen(MainMenu())

    def compose(self) -> ComposeResult:
        yield Header(show_clock=False)
        yield Vertical(id="root")
        yield Footer()


def main() -> None:
    FastLEDCI().run()


if __name__ == "__main__":
    main()
