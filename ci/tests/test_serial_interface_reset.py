"""RED tests: SerialInterface protocol and adapters must have reset_device()."""

from __future__ import annotations


def test_serial_interface_protocol_has_reset_device() -> None:
    """SerialInterface protocol must declare reset_device."""
    from ci.util.serial_interface import SerialInterface

    assert "reset_device" in dir(SerialInterface), (
        "SerialInterface protocol missing reset_device method"
    )


def test_fbuild_adapter_has_reset_device() -> None:
    """FbuildSerialAdapter must implement reset_device."""
    from ci.util.serial_interface import FbuildSerialAdapter

    assert hasattr(FbuildSerialAdapter, "reset_device"), (
        "FbuildSerialAdapter missing reset_device method"
    )


def test_pyserial_adapter_has_reset_device() -> None:
    """PySerialAdapter must implement reset_device."""
    from ci.util.serial_interface import PySerialAdapter

    assert hasattr(PySerialAdapter, "reset_device"), (
        "PySerialAdapter missing reset_device method"
    )
