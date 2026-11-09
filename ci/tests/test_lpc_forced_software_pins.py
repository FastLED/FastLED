"""LPC must reject FASTLED_FORCE_SOFTWARE_PINS instead of ignoring it (#5073).

The native `_FL_DEFPIN` specializations override the forced software primary
template in `fastpin_base.h`, so the option used to be silently ignored. It
cannot simply be honored: LPC defines `FASTLED_NO_PINMAP`, so forced
`FastPin` is `NoPinMapPin`, which calls `fl::pinMode` / `fl::digitalWrite`,
and on LPC those come from `platforms/shared/pin_noop.hpp`. Until an LPC pin
backend exists, `fl/system/fastpin_base.h` stops the build with an explicit
error. Every fastpin route goes through it, including a direct
`fl/system/fastpin.h` include and the modern / legacy LPC11xx headers (#5189).

Real lpc845 / lpc804 builds show the error fires in forced mode and normal
builds are unaffected; this test pins the guard and its preconditions.
"""

import re
from pathlib import Path


SRC = Path(__file__).resolve().parents[2] / "src" / "platforms"
FASTPIN_BASE_INCLUDE = '#include "fl/system/fastpin_base.h"'


def _read(rel: str) -> str:
    return (SRC / rel).read_text(encoding="utf-8")


def test_shared_fastpin_base_rejects_lpc_forced_software_pins() -> None:
    text = (
        Path(__file__).resolve().parents[2] / "src" / "fl" / "system" / "fastpin_base.h"
    ).read_text(encoding="utf-8")
    guard = re.search(
        r"#if defined\(FL_IS_ARM_LPC\) && defined\(FASTLED_FORCE_SOFTWARE_PINS\)\n#error \"FASTLED_FORCE_SOFTWARE_PINS is not supported on LPC",
        text,
    )
    assert guard
    # is_platform.h -> platforms/arm/is_arm.h -> lpc/is_lpc.h defines FL_IS_ARM_LPC.
    assert text.index('#include "platforms/is_platform.h"') < guard.start()
    assert guard.start() < text.index('#include "fl/system/fastpin_no_pinmap.h"')
    assert guard.start() < text.index("namespace fl {")


def test_lpc_header_includes_shared_guard_before_any_pin_map() -> None:
    text = _read("arm/lpc/fastpin_arm_lpc.h")
    include = text.index(FASTPIN_BASE_INCLUDE)
    assert include < text.index(
        '#include "platforms/arm/lpc/fastpin_arm_lpc11_legacy.h"'
    )
    assert include < text.index("#define _FL_DEFPIN(")


def test_premise_lpc_pin_api_is_a_noop() -> None:
    # If an LPC pin backend lands, this fails and the guard can be replaced by
    # real forced-software-pin support.
    dispatch = _read("pin.h")
    lpc = re.search(
        r"#elif defined\(FL_IS_ARM_LPC\)\n\s*#include \"([^\"]+)\"", dispatch
    )
    assert lpc and lpc.group(1) == "platforms/shared/pin_noop.hpp"
    assert "#define FASTLED_NO_PINMAP" in _read("arm/lpc/led_sysdefs_arm_lpc.h")


def test_legacy_header_includes_shared_guard() -> None:
    text = _read("arm/lpc/fastpin_arm_lpc11_legacy.h")
    assert text.index(FASTPIN_BASE_INCLUDE) < text.index(
        "template<> class FastPin<PIN>"
    )
