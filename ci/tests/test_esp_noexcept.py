"""Compile-time ESP annotation selection contracts for FastLED #4773.

These isolated compiler-feature probes avoid the native PCH, which has already
selected the host contract. Actual ESP SDK/driver builds are verified by fbuild.
"""

import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


SRC = Path(__file__).resolve().parents[2] / "src"
TARGETS = [
    "ESP8266",
    "ESP32",
    "ARDUINO_ARCH_ESP8266",
    "ARDUINO_ARCH_ESP32",
    "CONFIG_IDF_TARGET_ESP32",
    "CONFIG_IDF_TARGET_ESP32S2",
    "CONFIG_IDF_TARGET_ESP32S3",
    "CONFIG_IDF_TARGET_ESP32C2",
    "CONFIG_IDF_TARGET_ESP32C3",
    "CONFIG_IDF_TARGET_ESP32C5",
    "CONFIG_IDF_TARGET_ESP32C6",
    "CONFIG_IDF_TARGET_ESP32H2",
    "CONFIG_IDF_TARGET_ESP32P4",
]


def _compile(source: str, flags: list[str], system_exceptions: bool = False) -> None:
    compiler = shutil.which("c++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("C++ compiler required for isolated feature probes")
    result = RunningProcess.run(
        [
            compiler,
            "-std=c++11",
            "-fexceptions" if system_exceptions else "-fno-exceptions",
            "-fsyntax-only",
            "-x",
            "c++",
            f"-I{SRC}",
            *flags,
            "-",
        ],
        input=(
            "#ifndef __EXCEPTIONS\n#error System exception policy must remain enabled\n#endif\n"
            if system_exceptions
            else "#ifdef __EXCEPTIONS\n#error C++ exceptions must be disabled\n#endif\n"
        )
        + source,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=120,
    )
    assert result.returncode == 0, result.stdout


def _probe(real: bool, advertised: bool | None = None) -> str:
    if advertised is None:
        advertised = real
    capability = (
        "#if !defined(FL_HAS_NOEXCEPT) || FL_HAS_NOEXCEPT != 1\n"
        "#error real contract must advertise capability\n#endif\n"
        if advertised
        else "#ifdef FL_HAS_NOEXCEPT\n#error noop must not advertise capability\n#endif\n"
    )
    return capability + (
        "void annotation_probe() FL_NO_EXCEPT;\n"
        f"static_assert(noexcept(annotation_probe()) == {'true' if real else 'false'},"
        ' "annotation contract");\n'
    )


@pytest.mark.parametrize("target", TARGETS)
@pytest.mark.parametrize("detection_first", [False, True])
def test_esp_selection(target: str, detection_first: bool) -> None:
    source = '#include "platforms/esp/is_esp.h"\n' if detection_first else ""
    source += '#include "fl/stl/noexcept.h"\n' + _probe(True)
    _compile(source, [f"-D{target}=1"])


def test_sdkconfig_target_is_loaded_before_selection(tmp_path: Path) -> None:
    (tmp_path / "sdkconfig.h").write_text("#define CONFIG_IDF_TARGET_ESP32C6 1\n")
    _compile('#include "fl/stl/noexcept.h"\n' + _probe(True), [f"-I{tmp_path}"])


def test_system_debug_exception_policy_preserves_nonthrowing_contract() -> None:
    _compile(
        '#include "fl/stl/noexcept.h"\n' + _probe(True),
        ["-DARDUINO_ARCH_ESP32=1"],
        system_exceptions=True,
    )


@pytest.mark.parametrize("target", [None, "ARDUINO_ARCH_AVR", "__EMSCRIPTEN__"])
def test_non_esp_default_is_unchanged(target: str | None) -> None:
    _compile(
        '#include "fl/stl/noexcept.h"\n' + _probe(False),
        [f"-D{target}=1"] if target else [],
    )


@pytest.mark.parametrize("esp", [False, True])
@pytest.mark.parametrize("real", [False, True])
def test_external_override_is_preserved(esp: bool, real: bool) -> None:
    source = f"#define FL_NO_EXCEPT {'noexcept' if real else ''}\n"
    if real:
        source += "#define FL_HAS_NOEXCEPT 1\n"
    source += '#include "fl/stl/noexcept.h"\n' + _probe(real)
    _compile(source, ["-DARDUINO_ARCH_ESP32=1"] if esp else [])


@pytest.mark.parametrize(
    "spec,real",
    [
        ("noexcept", True),
        ("noexcept(false)", False),
        ("noexcept(sizeof(int) != 0)", True),
    ],
)
def test_external_override_without_capability_makes_no_claim(
    spec: str, real: bool
) -> None:
    _compile(
        f"#define FL_NO_EXCEPT {spec}\n"
        '#include "fl/stl/noexcept.h"\n' + _probe(real, advertised=False),
        ["-DARDUINO_ARCH_ESP32=1"],
    )
