"""Contract for the Arduino-ESP32 startup replacement (#4892, #4911).

The library half redeclares core symbols weak. GCC applies a weak declaration
to every reference in the same object file. src/platforms is one unity object
and pulls in src/fl headers, so any other strong use there would silently
become weak and could resolve to null at runtime. `Serial` is checked too: with
CDC on boot it is a macro for HWCDCSerial.
"""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
IMPL = ROOT / "src/platforms/arduino/esp32_lazy_serial0_main.cpp.hpp"
WEAKENED = re.compile(
    r"\b(HWCDCSerial|uartSetPins|HWCDCSerialEvent|serialEventRun|Serial0|"
    r"getArduinoLoopTaskStackSize|getArduinoSetupWaitTime_ms|"
    r"shouldPrintChipDebugReport)\b|(?<![\w:])Serial\s*\."
)
# Code that can be compiled into an ESP32 Arduino image.
SCANNED = (
    "src/fl",
    "src/platforms/esp/32",
    "src/platforms/shared",
    "src/platforms/arduino",
)
# Sketch-side helpers that are never part of the library unity object.
SKETCH_ONLY = {ROOT / "src/platforms/sketch_fake.hpp"}
EXEMPT = {
    # The Arduino serial bridge; EspArduinoHeaderChecker forbids ESP code from
    # reaching it.
    ROOT / "src/platforms/arduino/io_arduino.hpp",
    # Already address-only weak, and compiled only for Arduino < 2.0.17, where
    # the startup replacement is off.
    ROOT / "src/platforms/esp/32/drivers/uart_esp32_idf.hpp",
}


def _code(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def test_weak_redeclarations_are_isolated() -> None:
    offenders = []
    paths = sorted(p for d in SCANNED for p in (ROOT / d).rglob("*"))
    for path in paths:
        if path.suffix not in {".h", ".hpp", ".cpp"} or path == IMPL:
            continue
        if path in SKETCH_ONLY or path in EXEMPT:
            continue
        for number, line in enumerate(_code(path.read_text()).splitlines(), 1):
            if WEAKENED.search(line):
                offenders.append(f"{path.relative_to(ROOT)}:{number}: {line.strip()}")
    assert not offenders, "\n".join(offenders)


def test_library_unity_files_define_marker_before_includes() -> None:
    """Library objects must not emit the sketch-side weak app_main (#5365).

    The Arduino IDE links library objects directly, so a weak app_main in any
    of them beats the core's main.o and defeats FL_ESP32_STOCK_ARDUINO_MAIN.
    esp32_lazy_serial0_main.h skips its definitions under this marker, and the
    marker only works if it is defined before the first include.
    """
    offenders = []
    for path in sorted((ROOT / "src/fl/build").glob("*.cpp")):
        code = _code(path.read_text())
        include = code.find("#include")
        if include < 0:
            continue  # Empty tombstone units compile nothing.
        marker = code.find("#define FL_BUILDING_FASTLED_LIBRARY 1")
        if marker < 0 or marker > include:
            offenders.append(str(path.relative_to(ROOT)))
    assert not offenders, (
        "define FL_BUILDING_FASTLED_LIBRARY before the first #include in: "
        + ", ".join(offenders)
    )


def test_sketch_startup_header_honors_library_marker() -> None:
    header = ROOT / "src/platforms/esp/32/esp32_lazy_serial0_main.h"
    assert "!defined(FL_BUILDING_FASTLED_LIBRARY)" in _code(header.read_text())


def test_sketch_only_files_stay_out_of_unity_build() -> None:
    builds = (ROOT / "src").rglob("_build.cpp.hpp")
    text = "\n".join(path.read_text() for path in builds)
    for path in SKETCH_ONLY:
        assert path.name not in text
