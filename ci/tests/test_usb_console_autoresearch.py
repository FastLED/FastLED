"""Contract for the ESP32 native USB console hardware validation (#4893)."""

import asyncio

import pytest

from ci.autoresearch.args import Args


def test_usb_console_autoresearch_args() -> None:
    args = Args.parse_args(
        ["esp32s3", "--usb-console", "--upload-port", "/dev/ttyACM0"]
    )
    assert args.usb_console
    assert args.environment_positional == "esp32s3"


@pytest.mark.parametrize(
    ("flags", "backend"),
    [
        (
            "-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=0 "
            "-DFASTLED_ESP_FORCE_USB_SERIAL_JTAG=1",
            "usb_serial_jtag",
        ),
        ("-DARDUINO_USB_MODE=0 -DFL_ESP_USE_TINYUSB_CDC=1", "tinyusb_cdc"),
        ("-DFASTLED_ESP_FORCE_UART_SERIAL=1", None),
        ("", None),
    ],
)
def test_selected_backend(flags: str, backend: str | None) -> None:
    from ci.autoresearch.usb_console import selected_backend

    assert selected_backend(flags) == backend


@pytest.mark.parametrize(
    "arguments",
    [
        ["--usb-console", "--upload-port", "/dev/ttyACM0"],
        ["esp32s3", "--usb-console"],
        ["esp32s3", "--usb-console", "--rpc-smoke", "--upload-port", "/dev/ttyACM0"],
    ],
)
def test_usb_console_rejects_incomplete_or_mixed_invocations(
    arguments: list[str],
) -> None:
    from ci.autoresearch.usb_console import run_usb_console

    assert asyncio.run(run_usb_console(Args.parse_args(arguments))) == 1
