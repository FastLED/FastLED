from __future__ import annotations

from ci.util import usb_vendors
from ci.util.port_utils import _describe_port


def test_vendor_table_lazy_loads_known_vids() -> None:
    # The compressed table decompresses on first use and resolves the VIDs
    # FastLED's boards actually use.
    assert usb_vendors.vendor_count() > 1000
    assert usb_vendors.vendor_name(0x1FC9) == "NXP Semiconductors"
    assert usb_vendors.vendor_name(0x10C4) == "Silicon Labs"
    assert usb_vendors.vendor_name(0x303A) == "Espressif Systems"
    assert usb_vendors.vendor_name(0x2341) == "Arduino SA"
    # 16C0 is the shared V-USB registrant (PJRC Teensy + legacy LPC VCOM).
    assert usb_vendors.vendor_name(0x16C0) == "Van Ooijen Technische Informatica"


def test_vendor_name_is_none_safe() -> None:
    assert usb_vendors.vendor_name(None) is None
    # A VID that isn't in the table resolves to None (not a crash).
    assert usb_vendors.vendor_name(0x0001_0000) is None


def test_describe_port_appends_vendor() -> None:
    class _P:
        device = "COM7"
        vid = 0x303A
        pid = 0x1001

    class _NoIds:
        device = "COM8"
        vid = None
        pid = None

    assert _describe_port(_P()) == "COM7=303A:1001 (Espressif Systems)"
    assert _describe_port(_NoIds()) == "COM8=----:----"
