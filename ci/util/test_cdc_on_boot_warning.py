"""Tests for ci.util.cdc_on_boot_warning module."""

import io
import sys
from pathlib import Path
from unittest.mock import patch

import pytest

from ci.util.cdc_on_boot_warning import (
    check_cdc_on_boot_in_file,
    check_cdc_on_boot_in_flags,
    print_cdc_on_boot_warning,
    warn_if_cdc_on_boot,
)


class TestCheckCdcOnBootInFlags:
    def test_none_flags(self) -> None:
        assert check_cdc_on_boot_in_flags(None) is False

    def test_empty_flags(self) -> None:
        assert check_cdc_on_boot_in_flags([]) is False

    def test_no_cdc_flag(self) -> None:
        assert check_cdc_on_boot_in_flags(["-DFOO=1", "-DBAR=2"]) is False

    def test_cdc_on_boot_enabled_no_space(self) -> None:
        assert check_cdc_on_boot_in_flags(["-DARDUINO_USB_CDC_ON_BOOT=1"]) is True

    def test_cdc_on_boot_enabled_with_space(self) -> None:
        assert check_cdc_on_boot_in_flags(["-D ARDUINO_USB_CDC_ON_BOOT=1"]) is True

    def test_cdc_on_boot_disabled(self) -> None:
        assert check_cdc_on_boot_in_flags(["-DARDUINO_USB_CDC_ON_BOOT=0"]) is False

    def test_cdc_on_boot_among_other_flags(self) -> None:
        flags = ["-DFOO=1", "-DARDUINO_USB_CDC_ON_BOOT=1", "-DBAR=2"]
        assert check_cdc_on_boot_in_flags(flags) is True


class TestCheckCdcOnBootInFile:
    def test_file_with_cdc_enabled(self, tmp_path: Path) -> None:
        ini = tmp_path / "platformio.ini"
        ini.write_text("[env:esp32c6]\nbuild_flags =\n  -D ARDUINO_USB_CDC_ON_BOOT=1\n")
        assert check_cdc_on_boot_in_file(ini) is True

    def test_file_with_cdc_disabled(self, tmp_path: Path) -> None:
        ini = tmp_path / "platformio.ini"
        ini.write_text("[env:esp32dev]\nbuild_flags =\n  -DARDUINO_USB_CDC_ON_BOOT=0\n")
        assert check_cdc_on_boot_in_file(ini) is False

    def test_file_without_cdc(self, tmp_path: Path) -> None:
        ini = tmp_path / "platformio.ini"
        ini.write_text("[env:uno]\nplatform = atmelavr\n")
        assert check_cdc_on_boot_in_file(ini) is False

    def test_nonexistent_file(self, tmp_path: Path) -> None:
        ini = tmp_path / "nonexistent.ini"
        assert check_cdc_on_boot_in_file(ini) is False


class TestPrintCdcOnBootWarning:
    def test_prints_yellow_warning(self, capsys: pytest.CaptureFixture[str]) -> None:
        print_cdc_on_boot_warning()
        captured = capsys.readouterr()
        assert "CDC ON BOOT" in captured.out
        assert "ARDUINO_USB_CDC_ON_BOOT=1" in captured.out
        assert "USB" in captured.out
        # Check for yellow ANSI escape code
        assert "\033[33m" in captured.out


class TestWarnIfCdcOnBoot:
    def test_returns_false_when_not_detected(self) -> None:
        assert warn_if_cdc_on_boot(build_flags=["-DFOO=1"]) is False

    def test_returns_true_and_prints_when_detected_in_flags(
        self, capsys: pytest.CaptureFixture[str]
    ) -> None:
        result = warn_if_cdc_on_boot(build_flags=["-DARDUINO_USB_CDC_ON_BOOT=1"])
        assert result is True
        captured = capsys.readouterr()
        assert "CDC ON BOOT" in captured.out

    def test_returns_true_when_detected_in_file(
        self, tmp_path: Path, capsys: pytest.CaptureFixture[str]
    ) -> None:
        ini = tmp_path / "platformio.ini"
        ini.write_text("build_flags = -D ARDUINO_USB_CDC_ON_BOOT=1\n")
        result = warn_if_cdc_on_boot(ini_path=ini)
        assert result is True
        captured = capsys.readouterr()
        assert "CDC ON BOOT" in captured.out

    def test_returns_false_with_no_args(self) -> None:
        assert warn_if_cdc_on_boot() is False
