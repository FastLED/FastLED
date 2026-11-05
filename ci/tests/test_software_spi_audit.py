"""Fail-closed source/evidence contract for the software SPI audit (#5031).

These tests use small source universes to exercise gate mutations. They are not
hardware or production-writer evidence; the C++ production-template traces and
real target configuration receipts must be collected separately.
"""

import hashlib
from dataclasses import dataclass
from pathlib import Path

import pytest
from running_process import RunningProcess

from ci.software_spi_audit import (
    BASELINE_SOURCE_COUNT,
    REQUIRED_FINDINGS,
    SEMANTIC_REQUIRED_PATHS,
    _check_ignored_boundaries,
    _current_sources,
    _run_native_traces,
    check_evidence,
    check_inventory,
    check_required_findings,
)


def digest(source: bytes) -> str:
    return hashlib.sha256(source).hexdigest()


def negative_rows(sources: dict[str, bytes]) -> dict[str, dict[str, object]]:
    return {
        path: {
            "disposition": "negative",
            "source_sha256": digest(source),
            "reason": "Reviewed fixture: no GPIO edge emitter.",
        }
        for path, source in sources.items()
    }


def test_zero_query_matches_remain_in_source_denominator() -> None:
    sources = {"src/unrelated.h": b"struct Colour {};\n"}
    assert check_inventory(sources, sources, negative_rows(sources)) == []
    assert "unclassified" in " ".join(check_inventory(sources, sources, {}))


@pytest.mark.parametrize(
    "mutation",
    ["new_backend", "remove_token", "rename", "delete", "reroute"],
)
def test_source_mutations_cannot_shrink_or_silently_reclassify_inventory(
    mutation: str,
) -> None:
    sources = {
        "src/platforms/pin.h": b"void writeBit();\n",
        "src/platforms/spi_output_template.h": b'#include "pin.h"\n',
    }
    current = dict(sources)
    if mutation == "new_backend":
        # No SPI keyword: discovering files must precede pattern matching.
        current["src/platforms/new/output.h"] = b"void emit() { *reg = value; }\n"
    elif mutation == "remove_token":
        current["src/platforms/pin.h"] = b"void emit();\n"
    elif mutation == "rename":
        current["src/platforms/renamed.h"] = current.pop("src/platforms/pin.h")
    elif mutation == "delete":
        del current["src/platforms/pin.h"]
    else:
        current["src/platforms/spi_output_template.h"] = b'#include "other.h"\n'
    assert check_inventory(sources, current, negative_rows(sources))


def test_deleting_a_zero_match_row_fails_even_when_all_matches_are_classified() -> None:
    sources = {
        "src/platforms/pin.h": b"void writeBit();\n",
        "src/no_matches.h": b"struct Colour {};\n",
    }
    rows = negative_rows(sources)
    del rows["src/no_matches.h"]
    assert "unclassified" in " ".join(check_inventory(sources, sources, rows))


def test_query_miss_semantic_emitter_cannot_be_marked_unrelated() -> None:
    sources = {"src/platforms/output.h": b"void emit() { *reg = value; }\n"}
    assert "semantic" in " ".join(
        check_inventory(sources, sources, negative_rows(sources))
    )


def test_false_coverage_does_not_replace_real_evidence() -> None:
    sources = {"src/platforms/output.h": b"void writeBit();\n"}
    findings = [
        {
            "id": "gpio-writer",
            "issue": 5028,
            "sources": list(sources),
            "required_configurations": ["esp32-low-bank", "esp32-high-bank"],
        }
    ]
    claims = [
        {
            "finding": "gpio-writer",
            "status": "covered",
            "issue_closed": True,
            "kAtomicWrite": True,
            "kTwoWriteStream": True,
            "configuration": "esp32-low-bank",
            "source_sha256": digest(sources["src/platforms/output.h"]),
        }
    ]
    assert check_evidence(findings, claims, sources)


def test_unresolved_production_configuration_remains_red() -> None:
    sources = {"src/platforms/output.h": b"void writeBit();\n"}
    findings = [
        {
            "id": "gpio-writer",
            "issue": 5028,
            "sources": list(sources),
            "required_configurations": ["vendor-core-low-bank"],
        }
    ]
    errors = check_evidence(findings, [], sources)
    assert "gpio-writer" in " ".join(errors)
    assert "vendor-core-low-bank" in " ".join(errors)


def test_restored_old_source_invalidates_previously_claimed_proof() -> None:
    original = b"if (value) pin.hi(); else pin.lo();\n"
    optimized = b"*selected_register = mask;\n"
    sources = {"src/platforms/output.h": original}
    findings = [
        {
            "id": "branchless-data",
            "issue": 5028,
            "sources": list(sources),
            "required_configurations": ["vendor-core"],
        }
    ]
    claims = [
        {
            "finding": "branchless-data",
            "configuration": "vendor-core",
            "source_sha256": digest(optimized),
            "status": "passed",
            "mutation": {"old_path_failed": True},
        }
    ]
    assert check_evidence(findings, claims, sources)


def test_host_model_cannot_substitute_for_vendor_or_physical_evidence() -> None:
    sources = {"src/platforms/output.h": b"void writeBit();\n"}
    findings = [
        {
            "id": "gpio-writer",
            "issue": 5028,
            "sources": list(sources),
            "required_configurations": ["vendor-core"],
            "required_evidence": ["target_compile", "restored_defect"],
        }
    ]
    claims = [
        {
            "finding": "gpio-writer",
            "configuration": "vendor-core",
            "kind": "host_gpio_model",
            "status": "passed",
            "source_sha256": digest(sources["src/platforms/output.h"]),
        }
    ]
    assert check_evidence(findings, claims, sources)


@pytest.mark.parametrize(
    "mutation", ["drop_all", "drop_finding", "drop_config", "declassify"]
)
def test_required_entrypoints_cannot_be_removed_by_manifest_edits(
    mutation: str,
) -> None:
    required = [
        {
            "id": "native-gpio-bit",
            "issue": 5052,
            "sources": ["src/platforms/avr/fastspi_avr.h"],
            "required_configurations": ["avr-spi-gpio", "avr-uart-gpio"],
        }
    ]
    proposed = [dict(required[0])]
    rows = {
        "src/platforms/avr/fastspi_avr.h": {
            "disposition": "positive",
            "finding": "native-gpio-bit",
        }
    }
    if mutation in {"drop_all", "drop_finding"}:
        proposed.clear()
    elif mutation == "drop_config":
        proposed[0]["required_configurations"] = ["avr-spi-gpio"]
    else:
        rows["src/platforms/avr/fastspi_avr.h"]["disposition"] = "hardware"
    assert check_required_findings(required, proposed, rows)


def test_complete_entrypoint_declarations_are_not_execution_proof() -> None:
    required = [
        {
            "id": "native-gpio-bit",
            "issue": 5052,
            "sources": ["src/platforms/avr/fastspi_avr.h"],
            "required_configurations": ["avr-spi-gpio", "avr-uart-gpio"],
        }
    ]
    rows = {
        "src/platforms/avr/fastspi_avr.h": {
            "disposition": "positive",
            "finding": "native-gpio-bit",
        }
    }
    assert check_required_findings(required, required, rows) == []
    assert check_evidence(
        required, [], {"src/platforms/avr/fastspi_avr.h": b"void emit();"}
    )


def test_pinned_registry_keeps_every_discovered_child_and_query_miss() -> None:
    assert BASELINE_SOURCE_COUNT == 2986
    assert {finding["issue"] for finding in REQUIRED_FINDINGS} == set(
        range(5025, 5054)
    ) | {5055, 5056, 5057}
    assert len(REQUIRED_FINDINGS) == 32
    assert "src/platforms/shared/spi_bitbang/spi_block_32.h" in SEMANTIC_REQUIRED_PATHS
    assert (
        "src/platforms/shared/spi_bitbang/spi_isr_engine.cpp.hpp"
        in SEMANTIC_REQUIRED_PATHS
    )
    assert "src/fl/system/pins.cpp.hpp" in SEMANTIC_REQUIRED_PATHS
    assert "src/platforms/arm/k66/clockless_arm_k66.h" in SEMANTIC_REQUIRED_PATHS
    assert (
        "src/platforms/arm/teensy/teensy31_32/clockless_arm_k20.h"
        in SEMANTIC_REQUIRED_PATHS
    )
    assert (
        "src/platforms/arm/teensy/teensy36/clockless_arm_k66.h"
        in SEMANTIC_REQUIRED_PATHS
    )
    assert (
        "src/platforms/arm/teensy/teensy4_common/drivers/flexio/channel_engine_flexio.cpp.hpp"
        in SEMANTIC_REQUIRED_PATHS
    )
    assert (
        "src/platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.cpp.hpp"
        in SEMANTIC_REQUIRED_PATHS
    )
    assert "src/platforms/avr/README.md" in SEMANTIC_REQUIRED_PATHS


def test_current_universe_excludes_ignored_dependencies_but_keeps_new_backends(
    tmp_path: Path,
) -> None:
    def git(*arguments: str) -> None:
        RunningProcess.run(
            ["git", *arguments],
            cwd=tmp_path,
            check=True,
            timeout=10,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )

    git("init", "--quiet")
    (tmp_path / "src").mkdir()
    (tmp_path / "src/base.h").write_text("struct Base {};\n", encoding="utf-8")
    (tmp_path / ".gitignore").write_text("node_modules/\n", encoding="utf-8")
    git("add", ".gitignore", "src/base.h")
    dependency = tmp_path / "src/platforms/wasm/compiler/node_modules"
    dependency.mkdir(parents=True)
    (dependency / "generated.js").write_text("generated dependency", encoding="utf-8")
    backend = tmp_path / "src/platforms/new/emitter.h"
    backend.parent.mkdir(parents=True)
    backend.write_text("void emit() { *reg = value; }\n", encoding="utf-8")
    current = _current_sources(tmp_path)
    assert set(current) == {"src/base.h", "src/platforms/new/emitter.h"}
    baseline = {"src/base.h": current["src/base.h"]}
    assert check_inventory(baseline, current, negative_rows(baseline))


def test_ignore_rule_cannot_hide_a_new_platform_backend() -> None:
    generated = [
        "src/platforms/wasm/compiler/node_modules/",
        "src/platforms/wasm/compiler/dist/",
    ]
    assert _check_ignored_boundaries(generated) == []
    assert _check_ignored_boundaries(generated + ["src/platforms/new/"])


def test_samd_native_gpio_registry_binds_the_actual_production_method() -> None:
    finding = next(entry for entry in REQUIRED_FINDINGS if entry["issue"] == 5053)
    assert finding["sources"] == ["src/platforms/arm/sam/fastspi_arm_sam.h"]
    assert "SAMDHardwareSPIOutput::writeBit" in finding["entrypoints"]
    assert "src/platforms/arm/sam/fastspi_arm_samd.hpp" in finding["dependencies"]


def test_avr_native_handoff_is_separate_from_selected_bit_predicate_evidence() -> None:
    handoff = next(
        (entry for entry in REQUIRED_FINDINGS if entry["issue"] == 5055), None
    )
    assert handoff is not None, (
        "native mux/drain/restore concern #5055 must remain required"
    )
    predicate = next(entry for entry in REQUIRED_FINDINGS if entry["issue"] == 5052)
    assert handoff["id"] != predicate["id"]
    assert handoff["sources"] == ["src/platforms/avr/fastspi_avr.h"]
    assert set(handoff["required_configurations"]) == {
        "UBRR1",
        "UBRR0",
        "SPI0_CTRLA",
        "SPSR",
        "SPSR0",
    }
    assert set(handoff["entrypoints"]) == {
        "AVRUSART1SPIOutput::writeBit [UBRR1]",
        "AVRUSART0SPIOutput::writeBit [UBRR0]",
        "AVRHardwareSPIOutput::writeBit [SPI0_CTRLA]",
        "AVRHardwareSPIOutput::writeBit [SPSR]",
        "AVRHardwareSPIOutput::writeBit [SPSR0]",
    }
    assert set(handoff["concerns"]) == {
        "prior-byte-shifter-drain",
        "peripheral-mux-release",
        "prior-state-restore",
        "following-byte-transfer",
    }
    assert set(handoff["dependencies"]) == {
        "src/fl/chipsets/sm16716.h",
        "src/platforms/shared/spi_pixel_writer.h",
    }
    # Predicate-only claims cannot certify peripheral ownership handoff.
    sources = {path: b"native source input\n" for path in handoff["sources"]}
    assert check_evidence(
        [handoff],
        [{"finding": predicate["id"], "status": "passed"}],
        sources,
    )


def test_forced_no_pinmap_fallback_is_a_required_shared_safety_finding() -> None:
    finding = next(
        (entry for entry in REQUIRED_FINDINGS if entry["issue"] == 5056), None
    )
    assert finding is not None, (
        "forced NoPinMap fallback safety #5056 must remain required"
    )
    assert finding["id"] == "forced-software-no-pinmap"
    assert finding["sources"] == ["src/fl/system/fastpin_base.h"]
    assert set(finding["entrypoints"]) == {
        "FastPin<>::_init [FASTLED_FORCE_SOFTWARE_PINS]",
        "FastPin<> GPIO operations [FASTLED_NO_PINMAP]",
        "GenericSoftwareSPIOutput scalar/bulk/pixel forced-pin paths",
    }
    assert set(finding["required_configurations"]) == {
        "ClearCore-forced-NoPinMap",
        "Arduino-NoPinMap-before-FastLED",
        "Arduino-NoPinMap-after-FastLED",
        "Arduino-pinmap-forced-control",
    }
    assert set(finding["concerns"]) == {
        "safe-Arduino-GPIO-fallback",
        "no-null-register-snapshots",
        "no-unproven-whole-port-streaming",
        "Arduino-declarations-include-order",
    }
    assert set(finding["dependencies"]) == {
        "src/fastled_config.h",
        "src/fl/system/arduino.h",
        "src/fl/system/forced_software_pin.h",
        "src/platforms/fastpin.h",
        "src/platforms/shared/spi_bitbang/generic_software_spi.h",
        "src/platforms/arm/same53/led_sysdefs_arm_same53.h",
    }


def test_clock_low_initialization_has_independent_first_edge_requirements() -> None:
    finding = next(
        (entry for entry in REQUIRED_FINDINGS if entry["issue"] == 5057), None
    )
    assert finding is not None, (
        "logical clock LOW initialization #5057 must remain required"
    )
    assert finding["id"] == "generic-init-clock-low"
    assert finding["sources"] == [
        "src/platforms/shared/spi_bitbang/generic_software_spi.h"
    ]
    assert finding["entrypoints"] == ["GenericSoftwareSPIOutput::init"]
    assert set(finding["required_configurations"]) == {
        "normal-same-port",
        "normal-independent-ports",
        "inverted-same-port",
        "inverted-independent-ports",
        "stream-8bit",
        "stream-16bit",
        "stream-32bit",
    }
    assert set(finding["concerns"]) == {
        "logical-idle-low-before-release",
        "primed-high-first-byte-eight-rises",
        "normal-and-inverted-pin-semantics",
        "scalar-bulk-stream-independent-port",
    }
    assert finding["dependencies"] == ["src/fl/system/fastpin.h"]


@dataclass
class NativeProcessResult:
    returncode: int
    stdout: str


NATIVE_TEST_INPUTS = (
    "tests/platforms/shared/spi_bitbang/generic_software_spi.cpp",
    "tests/platforms/shared/spi_bitbang/generic_software_spi_inverted.cpp",
    "tests/platforms/shared/spi_bitbang/generic_software_spi_fixture.h",
    "tests/fl/system/forced_software_pin.cpp",
)


def prepare_native_inputs(root: Path) -> dict[str, bytes]:
    sources = {"src/production.h": b"original production input\n"}
    for name in (*sources, *NATIVE_TEST_INPUTS):
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(sources.get(name, b"original test input\n"))
    return sources


@pytest.mark.parametrize("changed", ["src/production.h", *NATIVE_TEST_INPUTS])
def test_native_run_invalidates_every_changed_fingerprinted_input(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    changed: str,
) -> None:
    sources = prepare_native_inputs(tmp_path)
    calls: list[list[str]] = []

    def run(command: list[str], **_options: object) -> NativeProcessResult:
        calls.append(command)
        (tmp_path / changed).write_bytes(b"changed during execution\n")
        return NativeProcessResult(0, "fake runner result; not production evidence\n")

    monkeypatch.setattr("ci.software_spi_audit.RunningProcess.run", run)
    monkeypatch.setattr(
        "ci.software_spi_audit._current_sources",
        lambda root: {name: (root / name).read_bytes() for name in sources},
    )
    with pytest.raises(ValueError, match="inputs changed"):
        _run_native_traces(tmp_path, sources)
    assert len(calls) == 1


def test_quick_native_failure_immediately_runs_same_target_debug_and_preserves_both(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    sources = prepare_native_inputs(tmp_path)
    calls: list[list[str]] = []

    def run(command: list[str], **_options: object) -> NativeProcessResult:
        calls.append(command)
        if len(calls) == 1:
            return NativeProcessResult(1, "original quick failure\n")
        return NativeProcessResult(0, "debug or later target success\n")

    monkeypatch.setattr("ci.software_spi_audit.RunningProcess.run", run)
    monkeypatch.setattr("ci.software_spi_audit._current_sources", lambda _root: sources)
    records = _run_native_traces(tmp_path, sources)
    assert calls == [
        ["bash", "test", "generic_software_spi"],
        ["bash", "test", "generic_software_spi", "--debug"],
        ["bash", "test", "generic_software_spi_inverted", "--debug"],
        ["bash", "test", "forced_software_pin", "--debug"],
    ]
    assert [record["returncode"] for record in records] == [1, 0, 0, 0]
    assert len({record["log"] for record in records}) == 4
    assert (tmp_path / str(records[0]["log"])).read_text(
        encoding="utf-8"
    ) == "original quick failure\n"
    assert (tmp_path / str(records[1]["log"])).read_text(
        encoding="utf-8"
    ) == "debug or later target success\n"


def test_fixed_native_runner_executes_forced_pin_debug_without_manifest_commands(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    sources = prepare_native_inputs(tmp_path)
    calls: list[list[str]] = []

    def run(command: list[str], **_options: object) -> NativeProcessResult:
        calls.append(command)
        return NativeProcessResult(
            0, "fake runner control-flow result, not behavioral proof\n"
        )

    monkeypatch.setattr("ci.software_spi_audit.RunningProcess.run", run)
    monkeypatch.setattr("ci.software_spi_audit._current_sources", lambda _root: sources)
    records = _run_native_traces(tmp_path, sources)
    assert calls == [
        ["bash", "test", "generic_software_spi"],
        ["bash", "test", "generic_software_spi_inverted", "--debug"],
        ["bash", "test", "forced_software_pin", "--debug"],
    ]
    assert [record["command"] for record in records] == calls
    assert [record["returncode"] for record in records] == [0, 0, 0]
