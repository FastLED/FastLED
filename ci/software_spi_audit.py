"""Fail-closed software SPI source accounting and evidence foundation.

Inventory classification is review bookkeeping, never proof of an optimization.
In particular, no externally supplied JSON receipt is currently accepted as
execution evidence. Target runners must be wired before this audit can pass a
positive finding. Ordinary unit tests may pass while audit completion stays RED.
"""

import argparse
import hashlib
import json
import re
from collections.abc import Mapping, Sequence
from pathlib import Path

from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[1]
BASELINE_COMMIT = "94dab9c6b720ea277ec9177c93129c7dc646c5c3"
BASELINE_SOURCE_COUNT = 2986


def _required(
    identity: str,
    issue: int,
    source: str,
    entrypoints: list[str],
    configurations: list[str],
    dependencies: list[str] | None = None,
    concerns: list[str] | None = None,
) -> dict[str, object]:
    return {
        "id": identity,
        "issue": issue,
        "sources": [source],
        "entrypoints": entrypoints,
        "required_configurations": configurations,
        "dependencies": dependencies or [],
        "concerns": concerns or [],
    }


# Independent of editable audit receipts/findings. These are required evidence
# slots, NOT declarations that boards were compiled or behavior was measured.
# Mixed peripheral/GPIO headers are classified by actual entrypoint.
REQUIRED_FINDINGS = (
    _required(
        "avr-pins",
        5025,
        "src/platforms/avr/atmega/common/avr_pin.h",
        ["GenericSoftwareSPIOutput"],
        ["atmega328p", "atmega32u4", "atmega2560", "attiny", "atmega4809"],
    ),
    _required(
        "teensy4-pins",
        5026,
        "src/platforms/arm/teensy/teensy4_common/fastpin_arm_mxrt1062.h",
        ["SoftwareSPIOutput", "non-LPSPI SPIOutput"],
        ["teensy40-GPIO6", "teensy41-GPIO6-GPIO9"],
    ),
    _required(
        "teensy4-rate",
        5027,
        "src/platforms/shared/spi_bitbang/generic_software_spi.h",
        ["CLOCK_HI_DELAY", "CLOCK_LO_DELAY"],
        ["teensy41-sub-MHz", "teensy41-fractional-MHz"],
    ),
    _required(
        "esp32-software",
        5028,
        "src/platforms/shared/spi_bitbang/generic_software_spi.h",
        ["ESP32 writeBit"],
        [
            "esp32dev",
            "esp32s2",
            "esp32s3",
            "esp32c3",
            "esp32c6",
            "esp32p4",
            "upper-bank",
            "cross-bank",
        ],
    ),
    _required(
        "ci13xx-pins",
        5029,
        "src/platforms/ci13xx/fastpin_ci13xx.h",
        ["GenericSoftwareSPIOutput"],
        ["ci1302", "ci1303", "ci1306", "external-crystal"],
    ),
    _required(
        "apollo3-loop",
        5030,
        "src/platforms/apollo3/fastspi_apollo3.h",
        ["APOLLO3HardwareSPIOutput::writeBit"],
        ["apollo3_red", "apollo3_thing_explorable"],
    ),
    _required(
        "production-gate",
        5031,
        "src/platforms/shared/spi_bitbang/generic_software_spi.h",
        [
            "scalar",
            "bulk",
            "repeat",
            "pixels",
            "start-bit",
            "callbacks",
            "stream",
            "atomic",
        ],
        ["host-register-models", "vendor-integration", "restored-defect"],
    ),
    _required(
        "default-delay",
        5032,
        "src/platforms/shared/spi_bitbang/bitbang_pin.h",
        ["BitbangPin::delay"],
        ["default-no-LTO-AVR", "default-no-LTO-ARM", "default-no-LTO-CI13XX"],
    ),
    _required(
        "samd21",
        5033,
        "src/platforms/arm/d21/fastpin_arm_d21.h",
        ["non-SERCOM GenericSoftwareSPIOutput"],
        ["arduino-zero", "feather-m0", "xiao-samd21"],
    ),
    _required(
        "samd51",
        5034,
        "src/platforms/arm/d51/fastpin_arm_d51.h",
        ["non-SERCOM GenericSoftwareSPIOutput"],
        ["itsybitsy-m4", "feather-m4", "metro-m4", "grand-central-m4"],
    ),
    _required(
        "k20",
        5035,
        "src/platforms/arm/teensy/teensy31_32/fastpin_arm_k20.h",
        ["SoftwareSPIOutput"],
        ["teensy30", "teensy31", "teensy32"],
    ),
    _required(
        "rp2040",
        5036,
        "src/platforms/arm/rp/rp2040/fastpin_arm_rp2040.h",
        ["legacy SoftwareSPIOutput"],
        ["pico-arduino", "pico-w", "pico-mbed", "dual-core-atomic"],
    ),
    _required(
        "k64-k66",
        5037,
        "src/platforms/arm/teensy/teensy36/fastpin_arm_k66.h",
        ["SoftwareSPIOutput"],
        ["teensy35", "teensy36"],
    ),
    _required(
        "rp2350",
        5038,
        "src/platforms/arm/rp/rp2350/fastpin_arm_rp2350.h",
        ["legacy SoftwareSPIOutput"],
        [
            "pico2-low-bank",
            "pico2-w",
            "rp2350b-high-bank",
            "cross-bank",
            "dual-core-atomic",
        ],
    ),
    _required(
        "kl26",
        5039,
        "src/platforms/arm/teensy/teensy_lc/fastpin_arm_kl26.h",
        ["SoftwareSPIOutput"],
        ["teensy-lc"],
    ),
    _required(
        "renesas",
        5040,
        "src/platforms/arm/renesas/fastpin_arm_renesas.h",
        ["GenericSoftwareSPIOutput"],
        ["uno-r4-minima", "uno-r4-wifi", "ra6m5", "portenta"],
    ),
    _required(
        "mgm240",
        5041,
        "src/platforms/arm/mgm240/fastpin_arm_mgm240.h",
        ["GenericSoftwareSPIOutput"],
        ["nano-matter-PA", "nano-matter-PB", "cross-port"],
    ),
    _required(
        "lpc-modern",
        5042,
        "src/platforms/arm/lpc/fastpin_arm_lpc.h",
        ["GenericSoftwareSPIOutput"],
        ["lpc845", "lpc804", "lpc11u24", "lpc11u35", "lpc1517", "lpc1549"],
    ),
    _required(
        "lpc-legacy",
        5043,
        "src/platforms/arm/lpc/fastpin_arm_lpc11_legacy.h",
        ["GenericSoftwareSPIOutput masked write", "full DATA address"],
        ["lpc1114", "lpc1115", "lpc1112", "vendor-CMSIS", "fallback-layout"],
    ),
    _required(
        "same53",
        5044,
        "src/platforms/arm/same53/fastpin_arm_same53.h",
        ["inverted GenericSoftwareSPIOutput"],
        ["clearcore-IO0-IO1", "clearcore-IO2-IO3", "connector-polarity"],
    ),
    _required(
        "stm32",
        5045,
        "src/platforms/arm/stm32/fastspi_arm_stm32.h",
        ["STM32SPIOutput non-HAL writeBit", "explicit SoftwareSPIOutput"],
        [
            "f103-libmaple",
            "f411",
            "f401",
            "f446",
            "g070",
            "u585",
            "photon-f205",
            "giga-h747",
        ],
    ),
    _required(
        "sam3x",
        5046,
        "src/platforms/arm/sam/fastspi_arm_sam.h",
        ["SAMHardwareSPIOutput::writeBit", "explicit SoftwareSPIOutput"],
        ["arduino-due", "digix", "PIO-OWSR"],
    ),
    _required(
        "nrf51",
        5047,
        "src/platforms/arm/nrf51/fastspi_arm_nrf51.h",
        ["NRF51SPIOutput::writeBit", "pixel start-bit", "explicit SoftwareSPIOutput"],
        ["microbit-v1", "rfduino", "forced-software-pins"],
    ),
    _required(
        "nrf52",
        5048,
        "src/platforms/arm/nrf52/fastspi_arm_nrf52.h",
        [
            "NRF52HardwareSPIOutput::writeBit",
            "pixel start-bit",
            "explicit SoftwareSPIOutput",
        ],
        ["nrf52832-P0", "nrf52840-P0", "nrf52840-P1", "cross-port", "softdevice"],
    ),
    _required(
        "runtime-manager",
        5049,
        "src/platforms/shared/spi_manager.cpp.hpp",
        ["SPIBusManager::softwareSPIWrite", "SOFT_SPI transmit"],
        ["forced-SOFT_SPI", "hardware-fallback", "ESP32", "ESP8266-GPIO16", "CI13XX"],
    ),
    _required(
        "runtime-channel",
        5050,
        "src/platforms/shared/bitbang/bitbang_channel_driver.cpp.hpp",
        ["BitBangChannelDriver::transmitSpi"],
        ["BIT_BANG-SPI-1-to-8-lanes", "unequal-length", "mixed-bank", "clock-groups"],
    ),
    _required(
        "esp32-rate",
        5051,
        "src/fastspi.h",
        ["SoftwareSPIOutput DATA_RATE_MHZ", "SoftwareSPIOutput DATA_RATE_KHZ"],
        ["esp32-Xtensa", "esp32-RISCV", "CPU-frequency", "literal-compatibility"],
    ),
    _required(
        "avr-native-gpio",
        5052,
        "src/platforms/avr/fastspi_avr.h",
        ["all five native SPI/UART writeBit implementations"],
        ["SPI-GPIO-methods", "UART-GPIO-methods", "all-bits-variable-values"],
    ),
    _required(
        "samd-sercom-gpio",
        5053,
        "src/platforms/arm/sam/fastspi_arm_sam.h",
        [
            "SAMDHardwareSPIOutput::writeBit",
            "SM16716 header",
            "free pixel writer start-bit",
        ],
        ["SAMD21-SERCOM-GPIO", "SAMD51-SERCOM-GPIO"],
        dependencies=["src/platforms/arm/sam/fastspi_arm_samd.hpp"],
    ),
    _required(
        "avr-native-handoff",
        5055,
        "src/platforms/avr/fastspi_avr.h",
        [
            "AVRUSART1SPIOutput::writeBit [UBRR1]",
            "AVRUSART0SPIOutput::writeBit [UBRR0]",
            "AVRHardwareSPIOutput::writeBit [SPI0_CTRLA]",
            "AVRHardwareSPIOutput::writeBit [SPSR]",
            "AVRHardwareSPIOutput::writeBit [SPSR0]",
        ],
        ["UBRR1", "UBRR0", "SPI0_CTRLA", "SPSR", "SPSR0"],
        dependencies=[
            "src/fl/chipsets/sm16716.h",
            "src/platforms/shared/spi_pixel_writer.h",
        ],
        concerns=[
            "prior-byte-shifter-drain",
            "peripheral-mux-release",
            "prior-state-restore",
            "following-byte-transfer",
        ],
    ),
    _required(
        "forced-software-no-pinmap",
        5056,
        "src/fl/system/fastpin_base.h",
        [
            "FastPin<>::_init [FASTLED_FORCE_SOFTWARE_PINS]",
            "FastPin<> GPIO operations [FASTLED_NO_PINMAP]",
            "GenericSoftwareSPIOutput scalar/bulk/pixel forced-pin paths",
        ],
        [
            "ClearCore-forced-NoPinMap",
            "Arduino-NoPinMap-before-FastLED",
            "Arduino-NoPinMap-after-FastLED",
            "Arduino-pinmap-forced-control",
        ],
        dependencies=[
            "src/fastled_config.h",
            "src/fl/system/arduino.h",
            "src/fl/system/forced_software_pin.h",
            "src/platforms/fastpin.h",
            "src/platforms/shared/spi_bitbang/generic_software_spi.h",
            "src/platforms/arm/same53/led_sysdefs_arm_same53.h",
        ],
        concerns=[
            "safe-Arduino-GPIO-fallback",
            "no-null-register-snapshots",
            "no-unproven-whole-port-streaming",
            "Arduino-declarations-include-order",
        ],
    ),
    _required(
        "generic-init-clock-low",
        5057,
        "src/platforms/shared/spi_bitbang/generic_software_spi.h",
        ["GenericSoftwareSPIOutput::init"],
        [
            "normal-same-port",
            "normal-independent-ports",
            "inverted-same-port",
            "inverted-independent-ports",
            "stream-8bit",
            "stream-16bit",
            "stream-32bit",
        ],
        dependencies=["src/fl/system/fastpin.h"],
        concerns=[
            "logical-idle-low-before-release",
            "primed-high-first-byte-eight-rises",
            "normal-and-inverted-pin-semantics",
            "scalar-bulk-stream-independent-port",
        ],
    ),
)

# Semantic candidates missed by the original five query families. Keep them
# named independently so token removal does not erase a required source row.
SEMANTIC_REQUIRED_PATHS = frozenset(
    [
        f"src/platforms/shared/spi_bitbang/spi_block_{width}.h"
        for width in (1, 2, 4, 8, 16, 32)
    ]
    + [
        f"src/platforms/shared/spi_bitbang/spi_isr_{width}.h"
        for width in (1, 2, 4, 8, 16, 32)
    ]
    + [
        "src/platforms/shared/spi_bitbang/spi_isr_engine.cpp.hpp",
        "src/platforms/shared/spi_bitbang/spi_platform.h",
        "src/platforms/shared/bitbang/bus_traits.h",
        "src/fl/system/pins.h",
        "src/fl/system/pins.cpp.hpp",
        "src/platforms/spi_output_template.h",
        "src/platforms/fastpin.h",
        "src/platforms/arm/k66/clockless_arm_k66.h",
        "src/platforms/arm/teensy/teensy31_32/clockless_arm_k20.h",
        "src/platforms/arm/teensy/teensy36/clockless_arm_k66.h",
        "src/platforms/arm/teensy/teensy4_common/drivers/flexio/channel_engine_flexio.cpp.hpp",
        "src/platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.cpp.hpp",
        "src/platforms/avr/README.md",
    ]
)

PINNED_EXCLUSIONS = {
    "src/platforms/arm/k66/clockless_arm_k66.h": (
        "clockless",
        "Single-data-wire pulse timing; not SPI clock/data output.",
    ),
    "src/platforms/arm/teensy/teensy31_32/clockless_arm_k20.h": (
        "clockless",
        "K20 clockless pulse engine; software SPI pin policy is tracked separately.",
    ),
    "src/platforms/arm/teensy/teensy36/clockless_arm_k66.h": (
        "clockless",
        "K66 clockless pulse engine; software SPI pin policy is tracked separately.",
    ),
    "src/platforms/arm/teensy/teensy4_common/drivers/flexio/channel_engine_flexio.cpp.hpp": (
        "hardware",
        "Channel engine delegates to FlexIO peripheral output; not a GPIO SPI loop.",
    ),
    "src/platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.cpp.hpp": (
        "hardware",
        "Channel engine delegates to ObjectFLED peripheral output; not a GPIO SPI loop.",
    ),
    "src/platforms/avr/README.md": (
        "documentation",
        "AVR platform documentation; live native GPIO methods tracked in #5052.",
    ),
}


# These catch a few semantic query misses; they are NOT the source denominator.
# Every path, including zero matches, must have a source-bound review row.
SEMANTIC_PATTERNS = (
    re.compile(rb"\*\s*[A-Za-z_]\w*\s*(?:\|=|&=|\^=|=(?!=))"),
    re.compile(rb"\b(?:writeBit|writeByte|fastset|digitalWrite)\s*(?:<|\()"),
    re.compile(rb"\b(?:GPIO|OUTSET|OUTCLR|W1TS|W1TC|MASKED_ACCESS)\b"),
)
DISPOSITIONS = frozenset(
    {
        "negative",
        "hardware",
        "clockless",
        "documentation",
        "alias",
        "positive",
        "dependency",
    }
)
SEMANTIC_DISPOSITIONS = DISPOSITIONS - {"negative", "documentation"}


def source_digest(source: bytes) -> str:
    return hashlib.sha256(source).hexdigest()


def check_inventory(
    baseline: Mapping[str, bytes],
    current: Mapping[str, bytes],
    classifications: Mapping[str, Mapping[str, object]],
) -> list[str]:
    """Check the entire supplied source universe, independently of token hits.

    Baseline and current must be obtained from independent source enumeration by
    the caller. No query, trait or manifest entry can remove a source from their
    union. A source change intentionally remains unresolved here: refreshing a
    review hash alone must not authorize a change or certify behavioral proof.
    """
    errors: list[str] = []
    for path in sorted(set(baseline) | set(current)):
        if path not in baseline:
            errors.append(f"new source requires review and evidence: {path}")
        elif path not in current:
            errors.append(f"deleted source requires disposition: {path}")
        elif baseline[path] != current[path]:
            errors.append(f"changed source requires review and evidence: {path}")

        row = classifications.get(path)
        if row is None:
            errors.append(f"unclassified source: {path}")
            continue
        source = current.get(path, baseline.get(path, b""))
        if row.get("source_sha256") != source_digest(source):
            errors.append(f"stale classification source hash: {path}")
        disposition = row.get("disposition")
        if disposition not in DISPOSITIONS:
            errors.append(f"unknown disposition: {path}")
        if not isinstance(row.get("reason"), str) or not str(row["reason"]).strip():
            errors.append(f"missing classification reason: {path}")
        if any(pattern.search(source) for pattern in SEMANTIC_PATTERNS):
            if disposition not in SEMANTIC_DISPOSITIONS:
                errors.append(f"semantic candidate needs explicit disposition: {path}")
        if disposition == "alias" and row.get("canonical") not in current:
            errors.append(f"alias requires an existing canonical source: {path}")
        if disposition == "positive" and not row.get("finding"):
            errors.append(f"positive source requires finding identity: {path}")

    for path in sorted(set(classifications) - set(baseline) - set(current)):
        errors.append(f"classification outside source universe: {path}")
    return errors


def check_evidence(
    findings: Sequence[Mapping[str, object]],
    claims: Sequence[Mapping[str, object]],
    current: Mapping[str, bytes],
) -> list[str]:
    """Keep each real implementation/configuration RED until runners exist.

    The foundation deliberately does not trust claimed success, command strings,
    closed issues, flags, model traces, filenames or hashes as executed proof.
    Later runner adapters must execute allowlisted production tests/target builds
    and restored-defect mutations, bind their full inputs, and distinguish host,
    target instruction and physical evidence. Merely adding receipts cannot make
    this function certify an unresolved positive.
    """
    errors: list[str] = []
    seen: set[str] = set()
    for finding in findings:
        identity = finding.get("id")
        if not isinstance(identity, str) or not identity:
            errors.append("finding missing implementation identity")
            continue
        if identity in seen:
            errors.append(f"duplicate finding identity: {identity}")
        seen.add(identity)

        paths = finding.get("sources")
        if not isinstance(paths, list) or not paths:
            errors.append(f"{identity}: missing production source binding")
        else:
            for path in paths:
                if not isinstance(path, str) or path not in current:
                    errors.append(f"{identity}: unavailable production source: {path}")

        configurations = finding.get("required_configurations")
        if not isinstance(configurations, list) or not configurations:
            errors.append(f"{identity}: missing real configuration matrix")
            continue
        for configuration in configurations:
            errors.append(
                f"{identity}/{configuration}: unresolved production execution, "
                "target configuration and restored-defect evidence (runner not wired)"
            )

    for claim in claims:
        identity = claim.get("finding", "unknown")
        errors.append(
            f"{identity}: unverified external claim is not execution evidence"
        )
    return errors


def check_required_findings(
    required: Sequence[Mapping[str, object]],
    proposed: Sequence[Mapping[str, object]],
    classifications: Mapping[str, Mapping[str, object]],
) -> list[str]:
    """Bind editable findings to an independently pinned entrypoint registry.

    Declarations passing this check are still not execution proof. Multiple
    entrypoints may share one source file: source-level hardware/alias labels
    cannot erase a GPIO-only method inside an otherwise peripheral driver.
    """
    errors: list[str] = []
    by_id: dict[str, Mapping[str, object]] = {}
    for finding in proposed:
        identity = finding.get("id")
        if not isinstance(identity, str) or not identity:
            errors.append("proposed finding missing identity")
        elif identity in by_id:
            errors.append(f"duplicate proposed finding: {identity}")
        else:
            by_id[identity] = finding
    required_ids = {entry.get("id") for entry in required}
    for identity in sorted(set(by_id) - required_ids):
        errors.append(f"new finding requires registry review: {identity}")

    for entry in required:
        identity = str(entry.get("id", "unknown"))
        finding = by_id.get(identity)
        if finding is None:
            errors.append(f"required production entrypoint missing: {identity}")
            continue
        for field in (
            "issue",
            "sources",
            "required_configurations",
            "entrypoints",
            "dependencies",
            "concerns",
        ):
            if finding.get(field) != entry.get(field):
                errors.append(f"required {field} changed or removed: {identity}")
        paths = entry.get("sources", [])
        if not isinstance(paths, list):
            errors.append(f"registry source binding malformed: {identity}")
            continue
        for path in paths:
            if not isinstance(path, str):
                errors.append(f"registry source path malformed: {identity}")
                continue
            row = classifications.get(path, {})
            bindings = row.get("findings", [row.get("finding")])
            if (
                row.get("disposition") != "positive"
                or not isinstance(bindings, list)
                or identity not in bindings
            ):
                errors.append(
                    f"required GPIO entrypoint declassified: {identity} in {path}"
                )
    return errors


def _baseline_objects(root: Path) -> dict[str, str]:
    """Read all baseline paths directly from the pinned Git tree, not a query."""
    result = RunningProcess.run(
        ["git", "ls-tree", "-r", "-z", BASELINE_COMMIT, "--", "src"],
        cwd=root,
        check=True,
        timeout=30,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    objects: dict[str, str] = {}
    for entry in result.stdout.split("\0"):
        if not entry:
            continue
        metadata, path = entry.split("\t", 1)
        mode, kind, object_id = metadata.split()
        if kind != "blob" or mode == "120000":
            raise ValueError(f"unexpected source object in pinned baseline: {path}")
        objects[path] = object_id
    if len(objects) != BASELINE_SOURCE_COUNT:
        raise ValueError(
            f"baseline universe mismatch: {len(objects)} != {BASELINE_SOURCE_COUNT}"
        )
    return objects


def _current_sources(root: Path) -> dict[str, bytes]:
    """Include tracked and nonignored untracked source, not generated deps.

    --cached retains tracked paths even when a rule matches them. No source
    extension, SPI token or directory-name discovery filter is used here.
    Ignored source boundaries/policy are independently checked by the CLI.
    """
    result = RunningProcess.run(
        [
            "git",
            "ls-files",
            "-z",
            "--cached",
            "--others",
            "--exclude-standard",
            "--",
            "src",
        ],
        cwd=root,
        check=True,
        timeout=30,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    sources: dict[str, bytes] = {}
    for name in sorted(set(result.stdout.split("\0")) - {""}):
        path = root / name
        if path.is_symlink():
            raise ValueError(f"source symlink requires explicit audit: {path}")
        if path.is_file():
            sources[name] = path.read_bytes()
    return sources


def _ignored_boundaries(root: Path) -> list[str]:
    result = RunningProcess.run(
        [
            "git",
            "ls-files",
            "-z",
            "--others",
            "--ignored",
            "--exclude-standard",
            "--directory",
            "--no-empty-directory",
            "--",
            "src",
        ],
        cwd=root,
        check=True,
        timeout=30,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    return sorted(set(result.stdout.split("\0")) - {""})


def _check_ignored_boundaries(boundaries: Sequence[str]) -> list[str]:
    generated_prefixes = (
        "src/platforms/wasm/compiler/node_modules/",
        "src/platforms/wasm/compiler/dist/",
    )
    return [
        f"ignored src boundary requires independent review: {path}"
        for path in boundaries
        if not path.startswith(generated_prefixes)
    ]


def _check_ignore_policy(root: Path) -> list[str]:
    result = RunningProcess.run(
        ["git", "rev-parse", f"{BASELINE_COMMIT}:.gitignore"],
        cwd=root,
        check=True,
        timeout=30,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    policy = root / ".gitignore"
    if (
        not policy.is_file()
        or _git_blob_digest(policy.read_bytes()) != result.stdout.strip()
    ):
        return [
            "root .gitignore policy changed; ignored source exclusion requires review"
        ]
    return []


def _git_blob_digest(source: bytes) -> str:
    # The pinned repository uses Git SHA-1 object identities. This is an object
    # integrity comparison, not a cryptographic authentication/security scheme.
    header = f"blob {len(source)}\0".encode("ascii")
    return hashlib.sha1(header + source, usedforsecurity=False).hexdigest()


def _source_changes(
    baseline: Mapping[str, str], current: Mapping[str, bytes]
) -> list[str]:
    errors: list[str] = []
    for path in sorted(set(baseline) | set(current)):
        if path not in baseline:
            errors.append(f"new source requires run-backed disposition: {path}")
        elif path not in current:
            errors.append(f"deleted source requires run-backed disposition: {path}")
        elif _git_blob_digest(current[path]) != baseline[path]:
            errors.append(
                f"changed/rerouted source requires run-backed disposition: {path}"
            )
    return errors


def _native_input_hashes(root: Path, sources: Mapping[str, bytes]) -> dict[str, str]:
    inputs = {path: source_digest(data) for path, data in sorted(sources.items())}
    for name in (
        "tests/platforms/shared/spi_bitbang/generic_software_spi.cpp",
        "tests/platforms/shared/spi_bitbang/generic_software_spi_inverted.cpp",
        "tests/platforms/shared/spi_bitbang/generic_software_spi_fixture.h",
        "tests/fl/system/forced_software_pin.cpp",
    ):
        inputs[name] = source_digest((root / name).read_bytes())
    return inputs


def _check_native_inputs(root: Path, expected: Mapping[str, str]) -> None:
    try:
        actual = _native_input_hashes(root, _current_sources(root))
    except OSError as error:
        raise ValueError("native trace inputs changed or became unreadable") from error
    if actual != expected:
        raise ValueError("native trace inputs changed during execution")


def _run_native_traces(
    root: Path, sources: Mapping[str, bytes]
) -> list[dict[str, object]]:
    """Execute fixed in-repo tests; no command is accepted from a manifest."""
    commands = (
        ("bash", "test", "generic_software_spi"),
        ("bash", "test", "generic_software_spi_inverted", "--debug"),
        ("bash", "test", "forced_software_pin", "--debug"),
    )
    inputs = _native_input_hashes(root, sources)
    input_digest = source_digest(json.dumps(inputs, sort_keys=True).encode("utf-8"))
    records: list[dict[str, object]] = []

    def execute(command: tuple[str, ...]) -> int:
        result = RunningProcess.run(
            list(command),
            cwd=root,
            check=False,
            timeout=900,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )
        mode = "debug" if "--debug" in command else "quick"
        log = root / ".build" / f"spi-audit-{command[2]}-{mode}.log"
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(result.stdout, encoding="utf-8")
        records.append(
            {
                "kind": "host_production_gpio_trace",
                "command": list(command),
                "returncode": result.returncode,
                "input_sha256": input_digest,
                "log": log.relative_to(root).as_posix(),
                "log_sha256": source_digest(result.stdout.encode("utf-8")),
                "scope": "host GPIO semantics only; no vendor, instruction or physical claim",
            }
        )
        # Source AND test/fixture changes invalidate the entire execution before
        # another target (or a debug rerun) can be mistaken for comparable proof.
        _check_native_inputs(root, inputs)
        return result.returncode

    _check_native_inputs(root, inputs)
    for command in commands:
        returncode = execute(command)
        if returncode != 0 and "--debug" not in command:
            # Keep the quick failure and its distinct log/record even if debug
            # succeeds. A diagnostic rerun cannot turn the original failure green.
            execute((*command, "--debug"))
    return records


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--classifications", type=Path, help="review rows; never execution proof"
    )
    parser.add_argument(
        "--findings",
        type=Path,
        help="editable finding rows checked against required registry",
    )
    parser.add_argument(
        "--run-native",
        action="store_true",
        help="run only fixed production GPIO trace targets",
    )
    parser.add_argument(
        "--report", type=Path, default=ROOT / ".build/software-spi-audit-report.json"
    )
    args = parser.parse_args()
    try:
        baseline = _baseline_objects(ROOT)
        current = _current_sources(ROOT)
        ignored = _ignored_boundaries(ROOT)
        rows = (
            json.loads(args.classifications.read_text(encoding="utf-8"))
            if args.classifications
            else {}
        )
        findings = (
            json.loads(args.findings.read_text(encoding="utf-8"))
            if args.findings
            else list(REQUIRED_FINDINGS)
        )
        if not isinstance(rows, dict) or not isinstance(findings, list):
            raise ValueError(
                "classifications must be an object and findings must be a list"
            )
        for path, (disposition, reason) in PINNED_EXCLUSIONS.items():
            if (
                path in current
                and path in baseline
                and _git_blob_digest(current[path]) == baseline[path]
            ):
                rows.setdefault(
                    path,
                    {
                        "disposition": disposition,
                        "reason": reason,
                        "source_sha256": source_digest(current[path]),
                    },
                )
        errors = _source_changes(baseline, current)
        errors.extend(_check_ignored_boundaries(ignored))
        errors.extend(_check_ignore_policy(ROOT))
        errors.extend(check_inventory(current, current, rows))
        errors.extend(check_required_findings(REQUIRED_FINDINGS, findings, rows))
        for path in sorted(SEMANTIC_REQUIRED_PATHS):
            if path not in current or path not in rows:
                errors.append(f"required semantic source not classified: {path}")
        # Passing source declarations cannot turn absent behavioral/configuration
        # evidence into success. Actual fixed executions are recorded separately.
        errors.extend(check_evidence(REQUIRED_FINDINGS, [], current))
        executions = _run_native_traces(ROOT, current) if args.run_native else []
        for execution in executions:
            if execution["returncode"] != 0:
                errors.append(f"fixed production trace failed: {execution['command']}")
        report = {
            "baseline": BASELINE_COMMIT,
            "baseline_files": len(baseline),
            "current_files": len(current),
            "ignored_generated_boundaries": ignored,
            "ignore_policy_sha256": source_digest((ROOT / ".gitignore").read_bytes()),
            "required_findings": list(REQUIRED_FINDINGS),
            "source_accounting": {
                path: {
                    "baseline_git_blob": baseline.get(path),
                    "current_sha256": source_digest(current[path])
                    if path in current
                    else None,
                    "classification": rows.get(path, {"disposition": "unknown"}),
                }
                for path in sorted(set(baseline) | set(current))
            },
            "executions": executions,
            "errors": errors,
            "complete": not errors,
            "limitation": "Foundation: target/codegen/restored-defect runners and run-backed source-change resolution remain unwired.",
        }
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(
            f"Software SPI audit {'RED' if errors else 'PASS'}: "
            f"{len(baseline)} baseline files, {len(current)} current files, "
            f"{len(REQUIRED_FINDINGS)} required findings, {len(errors)} unresolved checks."
        )
        print(f"Full source accounting and evidence gaps: {args.report}")
        return 1 if errors else 0
    except (OSError, ValueError) as error:
        print(f"Software SPI audit RED: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
