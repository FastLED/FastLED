"""Fail-closed source/evidence contract for the software SPI audit (#5031).

These tests use small source universes to exercise gate mutations. They are not
hardware or production-writer evidence; the C++ production-template traces and
real target configuration receipts must be collected separately.
"""

import hashlib
from pathlib import Path

import pytest
from running_process import RunningProcess

from ci.software_spi_audit import (
    BASELINE_SOURCE_COUNT,
    REQUIRED_FINDINGS,
    SEMANTIC_REQUIRED_PATHS,
    _check_ignored_boundaries,
    _current_sources,
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
    assert {finding["issue"] for finding in REQUIRED_FINDINGS} == set(range(5025, 5054))
    assert len(REQUIRED_FINDINGS) == 29
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
