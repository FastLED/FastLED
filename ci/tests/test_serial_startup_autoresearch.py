"""Validate real CLI safety checks before any serial deployment."""

import asyncio

import pytest

from ci.autoresearch.args import Args
from ci.autoresearch.runner import run
from ci.autoresearch.serial_startup import verify_serial_records


def test_serial_records_reject_reordered_output() -> None:
    with pytest.raises(ValueError):
        verify_serial_records(
            ["ARDUINO_4892_RECORD"] * 8 + ["NATIVE_4892_RECORD"] * 8, False
        )


def test_native_only_rejects_unexpected_arduino_output() -> None:
    with pytest.raises(ValueError):
        verify_serial_records(
            ["NATIVE_4892_RECORD"] * 8 + ["ARDUINO_4892_RECORD"], True
        )


@pytest.mark.parametrize("native_only", [False, True])
def test_serial_records_accept_complete_device_sequence(native_only: bool) -> None:
    records = (
        ["NATIVE_4892_RECORD"]
        if native_only
        else ["ARDUINO_4892_RECORD", "NATIVE_4892_RECORD"]
    )
    verify_serial_records(["boot log", *records * 8, "RPC response"], native_only)


@pytest.mark.parametrize("mode", ["native-only", "native-first", "arduino-first"])
def test_serial_startup_mode_is_explicit(mode: str) -> None:
    args = Args.parse_args(["esp32dev", "--serial-startup", mode])
    assert args.serial_startup == mode


@pytest.mark.parametrize(
    "argv,diagnostic",
    [
        (["esp32dev", "--serial-startup", "native-first"], "explicit"),
        (
            [
                "esp32dev_idf44",
                "--serial-startup",
                "native-first",
                "--upload-port",
                "unused",
            ],
            "Arduino 3.3.11",
        ),
        (
            [
                "esp32dev",
                "--serial-startup",
                "native-first",
                "--upload-port",
                "unused",
                "--rmt",
            ],
            "separately",
        ),
        (
            ["esp32s3", "--serial-startup", "native-first", "--upload-port", "unused"],
            "classic ESP32",
        ),
    ],
)
def test_serial_startup_rejects_unsafe_configuration_before_deploy(
    argv: list[str], diagnostic: str, capsys: pytest.CaptureFixture[str]
) -> None:
    args = Args.parse_args(argv)
    assert asyncio.run(run(args)) == 1
    assert diagnostic in capsys.readouterr().err
