"""Guard the verified P4 framework/SDK pair without changing legacy SDKs."""

import re

import pytest

from ci.boards import create_board
from ci.compiler.project_ini import ProjectIni


@pytest.mark.parametrize("environment", ["esp32dev", "esp32s3", "esp32p4"])
def test_modern_profile_uses_immutable_startup_fix_with_matching_sdk(
    environment: str,
) -> None:
    board = create_board(environment)
    project = ProjectIni.parseString(board.to_project_ini())
    section = "env:" + environment
    assert project.get_option(section, "platform") == (
        "https://github.com/pioarduino/platform-espressif32/releases/download/"
        "55.03.311/platform-espressif32.zip"
    )
    package = project.get_option(section, "platform_packages", "") or ""
    assert re.fullmatch(
        r"framework-arduinoespressif32@https://github.com/zackees/arduino-esp32/"
        r"archive/([0-9a-f]{40})\.tar\.gz#\1",
        package,
    ), "Framework startup repair must use an immutable source archive"
    flags = project.get_option(section, "build_flags", "") or ""
    assert "NO_GLOBAL_SERIAL" not in flags
    assert "NO_GLOBAL_INSTANCES" not in flags
    if environment != "esp32dev":
        assert "-DARDUINO_USB_CDC_ON_BOOT=0" in flags


def test_legacy_idf44_does_not_receive_an_incompatible_modern_core() -> None:
    board = create_board("esp32dev_idf44")
    project = ProjectIni.parseString(board.to_project_ini())
    package = project.get_option("env:esp32dev_idf44", "platform_packages", "") or ""
    assert "zackees/arduino-esp32" not in package
    assert board.platform != create_board("esp32p4").platform
