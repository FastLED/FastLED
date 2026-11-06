"""Regression contract for ESP32 FastPin atomic GPIO toggling."""

import re
from pathlib import Path

import pytest


SOURCE = (
    Path(__file__).resolve().parents[2]
    / "src"
    / "platforms"
    / "esp"
    / "32"
    / "core"
    / "fastpin_esp32.h"
)


def test_esp32_fastpin_toggle_never_rewrites_unrelated_gpio_bits() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    toggle = source[
        source.index("inline static void toggle()") : source.index(
            "inline static void hi(FASTLED_REGISTER"
        )
    ]

    assert "*port() ^= MASK" not in toggle
    assert "if (isset())" in toggle
    assert "lo();" in toggle
    assert "hi();" in toggle


def _selected_esp32_guard_lines(
    source: str, force: int | None, no_pinmap: bool
) -> list[str]:
    """Source-bound guard projection only, not SDK preprocessing/type proof.

    Track selection guards for these two public switches. Other board-selection
    branches do not enclose the final native specializations and stay symbolic.
    """
    defined = {
        "FASTLED_FORCE_SOFTWARE_PINS": force is not None,
        "FASTLED_NO_PINMAP": no_pinmap,
    }
    stack: list[bool | None] = []
    selected = []
    for line in source.splitlines():
        directive = re.match(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", line)
        if directive:
            kind, condition = directive.groups()
            condition = condition.strip()
            if kind in ("if", "ifdef", "ifndef"):
                value = None
                for name, present in defined.items():
                    if kind in ("ifdef", "ifndef") and condition == name:
                        value = present if kind == "ifdef" else not present
                    elif kind == "if" and condition in (
                        f"defined({name})",
                        f"!defined({name})",
                    ):
                        value = (
                            present if condition.startswith("defined") else not present
                        )
                stack.append(value)
            elif kind == "else":
                assert stack, "unbalanced selection directive"
                if stack[-1] is not None:
                    stack[-1] = not stack[-1]
            elif kind == "elif":
                assert stack and stack[-1] is None, "unsupported public-switch elif"
            else:
                assert stack, "unbalanced selection directive"
                stack.pop()
            continue
        if False in stack:
            continue
        selected.append(line)
    assert not stack, "unclosed selection directive"
    return selected


def _selected_native_definitions(
    source: str, force: int | None, no_pinmap: bool
) -> tuple[list[int], bool]:
    selected = _selected_esp32_guard_lines(source, force, no_pinmap)
    pins = [
        int(pin)
        for line in selected
        if not line.lstrip().startswith("#define")
        for pin in re.findall(r"_FL_DEFPIN\(\s*(\d+)\s*\)", line)
    ]
    hardware = any(
        re.match(r"\s*#define\s+HAS_HARDWARE_PIN_SUPPORT\b", line) for line in selected
    )
    return pins, hardware


@pytest.mark.parametrize("force", [None, 1, 0])
@pytest.mark.parametrize("no_pinmap", [False, True])
def test_esp32_native_specializations_respect_forced_primary_selection(
    force: int | None,
    no_pinmap: bool,
) -> None:
    # The primary template uses #ifdef: an explicitly defined0 is still forced.
    pins, hardware = _selected_native_definitions(SOURCE.read_text(), force, no_pinmap)
    if force is None:
        assert pins == list(range(64))
        assert hardware is True
    else:
        assert pins == [], "native explicit specializations override the forced primary"
        assert hardware is False, (
            "forced selection must not advertise native pin support"
        )


@pytest.mark.parametrize("force", [None, 1, 0])
@pytest.mark.parametrize("no_pinmap", [False, True])
def test_esp32_my9221_forced_inclusion_defers_unsupported_provider_failure(
    force: int | None,
    no_pinmap: bool,
) -> None:
    # Guard/dependent-diagnostic source contract only; actual SDK include and
    # public-controller instantiation decide compilation, never this projection.
    emitter = SOURCE.parents[1] / "my9221_emitter.h"
    selected = "\n".join(
        _selected_esp32_guard_lines(emitter.read_text(), force, no_pinmap)
    )
    if force is None:
        assert "struct My9221Esp32PinFunctions" in selected
        assert "struct My9221Esp32PinOps" in selected
        assert "my9221Esp32Pin2_hi" in selected
        assert "void IRAM_ATTR my9221EmitEsp32(" in selected
        assert "MY9221 requires a valid selected data pin" in selected
    else:
        assert "struct My9221Esp32PinFunctions" not in selected
        assert "My9221Esp32PinOps" not in selected
        assert "my9221Esp32Pin2_hi" not in selected
        assert "my9221EmitEsp32(" not in selected
        assert (
            "template <int Data, fl::u8 Clock> struct My9221EmitterDispatch" in selected
        )
        assert "static FASTLED_FORCE_INLINE void run(" in selected
        assert "FL_STATIC_ASSERT" in selected
        assert "My9221Esp32ForcedUnsupported<Data, Clock>::value" in selected
        assert "static constexpr bool value = false;" in selected
        assert (
            "MY9221 forced ESP32 pin provider/cache closure is unsupported" in selected
        )


def _forced_mapping_condition_value(condition: str, defined: dict[str, bool]) -> bool:
    """Closed Boolean preprocessor-policy projection, not compiler/SDK proof."""
    import ast

    text = re.sub(r"defined\((\w+)\)", lambda m: str(defined[m[1]]), condition)
    text = (
        text.replace("&&", " and ").replace("||", " or ").replace("!", " not ").strip()
    )

    def boolean(node: ast.AST) -> bool:
        if isinstance(node, ast.Constant) and type(node.value) is bool:
            return node.value
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.Not):
            return not boolean(node.operand)
        if isinstance(node, ast.BoolOp):
            values = [boolean(value) for value in node.values]
            if isinstance(node.op, ast.And):
                return all(values)
            if isinstance(node.op, ast.Or):
                return any(values)
        raise AssertionError("unsupported mapper selection expression")

    return boolean(ast.parse(text, mode="eval").body)


@pytest.mark.parametrize("esp32", [False, True])
@pytest.mark.parametrize("no_pinmap", [None, 1, 0])
@pytest.mark.parametrize("arduino_mapper_macros", [False, True])
def test_esp32_forced_mapping_policy_is_independent_of_arduino_include_order(
    esp32: bool,
    no_pinmap: int | None,
    arduino_mapper_macros: bool,
) -> None:
    source = (SOURCE.parents[4] / "fl/system/forced_software_pin.h").read_text()
    begin = source.index("template <u8 PIN> struct ForcedSoftwarePinMap")
    end = source.index("template<fl::u8 PIN, class Mapping", begin)
    mapping = source[begin:end]
    condition = re.search(r"^#if (.+)$", mapping, re.MULTILINE)
    assert condition is not None
    defined = {
        "FL_IS_ESP32": esp32,
        "FASTLED_NO_PINMAP": no_pinmap is not None,
        **{
            key: arduino_mapper_macros
            for key in (
                "digitalPinToBitMask",
                "digitalPinToPort",
                "portOutputRegister",
                "portInputRegister",
            )
        },
    }
    has_map = _forced_mapping_condition_value(condition[1], defined)
    assert has_map is (not esp32 and no_pinmap is None), (
        "ESP32 forced provider must deterministically use fl::pin APIs even when Arduino was included first"
    )
    assert "#define FASTLED_NO_PINMAP" not in source, (
        "do not override the user's mapper switch"
    )
    # Non-ESP mapped branch and explicit NoMap semantics remain unchanged.
    mapped, fallback = mapping.split("#else", 1)
    assert "kHasPinMap = true" in mapped
    assert "digitalPinToBitMask(PIN)" in mapped
    assert "portOutputRegister(digitalPinToPort(PIN))" in mapped
    assert "portInputRegister(digitalPinToPort(PIN))" in mapped
    assert "kHasPinMap = false" in fallback
    assert "return nullptr" in fallback and "return 0" in fallback
    assert "(fl::digitalWrite)(PIN, PinValue::High)" in source
    assert "(fl::digitalRead)(PIN)" in source


@pytest.mark.parametrize(
    "relative,expected",
    [
        ("drivers/parlio/clockless_parlio_esp32.h", ("DATA_PIN",)),
        ("drivers/rmt/rmt_4/idf4_clockless_rmt_esp32.h", ("DATA_PIN",)),
        ("drivers/rmt/rmt_5/idf5_clockless.h", ("DATA_PIN",)),
        ("drivers/spi/idf5_clockless_spi_esp32.h", ("DATA_PIN",)),
        ("core/fastspi_esp32_idf.h", ("DATA_PIN", "CLOCK_PIN")),
    ],
)
def test_esp32_hardware_consumers_use_shared_output_validity_not_selected_fastpin(
    relative: str,
    expected: tuple[str, ...],
) -> None:
    # Hardware peripherals retain SDK/user-mask validity even when the software
    # GPIO provider changes. This source contract is not a vendor compile proof.
    source = (SOURCE.parents[1] / relative).read_text()
    assert not re.search(r"FastPin<[^>]+>::validpin\(", source)
    compact = re.sub(r"\s+", "", source)
    assert compact.count("Esp32OutputPinValidity<") == len(expected)
    for pin in expected:
        assert f"FL_STATIC_ASSERT(Esp32OutputPinValidity<{pin}>::value," in compact
    assert '#include "platforms/esp/32/core/output_pin_policy.h"' in source
    if relative.endswith("fastspi_esp32_idf.h"):
        assert '"Invalid data pin specified"' in source
        assert '"Invalid clock pin specified"' in source
    else:
        assert '"This pin has been marked as an invalid pin,' in source


@pytest.mark.parametrize("force", [None, 1, 0])
def test_esp32_shared_output_validity_remains_bounded_and_available_under_force(
    force: int | None,
) -> None:
    selected = "\n".join(
        _selected_esp32_guard_lines(OUTPUT_POLICY.read_text(), force, False)
    )
    compact = re.sub(r"\s+", "", selected)
    # Separate range specialization prevents even forming a shift for -1/64.
    # Actual SDK compile assertions will exercise those values independently.
    assert "boolInRange=(Pin>=0&&Pin<64)>structEsp32OutputPinValidity" in compact
    assert "structEsp32OutputPinValidity{staticconstexprboolvalue=false;};" in compact
    assert "structEsp32OutputPinValidity<Pin,true>" in compact
    assert "staticconstexprboolvalue=_FL_PIN_VALID(Pin);" in compact
    provider = (SOURCE.parents[4] / "fl/system/forced_software_pin.h").read_text()
    assert not re.search(r"\bvalidpin\s*\(", provider), (
        "do not invent validity on the forced provider"
    )


def test_esp32_native_fastpin_uses_the_same_sdk_user_mask_output_policy() -> None:
    source = SOURCE.read_text()
    declaration = next(
        line
        for line in source.splitlines()
        if line.startswith("#define _FL_DEFPIN(PIN)")
    )
    assert "Esp32OutputPinValidity<PIN>::value" in declaration
    assert "SOC_GPIO_VALID_OUTPUT_GPIO_MASK" in OUTPUT_POLICY.read_text()
    assert "~FASTLED_UNUSABLE_PIN_MASK" in OUTPUT_POLICY.read_text()


OUTPUT_POLICY = SOURCE.parent / "output_pin_policy.h"


def test_esp32_output_policy_is_independent_of_gpio_provider_selection() -> None:
    policy = OUTPUT_POLICY.read_text()
    assert not re.search(
        r"#include[^\n]*(?:fastpin|forced_software_pin|fl/system/pin\.h)", policy
    )
    assert not re.search(r"\bFASTLED_FORCE_SOFTWARE_PINS\b", policy)
    assert "SOC_GPIO_VALID_OUTPUT_GPIO_MASK" in policy
    assert "~FASTLED_UNUSABLE_PIN_MASK" in policy
    assert "Esp32OutputPinValidity" in policy
    assert '#include "platforms/esp/32/core/output_pin_policy.h"' in SOURCE.read_text()
    provider = (SOURCE.parents[4] / "fl/system/forced_software_pin.h").read_text()
    assert '#include "platforms/output_pin_policy.h"' in provider
    trampoline = (SOURCE.parents[4] / "platforms/output_pin_policy.h").read_text()
    assert '#include "platforms/esp/32/core/output_pin_policy.h"' in trampoline
    assert "defined(FL_IS_ESP32)" in trampoline
    assert "!defined(FASTLED_STUB_IMPL)" in trampoline
    assert "!defined(FL_IS_WASM)" in trampoline


def _forced_pin_method_bodies(source: str, method: str) -> list[str]:
    bodies = []
    for match in re.finditer(r"\bvoid\s+" + method + r"\s*\([^)]*\)", source):
        start = source.index("{", match.end())
        depth = 1
        end = start + 1
        while depth:
            assert end < len(source), "unterminated forced method"
            depth += (source[end] == "{") - (source[end] == "}")
            end += 1
        bodies.append(source[start + 1 : end - 1])
    return bodies


@pytest.mark.parametrize(
    "method,count",
    [
        ("setOutput", 1),
        ("hi", 2),
        ("lo", 2),
        ("toggle", 1),
        ("set", 1),
        ("fastset", 1),
    ],
)
def test_esp32_forced_output_methods_enforce_the_shared_output_policy(
    method: str,
    count: int,
) -> None:
    source = (SOURCE.parents[4] / "fl/system/forced_software_pin.h").read_text()
    bodies = _forced_pin_method_bodies(source, method)
    assert len(bodies) == count
    for body in bodies:
        assert body.lstrip().startswith("validateOutputPin();"), (
            "output validation must precede GPIO/register side effects, including non-null port overloads"
        )
    assert "Esp32OutputPinValidity<PIN>::value" in source
    assert "Invalid ESP32 forced output pin" in source
    assert not re.search(r"\bvalidpin\s*\(", source), "do not invent provider validity"


def test_esp32_forced_input_only_use_remains_separate_from_output_validation() -> None:
    source = (SOURCE.parents[4] / "fl/system/forced_software_pin.h").read_text()
    inputs = _forced_pin_method_bodies(source, "setInput")
    assert len(inputs) == 1
    assert "validateOutputPin" not in inputs[0]
    assert "PinMode::Input" in inputs[0]
    strobes = _forced_pin_method_bodies(source, "strobe")
    assert len(strobes) == 1
    assert strobes[0].count("toggle();") == 2


@pytest.mark.parametrize(
    "mode,sdk_mode,pullup,pulldown",
    [
        (
            "Output",
            "GPIO_MODE_INPUT_OUTPUT",
            "GPIO_PULLUP_DISABLE",
            "GPIO_PULLDOWN_DISABLE",
        ),
        ("Input", "GPIO_MODE_INPUT", "GPIO_PULLUP_DISABLE", "GPIO_PULLDOWN_DISABLE"),
        (
            "InputPullup",
            "GPIO_MODE_INPUT",
            "GPIO_PULLUP_ENABLE",
            "GPIO_PULLDOWN_DISABLE",
        ),
        (
            "InputPulldown",
            "GPIO_MODE_INPUT",
            "GPIO_PULLUP_DISABLE",
            "GPIO_PULLDOWN_ENABLE",
        ),
    ],
)
def test_esp32_native_pin_modes_preserve_output_readback_and_input_controls(
    mode: str,
    sdk_mode: str,
    pullup: str,
    pulldown: str,
) -> None:
    # Real SDK gpio_get_level requires the input buffer enabled. This checks
    # configuration source only; it does not simulate or measure pad levels.
    source = (SOURCE.parents[1] / "pin_esp32_native_impl.hpp").read_text()
    branch = re.search(r"case PinMode::" + mode + r":(.*?)break;", source, re.DOTALL)
    assert branch is not None
    assert f"io_conf.mode = {sdk_mode};" in branch[1]
    assert f"io_conf.pull_up_en = {pullup};" in branch[1]
    assert f"io_conf.pull_down_en = {pulldown};" in branch[1]
    assert "io_conf.pin_bit_mask = (1ULL << pin);" in source
    assert "gpio_config(&io_conf);" in source
    assert "gpio_get_level(static_cast<gpio_num_t>(pin))" in source
