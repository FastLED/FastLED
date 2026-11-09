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
import shutil
from pathlib import Path

import pytest
from running_process import CompletedProcess, RunningProcess


ROOT: Path = Path(__file__).resolve().parents[2]
SRC: Path = ROOT / "src" / "platforms"
FASTPIN_BASE_INCLUDE: str = '#include "fl/system/fastpin_base.h"'


def _read(rel: str) -> str:
    """Return the text of a header under src/platforms."""
    return (SRC / rel).read_text(encoding="utf-8")


def test_shared_fastpin_base_rejects_lpc_forced_software_pins() -> None:
    """fastpin_base.h holds the LPC guard after is_platform.h, before any pin map."""
    text: str = (ROOT / "src" / "fl" / "system" / "fastpin_base.h").read_text(
        encoding="utf-8"
    )
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
    """fastpin_arm_lpc.h includes the shared guard before its pin maps."""
    text: str = _read("arm/lpc/fastpin_arm_lpc.h")
    include: int = text.index(FASTPIN_BASE_INCLUDE)
    assert include < text.index(
        '#include "platforms/arm/lpc/fastpin_arm_lpc11_legacy.h"'
    )
    assert include < text.index("#define _FL_DEFPIN(")


def test_premise_lpc_pin_api_is_a_noop() -> None:
    """LPC pin API is the no-op backend and LPC defines FASTLED_NO_PINMAP."""
    # If an LPC pin backend lands, this fails and the guard can be replaced by
    # real forced-software-pin support.
    dispatch = _read("pin.h")
    lpc = re.search(
        r"#elif defined\(FL_IS_ARM_LPC\)\n\s*#include \"([^\"]+)\"", dispatch
    )
    assert lpc and lpc.group(1) == "platforms/shared/pin_noop.hpp"
    assert "#define FASTLED_NO_PINMAP" in _read("arm/lpc/led_sysdefs_arm_lpc.h")


def test_legacy_header_includes_shared_guard() -> None:
    """The legacy LPC11xx header includes the shared guard before its FastPin."""
    text: str = _read("arm/lpc/fastpin_arm_lpc11_legacy.h")
    assert text.index(FASTPIN_BASE_INCLUDE) < text.index(
        "template<> class FastPin<PIN>"
    )


# Vendor CMSIS headers are not on the host; stub just what the LPC sysdefs check.
_LPC_VARIANTS: list[tuple[str, str, str]] = [
    ("__LPC845__", "LPC845.h", "#define SCT0_BASE 0\n"),
    ("__LPC804__", "LPC804.h", "#define PLU_BASE 0\n"),
]


def _preprocess_direct_fastpin(
    tmp_path: Path, define: str, header: str, body: str, forced: bool
) -> CompletedProcess[str]:
    """Preprocess a TU that includes only fl/system/fastpin.h for an LPC variant."""
    compiler: str | None = shutil.which("clang++") or shutil.which("g++")
    assert compiler is not None, (
        "no host C++ compiler (clang++ or g++) found to preprocess with"
    )
    (tmp_path / header).write_text(body, encoding="utf-8")
    cmd: list[str] = [
        compiler,
        "-std=gnu++17",
        "-E",
        "-x",
        "c++",
        "-",
        "-o",
        str(tmp_path / "out.i"),
        f"-I{ROOT / 'src'}",
        f"-I{tmp_path}",
        f"-D{define}",
    ]
    if forced:
        cmd.append("-DFASTLED_FORCE_SOFTWARE_PINS")
    return RunningProcess.run(
        cmd,
        input='#include "fl/system/fastpin.h"\n',
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=120,
        check=False,
    )


@pytest.mark.parametrize("define,header,body", _LPC_VARIANTS)
def test_direct_fastpin_include_rejects_forced_software_pins(
    tmp_path: Path, define: str, header: str, body: str
) -> None:
    """A direct fastpin.h include hits the #error in forced-software mode."""
    result: CompletedProcess[str] = _preprocess_direct_fastpin(
        tmp_path, define, header, body, forced=True
    )
    assert result.returncode != 0
    output: str = (result.stdout or "") + (result.stderr or "")
    assert "FASTLED_FORCE_SOFTWARE_PINS is not supported on LPC" in output


@pytest.mark.parametrize("define,header,body", _LPC_VARIANTS)
def test_direct_fastpin_include_succeeds_without_forced_pins(
    tmp_path: Path, define: str, header: str, body: str
) -> None:
    """A direct fastpin.h include preprocesses cleanly without forced pins."""
    result: CompletedProcess[str] = _preprocess_direct_fastpin(
        tmp_path, define, header, body, forced=False
    )
    output: str = (result.stdout or "") + (result.stderr or "")
    assert result.returncode == 0, output[-2000:]
