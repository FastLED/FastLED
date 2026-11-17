"""LPC forced software pins: honored on LPC845 / LPC804, rejected elsewhere.

LPC defines `FASTLED_NO_PINMAP`, so forced `FastPin` is `NoPinMapPin`, which
calls `fl::pinMode` / `fl::digitalWrite`. LPC845 / LPC804 route those to the
native backend `platforms/arm/lpc/pin_lpc.hpp` (#5260), and the `_FL_DEFPIN`
grid derives from `NoPinMapPin` in forced mode so the option is no longer
silently bypassed (#5073). LPC11xx / LPC11Uxx / LPC15xx still use
`pin_noop.hpp`, so `fl/system/fastpin_base.h` keeps an explicit `#error` for
them on every fastpin route, including a direct `fl/system/fastpin.h` include
and the legacy LPC11xx header (#5189). The cycle-counted and PLU clockless
drivers need `FastPin::port()` and reject forced pins with a static assert.

Real lpc845 / lpc804 builds are in the PR; this test pins the selection.
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


def test_shared_fastpin_base_rejects_only_lpc_without_backend() -> None:
    """fastpin_base.h holds the narrowed LPC guard before any pin map."""
    text: str = (ROOT / "src" / "fl" / "system" / "fastpin_base.h").read_text(
        encoding="utf-8"
    )
    guard = re.search(
        r"#if defined\(FL_IS_ARM_LPC\) && defined\(FASTLED_FORCE_SOFTWARE_PINS\) && \\\n"
        r"\s*!\(defined\(FL_IS_ARM_LPC_845\) \|\| defined\(FL_IS_ARM_LPC_804\)\)\n"
        r"#error \"FASTLED_FORCE_SOFTWARE_PINS is not supported on LPC11xx",
        text,
    )
    assert guard
    # is_platform.h -> platforms/arm/is_arm.h -> lpc/is_lpc.h defines FL_IS_ARM_LPC.
    assert text.index('#include "platforms/is_platform.h"') < guard.start()
    assert guard.start() < text.index('#include "fl/system/fastpin_no_pinmap.h"')
    assert guard.start() < text.index("namespace fl {")


def test_unspecified_lpc8xx_software_pins_are_invalid() -> None:
    """Forced LPC8xx pins outside the PIO0_0..31 grid stay invalid."""
    text: str = (ROOT / "src" / "fl" / "system" / "fastpin_base.h").read_text(
        encoding="utf-8"
    )
    selected: str = text.split("#define FL_FASTPIN_HAS_NO_PINMAP", 1)[1].split(
        "#elif defined(FASTLED_FORCE_SOFTWARE_PINS)", 1
    )[0]
    checked = re.search(
        r"#if [^\n]*\\\n\s*defined\(FL_IS_ARM_LPC_845\) \|\| defined\(FL_IS_ARM_LPC_804\)\n"
        r"(.*?)#else",
        selected,
        re.S,
    )
    assert checked
    assert "return false;" in checked.group(1)
    assert "FL_STATIC_ASSERT(PIN != PIN" in checked.group(1)


def test_lpc_header_includes_shared_guard_before_any_pin_map() -> None:
    """fastpin_arm_lpc.h includes the shared guard before its pin maps."""
    text: str = _read("arm/lpc/fastpin_arm_lpc.h")
    include: int = text.index(FASTPIN_BASE_INCLUDE)
    assert include < text.index(
        '#include "platforms/arm/lpc/fastpin_arm_lpc11_legacy.h"'
    )
    assert include < text.index("#define _FL_DEFPIN(")


def test_lpc8xx_dispatch_to_native_pin_backend() -> None:
    """LPC845 / LPC804 use pin_lpc.hpp; other LPC families keep pin_noop.hpp."""
    dispatch = _read("pin.h")
    lpc8xx = re.search(
        r"#elif defined\(FL_IS_ARM_LPC_845\) \|\| defined\(FL_IS_ARM_LPC_804\)\n"
        r"\s*#include \"([^\"]+)\"",
        dispatch,
    )
    assert lpc8xx and lpc8xx.group(1) == "platforms/arm/lpc/pin_lpc.hpp"
    rest = re.search(
        r"#elif defined\(FL_IS_ARM_LPC\)\n(?:\s*//[^\n]*\n)*\s*#include \"([^\"]+)\"",
        dispatch,
    )
    assert rest and rest.group(1) == "platforms/shared/pin_noop.hpp"
    assert lpc8xx.start() < rest.start()
    assert "#define FASTLED_NO_PINMAP" in _read("arm/lpc/led_sysdefs_arm_lpc.h")


def test_lpc_backend_does_not_recurse_through_fastpin() -> None:
    """Forced FastPin calls fl::digitalWrite, so the backend must not use FastPin."""
    text: str = _read("arm/lpc/pin_lpc.hpp")
    code: str = "\n".join(
        line for line in text.splitlines() if not line.lstrip().startswith("//")
    )
    assert "FastPin<" not in code
    assert "IOCON->PIO[Iocon::kIndex[pin]]" in code
    assert "SWM" not in code


def test_lpc_armpin_uses_core_api_in_forced_mode() -> None:
    """_ARMPIN derives from NoPinMapPin and register-level extras are gated."""
    text: str = _read("arm/lpc/fastpin_arm_lpc.h")
    software: str = text.split("#ifdef FL_FASTPIN_HAS_NO_PINMAP", 1)[1].split(
        "#else", 1
    )[0]
    assert "class _ARMPIN : public NoPinMapPin<PIN>" in software
    assert (
        "#if FL_LPC_ENABLE_FAST_SOFTWARE_SPI && !defined(FL_FASTPIN_HAS_NO_PINMAP)"
        in text
    )
    assert re.search(
        r"#ifndef FL_FASTPIN_HAS_NO_PINMAP\s*\n#define HAS_HARDWARE_PIN_SUPPORT\s*\n#endif",
        text,
    )


@pytest.mark.parametrize(
    "header", ["arm/lpc/clockless_arm_lpc.h", "arm/lpc/clockless_arm_lpc_plu.h"]
)
def test_port_based_clockless_rejects_forced_software_pins(header: str) -> None:
    """Drivers that write at offsets from FastPin::port() reject forced pins."""
    assert re.search(
        r"#ifdef FL_FASTPIN_HAS_NO_PINMAP\s*\n(?:\s*//[^\n]*\n)*"
        r"\s*FL_STATIC_ASSERT\(DATA_PIN != DATA_PIN,",
        _read(header),
    )


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
# Families with no pin backend; they need no vendor header to preprocess.
_LPC_NO_BACKEND: list[str] = ["__LPC11xx__", "__LPC11Uxx__", "__LPC15xx__"]


def _preprocess_direct_fastpin(
    tmp_path: Path, define: str, header: str, body: str, forced: bool
) -> CompletedProcess[str]:
    """Preprocess a TU that includes only fl/system/fastpin.h for an LPC variant."""
    compiler: str | None = shutil.which("clang++") or shutil.which("g++")
    assert compiler is not None, (
        "no host C++ compiler (clang++ or g++) found to preprocess with"
    )
    if header:
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


@pytest.mark.parametrize("define", _LPC_NO_BACKEND)
def test_direct_fastpin_include_rejects_forced_pins_without_backend(
    tmp_path: Path, define: str
) -> None:
    """LPC11xx / LPC11Uxx / LPC15xx still hit the #error in forced mode."""
    result: CompletedProcess[str] = _preprocess_direct_fastpin(
        tmp_path, define, "", "", forced=True
    )
    assert result.returncode != 0
    output: str = (result.stdout or "") + (result.stderr or "")
    assert "FASTLED_FORCE_SOFTWARE_PINS is not supported on LPC11xx" in output


@pytest.mark.parametrize("forced", [False, True])
@pytest.mark.parametrize("define,header,body", _LPC_VARIANTS)
def test_direct_fastpin_include_succeeds_on_lpc8xx(
    tmp_path: Path, define: str, header: str, body: str, forced: bool
) -> None:
    """LPC845 / LPC804 preprocess cleanly with and without forced pins."""
    result: CompletedProcess[str] = _preprocess_direct_fastpin(
        tmp_path, define, header, body, forced=forced
    )
    output: str = (result.stdout or "") + (result.stderr or "")
    assert result.returncode == 0, output[-2000:]
