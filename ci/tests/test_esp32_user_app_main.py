"""Compile the actual sketch startup header with and without a user app_main."""

import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize("user_main", [False, True])
def test_sketch_startup_header(tmp_path: Path, user_main: bool) -> None:
    compiler = shutil.which("c++")
    if compiler is None:
        pytest.fail("C++ compiler required: c++ not found on PATH")
    headers = {
        "esp_arduino_version.h": "#define ESP_ARDUINO_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))\n#define ESP_ARDUINO_VERSION 30311\n",
        "sdkconfig.h": "#define CONFIG_AUTOSTART_ARDUINO 1\n",
        "esp32-hal.h": "",
        "esp32-hal-uart.h": "",
        "chip-debug-report.h": "",
        "io_pin_remap.h": "",
        "freertos/FreeRTOS.h": "using TaskHandle_t = void*;\n",
        "freertos/task.h": "",
    }
    for name, contents in headers.items():
        header = tmp_path / name
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_text(contents)
    source = tmp_path / "main.cpp"
    source.write_text(
        ("#define FL_ESP32_USER_APP_MAIN 1\n" if user_main else "")
        + '#include "platforms/esp/32/esp32_lazy_serial0_main.h"\n'
        + "#if !FL_ESP32_ARDUINO_LAZY_SERIAL0\n#error Test must exercise the lazy startup gate\n#endif\n"
        + ('extern "C" void app_main() noexcept {}\n' if user_main else "")
    )
    result = RunningProcess.run(
        [
            compiler,
            "-std=c++17",
            "-fsyntax-only",
            "-DESP32",
            "-DARDUINO_ARCH_ESP32",
            "-I" + str(tmp_path),
            "-I" + str(ROOT / "src"),
            str(source),
        ],
        check=False,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=30,
    )
    assert result.returncode == 0, result.stdout


@pytest.mark.parametrize(
    "esp32,forbidden,success",
    [(True, True, True), (False, True, False), (False, False, True)],
)
def test_shared_arduino_header(
    tmp_path: Path, esp32: bool, forbidden: bool, success: bool
) -> None:
    compiler = shutil.which("c++")
    if compiler is None:
        pytest.fail("C++ compiler required: c++ not found on PATH")
    (tmp_path / "Arduino.h").write_text("#define TEST_ARDUINO_INCLUDED 1\n")
    source = tmp_path / "guard.cpp"
    source.write_text(
        '#include "fl/system/arduino.h"\n'
        + (
            "#ifdef TEST_ARDUINO_INCLUDED\n#error ESP32 included Arduino.h\n#endif\n"
            if esp32
            else ""
        )
        + (
            "#ifndef TEST_ARDUINO_INCLUDED\n#error Arduino compatibility missing\n#endif\n"
            if not esp32 and not forbidden
            else ""
        )
    )
    flags = (["-DESP32"] if esp32 else []) + (
        ["-DFL_NO_ARDUINO=1"] if forbidden else []
    )
    result = RunningProcess.run(
        [
            compiler,
            "-fsyntax-only",
            *flags,
            "-I" + str(tmp_path),
            "-I" + str(ROOT / "src"),
            str(source),
        ],
        check=False,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=30,
    )
    assert (result.returncode == 0) == success, result.stdout
    if not success:
        assert "Arduino.h is forbidden by FL_NO_ARDUINO" in result.stdout


def test_user_app_main_keeps_loop_task_globals(tmp_path: Path) -> None:
    """A user app_main must not remove the weak loop-task globals.

    arduino-esp32's esp32-hal-misc.c (loop WDT) and esp32-hal-rmt.c link against
    loopTaskHandle / loopTaskWDTEnabled weakly. Guarding them behind
    !FL_ESP32_USER_APP_MAIN silently breaks the watchdog for applications that
    supply their own app_main, so they must survive that define.
    """
    compiler = shutil.which("c++")
    if compiler is None:
        pytest.fail("C++ compiler required: c++ not found on PATH")
    headers = {
        "esp_arduino_version.h": "#define ESP_ARDUINO_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))\n#define ESP_ARDUINO_VERSION 30311\n",
        "sdkconfig.h": "#define CONFIG_AUTOSTART_ARDUINO 1\n",
        "esp32-hal.h": "",
        "esp32-hal-uart.h": "",
        "chip-debug-report.h": "",
        "io_pin_remap.h": "",
        "freertos/FreeRTOS.h": "using TaskHandle_t = void*;\n",
        "freertos/task.h": "",
    }
    for name, contents in headers.items():
        header = tmp_path / name
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_text(contents)
    source = tmp_path / "main.cpp"
    source.write_text(
        "#define FL_ESP32_USER_APP_MAIN 1\n"
        '#include "platforms/esp/32/esp32_lazy_serial0_main.h"\n'
        'extern "C" void app_main() noexcept {}\n'
    )
    result = RunningProcess.run(
        [
            compiler,
            "-std=c++17",
            "-E",
            "-DESP32",
            "-DARDUINO_ARCH_ESP32",
            "-I" + str(tmp_path),
            "-I" + str(ROOT / "src"),
            str(source),
        ],
        check=False,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=30,
    )
    assert result.returncode == 0, result.stdout
    preprocessed = result.stdout
    assert "loopTaskHandle" in preprocessed, (
        "loopTaskHandle vanished under FL_ESP32_USER_APP_MAIN"
    )
    assert "loopTaskWDTEnabled" in preprocessed, (
        "loopTaskWDTEnabled vanished under FL_ESP32_USER_APP_MAIN"
    )
