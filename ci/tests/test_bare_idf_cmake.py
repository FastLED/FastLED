"""Exercise component requirement selection with CMake, without an IDF install."""

from pathlib import Path

import pytest
from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize(
    "major,minor,extra",
    [
        (3, 3, set()),
        (4, 0, {"esp_rom"}),
        (4, 1, {"esp_rom", "esp_netif"}),
        (4, 4, {"esp_rom", "esp_netif", "esp_lcd"}),
        (5, 0, {"esp_rom", "esp_netif", "esp_lcd", "esp_adc"}),
        (5, 1, {"esp_rom", "esp_netif", "esp_lcd", "esp_mm", "esp_adc"}),
        (
            5,
            3,
            {
                "esp_rom",
                "esp_netif",
                "esp_lcd",
                "esp_mm",
                "esp_adc",
                "esp_driver_gpio",
                "esp_driver_gptimer",
                "esp_driver_ledc",
                "esp_driver_parlio",
                "esp_driver_spi",
                "esp_driver_uart",
                "esp_driver_usb_serial_jtag",
                "esp_driver_i2s",
                "esp_driver_mcpwm",
                "esp_driver_rmt",
            },
        ),
        (
            5,
            5,
            {
                "esp_rom",
                "esp_netif",
                "esp_lcd",
                "esp_mm",
                "esp_adc",
                "esp_driver_gpio",
                "esp_driver_gptimer",
                "esp_driver_i2s",
                "esp_driver_ledc",
                "esp_driver_mcpwm",
                "esp_driver_parlio",
                "esp_driver_rmt",
                "esp_driver_spi",
                "esp_driver_uart",
                "esp_driver_usb_serial_jtag",
            },
        ),
        (
            6,
            0,
            {
                "esp_rom",
                "esp_netif",
                "esp_lcd",
                "esp_mm",
                "esp_adc",
                "esp_driver_gpio",
                "esp_driver_gptimer",
                "esp_driver_ledc",
                "esp_driver_parlio",
                "esp_driver_spi",
                "esp_driver_uart",
                "esp_driver_usb_serial_jtag",
                "esp_driver_i2s",
                "esp_driver_mcpwm",
                "esp_driver_rmt",
            },
        ),
    ],
)
@pytest.mark.parametrize("arduino", ["", "arduino-esp32", "espressif__arduino-esp32"])
@pytest.mark.parametrize("phase", ["early", "late", "legacy_early", "legacy_late"])
def test_component_requirements(
    tmp_path: Path, major: int, minor: int, extra: set[str], arduino: str, phase: str
) -> None:
    """Both requirement expansion and registration must retain optional Arduino."""
    output = tmp_path / "requires.txt"
    script = tmp_path / "requirements.cmake"
    prop = "__COMPONENT_TARGETS" if phase == "early" else "BUILD_COMPONENTS"
    value = f"___idf_{arduino}" if phase == "early" and arduino else arduino
    inventory = (
        "function(idf_build_get_property out prop)\n"
        f'  if(prop STREQUAL "{prop}")\n'
        f'    set(${{out}} "{value}" PARENT_SCOPE)\n'
        "  endif()\nendfunction()\n"
    )
    if phase.startswith("legacy"):
        variable = "COMPONENTS" if phase == "legacy_early" else "BUILD_COMPONENTS"
        inventory = f'set({variable} "{arduino}")\n'
    script.write_text(
        f"set(IDF_VERSION_MAJOR {major})\nset(IDF_VERSION_MINOR {minor})\n"
        + inventory
        + "macro(idf_component_register)\n"
        '  cmake_parse_arguments(ARG "" "" "SRCS;INCLUDE_DIRS;REQUIRES" ${ARGN})\n'
        f'  file(WRITE "{output.as_posix()}" "${{ARG_REQUIRES}}")\n'
        "  return()\nendmacro()\n"
        f'include("{(ROOT / "CMakeLists.txt").as_posix()}")\n',
        encoding="utf-8",
    )
    result = RunningProcess.run(
        ["cmake", "-P", str(script)],
        cwd=str(ROOT),
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        check=False,
        timeout=30,
    )
    assert result.returncode == 0, result.stdout
    expected = {
        "esp_http_server",
        "driver",
        "esp_wifi",
        "app_update",
        "esp_timer",
        "mbedtls",
    } | extra
    if major >= 6:
        expected.add("bootloader_support")
    if major == 4:
        expected.add("mdns")
    if arduino:
        expected.add(arduino)
    assert set(output.read_text(encoding="utf-8").split(";")) == expected


def test_idf6_h264_dependency(tmp_path: Path) -> None:
    """Configure the managed H264 target in its own directory, as IDF does."""
    h264 = tmp_path / "h264"
    h264.mkdir()
    (h264 / "empty.c").write_text("void h264_stub(void) {}\n", encoding="utf-8")
    (h264 / "CMakeLists.txt").write_text(
        "add_library(__idf_espressif__esp_h264 STATIC empty.c)\n"
        "add_library(idf::espressif__esp_h264 ALIAS __idf_espressif__esp_h264)\n",
        encoding="utf-8",
    )
    (tmp_path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.16)\nproject(check_h264 C)\n"
        "set(IDF_VERSION_MAJOR 6)\nset(IDF_VERSION_MINOR 0)\n"
        "add_library(bootloader_support INTERFACE)\n"
        "add_library(idf::bootloader_support ALIAS bootloader_support)\n"
        "add_subdirectory(h264)\n"
        "function(idf_build_get_property out prop)\nendfunction()\n"
        "function(idf_component_register)\nendfunction()\n"
        "function(idf_component_get_property out component prop)\n"
        "  set(${out} __idf_espressif__esp_h264 PARENT_SCOPE)\nendfunction()\n"
        f'include("{(ROOT / "CMakeLists.txt").as_posix()}")\n'
        "get_target_property(deps __idf_espressif__esp_h264 LINK_LIBRARIES)\n"
        'if(NOT "idf::bootloader_support" IN_LIST deps)\n'
        '  message(FATAL_ERROR "H264 is missing bootloader_support")\nendif()\n',
        encoding="utf-8",
    )
    result = RunningProcess.run(
        ["cmake", "-S", str(tmp_path), "-B", str(tmp_path / "build")],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        check=False,
        timeout=30,
    )
    assert result.returncode == 0, result.stdout
