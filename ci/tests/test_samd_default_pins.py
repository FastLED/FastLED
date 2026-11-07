"""Every SAMD board's default example pins must be pins that board actually has.

`Blink` and `Apa102` used to hardcode pins 3, and 1/2. Adafruit Feather M4 has
none of 2 or 3 -- `fastpin_arm_d51.h` says so in a comment, "no pins 2 3" -- so
both examples failed to compile for it, with a `FastPin<3>::validpin()` static
assertion whose message talks about ground and read-only and noisy pins. That
sends you looking at wiring rather than at a pin table. It went unnoticed
because SAMD had never built at all (#4011).

The examples now default to `FL_PIN_CLOCKLESS_1` / `FL_PIN_SPI_DATA_1` /
`FL_PIN_SPI_CLOCK_1`, which a board may define beside its `_FL_DEFPIN` table.
These tests check the resulting value is in that table, for every SAMD board --
not only the four that CI compiles.

Parser limitation, stated rather than hidden: pins are collected across a
board's nested conditionals, so the set is a superset for boards that vary
their table by sub-variant. That still catches "this pin exists nowhere for
this board", which is the failure that occurred; it would not catch a pin valid
only for a sibling sub-variant.
"""

import re
import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


REPO_ROOT = Path(__file__).resolve().parents[2]
SRC = REPO_ROOT / "src"

FASTPIN_HEADERS = (
    SRC / "platforms" / "arm" / "d21" / "fastpin_arm_d21.h",
    SRC / "platforms" / "arm" / "d51" / "fastpin_arm_d51.h",
)

# Fallbacks in platforms/default_pins.h, used when a board defines no override.
FALLBACKS = {"FL_PIN_CLOCKLESS_1": 3, "FL_PIN_SPI_DATA_1": 1, "FL_PIN_SPI_CLOCK_1": 2}

_OPEN = re.compile(r"^\s*#\s*(if|ifdef|ifndef)\b")
_CLOSE = re.compile(r"^\s*#\s*endif\b")
_BRANCH = re.compile(r"^\s*#\s*(?:el)?if\s+defined\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)")
_DEFPIN = re.compile(r"_FL_DEFPIN\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)")
_DEFAULT = re.compile(
    r"^\s*#\s*define\s+(FL_PIN_CLOCKLESS_1|FL_PIN_SPI_DATA_1|FL_PIN_SPI_CLOCK_1)\s+(\d+)"
)


def _board_blocks(header: Path) -> dict[str, dict]:
    """Map each top-level board branch to its pins and default-pin overrides.

    Board branches sit one level inside the file's
    `#if defined(FASTLED_FORCE_SOFTWARE_PINS) / #else` wrapper. That wrapper's
    own depth is derived rather than assumed -- these headers sit inside include
    guards, so it is not at depth zero.
    """
    boards: dict[str, dict] = {}
    depth = 0
    wrapper_depth: int | None = None
    board_depth: int | None = None
    current: str | None = None

    for line in header.read_text(encoding="utf-8").splitlines():
        if _CLOSE.match(line):
            depth -= 1
            if board_depth is not None and depth < board_depth:
                current = None
            continue

        opening = bool(_OPEN.match(line))
        if opening:
            depth += 1
        m = _BRANCH.match(line)

        if wrapper_depth is None:
            if m and "FASTLED_FORCE_SOFTWARE_PINS" in line:
                wrapper_depth = depth
                board_depth = depth + 1
            continue

        if m and depth == board_depth:
            current = m.group(1)
            boards.setdefault(current, {"pins": set(), "mappings": {}, "defaults": {}})
            continue

        if current is None:
            continue
        for pm in _DEFPIN.finditer(line):
            pin, bit, group = map(int, pm.groups())
            boards[current]["pins"].add(pin)
            boards[current]["mappings"][pin] = (bit, group)
        dm = _DEFAULT.match(line)
        if dm:
            boards[current]["defaults"][dm.group(1)] = int(dm.group(2))

    return {name: b for name, b in boards.items() if b["pins"]}


def _all_boards() -> list[tuple[str, str, dict]]:
    out = []
    for header in FASTPIN_HEADERS:
        for name, block in _board_blocks(header).items():
            out.append((header.name, name, block))
    return out


BOARDS = _all_boards()


def test_the_parser_found_boards() -> None:
    """Guard against the parser silently matching nothing and passing."""
    assert len(BOARDS) >= 15, f"only found {len(BOARDS)} boards; parser likely broken"
    names = {n for _, n, _ in BOARDS}
    assert "ADAFRUIT_FEATHER_M4_EXPRESS" in names
    assert "ADAFRUIT_ITSYBITSY_M0" in names


@pytest.mark.parametrize(
    "header,board,block",
    [(h, b, blk) for h, b, blk in BOARDS],
    ids=[f"{h.split('_')[-1].rstrip('.h')}:{b}" for h, b, _ in BOARDS],
)
@pytest.mark.parametrize("macro", sorted(FALLBACKS))
def test_default_pin_exists_on_this_board(
    header: str, board: str, block: dict, macro: str
) -> None:
    effective = block["defaults"].get(macro, FALLBACKS[macro])
    assert effective in block["pins"], (
        f"{header}: {board} resolves {macro} to pin {effective}, which it does not "
        f"define. FastPin<{effective}>::validpin() will fail and any example using "
        f"this default will not compile. Define {macro} in this board's branch "
        f"(see platforms/default_pins.h). Pins available: "
        f"{sorted(block['pins'])[:20]}"
    )


def test_feather_m4_regression() -> None:
    """The board that started this: it must not resolve to pin 2 or 3."""
    blocks = _board_blocks(SRC / "platforms" / "arm" / "d51" / "fastpin_arm_d51.h")
    for board in ("ADAFRUIT_FEATHER_M4_EXPRESS", "ADAFRUIT_FEATHER_M4_CAN"):
        block = blocks[board]
        assert 2 not in block["pins"] and 3 not in block["pins"], (
            f"{board} now defines pin 2 or 3; this test's premise changed"
        )
        for macro in FALLBACKS:
            assert block["defaults"].get(macro, FALLBACKS[macro]) not in (2, 3)


def test_qtpy_m0_onboard_neopixel_mapping() -> None:
    """QT Py M0's PIN_NEOPIXEL (D11) is PA18, only in its board branch."""
    blocks = _board_blocks(SRC / "platforms" / "arm" / "d21" / "fastpin_arm_d21.h")
    qtpy = blocks["ADAFRUIT_QTPY_M0"]
    assert qtpy["mappings"].get(11) == (18, 0)
    assert qtpy["defaults"].get("FL_PIN_CLOCKLESS_1") == 11


def _ci13_pin_configuration(defines: dict[str, int]) -> str:
    """Preprocess actual pin-table/default directives without vendor C++ bodies.

    This projects only configuration directives and literal pin registrations;
    it supplies no SDK types, GPIO implementation or copied SPI writer.
    """
    compiler = next(
        (path for name in ("clang++", "g++") if (path := shutil.which(name))),
        None,
    )
    assert compiler is not None, "host preprocessor required for pin configuration"
    header = SRC / "platforms/ci13xx/fastpin_ci13xx.h"
    projected = []
    for line in header.read_text(encoding="utf-8").splitlines():
        if re.match(r"^\s*#\s*(if|ifdef|ifndef|else|elif|endif)\b", line):
            projected.append(line)
        elif re.match(r"^\s*#\s*define\s+FL_PIN_", line):
            projected.append(line)
        elif re.match(r"^_FL_CI13XX_DEFPIN\(\d+", line):
            projected.append(line)
    source = "\n".join(projected)
    defaults = (SRC / "platforms/default_pins.h").read_text(encoding="utf-8")
    source += "\n" + "\n".join(
        line
        for line in defaults.splitlines()
        if not re.fullmatch(r"\s*#\s*pragma\s+once\s*", line)
    )
    source += "\nCI13_SELECTED FL_PIN_SPI_DATA_1 FL_PIN_SPI_CLOCK_1\n"
    result = RunningProcess.run(
        [compiler, "-E", "-P", "-x", "c++"]
        + [f"-D{name}={value}" for name, value in defines.items()]
        + ["-"],
        input=source,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=30,
    )
    assert result.returncode == 0, result.stderr
    return result.stdout


@pytest.mark.parametrize(
    "board", ("ARDUINO_CI1306", "ARDUINO_CI1302", "ARDUINO_CI1303")
)
@pytest.mark.parametrize("crystal", (0, 1))
def test_ci13_spi_defaults_select_common_valid_pins(board: str, crystal: int) -> None:
    output = _ci13_pin_configuration({board: 1, "USE_EXTERNAL_CRYSTAL_OSC": crystal})
    mappings = {
        int(pin): (port, int(bit))
        for pin, port, bit in re.findall(
            r"_FL_CI13XX_DEFPIN\((\d+),\s*(_FL_CI13XX_P[A-D]),\s*(\d+)\)", output
        )
    }
    assert mappings[2] == ("_FL_CI13XX_PA", 2)
    assert mappings[3] == ("_FL_CI13XX_PA", 3)
    assert re.search(r"CI13_SELECTED\s+2\s+3\b", output), output
    if board == "ARDUINO_CI1306" or crystal:
        assert 0 not in mappings and 1 not in mappings


@pytest.mark.parametrize("forced", (0, 1))
def test_ci13_spi_defaults_survive_forced_pins_and_honor_overrides(forced: int) -> None:
    defines = {"ARDUINO_CI1306": 1}
    if forced:
        defines["FASTLED_FORCE_SOFTWARE_PINS"] = 1
    assert re.search(r"CI13_SELECTED\s+2\s+3\b", _ci13_pin_configuration(defines))
    defines.update(FL_PIN_SPI_DATA_1=5, FL_PIN_SPI_CLOCK_1=6)
    assert re.search(r"CI13_SELECTED\s+5\s+6\b", _ci13_pin_configuration(defines))


def test_ci13_software_spi_probe_consumes_platform_pin_defaults() -> None:
    probe = (
        REPO_ROOT / "examples/AutoResearch/AutoResearchSoftwareSpiPins.h"
    ).read_text(encoding="utf-8")
    ci13, other = probe.split("#else", 1)
    assert "#if defined(FL_IS_CI13XX)" in ci13
    assert re.search(r"kDataPin\s*=\s*FL_PIN_SPI_DATA_1\s*;", ci13)
    assert re.search(r"kClockPin\s*=\s*FL_PIN_SPI_CLOCK_1\s*;", ci13)
    assert re.search(r"kDataPin\s*=\s*0\s*;", other)
    assert re.search(r"kClockPin\s*=\s*1\s*;", other)
