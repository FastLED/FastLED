"""Pytest configuration for FastLED test suite."""

from typing import Any, NoReturn

import pytest
from _pytest.config import Config
from _pytest.config.argparsing import Parser
from _pytest.nodes import Item
from pytest import FixtureRequest, MonkeyPatch


def pytest_addoption(parser: Parser) -> None:
    """Add custom command line options."""
    parser.addoption(
        "--runslow",
        action="store_true",
        default=False,
        help="run slow tests (e.g., compilation tests)",
    )


def pytest_configure(config: Config) -> None:
    """Register custom markers."""
    config.addinivalue_line("markers", "slow: mark test as slow to run")
    config.addinivalue_line(
        "markers", "serial: mark test to run serially (not in parallel)"
    )
    config.addinivalue_line(
        "markers",
        "hardware: test genuinely needs a real serial device; exempt from the "
        "no-real-ports guard",
    )


@pytest.fixture(autouse=True)
def block_real_serial_ports(request: FixtureRequest, monkeypatch: MonkeyPatch) -> None:
    """Fail loudly if a unit test tries to open a real serial port.

    Unit tests must never touch hardware. When they do, the symptom is
    confusing and machine-dependent: on a bench host the call may *succeed*
    and talk to whatever is plugged in, while in CI it fails with an opaque
    ``FileNotFoundError`` on a COM port nobody configured. Issue #3582 was
    exactly that -- an RPC path that escaped its mocks and opened COM5, so
    the suite passed or failed depending on the machine.

    Replacing ``serial.Serial`` with a raising stub converts that whole class
    of escape into an immediate, self-explanatory failure that names the port
    the test tried to open. Tests that legitimately need a device opt out with
    ``@pytest.mark.hardware``.
    """
    if "hardware" in request.keywords:
        return

    try:
        import serial
    except ImportError:  # pyserial absent -- nothing to guard
        return

    def _blocked(*args: Any, **kwargs: Any) -> NoReturn:
        port = args[0] if args else kwargs.get("port", "<unknown>")
        raise AssertionError(
            f"Unit test tried to open real serial port {port!r}. Mock the "
            "serial/RPC layer (or the function that reaches it), or mark the "
            "test @pytest.mark.hardware if it truly needs a device. "
            "See issue #3582."
        )

    monkeypatch.setattr(serial, "Serial", _blocked)


def pytest_collection_modifyitems(config: Config, items: list[Item]) -> None:
    """Skip slow tests by default unless --runslow is given.

    Also marks serial tests to prevent parallel execution.
    """
    if config.getoption("--runslow"):
        # --runslow given in cli: do not skip slow tests
        return

    skip_slow = pytest.mark.skip(reason="need --runslow option to run")
    for item in items:
        if "slow" in item.keywords:
            item.add_marker(skip_slow)
        # Mark serial tests to prevent parallel execution
        if "serial" in item.keywords:
            item.add_marker(pytest.mark.xdist_group(name="serial"))
