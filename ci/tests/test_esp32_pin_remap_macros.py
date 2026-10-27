"""Contract for Arduino-ESP32 pin-remap macro immunity (#4914).

`cores/esp32/io_pin_remap.h` defines function-like macros named `pinMode`,
`digitalWrite`, `digitalRead`, `analogRead` and `analogWrite` whenever
`BOARD_HAS_PIN_REMAP` is set and the board does not use hardware GPIO numbers
(Arduino Nano ESP32). A function-like macro expands wherever its name is
followed by `(`, including in a declaration and after `fl::`, so it mangled
`src/fl/system/pin.h` and every FastLED call that reached it:

    cores/esp32/io_pin_remap.h:47:49: error: variable or field 'pinMode' declared void
    src/fl/system/pin.h:75:6: note: in expansion of macro 'pinMode'

FastLED's pin numbers are raw GPIO numbers, never Arduino D-numbers, so the
remap must not reach them at all. Parenthesising the name is the only C++
spelling that suppresses function-like macro expansion, so every FastLED-side
declaration, definition and call site writes `void (pinMode)(...)` or
`(fl::pinMode)(...)`.

`src/platforms/stub` is excluded: it implements the Arduino API on purpose.
`src/platforms/arduino` is excluded: it calls the Arduino API with Arduino pin
numbers, where the remap is correct.
"""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
NAMES = r"(pinMode|digitalWrite|digitalRead|analogRead|analogWrite)"
# A use the remap macros would rewrite: the bare name awaiting an argument list.
# A shielded use ends at `)`, so it never matches.
UNSHIELDED = re.compile(r"(?<![\w])" + NAMES + r"\s*\(")
SCANNED: tuple[str, ...] = (
    "src/fl",
    "src/platforms/esp/32",
    "src/platforms/shared",
)


def _code(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def test_fastled_spells_remapped_pin_names_in_parentheses() -> None:
    offenders: list[str] = []
    paths = sorted(p for d in SCANNED for p in (ROOT / d).rglob("*"))
    for path in paths:
        if path.suffix not in {".h", ".hpp", ".cpp"}:
            continue
        for number, line in enumerate(
            _code(path.read_text(errors="ignore")).splitlines(), 1
        ):
            if UNSHIELDED.search(line):
                offenders.append(f"{path.relative_to(ROOT)}:{number}: {line.strip()}")
    assert not offenders, (
        "io_pin_remap.h macros would rewrite these (see #4914):\n"
        + "\n".join(offenders)
    )
