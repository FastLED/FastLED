"""Closed C6 compiler selected-type checks, not execution or admitted evidence.

Diagnostic excerpts and synthetic paired-output fixtures exercise this parser.
Only a future managed attempt can bind actual builds, source restoration and
sessions. Classic ESP32, GPIO behavior, MY support, masks, timing and full
finding configurations are not qualified by these functions.
"""

import re
from pathlib import Path, PurePosixPath


SOURCE = "src/platforms/esp/32/core/fastpin_esp32.h"
STAGES = ("baseline", "unguarded-native-mutant", "restored")
_FIXTURES = "ci/fixtures/software_spi_audit"
_TUS = {
    "src/sketch/AutoResearch.ino": "esp32_c6_selection_producer.cpp.txt",
    "src/sketch/AutoResearchStartupHook.cpp": "esp32_c6_selection_startup.cpp.txt",
}
_MESSAGES = (
    '#error "forced selection must not advertise hardware pin support"',
    "static assertion failed: forced GPIO2 must use ForcedSoftwarePin",
    "static assertion failed: forced GPIO3 must use ForcedSoftwarePin",
)


def unguarded_native_mutant(source: bytes) -> bytes:
    opening = b"#if !defined(FASTLED_FORCE_SOFTWARE_PINS)\n"
    closing = b"#endif // !defined(FASTLED_FORCE_SOFTWARE_PINS)\n"
    if source.count(opening) != 1 or source.count(closing) != 1:
        raise ValueError("missing or ambiguous native specialization guard")
    start, end = source.index(opening), source.index(closing)
    body = source[start + len(opening) : end]
    # The closed mutation removes only this complete native specialization/HAS
    # boundary. Never change mapping, producer assertions or the forced provider.
    if (
        start >= end
        or body.count(b"_FL_DEFPIN(") != 65
        or body.count(b"#define HAS_HARDWARE_PIN_SUPPORT") != 1
    ):
        raise ValueError("unbound native specialization block")
    return source[:start] + body + source[end + len(closing) :]


def validate_stage_sequence(stages: tuple[str, ...]) -> None:
    if stages != STAGES:
        raise ValueError("incomplete or reordered closed selected-type attempt")


def producer_sources(root: Path) -> dict[str, bytes]:
    base = root / _FIXTURES
    return {
        "examples/AutoResearch/AutoResearch.ino": (
            base / _TUS["src/sketch/AutoResearch.ino"]
        ).read_bytes(),
        "examples/AutoResearch/AutoResearchStartupHook.cpp": (
            base / _TUS["src/sketch/AutoResearchStartupHook.cpp"]
        ).read_bytes(),
    }


def _diagnostic_contract() -> set[tuple[str, int, str]]:
    base = Path(__file__).resolve().parents[1] / _FIXTURES
    expected = set()
    for source, filename in _TUS.items():
        lines = (base / filename).read_text().splitlines()
        for message in _MESSAGES:
            token = (
                "must not advertise hardware pin support"
                if message.startswith("#error")
                else message.removeprefix("static assertion failed: ")
            )
            matching = [index for index, line in enumerate(lines, 1) if token in line]
            if len(matching) != 1:
                raise ValueError("ambiguous code-owned selection diagnostic")
            expected.add((source, matching[0], message))
    return expected


def validate_stage_output(stage: str, returncode: int, output: str) -> None:
    """Check designated compiler results; never turn caller text into a receipt."""
    if (
        stage not in STAGES
        or type(returncode) is not int
        or not isinstance(output, str)
    ):
        raise ValueError("unknown closed selection stage/result")
    expected = _diagnostic_contract()
    observed = set()
    for line in output.splitlines():
        if not re.search(
            r"\b(?:fatal\s+)?error:|AddressSanitizer|UndefinedBehaviorSanitizer", line
        ):
            continue
        match = re.fullmatch(
            r"(src/sketch/(?:AutoResearch\.ino|AutoResearchStartupHook\.cpp)):(\d+):(\d+): error: (.+)",
            line,
        )
        if match is None or int(match[3]) < 1:
            raise ValueError("unrelated or spoofed compiler diagnostic")
        diagnostic = (match[1], int(match[2]), match[4])
        if diagnostic not in expected:
            raise ValueError("unbound selection assertion origin or message")
        observed.add(diagnostic)
    if stage == STAGES[1]:
        if returncode != 1 or observed != expected:
            raise ValueError("mutant lacks all six designated selection diagnostics")
    elif returncode != 0 or observed:
        raise ValueError("positive selected-type build failed")


def validate_global_defines(
    tokens: list[str], expected_force: int | None, expected_no_pinmap: int | None = None
) -> None:
    """Validate finite preprocessing cells; presence, including zero, forces."""
    if not isinstance(tokens, list) or any(
        not isinstance(token, str) for token in tokens
    ):
        raise ValueError("selection flags must be string tokens")
    if expected_force not in (None, 0, 1) or expected_no_pinmap not in (None, 0, 1):
        raise ValueError("unreviewed selection macro value")
    required = {
        "EXPECT_FORCED": str(int(expected_force is not None)),
        "EXPECT_NO_PINMAP": str(int(expected_no_pinmap is not None)),
        "FASTLED_FORCE_SOFTWARE_PINS": None
        if expected_force is None
        else str(expected_force),
        "FASTLED_NO_PINMAP": None
        if expected_no_pinmap is None
        else str(expected_no_pinmap),
    }
    observed: dict[str, list[str]] = {name: [] for name in required}
    index = 0
    while index < len(tokens):
        token = tokens[index]
        index += 1
        if token == "-Xpreprocessor":
            raise ValueError("unreviewed forwarded preprocessor flags")
        if token.startswith("-Wp,"):
            for part in token[4:].split(","):
                operand = part[2:] if part.startswith(("-D", "-U")) else part
                name = re.match(r"[A-Za-z_]\w*", operand)
                if name and name[0] in required:
                    raise ValueError("opaque selection macro override")
        operation, operand = token[:2], token[2:]
        if operation not in ("-D", "-U"):
            continue
        if not operand:
            if index == len(tokens):
                raise ValueError("missing preprocessor operand")
            operand = tokens[index]
            index += 1
        name = re.match(r"[A-Za-z_]\w*", operand)
        if name and name[0] in required:
            # Prefix recognition detects attempts to redefine a required name,
            # but only an exact scalar NAME or NAME=VALUE is admissible.
            if re.fullmatch(r"[A-Za-z_]\w*(?:=[^\s=]+)?", operand) is None:
                raise ValueError("required selection macro must be scalar")
            if operation == "-U":
                raise ValueError("selection macro undefinition")
            observed[name[0]].append(
                operand.partition("=")[2] if "=" in operand else "1"
            )
    for name, value in required.items():
        if observed[name] != ([] if value is None else [value]):
            raise ValueError("missing, duplicate or conflicting selection macro")


def validate_c6_profile(info: dict) -> None:
    """Configuration predicate only; SDK/tool/session identity needs a runner."""
    flags = info.get("cxx_flags", [])
    compiler = info.get("cc_path", "")
    compiler_parts = PurePosixPath(compiler).parts if isinstance(compiler, str) else ()
    if (
        info.get("env") != "esp32c6"
        or info.get("board") != "ESP32_C6_DEVKITC_1"
        or info.get("platform") != "espressif32"
        or [flag for flag in flags if flag.startswith("-DF_CPU=")]
        != ["-DF_CPU=160000000L"]
        or [flag for flag in flags if flag.startswith("-march=")]
        != ["-march=rv32imac_zicsr_zifencei"]
        # Fixed Linux x86_64 SDK namespace, not a permissive version substring.
        or compiler_parts[-4:]
        != (
            "riscv32-esp-elf-14.2.0_20260121-x86_64-linux-gnu",
            "riscv32-esp-elf",
            "bin",
            "riscv32-esp-elf-gcc",
        )
    ):
        raise ValueError("unsupported closed C6 selection profile")
