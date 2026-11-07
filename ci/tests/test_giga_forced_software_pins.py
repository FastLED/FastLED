"""Arduino GIGA must honor FASTLED_FORCE_SOFTWARE_PINS (#5071).

`_FL_DEFPIN` used to expand to an explicit `fl::FastPin<PIN>` specialization,
which overrides the forced software primary template in `fastpin_base.h`. It
now always emits the native `GigaHwPin<PIN>` and adds the `FastPin`
specialization only outside forced mode. The GIGA clockless driver uses
`GigaHwPin` directly: in forced mode `FastPin` is `NoPinMapPin` (GIGA defines
`FASTLED_NO_PINMAP`), whose `port()` is null.

The headers are parsed textually; real-SDK builds cover selection.
"""

import re
from pathlib import Path


GIGA = Path(__file__).resolve().parents[2] / "src" / "platforms" / "arm" / "giga"


def _read(name: str) -> str:
    return (GIGA / name).read_text(encoding="utf-8")


def _defpin_branches() -> list[str]:
    match = re.search(
        r"#if defined\(FASTLED_FORCE_SOFTWARE_PINS\)\n(.*?)#else\n(.*?)#endif",
        _read("armpin.h"),
        re.S,
    )
    assert match, "force guard around _FL_DEFPIN not found"
    return [match.group(1), match.group(2)]


def test_forced_mode_adds_no_fastpin_specialization() -> None:
    forced, normal = _defpin_branches()
    assert "#define _FL_GIGA_FASTPIN(PIN)" in forced
    assert "FastPin" not in forced
    assert "template<> class FastPin<PIN> : public GigaHwPin<PIN>" in normal


def test_defpin_emits_native_pin_and_conditional_fastpin() -> None:
    text = _read("armpin.h")
    assert text.count("#define _FL_DEFPIN(") == 1
    body = re.search(r"#define _FL_DEFPIN\(.*?\n((?:.*\\\n)*.*)", text)
    assert body
    assert "class GigaHwPin<PIN>" in body.group(1)
    assert "_FL_GIGA_FASTPIN(PIN)" in body.group(1)
    assert "FastPin" not in body.group(1).replace("_FL_GIGA_FASTPIN", "")


def test_clockless_driver_uses_the_native_pin() -> None:
    text = _read("clockless_arm_giga.h")
    assert "FastPin<" not in text
    assert "GigaHwPin<DATA_PIN>::port()" in text
