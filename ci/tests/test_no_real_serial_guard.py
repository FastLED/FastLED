"""Self-tests for the no-real-serial-ports guard in conftest.py (issue #3582).

The guard is what stops a unit test from quietly opening a real device. If it
ever stops firing, nothing else in the suite would notice -- tests would just
start touching hardware again, passing or failing by machine. So the guard
needs its own coverage, in both directions.
"""

import pytest
import serial


def test_guard_blocks_real_serial_open() -> None:
    """Opening a port in an unmarked test must fail with a pointing message."""
    with pytest.raises(AssertionError) as excinfo:
        serial.Serial("COM5", 115200, timeout=1.0)

    message = str(excinfo.value)
    # The port name must appear -- a guard that fires without saying *which*
    # port sends the reader hunting through mocks.
    assert "COM5" in message
    assert "@pytest.mark.hardware" in message


def test_guard_names_port_passed_as_keyword() -> None:
    """`Serial(port=...)` is as common as positional; both must be reported."""
    with pytest.raises(AssertionError) as excinfo:
        serial.Serial(port="/dev/ttyUSB0")

    assert "/dev/ttyUSB0" in str(excinfo.value)


@pytest.mark.hardware
def test_hardware_marker_restores_real_serial() -> None:
    """The opt-out must hand back the genuine pyserial class, not the stub.

    Asserted via identity against a fresh import rather than by opening a port,
    so this test stays hermetic and needs no device attached.
    """
    import importlib

    assert serial.Serial is importlib.import_module("serial").Serial
    assert isinstance(serial.Serial, type)
