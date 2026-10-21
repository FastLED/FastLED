from types import SimpleNamespace
from typing import Any

import pytest

from ci.util import port_utils


def _ports(*pairs: tuple[str, str]) -> list[Any]:
    return [SimpleNamespace(device=d, serial_number=s) for d, s in pairs]


def test_follows_board_that_reenumerated(monkeypatch: pytest.MonkeyPatch) -> None:
    # USB-JTAG boards come back under a new /dev name after flashing.
    monkeypatch.setattr(
        port_utils.serial.tools.list_ports,
        "comports",
        lambda: _ports(("/dev/ttyACM0", "TEENSY"), ("/dev/ttyACM3", "80:F1")),
    )
    assert port_utils.find_port_by_serial("80:F1", wait_s=0) == "/dev/ttyACM3"


def test_returns_none_when_serial_absent(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(
        port_utils.serial.tools.list_ports,
        "comports",
        lambda: _ports(("/dev/ttyACM0", "TEENSY")),
    )
    assert port_utils.find_port_by_serial("80:F1", wait_s=0) is None
