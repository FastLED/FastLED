"""Pytest configuration for FastLED test suite."""

import pytest
from _pytest.config import Config
from _pytest.config.argparsing import Parser
from _pytest.nodes import Item


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
