"""Compile the real ESP console dispatcher without loading SDK implementations."""

import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


SRC = Path(__file__).resolve().parents[2] / "src"


@pytest.mark.parametrize(
    "target,uart,override,expected",
    [
        ("ESP32", True, None, 1),
        ("ESP32", False, None, 1),
        ("ESP32", True, 0, -1),
        ("ESP32", True, 1, 1),
        ("ESP32S3", True, None, 1),
        ("ESP32S2", True, None, 1),
        ("ESP32C3", True, None, 1),
        ("ESP32C6", True, None, 1),
        ("ESP32H2", True, None, 1),
        ("ESP32P4", True, None, 1),
    ],
)
def test_console_dispatch(
    tmp_path: Path, target: str, uart: bool, override: int | None, expected: int
) -> None:
    compiler = shutil.which("c++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("C++ compiler required for console selection probes")
    (tmp_path / "Arduino.h").touch()
    (tmp_path / "sdkconfig.h").write_text(f"#define CONFIG_IDF_TARGET_{target} 1\n")
    if uart:
        (tmp_path / "driver").mkdir()
        (tmp_path / "driver/uart.h").touch()
    for name, native in (
        ("platforms/esp/32/io_esp_idf.hpp", 1),
        ("platforms/arduino/io_arduino.hpp", 0),
    ):
        header = tmp_path / name
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_text(f"#define SELECTED_NATIVE_CONSOLE {native}\n")
    flags = [f"-DFL_ESP_USE_IDF_SERIAL={override}"] if override is not None else []
    result = RunningProcess.run(
        [
            compiler,
            "-std=c++11",
            "-fsyntax-only",
            "-x",
            "c++",
            f"-I{tmp_path}",
            f"-I{SRC}",
            *flags,
            "-",
        ],
        input=(
            f'#include "{SRC / "platforms/esp/32/io_esp.cpp.hpp"}"\n'
            f'static_assert(SELECTED_NATIVE_CONSOLE == {expected}, "console backend");\n'
        ),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=30,
    )
    if expected < 0:
        assert result.returncode != 0
        assert "remove FL_ESP_USE_IDF_SERIAL=0" in result.stdout
    else:
        assert result.returncode == 0, result.stdout


@pytest.mark.parametrize(
    "target,flags,usb,expected,error",
    [
        ("ESP32", [], False, (0, 0), None),
        (
            "ESP32S3",
            ["ARDUINO_USB_CDC_ON_BOOT=0", "FASTLED_ESP_FORCE_USB_SERIAL_JTAG=1"],
            True,
            (0, 1),
            None,
        ),
        (
            "ESP32C5",
            ["ARDUINO_USB_CDC_ON_BOOT=0", "FASTLED_ESP_FORCE_USB_SERIAL_JTAG=1"],
            True,
            (0, 1),
            None,
        ),
        (
            "ESP32P4",
            ["ARDUINO_USB_CDC_ON_BOOT=0", "FASTLED_ESP_FORCE_USB_SERIAL_JTAG=1"],
            True,
            (0, 1),
            None,
        ),
        (
            "ESP32S3",
            ["ARDUINO_USB_MODE=1", "ARDUINO_USB_CDC_ON_BOOT=1"],
            True,
            (0, 0),
            None,
        ),
        (
            "ESP32S3",
            ["FASTLED_ESP_FORCE_USB_SERIAL_JTAG=1", "ARDUINO_USB_CDC_ON_BOOT=1"],
            True,
            (0, 0),
            "Native USB console requires",
        ),
        ("ESP32S3", ["FL_ESP_USE_TINYUSB_CDC=1"], True, (1, 0), None),
        ("ESP32S2", ["FL_ESP_USE_TINYUSB_CDC=1"], False, (1, 0), None),
        ("ESP32P4", ["FL_ESP_USE_TINYUSB_CDC=1"], True, (1, 0), None),
        (
            "ESP32S3",
            ["ARDUINO_USB_MODE=0", "ARDUINO_USB_CDC_ON_BOOT=1"],
            True,
            (0, 0),
            None,
        ),
        (
            "ESP32S3",
            ["FL_ESP_USE_TINYUSB_CDC=1", "ARDUINO_USB_CDC_ON_BOOT=1"],
            True,
            (0, 0),
            "startup disabled",
        ),
        (
            "ESP32S3",
            ["FL_ESP_USE_TINYUSB_CDC=1", "ARDUINO_USB_MSC_ON_BOOT=1"],
            True,
            (0, 0),
            "startup disabled",
        ),
        (
            "ESP32S3",
            ["ARDUINO_USB_CDC_ON_BOOT=0", "FASTLED_ESP_FORCE_USB_SERIAL_JTAG=1"],
            False,
            (0, 0),
            "USB Serial/JTAG support",
        ),
    ],
)
def test_native_console_transport(
    tmp_path: Path,
    target: str,
    flags: list[str],
    usb: bool,
    expected: tuple[int, int],
    error: str | None,
) -> None:
    compiler = shutil.which("c++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("C++ compiler required for console selection probes")
    (tmp_path / "sdkconfig.h").write_text(f"#define CONFIG_IDF_TARGET_{target} 1\n")
    (tmp_path / "esp_idf_version.h").write_text(
        "#define ESP_IDF_VERSION_MAJOR 5\n#define ESP_IDF_VERSION_MINOR 5\n#define ESP_IDF_VERSION_PATCH 0\n"
    )
    if usb:
        (tmp_path / "driver").mkdir()
        (tmp_path / "driver/usb_serial_jtag.h").touch()
    result = RunningProcess.run(
        [
            compiler,
            "-std=c++11",
            "-fsyntax-only",
            "-x",
            "c++",
            f"-I{tmp_path}",
            f"-I{SRC}",
            *[f"-D{flag}" for flag in flags],
            "-",
        ],
        input=(
            '#include "platforms/esp/32/detail/console_config.h"\n'
            f'static_assert(FL_ESP_IO_USE_TINYUSB_CDC == {expected[0]}, "TinyUSB selection");\n'
            f'static_assert(FL_ESP_IO_USE_USB_SERIAL_JTAG == {expected[1]}, "JTAG selection");\n'
        ),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=30,
    )
    if error:
        assert result.returncode != 0
        assert error in result.stdout
    else:
        assert result.returncode == 0, result.stdout


@pytest.mark.parametrize("target", ["ESP32S2", "ESP32S3", "ESP32C3", "ESP32P4"])
@pytest.mark.parametrize("usb_mode", [0, 1])
@pytest.mark.parametrize("force_uart", [False, True])
def test_stock_arduino_cdc_keeps_framework_owner(
    tmp_path: Path, target: str, usb_mode: int, force_uart: bool
) -> None:
    """Stock CDC startup must compile without selecting a competing USB owner."""
    flags = [f"ARDUINO_USB_MODE={usb_mode}", "ARDUINO_USB_CDC_ON_BOOT=1"]
    if force_uart:
        flags.append("FASTLED_ESP_FORCE_UART_SERIAL=1")
    test_native_console_transport(
        tmp_path, target, flags, target != "ESP32S2", (0, 0), None
    )
