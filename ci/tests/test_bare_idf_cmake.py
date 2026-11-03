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
    if major == 4:
        expected.add("mdns")
    if arduino:
        expected.add(arduino)
    assert set(output.read_text(encoding="utf-8").split(";")) == expected
