from __future__ import annotations

from typing import TYPE_CHECKING, Any

from textual.app import ComposeResult
from textual.screen import Screen
from textual.widgets import OptionList, Static
from textual.widgets.option_list import Option

from ci.run.views.all_tests import AllTestsView
from ci.run.views.build import BuildView
from ci.run.views.python_qa import PythonQAView
from ci.run.views.unit_tests import UnitTestsView


if TYPE_CHECKING:
    from textual.app import App


class MainMenu(Screen[Any]):
    BINDINGS = [
        ("q", "app.pop_screen", "Back"),
        ("1", "unit", "Run unit test"),
        ("2", "build", "Compile platform"),
        ("3", "qa", "Run Python tests/lints"),
        ("4", "all", "Run all tests"),
    ]

    if TYPE_CHECKING:

        @property
        def app(self) -> App[Any]:
            """Type hint for app property."""
            ...

    def compose(self) -> ComposeResult:
        yield Static("[b]FASTLED CI[/b]", id="title")
        menu = OptionList()
        menu.add_option(Option("Run unit test", id="unit"))
        menu.add_option(Option("Compile platform", id="build"))
        menu.add_option(Option("Run Python tests / lints", id="qa"))
        menu.add_option(Option("Run all tests", id="all"))
        yield menu

    def on_option_list_option_selected(self, event: OptionList.OptionSelected) -> None:
        # Get the selected option ID
        if event.option_id:
            self._route(event.option_id)

    def action_unit(self) -> None:
        self._route("unit")

    def action_build(self) -> None:
        self._route("build")

    def action_qa(self) -> None:
        self._route("qa")

    def action_all(self) -> None:
        self._route("all")

    def _route(self, key: str) -> None:
        match key:
            case "unit":
                self.app.push_screen(UnitTestsView())
            case "build":
                self.app.push_screen(BuildView())
            case "qa":
                self.app.push_screen(PythonQAView())
            case "all":
                self.app.push_screen(AllTestsView())
            case _:
                pass  # Ignore unknown keys
