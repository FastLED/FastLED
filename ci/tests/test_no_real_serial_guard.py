"""Self-tests for the no-real-serial-ports guard in conftest.py (issue #3582).

The guard is what stops a unit test from quietly opening a real device. If it
ever stops firing, nothing else in the suite would notice -- tests would just
start touching hardware again, passing or failing by machine. So the guard
needs its own coverage, in both directions.
"""

import pytest
import serial


# Captured at MODULE SCOPE, which runs during collection -- before any fixture
# for this module has executed. If the guard were fixture-only, this would hold
# the real pyserial class and the collection-time test below would fail.
_SERIAL_AT_IMPORT_TIME = serial.Serial


def test_guard_is_active_at_collection_time(real_serial_class) -> None:
    """The guard must be installed before test modules are imported.

    A port opened at module scope (import side effect) runs before any fixture,
    so a fixture-only guard would let it straight through to real hardware.
    """
    assert _SERIAL_AT_IMPORT_TIME is not real_serial_class
    with pytest.raises(AssertionError):
        _SERIAL_AT_IMPORT_TIME("COM9")


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
def test_hardware_marker_restores_real_serial(real_serial_class) -> None:
    """The opt-out must hand back the genuine pyserial class, not the stub.

    Asserted by identity against the saved original rather than by opening a
    port, so this test stays hermetic and needs no device attached.
    """
    assert serial.Serial is real_serial_class
    assert isinstance(serial.Serial, type)


def test_guard_restored_after_hardware_test(real_serial_class) -> None:
    """monkeypatch teardown must put the stub back.

    Ordering-sensitive: this runs after the hardware-marked test above. If the
    opt-out leaked, every later test in the session would silently lose the
    guard.
    """
    assert serial.Serial is not real_serial_class
    with pytest.raises(AssertionError):
        serial.Serial("COM7")
