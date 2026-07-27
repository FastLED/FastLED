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


def _blocked_serial(*args: Any, **kwargs: Any) -> NoReturn:
    """Stand-in for serial.Serial that refuses to open a real device."""
    port = args[0] if args else kwargs.get("port", "<unknown>")
    raise AssertionError(
        f"Unit test tried to open real serial port {port!r}. Mock the "
        "serial/RPC layer (or the function that reaches it), or mark the "
        "test @pytest.mark.hardware if it truly needs a device. "
        "See issue #3582."
    )


def _install_serial_guard() -> "Any | None":
    """Swap in the stub at conftest import time; return the real class.

    conftest is imported before the test modules beside it, so installing here
    also covers module-import/collection time -- a port opened at import scope
    would run before any fixture and escape a fixture-only guard.

    Returns None when pyserial is absent (nothing to guard).
    """
    try:
        import serial
    except ImportError:
        return None
    real = serial.Serial
    serial.Serial = _blocked_serial  # type: ignore[misc, assignment]
    return real


_REAL_SERIAL = _install_serial_guard()


@pytest.fixture
def real_serial_class() -> "Any | None":
    """The genuine pyserial class saved before the guard replaced it.

    Exposed as a fixture because conftest.py is not importable by name from
    the test modules beside it; the guard's own self-tests need this handle to
    assert stub-vs-real identity without opening a port.
    """
    return _REAL_SERIAL


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

    The stub is installed at conftest import time (see _install_serial_guard),
    so this fixture only has to hand the real class *back* to hardware-marked
    tests -- and monkeypatch undoes that automatically at teardown, so the
    guard is restored for everything that follows.
    """
    if _REAL_SERIAL is None:
        return  # pyserial absent -- nothing to guard

    if "hardware" in request.keywords:
        import serial

        monkeypatch.setattr(serial, "Serial", _REAL_SERIAL)


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
