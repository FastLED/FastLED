"""Fail-closed source/evidence contract for the software SPI audit (#5031).

These tests use small source universes to exercise gate mutations. They are not
hardware or production-writer evidence; the C++ production-template traces and
real target configuration receipts must be collected separately.
"""

import hashlib
import json
import os
import sys
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import pytest
from running_process import CalledProcessError, RunningProcess

from ci import software_spi_audit as audit
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
    rows = {
        path: inventory_row(source, "dependency") for path, source in sources.items()
    }
    assert check_inventory(sources, sources, rows) == []
    objects = {path: audit._git_blob_digest(source) for path, source in sources.items()}
    assert audit._source_changes(objects, sources) == []
    current = dict(sources)
    if mutation == "new_backend":
        # No SPI keyword: discovering files must precede pattern matching.
        current["src/platforms/new/output.h"] = b"void emit() { *reg = value; }\n"
        expected_inventory = [
            "new source requires review and evidence: src/platforms/new/output.h"
        ]
        expected_changes = [
            "new source requires run-backed disposition: src/platforms/new/output.h"
        ]
    elif mutation == "remove_token":
        current["src/platforms/pin.h"] = b"void emit();\n"
        expected_inventory = [
            "changed source requires review and evidence: src/platforms/pin.h"
        ]
        expected_changes = [
            "changed/rerouted source requires run-backed disposition: src/platforms/pin.h"
        ]
    elif mutation == "rename":
        current["src/platforms/renamed.h"] = current.pop("src/platforms/pin.h")
        expected_inventory = [
            "deleted source requires disposition: src/platforms/pin.h",
            "new source requires review and evidence: src/platforms/renamed.h",
        ]
        expected_changes = [
            "deleted source requires run-backed disposition: src/platforms/pin.h",
            "new source requires run-backed disposition: src/platforms/renamed.h",
        ]
    elif mutation == "delete":
        del current["src/platforms/pin.h"]
        expected_inventory = [
            "deleted source requires disposition: src/platforms/pin.h"
        ]
        expected_changes = [
            "deleted source requires run-backed disposition: src/platforms/pin.h"
        ]
    else:
        current["src/platforms/spi_output_template.h"] = b'#include "other.h"\n'
        expected_inventory = [
            "changed source requires review and evidence: src/platforms/spi_output_template.h"
        ]
        expected_changes = [
            "changed/rerouted source requires run-backed disposition: src/platforms/spi_output_template.h"
        ]
    # Keep every row valid and hash-current, including new/renamed entries. An
    # incidental stale hash, negative disposition or missing row cannot catch
    # removal of the actual new/changed/deleted-source check.
    rows.update(
        {path: inventory_row(source, "dependency") for path, source in current.items()}
    )
    assert sorted(check_inventory(sources, current, rows)) == sorted(expected_inventory)
    assert sorted(audit._source_changes(objects, current)) == sorted(expected_changes)


def test_deleting_a_zero_match_row_fails_even_when_all_matches_are_classified() -> None:
    sources = {
        "src/platforms/pin.h": b"void writeBit();\n",
        "src/no_matches.h": b"struct Colour {};\n",
    }
    rows = {
        path: inventory_row(source, "dependency") for path, source in sources.items()
    }
    assert check_inventory(sources, sources, rows) == []
    del rows["src/no_matches.h"]
    assert check_inventory(sources, sources, rows) == [
        "unclassified source: src/no_matches.h"
    ]


def test_query_miss_semantic_emitter_cannot_be_marked_unrelated() -> None:
    sources = {"src/platforms/output.h": b"void emit() { *reg = value; }\n"}
    assert "semantic" in " ".join(
        check_inventory(sources, sources, negative_rows(sources))
    )


def inventory_row(
    source: bytes, disposition: str, **fields: object
) -> dict[str, object]:
    return {
        "disposition": disposition,
        "source_sha256": digest(source),
        "reason": "Source-bound classification fixture; not execution evidence.",
        **fields,
    }


def test_source_bound_pure_include_alias_is_a_valid_classification() -> None:
    sources = {
        "src/platforms/forward.h": b'#pragma once\n#include "real.h"\n',
        "src/platforms/real.h": b"void emit() { *reg = value; }\n",
    }
    rows = {
        "src/platforms/forward.h": inventory_row(
            sources["src/platforms/forward.h"],
            "alias",
            canonical="src/platforms/real.h",
        ),
        "src/platforms/real.h": inventory_row(
            sources["src/platforms/real.h"], "dependency"
        ),
    }
    assert check_inventory(sources, sources, rows) == []


@pytest.mark.parametrize(
    "forwarder",
    [
        b'#if 0\n#include "real.h"\n#endif\n',
        b'#if UNKNOWN_FEATURE\n#include "real.h"\n#endif\n',
        b'#define OWN_WRITER() do { *reg = value; } while (0)\n#include "real.h"\n',
        b'#ifndef FORWARD_H\n#define FORWARD_H\n#include "real.h"\n#else\nstruct Other {};\n#endif\n',
    ],
    ids=[
        "inactive-include",
        "unknown-condition",
        "independent-macro-body",
        "guard-else",
    ],
)
def test_alias_cannot_claim_conditional_or_macro_implementation_as_pure_forwarding(
    forwarder: bytes,
) -> None:
    sources = {
        "src/platforms/forward.h": forwarder,
        "src/platforms/real.h": b"void emit() { *reg = value; }\n",
    }
    rows = {
        "src/platforms/forward.h": inventory_row(
            forwarder,
            "alias",
            canonical="src/platforms/real.h",
        ),
        "src/platforms/real.h": inventory_row(
            sources["src/platforms/real.h"], "dependency"
        ),
    }
    assert check_inventory(sources, sources, rows)


@pytest.mark.parametrize(
    "forwarder",
    [
        b"// comment " + b"\\" + b'\n#include "real.h"\n',
        b"// comment " + b"\\" + b'\r\n#include "real.h"\r\n',
        b'// comment ??/\n#include "real.h"\n',
        b'// comment ??/\r\n#include "real.h"\r\n',
    ],
    ids=["backslash-LF", "backslash-CRLF", "trigraph-LF", "trigraph-CRLF"],
)
def test_alias_rejects_line_splices_that_hide_the_include_inside_a_comment(
    forwarder: bytes,
) -> None:
    # C++ line splicing precedes comment removal; on trigraph-enabled targets
    # ??/ first becomes a backslash. These apparent includes are commented out.
    # Reject unsupported lexical forms instead of pretending to be a C++ lexer.
    sources = {
        "src/platforms/forward.h": forwarder,
        "src/platforms/real.h": b"void emit() { *reg = value; }\n",
    }
    rows = {
        "src/platforms/forward.h": inventory_row(
            forwarder,
            "alias",
            canonical="src/platforms/real.h",
        ),
        "src/platforms/real.h": inventory_row(
            sources["src/platforms/real.h"], "dependency"
        ),
    }
    assert check_inventory(sources, sources, rows), (
        "commented-out includes cannot establish an alias"
    )


def test_quoted_alias_include_cannot_skip_a_shadowing_local_header() -> None:
    sources = {
        "src/platforms/forward.h": b'#pragma once\n#include "real.h"\n',
        "src/platforms/real.h": b"void localOutput() { *reg = value; }\n",
        "src/real.h": b"struct DifferentRootOutput {};\n",
    }
    rows = {
        path: inventory_row(source, "dependency") for path, source in sources.items()
    }
    rows["src/platforms/forward.h"] = inventory_row(
        sources["src/platforms/forward.h"],
        "alias",
        canonical="src/real.h",
    )
    assert check_inventory(sources, sources, rows), (
        "quoted includes resolve the existing local header first"
    )


@pytest.mark.parametrize(
    "live_source",
    [b'#include "extra.h"\n', b"void ownOutput() { *other = value; }\n"],
    ids=["live-second-include", "live-independent-writer"],
)
def test_alias_comment_delimiters_cannot_erase_live_source(live_source: bytes) -> None:
    # Neither delimiter is a block-comment token: both occur in // comments.
    # Removing block-shaped text before recognizing line comments hides code.
    forwarder = b"// /*\n" + live_source + b'// */\n#pragma once\n#include "real.h"\n'
    sources = {
        "src/platforms/forward.h": forwarder,
        "src/platforms/real.h": b"void emit() { *reg = value; }\n",
        "src/platforms/extra.h": b"struct Extra {};\n",
    }
    rows = {
        path: inventory_row(source, "dependency") for path, source in sources.items()
    }
    rows["src/platforms/forward.h"] = inventory_row(
        forwarder, "alias", canonical="src/platforms/real.h"
    )
    assert check_inventory(sources, sources, rows), (
        "comment recognition must not delete a live implementation"
    )


@pytest.mark.parametrize(
    "guard", ["__cplusplus", "FASTLED_FORCE_SOFTWARE_SPI", "FORWARD_H"]
)
def test_alias_does_not_infer_unconditional_forwarding_from_macro_guards(
    guard: str,
) -> None:
    # A macro may already be defined. This includes conventional-looking guards:
    # the conservative contract must not assume their build configuration.
    forwarder = f'#ifndef {guard}\n#define {guard}\n#include "real.h"\n#endif\n'.encode(
        "ascii"
    )
    sources = {
        "src/platforms/forward.h": forwarder,
        "src/platforms/real.h": b"void emit() { *reg = value; }\n",
    }
    rows = {
        "src/platforms/forward.h": inventory_row(
            forwarder, "alias", canonical="src/platforms/real.h"
        ),
        "src/platforms/real.h": inventory_row(
            sources["src/platforms/real.h"], "dependency"
        ),
    }
    assert check_inventory(sources, sources, rows), (
        "macro-guard wrappers need explicit dependency/review disposition"
    )


def test_alias_line_comment_cannot_hide_live_code_after_a_lone_cr() -> None:
    forwarder = b'#pragma once\n#include "real.h"\n// comment\rstruct OwnWriter {};\n'
    sources = {
        "src/platforms/forward.h": forwarder,
        "src/platforms/real.h": b"void emit() { *reg = value; }\n",
    }
    rows = {
        "src/platforms/forward.h": inventory_row(
            forwarder, "alias", canonical="src/platforms/real.h"
        ),
        "src/platforms/real.h": inventory_row(
            sources["src/platforms/real.h"], "dependency"
        ),
    }
    assert check_inventory(sources, sources, rows), (
        "logical CR newlines end line comments before live code"
    )


@pytest.mark.parametrize("cycle", ["self", "two-node"])
def test_alias_graph_must_terminate_in_a_distinct_implementation(cycle: str) -> None:
    sources = {"src/platforms/a.h": b'#include "a.h"\n'}
    targets = {"src/platforms/a.h": "src/platforms/a.h"}
    if cycle == "two-node":
        sources = {
            "src/platforms/a.h": b'#include "b.h"\n',
            "src/platforms/b.h": b'#include "a.h"\n',
        }
        targets = {
            "src/platforms/a.h": "src/platforms/b.h",
            "src/platforms/b.h": "src/platforms/a.h",
        }
    rows = {
        path: inventory_row(source, "alias", canonical=targets[path])
        for path, source in sources.items()
    }
    assert check_inventory(sources, sources, rows), (
        "existing paths do not prove a terminating alias"
    )


@pytest.mark.parametrize(
    "mutation", ["unrelated-canonical", "independent-writer", "include-and-own-writer"]
)
def test_alias_metadata_cannot_hide_an_unforwarded_independent_implementation(
    mutation: str,
) -> None:
    sources = {
        "src/platforms/forward.h": b'#include "real.h"\n',
        "src/platforms/real.h": b"void emit() { *reg = value; }\n",
        "src/platforms/unrelated.h": b"struct Unrelated {};\n",
    }
    canonical = "src/platforms/real.h"
    if mutation == "unrelated-canonical":
        canonical = "src/platforms/unrelated.h"
    elif mutation == "independent-writer":
        sources["src/platforms/forward.h"] = b"void emitOwn() { *other = value; }\n"
    else:
        sources["src/platforms/forward.h"] += b"void emitOwn() { *other = value; }\n"
    rows = {
        path: inventory_row(source, "dependency") for path, source in sources.items()
    }
    rows["src/platforms/forward.h"] = inventory_row(
        sources["src/platforms/forward.h"], "alias", canonical=canonical
    )
    assert check_inventory(sources, sources, rows), (
        "an existing unrelated target cannot substitute for source forwarding"
    )


def test_positive_inventory_row_supports_multiple_known_finding_ids() -> None:
    path = "src/platforms/shared/spi_bitbang/generic_software_spi.h"
    sources = {path: b"void writeBit();\n"}
    rows = {
        path: inventory_row(
            sources[path], "positive", findings=["production-gate", "esp32-software"]
        )
    }
    assert check_inventory(sources, sources, rows) == []
    # A classification positive control is not a behavior/target positive control.
    required = [
        entry for entry in REQUIRED_FINDINGS if entry["id"] in rows[path]["findings"]
    ]
    assert check_evidence(required, [], sources)


@pytest.mark.parametrize(
    "mutation", ["unknown", "empty", "duplicate", "legacy-only", "wrong-type"]
)
def test_positive_inventory_row_rejects_invalid_or_legacy_only_finding_lists(
    mutation: str,
) -> None:
    path = "src/platforms/shared/spi_bitbang/generic_software_spi.h"
    sources = {path: b"void writeBit();\n"}
    # A legacy flag must not rescue an invalid plural binding.
    row = inventory_row(sources[path], "positive", finding="production-gate")
    if mutation == "unknown":
        row["findings"] = ["not-a-required-finding"]
    elif mutation == "empty":
        row["findings"] = []
    elif mutation == "duplicate":
        row["findings"] = ["production-gate", "production-gate"]
    elif mutation == "wrong-type":
        row["findings"] = "production-gate"
    assert check_inventory(sources, sources, {path: row}), (
        "positive rows require explicit known plural bindings"
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


def test_external_claims_alone_cannot_pass_without_unrelated_positive_findings() -> (
    None
):
    # This isolates the external-claim rejection branch. It is not a whole-audit
    # positive control: the real CLI separately requires its pinned findings.
    assert check_evidence([], [], {}) == []
    claims = [{"finding": "unverified", "status": "PASS", "command": ["true"]}]
    assert check_evidence([], claims, {}) == [
        "unverified: unverified external claim is not execution evidence"
    ]


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
    ) | {
        5055,
        5056,
        5057,
        5059,
        5060,
        5061,
        5062,
        5063,
        5064,
        5065,
        5066,
        5067,
        5068,
        5069,
        5070,
        5071,
        5072,
        5073,
        5074,
    }
    assert len(REQUIRED_FINDINGS) == 48
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

    git("init", "--quiet", "--initial-branch=audit-fixture")
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


def test_kl26_native_single_bit_protocol_requires_both_independent_implementations() -> (
    None
):
    finding = next(
        (entry for entry in REQUIRED_FINDINGS if entry["issue"] == 5059), None
    )
    assert finding is not None, (
        "native KL26 missing-bit protocol #5059 must remain required"
    )
    assert finding["id"] == "kl26-native-bit"
    assert finding["id"] != next(
        entry["id"] for entry in REQUIRED_FINDINGS if entry["issue"] == 5039
    )
    canonical = "src/platforms/arm/teensy/teensy_lc/fastspi_arm_kl26.h"
    legacy = "src/platforms/arm/kl26/fastspi_arm_kl26.h"
    assert finding["sources"] == [canonical, legacy]
    assert set(finding["entrypoints"]) == {
        "ARMHardwareSPIOutput::writeBit [teensy_lc canonical]",
        "ARMHardwareSPIOutput::writeBit [kl26 legacy]",
        "Teensy LC SPIOutput<11,13> -> SPI0 0x40076000",
    }
    assert set(finding["dependencies"]) == {
        legacy,
        "src/platforms/spi_output_template.h",
        "src/platforms/arm/teensy/teensy_lc/spi_output_template.h",
        "src/fl/chipsets/sm16716.h",
        "src/platforms/shared/spi_pixel_writer.h",
    }
    assert set(finding["required_configurations"]) == {
        "teensy-lc-native-all-bits-all-256-values",
        "teensy-lc-native-SM16716-start-bits",
        "teensy-lc-native-SM16716-header",
        "kl26-legacy-separate",
        "mux-drain-following-byte",
    }
    assert set(finding["concerns"]) == {
        "single-bit-GPIO-waveform-or-whole-protocol-fallback",
        "prior-byte-shifter-drain",
        "peripheral-mux-release-and-restore",
        "following-byte-transfer",
        "SM16716-one-start-bit-per-pixel",
        "SM16716-two-header-bits",
        "legacy-implementation-independent-validation",
    }
    assert check_evidence(
        [finding],
        [],
        {path: b"native implementation input\n" for path in finding["sources"]},
    )


def test_esp32_native_single_bit_registry_requires_all_three_real_implementations() -> (
    None
):
    finding = next(
        (entry for entry in REQUIRED_FINDINGS if entry["issue"] == 5060), None
    )
    assert finding is not None, (
        "native ESP32 byte-for-bit substitution #5060 must remain required"
    )
    assert finding["id"] == "esp32-native-single-bit"
    assert finding["id"] not in {
        entry["id"] for entry in REQUIRED_FINDINGS if entry["issue"] in {5028, 5051}
    }
    assert finding["sources"] == [
        "src/platforms/esp/32/drivers/spi/spi_device_proxy.h",
        "src/platforms/esp/32/core/fastspi_esp32_idf.h",
        "src/platforms/arduino/spi/fastspi_esp32_arduino.h",
    ]
    assert set(finding["entrypoints"]) == {
        "SPIDeviceProxy::writeBit [single/quad]",
        "ESP32SPIOutput::writeBit [ESP-IDF]",
        "ESP32SPIOutput::writeBit [Arduino]",
    }
    assert set(finding["dependencies"]) == {
        "src/platforms/spi_output_template.h",
        "src/platforms/esp/32/drivers/spi/spi_output_template.h",
        "src/platforms/esp/32/core/fastspi_esp32.h",
        "src/fl/chipsets/sm16716.h",
        "src/platforms/shared/spi_pixel_writer.h",
    }
    assert set(finding["required_configurations"]) == {
        "all-BIT0-7-all-256-values",
        "ESP32-proxy-single",
        "ESP32-proxy-quad",
        "IDF-buffer-and-flush",
        "bit-byte-bit",
        "consecutive-frames",
        "Xtensa-IDF",
        "RISC-V-IDF",
        "Arduino-FL_ESP32_SPI_ARDUINO",
    }
    assert set(finding["concerns"]) == {
        "one-clock-per-writeBit",
        "SM16716-25-bits-per-pixel-and-50-bit-header",
        "separate-method-restored-defect-mutations",
        "byte-only-APA102-preservation",
        "true-single-bit-or-whole-protocol-fallback-or-rejection",
    }
    assert check_evidence(
        [finding],
        [],
        {path: b"native implementation input\n" for path in finding["sources"]},
    )


def test_arduino_core_public_bit_registry_remains_required_without_a_selector() -> None:
    finding = next(
        (entry for entry in REQUIRED_FINDINGS if entry["issue"] == 5061), None
    )
    assert finding is not None, (
        "guarded callable ArdunioCoreSPIOutput missing-bit #5061 is unresolved "
        "even without a current in-tree selector"
    )
    assert finding["id"] == "arduino-core-native-single-bit"
    assert finding["sources"] == ["src/platforms/fastspi_arduino_core.h"]
    assert set(finding["entrypoints"]) == {
        "ArdunioCoreSPIOutput::writeBit",
        "ArdunioCoreSPIOutput::writePixels [FLAG_START_BIT]",
    }
    assert set(finding["dependencies"]) == {
        "src/platforms/arm/d51/fastpin_arm_d51.h",
        "src/platforms/arm/d51/fastled_arm_d51.h",
        "src/platforms/spi_output_template.h",
        "src/fastspi_types.h",
        "src/pixel_controller.h",
    }
    assert set(finding["required_configurations"]) == {
        "ARDUNIO_CORE_SPI-defined",
        "D51-guard-and-include-route",
        "public-direct-instantiation-on-real-Arduino-SDK",
        "BIT0-BIT3-BIT7-zero-one-payloads",
        "FLAG_START_BIT-pixels",
        "bit-byte-bit-and-state-restoration",
    }
    assert set(finding["concerns"]) == {
        "guarded-public-callable-positive",
        "no-current-in-tree-selector-not-a-negative",
        "one-clock-per-writeBit",
        "true-single-bit-or-explicit-compile-time-rejection",
        "peripheral-drain-mux-ownership-and-restoration",
        "restored-empty-body-mutation-must-fail",
    }
    assert check_evidence(
        [finding],
        [],
        {path: b"guarded public implementation input\n" for path in finding["sources"]},
    ), "a required row alone cannot prove callable protocol correctness"


@dataclass
class NativeProcessResult:
    returncode: int
    stdout: str


NATIVE_TEST_INPUTS = (
    "tests/platforms/shared/spi_bitbang/generic_software_spi.cpp",
    "tests/platforms/shared/spi_bitbang/generic_software_spi_inverted.cpp",
    "tests/platforms/shared/spi_bitbang/generic_software_spi_fixture.h",
    "tests/fl/system/forced_software_pin.cpp",
    "test",
    "test.py",
    "meson.build",
    "meson.options",
    "pyproject.toml",
    "tests/meson.build",
    "tests/test.h",
    "tests/test_pch.h",
    "tests/test_config.py",
    "tests/shared/runner.cpp",
    "tests/shared/fl_unittest.h",
    "ci/meson/native/meson.build",
    "ci/meson/runner.py",
    "ci/util/meson_runner.py",
    "ci/util/keep_editable_fbuild.sh",
)


def prepare_native_inputs(root: Path) -> dict[str, bytes]:
    sources = {"src/production.h": b"original production input\n"}
    for name in (*sources, *NATIVE_TEST_INPUTS):
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(sources.get(name, b"original test input\n"))
    (root / ".gitignore").write_bytes(b"node_modules/\n.build/\n")
    RunningProcess.run(
        ["git", "init", "--quiet", "--initial-branch=audit-fixture"],
        cwd=root,
        check=True,
        timeout=10,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    return sources


def patch_native_runner(
    monkeypatch: pytest.MonkeyPatch, run: Callable[..., object]
) -> None:
    # Keep real Git enumeration in these fixtures; fake only the expensive native
    # target execution. Otherwise support-input fingerprint coverage is vacuous.
    actual_run = RunningProcess.run

    def dispatch(command: list[str], **options: object) -> object:
        if command[:2] == ["bash", "test"]:
            return run(command, **options)
        return actual_run(command, **options)

    monkeypatch.setattr(audit.RunningProcess, "run", dispatch)


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

    patch_native_runner(monkeypatch, run)
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

    patch_native_runner(monkeypatch, run)
    monkeypatch.setattr("ci.software_spi_audit._current_sources", lambda _root: sources)
    records = _run_native_traces(tmp_path, sources)
    assert calls == [
        ["bash", "test", "generic_software_spi", "--clean"],
        ["bash", "test", "generic_software_spi", "--clean", "--debug"],
        ["bash", "test", "generic_software_spi_inverted", "--debug", "--clean"],
        ["bash", "test", "forced_software_pin", "--debug", "--clean"],
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

    patch_native_runner(monkeypatch, run)
    monkeypatch.setattr("ci.software_spi_audit._current_sources", lambda _root: sources)
    records = _run_native_traces(tmp_path, sources)
    assert calls == [
        ["bash", "test", "generic_software_spi", "--clean"],
        ["bash", "test", "generic_software_spi_inverted", "--debug", "--clean"],
        ["bash", "test", "forced_software_pin", "--debug", "--clean"],
    ]
    assert [record["command"] for record in records] == calls
    assert [record["returncode"] for record in records] == [0, 0, 0]


def test_native_fingerprint_excludes_ignored_generated_dependency_inputs(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    sources = prepare_native_inputs(tmp_path)
    generated = tmp_path / "ci/docker_utils/node_modules/package/generated.js"
    generated.parent.mkdir(parents=True)
    generated.write_bytes(b"generated dependency input\n")

    def run(_command: list[str], **_options: object) -> NativeProcessResult:
        generated.write_bytes(b"dependency changed during run\n")
        return NativeProcessResult(0, "runner control fixture, not production proof\n")

    patch_native_runner(monkeypatch, run)
    monkeypatch.setattr(audit, "_current_sources", lambda _root: sources)
    records = _run_native_traces(tmp_path, sources)
    assert [record["returncode"] for record in records] == [0, 0, 0]


def test_native_log_digest_matches_saved_bytes_under_windows_text_newlines(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    sources = prepare_native_inputs(tmp_path)
    original_write_text = Path.write_text

    def windows_write_text(path: Path, text: str, **options: object) -> int:
        if path.suffix == ".log":
            # Simulate Windows text-mode newline translation on any host.
            encoded = text.replace("\n", "\r\n").encode("utf-8")
            path.write_bytes(encoded)
            return len(text)
        return original_write_text(path, text, **options)

    monkeypatch.setattr(Path, "write_text", windows_write_text)
    patch_native_runner(
        monkeypatch, lambda *_args, **_kwargs: NativeProcessResult(0, "first\nsecond\n")
    )
    monkeypatch.setattr(audit, "_current_sources", lambda _root: sources)
    records = _run_native_traces(tmp_path, sources)
    for record in records:
        assert (
            digest((tmp_path / str(record["log"])).read_bytes()) == record["log_sha256"]
        )


@pytest.mark.parametrize(
    "malformation",
    [
        "nonobject-row",
        "unhashable-disposition",
        "nonstring-reason",
        "unhashable-canonical",
        "nonobject-alias-chain",
    ],
)
def test_nested_classification_schema_errors_are_structured_not_exceptions(
    malformation: str,
) -> None:
    sources = {
        "src/a.h": b'#pragma once\n#include "z.h"\n',
        "src/z.h": b"struct Z {};\n",
    }
    rows = {
        path: inventory_row(source, "dependency") for path, source in sources.items()
    }
    if malformation == "nonobject-row":
        rows["src/a.h"] = 7
    elif malformation == "unhashable-disposition":
        rows["src/a.h"]["disposition"] = []
    elif malformation == "nonstring-reason":
        rows["src/a.h"]["reason"] = {"not": "a string"}
    elif malformation == "unhashable-canonical":
        rows["src/a.h"] = inventory_row(sources["src/a.h"], "alias", canonical=[])
    else:
        rows["src/a.h"] = inventory_row(
            sources["src/a.h"], "alias", canonical="src/z.h"
        )
        rows["src/z.h"] = "not an object"
    errors = check_inventory(sources, sources, rows)
    assert errors
    assert all(isinstance(error, str) for error in errors)


def configure_tooling_cli(
    root: Path, monkeypatch: pytest.MonkeyPatch
) -> dict[str, bytes]:
    sources = {"src/pin.h": b"struct Pin {};\n"}
    monkeypatch.setattr(audit, "ROOT", root)
    monkeypatch.setattr(
        audit,
        "_baseline_objects",
        lambda _root: {
            path: audit._git_blob_digest(source) for path, source in sources.items()
        },
    )
    monkeypatch.setattr(audit, "_current_sources", lambda _root: dict(sources))
    monkeypatch.setattr(audit, "_ignored_boundaries", lambda _root: [])
    monkeypatch.setattr(audit, "_check_ignore_policy", lambda _root: [])
    monkeypatch.setattr(audit, "SEMANTIC_REQUIRED_PATHS", frozenset())
    (root / ".gitignore").write_bytes(b"")
    return sources


@pytest.mark.parametrize("malformation", ["row", "findings-entry", "top-level"])
def test_cli_malformed_nested_json_writes_a_red_report(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    malformation: str,
) -> None:
    sources = configure_tooling_cli(tmp_path, monkeypatch)
    classifications: object = negative_rows(sources)
    findings: object = list(audit.REQUIRED_FINDINGS)
    if malformation == "row":
        classifications = {"src/pin.h": 7}
    elif malformation == "findings-entry":
        findings = ["not a finding object"]
    else:
        classifications = ["not a row mapping"]
    classes_file = tmp_path / "classifications.json"
    findings_file = tmp_path / "findings.json"
    report = tmp_path / "report.json"
    classes_file.write_text(json.dumps(classifications), encoding="utf-8")
    findings_file.write_text(json.dumps(findings), encoding="utf-8")
    monkeypatch.setattr(
        sys,
        "argv",
        [
            "spi-audit",
            "--classifications",
            str(classes_file),
            "--findings",
            str(findings_file),
            "--report",
            str(report),
        ],
    )
    assert audit.main() == 1
    result = json.loads(report.read_text(encoding="utf-8"))
    assert result["complete"] is False
    assert result["errors"] and all(
        isinstance(error, str) for error in result["errors"]
    )


@pytest.mark.parametrize("classify_deleted", [False, True])
def test_cli_deleted_source_uses_actual_pinned_git_bytes_in_inventory(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    classify_deleted: bool,
) -> None:
    def git(*arguments: str) -> str:
        result = RunningProcess.run(
            ["git", *arguments],
            cwd=tmp_path,
            check=True,
            timeout=10,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )
        return result.stdout.strip()

    git("init", "--quiet", "--initial-branch=audit-fixture")
    sources = {
        "src/keep.h": b"struct Keep {};\n",
        "src/deleted.h": b"struct Deleted {};\n",
    }
    for name, source in sources.items():
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(source)
    (tmp_path / ".gitignore").write_bytes(b"")
    git("add", "src", ".gitignore")
    git(
        "-c",
        "user.name=Audit Fixture",
        "-c",
        "user.email=audit@example.invalid",
        "commit",
        "--quiet",
        "-m",
        "baseline fixture",
    )
    baseline_commit = git("rev-parse", "HEAD")
    # Move out of src instead of deleting; the Git index still records the blob.
    (tmp_path / "src/deleted.h").rename(tmp_path / "moved-outside-src.h")
    rows = negative_rows(sources)
    if not classify_deleted:
        del rows["src/deleted.h"]
    classes_file = tmp_path / "classifications.json"
    classes_file.write_text(json.dumps(rows), encoding="utf-8")
    report = tmp_path / "report.json"
    monkeypatch.setattr(audit, "ROOT", tmp_path)
    monkeypatch.setattr(audit, "BASELINE_COMMIT", baseline_commit)
    monkeypatch.setattr(audit, "BASELINE_SOURCE_COUNT", 2)
    monkeypatch.setattr(audit, "SEMANTIC_REQUIRED_PATHS", frozenset())
    monkeypatch.setattr(
        sys,
        "argv",
        ["spi-audit", "--classifications", str(classes_file), "--report", str(report)],
    )
    assert audit.main() == 1
    result = json.loads(report.read_text(encoding="utf-8"))
    assert result["baseline_files"] == 2
    assert result["current_files"] == 1
    assert (
        "deleted source requires run-backed disposition: src/deleted.h"
        in result["errors"]
    )
    assert "deleted source requires disposition: src/deleted.h" in result["errors"]
    if classify_deleted:
        assert not any(
            "classification outside source universe" in error
            for error in result["errors"]
        )
        assert not any(
            "stale classification source hash: src/deleted.h" in error
            for error in result["errors"]
        )
    else:
        assert "unclassified source: src/deleted.h" in result["errors"]


def test_real_spi_audit_wrapper_from_outside_checkout_writes_correct_report(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    root = Path(audit.__file__).resolve().parents[1]
    report = tmp_path / "outside-cwd-report.json"
    decoy = tmp_path / "ci"
    decoy.mkdir()
    (decoy / "__init__.py").write_bytes(b"")
    (decoy / "software_spi_audit.py").write_bytes(
        b'raise SystemExit("foreign module selected")\n'
    )
    for name in ("PYTHONPATH", "PYTHONSAFEPATH", "UV_PROJECT", "UV_WORKING_DIRECTORY"):
        monkeypatch.delenv(name, raising=False)
    monkeypatch.setenv("UV_NO_SYNC", "1")
    result = RunningProcess.run(
        ["bash", str(root / "spi-audit"), "--report", str(report)],
        cwd=tmp_path,
        check=False,
        timeout=90,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    assert result.returncode == 1, "unwired audit completion remains RED"
    assert "Software SPI audit RED" in result.stdout
    actual = json.loads(report.read_text(encoding="utf-8"))
    assert actual["baseline"] == audit.BASELINE_COMMIT
    assert actual["required_findings"] == list(REQUIRED_FINDINGS)
    assert actual["complete"] is False
    # Prove project identity/current enumeration independently of whether this
    # checkout contains the pinned history. Hosted shallow checkouts need not.
    current = _current_sources(root)
    assert current["src/fastspi.h"] == (root / "src/fastspi.h").read_bytes()
    assert "src/platforms/shared/spi_bitbang/generic_software_spi.h" in current
    baseline_command = [
        "git",
        "ls-tree",
        "-r",
        "-z",
        audit.BASELINE_COMMIT,
        "--",
        "src",
    ]
    baseline = RunningProcess.run(
        baseline_command,
        cwd=root,
        check=False,
        timeout=30,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    if baseline.returncode == 0:
        assert actual["baseline_files"] == BASELINE_SOURCE_COUNT
        assert actual["current_files"] == len(current)
        for path, source in current.items():
            assert actual["source_accounting"][path]["current_sha256"] == digest(source)
    else:
        # Accept only the exact real-Git missing-baseline failure, never an
        # arbitrary audit RED or a fabricated empty successful inventory.
        assert baseline.returncode == 128
        missing = CalledProcessError(baseline.returncode, baseline_command)
        assert actual["errors"] == [f"audit input/runtime error: {missing}"]
        assert actual["baseline_files"] == 0
        assert actual["current_files"] == 0
        assert actual["source_accounting"] == {}


def test_cli_real_git_missing_pinned_baseline_remains_fail_closed(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    # A fresh real repository deterministically lacks the pinned commit even
    # when the surrounding developer checkout has full history. Do not mock
    # Git, the baseline loader or the audit's rejection path.
    for name in (
        "GIT_DIR",
        "GIT_WORK_TREE",
        "GIT_COMMON_DIR",
        "GIT_OBJECT_DIRECTORY",
        "GIT_ALTERNATE_OBJECT_DIRECTORIES",
    ):
        monkeypatch.delenv(name, raising=False)
    RunningProcess.run(
        ["git", "init", "--quiet", "--initial-branch=audit-fixture"],
        cwd=tmp_path,
        check=True,
        timeout=10,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    source = tmp_path / "src" / "present.h"
    source.parent.mkdir()
    source.write_bytes(b"struct Present {};\n")
    assert _current_sources(tmp_path) == {"src/present.h": source.read_bytes()}
    with pytest.raises(CalledProcessError) as missing:
        audit._baseline_objects(tmp_path)
    assert missing.value.returncode == 128
    assert missing.value.cmd == [
        "git",
        "ls-tree",
        "-r",
        "-z",
        audit.BASELINE_COMMIT,
        "--",
        "src",
    ]
    report = tmp_path / "missing-baseline-report.json"
    monkeypatch.setattr(audit, "ROOT", tmp_path)
    monkeypatch.setattr(sys, "argv", ["spi-audit", "--report", str(report)])
    assert audit.main() == 1
    actual = json.loads(report.read_text(encoding="utf-8"))
    assert actual["baseline"] == audit.BASELINE_COMMIT
    assert actual["required_findings"] == list(REQUIRED_FINDINGS)
    assert actual["errors"] == [f"audit input/runtime error: {missing.value}"]
    assert actual["baseline_files"] == 0
    assert actual["current_files"] == 0
    assert actual["source_accounting"] == {}
    assert actual["complete"] is False


def test_baseline_blob_reader_preserves_real_git_binary_bytes(tmp_path: Path) -> None:
    payload = b"\x00\xff\r\ncaf\xc3\xa9\ntrailing\x00\r\n"
    binary = tmp_path / "blob.bin"
    binary.write_bytes(payload)
    RunningProcess.run(
        ["git", "init", "--quiet", "--initial-branch=audit-fixture"],
        cwd=tmp_path,
        check=True,
        timeout=10,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    created = RunningProcess.run(
        ["git", "hash-object", "-w", str(binary)],
        cwd=tmp_path,
        check=True,
        timeout=10,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    actual = audit._read_baseline_blob(tmp_path, created.stdout.strip())
    assert isinstance(actual, bytes)
    assert actual == payload
    assert digest(actual) == digest(payload)


@dataclass
class ReviewedBaselineFixture:
    root: Path
    sources: dict[str, bytes]
    objects: dict[str, str]
    document: dict[str, Any]
    blob_reads: list[str]


@pytest.fixture
def reviewed_baseline(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> ReviewedBaselineFixture:
    # The independent Git-tree fixture is not derived from the manifest being
    # validated. No real repository/test execution is claimed by this fixture.
    canonical = "src/platforms/shared/spi_bitbang/generic_software_spi.h"
    forward = "src/platforms/shared/spi_bitbang/forward.h"
    sources = {
        canonical: b"void emit() { *reg = value; }\n",
        forward: b'#pragma once\n#include "generic_software_spi.h"\n',
    }
    objects = {path: audit._git_blob_digest(source) for path, source in sources.items()}
    files: dict[str, dict[str, object]] = {}
    for path, source in sources.items():
        files[path] = {
            "baseline_blob_oid": objects[path],
            "baseline_sha256": digest(source),
            "disposition": "positive" if path == canonical else "alias",
            "role": "implementation" if path == canonical else "forwarding-alias",
            "findings": ["production-gate"] if path == canonical else [],
            "reason": "Reviewed source fixture, with literal baseline evidence below.",
            "source_evidence": [
                {"line": 1, "text": source.decode("ascii").splitlines()[0]}
            ],
        }
        if path == forward:
            files[path]["canonical"] = canonical
    document = {"schema_version": 1, "baseline": audit.BASELINE_COMMIT, "files": files}
    reads: list[str] = []

    def read_blob(_root: Path, object_id: str) -> bytes:
        reads.append(object_id)
        return next(
            source for path, source in sources.items() if objects[path] == object_id
        )

    monkeypatch.setattr(audit, "_baseline_objects", lambda _root: dict(objects))
    monkeypatch.setattr(audit, "_read_baseline_blob", read_blob, raising=False)
    return ReviewedBaselineFixture(tmp_path, sources, objects, document, reads)


def test_reviewed_baseline_loader_accepts_exact_source_bound_classification_only(
    reviewed_baseline: ReviewedBaselineFixture,
) -> None:
    fixture = reviewed_baseline
    loaded = audit.load_baseline_classifications(
        fixture.root, fixture.document, fixture.sources
    )
    assert loaded.errors == []
    assert set(loaded.rows) == set(fixture.objects)
    assert fixture.blob_reads == [], (
        "unchanged current bytes can be reused only after Git blob identity matches"
    )
    for path in fixture.sources:
        assert loaded.rows[path]["source_sha256"] == digest(fixture.sources[path])
    assert audit.check_evidence(audit.REQUIRED_FINDINGS, [], fixture.sources)


@pytest.mark.parametrize(
    "mutation",
    [
        "missing",
        "extra",
        "wrong-baseline",
        "bad-blob",
        "bad-sha256",
        "unknown-role",
        "unknown-finding",
        "empty-reason",
        "empty-evidence",
        "wrong-anchor",
        "claim-flag",
    ],
)
def test_reviewed_baseline_loader_rejects_incomplete_or_unverified_rows(
    reviewed_baseline: ReviewedBaselineFixture,
    mutation: str,
) -> None:
    fixture = reviewed_baseline
    document = json.loads(json.dumps(fixture.document))
    files = document["files"]
    path = next(iter(fixture.sources))
    row = files[path]
    if mutation == "missing":
        del files[path]
    elif mutation == "extra":
        files["src/unreviewed-extra.h"] = dict(row)
    elif mutation == "wrong-baseline":
        document["baseline"] = "0" * 40
    elif mutation == "bad-blob":
        row["baseline_blob_oid"] = "0" * 40
    elif mutation == "bad-sha256":
        row["baseline_sha256"] = "0" * 64
    elif mutation == "unknown-role":
        row["role"] = "assume-fast-enough"
    elif mutation == "unknown-finding":
        row["findings"] = ["not-in-fixed-registry"]
    elif mutation == "empty-reason":
        row["reason"] = "  "
    elif mutation == "empty-evidence":
        row["source_evidence"] = []
    elif mutation == "wrong-anchor":
        row["source_evidence"] = [{"line": 1, "text": "invented source evidence"}]
    else:
        row["status"] = "PASS"
    loaded = audit.load_baseline_classifications(
        fixture.root, document, fixture.sources
    )
    assert loaded.errors, (
        "review metadata cannot substitute for exact pinned source classification"
    )


def test_reviewed_baseline_loader_preserves_explicit_unknown_instead_of_default_negative(
    reviewed_baseline: ReviewedBaselineFixture,
) -> None:
    fixture = reviewed_baseline
    path = next(iter(fixture.sources))
    fixture.document["files"][path]["disposition"] = "unknown"
    fixture.document["files"][path]["findings"] = []
    fixture.document["files"][path]["reason"] = (
        "A newly discovered fixture entrypoint remains unclassified pending source review."
    )
    loaded = audit.load_baseline_classifications(
        fixture.root, fixture.document, fixture.sources
    )
    assert loaded.rows[path]["disposition"] == "unknown"
    assert "unknown" in " ".join(loaded.errors).lower()


def test_reviewed_baseline_loader_fetches_changed_source_but_never_refreshes_its_hash(
    reviewed_baseline: ReviewedBaselineFixture,
) -> None:
    fixture = reviewed_baseline
    path = next(iter(fixture.sources))
    current = dict(fixture.sources)
    current[path] = b"void changedOutput() { *reg = changed; }\n"
    loaded = audit.load_baseline_classifications(
        fixture.root, fixture.document, current
    )
    assert fixture.blob_reads == [fixture.objects[path]]
    assert loaded.rows[path]["source_sha256"] == digest(fixture.sources[path])
    assert loaded.rows[path]["source_sha256"] != digest(current[path])
    assert "changed" in " ".join(loaded.errors).lower()


def test_reviewed_baseline_loader_verifies_alias_against_real_baseline_source(
    reviewed_baseline: ReviewedBaselineFixture,
) -> None:
    fixture = reviewed_baseline
    path = "src/platforms/shared/spi_bitbang/forward.h"
    fixture.sources[path] = b"void independentOutput() { *reg = value; }\n"
    fixture.objects[path] = audit._git_blob_digest(fixture.sources[path])
    row = fixture.document["files"][path]
    row["baseline_blob_oid"] = fixture.objects[path]
    row["baseline_sha256"] = digest(fixture.sources[path])
    row["source_evidence"] = [
        {"line": 1, "text": fixture.sources[path].decode("ascii").strip()}
    ]
    loaded = audit.load_baseline_classifications(
        fixture.root, fixture.document, fixture.sources
    )
    assert "alias" in " ".join(loaded.errors).lower()


def test_cli_loads_reviewed_classification_without_turning_missing_execution_green(
    reviewed_baseline: ReviewedBaselineFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    fixture = reviewed_baseline
    classification = fixture.root / "reviewed.json"
    classification.write_text(json.dumps(fixture.document), encoding="utf-8")
    report = fixture.root / "report.json"
    monkeypatch.setattr(audit, "ROOT", fixture.root)
    monkeypatch.setattr(audit, "_current_sources", lambda _root: dict(fixture.sources))
    monkeypatch.setattr(audit, "_ignored_boundaries", lambda _root: [])
    monkeypatch.setattr(audit, "_check_ignore_policy", lambda _root: [])
    monkeypatch.setattr(audit, "SEMANTIC_REQUIRED_PATHS", frozenset())
    (fixture.root / ".gitignore").write_text("", encoding="utf-8")
    monkeypatch.setattr(
        sys,
        "argv",
        [
            "spi-audit",
            "--classifications",
            str(classification),
            "--report",
            str(report),
        ],
    )
    assert audit.main() == 1
    result = json.loads(report.read_text(encoding="utf-8"))
    assert result["baseline_files"] == len(fixture.objects)
    assert result["complete"] is False
    for path in fixture.sources:
        actual = result["source_accounting"][path]["classification"]
        assert actual["role"] == fixture.document["files"][path]["role"]
        assert actual["disposition"] != "unknown"
    assert any("runner not wired" in error for error in result["errors"])
    assert not any("unclassified source:" in error for error in result["errors"])


@pytest.mark.parametrize(
    ("target", "field", "value"),
    [
        ("top", "schema_version", True),
        ("anchor", "line", True),
        ("top", "files", []),
        ("row", None, []),
        ("row", "findings", {}),
        ("row", "role", []),
        ("row", "reason", []),
        ("row", "source_evidence", {}),
        ("anchor", "text", {}),
        ("top", "status", "PASS"),
        ("top", "complete", True),
        ("top", "executions", [{"returncode": 0}]),
        ("row", "status", "PASS"),
        ("anchor", "complete", True),
        ("row", "canonical", "src/unrelated.h"),
    ],
)
def test_reviewed_loader_rejects_boolean_indices_nested_types_and_claim_keys(
    reviewed_baseline: ReviewedBaselineFixture,
    target: str,
    field: str | None,
    value: object,
) -> None:
    fixture = reviewed_baseline
    document = json.loads(json.dumps(fixture.document))
    path = next(iter(fixture.sources))
    if target == "row" and field is None:
        document["files"][path] = value
    else:
        destination = document if target == "top" else document["files"][path]
        if target == "anchor":
            destination = destination["source_evidence"][0]
        destination[field] = value
    loaded = audit.load_baseline_classifications(
        fixture.root, document, fixture.sources
    )
    assert loaded.errors, "malformed review data must produce structured schema errors"
    assert all(isinstance(error, str) for error in loaded.errors)
    assert any(
        "classification schema" in error or "invalid reviewed classification" in error
        for error in loaded.errors
    ), "unrelated source/evidence errors must not conceal malformed input acceptance"


@pytest.mark.parametrize("duplicate", ["top-key", "path-key"])
def test_cli_rejects_raw_duplicate_classification_json_keys(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, duplicate: str
) -> None:
    sources = configure_tooling_cli(tmp_path, monkeypatch)
    rows = negative_rows(sources)
    path = next(iter(sources))
    row = json.dumps(rows[path])
    # Raw JSON is essential: constructing a dict first would erase the defect.
    text = (
        '{"schema_version":1,"schema_version":1,"baseline":'
        + json.dumps(audit.BASELINE_COMMIT)
        + ',"files":{}}'
        if duplicate == "top-key"
        else "{"
        + json.dumps(path)
        + ":"
        + row
        + ","
        + json.dumps(path)
        + ":"
        + row
        + "}"
    )
    classification = tmp_path / "reviewed.json"
    classification.write_text(text, encoding="utf-8")
    report = tmp_path / "report.json"
    monkeypatch.setattr(
        sys,
        "argv",
        [
            "spi-audit",
            "--classifications",
            str(classification),
            "--report",
            str(report),
        ],
    )
    assert audit.main() == 1
    errors = json.loads(report.read_text(encoding="utf-8"))["errors"]
    assert any("duplicate JSON key" in error for error in errors), (
        "global unwired-runner RED must not conceal acceptance of duplicate input keys"
    )


def test_reviewed_loader_reads_deleted_pinned_source_and_keeps_deletion_red(
    reviewed_baseline: ReviewedBaselineFixture,
) -> None:
    fixture = reviewed_baseline
    path = next(iter(fixture.sources))
    current = {name: source for name, source in fixture.sources.items() if name != path}
    loaded = audit.load_baseline_classifications(
        fixture.root, fixture.document, current
    )
    assert fixture.blob_reads == [fixture.objects[path]]
    assert loaded.rows[path]["source_sha256"] == digest(fixture.sources[path])
    assert f"deleted source requires run-backed disposition: {path}" in loaded.errors


def test_reviewed_loader_validates_changed_alias_against_pinned_bytes_only(
    reviewed_baseline: ReviewedBaselineFixture,
) -> None:
    fixture = reviewed_baseline
    path = "src/platforms/shared/spi_bitbang/forward.h"
    current = dict(fixture.sources)
    current[path] = b"void independentCurrentWriter() { *reg = value; }\n"
    loaded = audit.load_baseline_classifications(
        fixture.root, fixture.document, current
    )
    assert fixture.blob_reads == [fixture.objects[path]]
    assert loaded.rows[path]["source_sha256"] == digest(fixture.sources[path])
    assert (
        f"changed/rerouted source requires run-backed disposition: {path}"
        in loaded.errors
    )
    assert not any("not a pure forwarder" in error for error in loaded.errors), (
        "current changes cannot replace the pinned source used to validate reviewed aliases"
    )


def test_global_registry_detects_known_finding_swap_on_wrong_source() -> None:
    rows: dict[str, dict[str, Any]] = {}
    for finding in REQUIRED_FINDINGS:
        for path in finding["sources"]:
            row = rows.setdefault(path, {"disposition": "positive", "findings": []})
            row["findings"].append(finding["id"])
    assert check_required_findings(REQUIRED_FINDINGS, REQUIRED_FINDINGS, rows) == []
    finding = next(entry for entry in REQUIRED_FINDINGS if entry["issue"] == 5061)
    path = finding["sources"][0]
    rows[path]["findings"] = [
        "production-gate"
    ]  # Known, but belongs to another source.
    errors = check_required_findings(REQUIRED_FINDINGS, REQUIRED_FINDINGS, rows)
    assert f"required GPIO entrypoint declassified: {finding['id']} in {path}" in errors


def test_parallel_spi_target_backend_registry_requires_actual_vendor_selection() -> (
    None
):
    finding = next((row for row in REQUIRED_FINDINGS if row["issue"] == 5062), None)
    assert finding is not None, (
        "public parallel SPI target register selection #5062 remains required"
    )
    prefix = "src/platforms/shared/spi_bitbang/"
    assert finding["id"] == "parallel-spi-target-backend"
    assert set(finding["sources"]) == {prefix + "spi_platform.h"}
    assert set(finding["dependencies"]) == {
        *(prefix + f"spi_block_{width}.h" for width in (1, 2, 4, 8, 16, 32)),
        prefix + "spi_isr_engine.cpp.hpp",
        prefix + "spi_isr_engine.h",
        "src/fl/channels/spi/parallel_device.h",
        "src/fl/channels/spi/parallel_device.cpp.hpp",
    }
    assert set(finding["entrypoints"]) == {
        "FL_GPIO_WRITE_SET / FL_GPIO_WRITE_CLEAR target selection",
        *(f"SpiBlock{width}::transmit" for width in (1, 2, 4, 8, 16, 32)),
        "fl_parallel_spi_isr GPIO writes",
    }
    assert set(finding["required_configurations"]) == {
        "real-classic-ESP32-Xtensa-vendor-registers",
        "real-ESP32-C3-C2-RISC-V-vendor-registers",
        "non-ESP-target-explicit-rejection",
        "explicit-custom-backend-contract",
        "all-six-blocking-callers-and-shared-ISR-engine",
        "empty-final-repeated-transfer-and-unrelated-GPIO",
    }
    assert set(finding["concerns"]) == {
        "public-direct-callable-no-active-ParallelDevice-producer-claim",
        "actual-vendor-register-identity-bank-and-operation-width",
        "host-simulation-does-not-prove-target-addresses",
        "restored-unconditional-C3-C2-address-mutation-must-fail",
        "independent-caller-configuration-evidence",
    }
    assert check_evidence(
        [finding], [], {prefix + "spi_platform.h": b"target input"}
    ), "registry declarations cannot certify real SDK register selection"


def test_parallel_spi_pin_mapping_registry_preserves_independent_api_contracts() -> (
    None
):
    finding = next((row for row in REQUIRED_FINDINGS if row["issue"] == 5063), None)
    assert finding is not None, (
        "public parallel pin-index/mask validation #5063 remains required"
    )
    prefix = "src/platforms/shared/spi_bitbang/"
    assert finding["id"] == "parallel-spi-pin-mapping"
    assert set(finding["sources"]) == {
        *(prefix + f"spi_block_{width}.h" for width in (1, 2, 4, 8, 16, 32)),
        *(prefix + f"spi_isr_{width}.h" for width in (1, 2, 4)),
    }
    assert set(finding["dependencies"]) == {
        prefix + "spi_platform.h",
        prefix + "spi_isr_engine.h",
        prefix + "spi_isr_engine.cpp.hpp",
        *(prefix + f"spi_isr_{width}.h" for width in (8, 16, 32)),
        "src/fl/channels/spi/parallel_device.h",
        "src/fl/channels/spi/parallel_device.cpp.hpp",
    }
    assert set(finding["entrypoints"]) == {
        *(f"SpiBlock{width}::setPinMapping" for width in (1, 2, 4, 8, 16, 32)),
        *(
            f"SpiIsr{width}::{method}"
            for width in (1, 2, 4)
            for method in ("setPinMapping", "setPinMappingWithMask")
        ),
    }
    assert set(finding["required_configurations"]) == {
        "pin-indices-31-32-255-and-unsigned-int-width-boundaries",
        "duplicate-data-and-data-clock-collisions",
        "failed-reconfiguration-preserves-prior-valid-state",
        "all-six-blocking-and-ISR1-2-4-pin-and-mask-variants",
        "ISR8-16-32-separate-raw-mask-contracts",
        "32-independent-data-plus-clock-one-bank-rejection-or-real-bank-model",
        "real-vendor-bank-width-and-host-undefined-shift-detection",
        "valid-edges-unrelated-pins-empty-and-repeated-transfer",
    }
    assert set(finding["concerns"]) == {
        "validate-before-shift-index-state-change-or-GPIO",
        "independent-implementations-are-not-forwarding-aliases",
        "raw-mask-APIs-are-not-pin-index-APIs",
        "public-direct-callable-no-active-ParallelDevice-producer-claim",
        "restored-unchecked-mapping-mutations-must-fail-independently",
        "invalid-input-must-not-write-GPIO-or-alias-bits",
    }
    assert check_evidence(
        [finding], [], {path: b"mapping input" for path in finding["sources"]}
    ), "required mappings cannot certify validation or target bank behavior"


@pytest.fixture
def reviewed_binary_baseline(
    reviewed_baseline: ReviewedBaselineFixture,
) -> ReviewedBaselineFixture:
    fixture = reviewed_baseline
    path = "src/fixture/module.wasm"
    source = b"\x00asm\x01\x00\x00\x00\xff\r\n"
    fixture.sources[path] = source
    fixture.objects[path] = audit._git_blob_digest(source)
    fixture.document["files"][path] = {
        "baseline_blob_oid": fixture.objects[path],
        "baseline_sha256": digest(source),
        "disposition": "dependency",
        "role": "dependency",
        "findings": [],
        "reason": "Literal executable-module fixture; byte identity alone does not establish its execution context.",
        "source_evidence": [{"kind": "bytes", "offset": 0, "hex": "0061736d"}],
    }
    return fixture


def test_reviewed_binary_anchor_binds_bytes_without_execution_proof(
    reviewed_binary_baseline: ReviewedBaselineFixture,
) -> None:
    fixture = reviewed_binary_baseline
    loaded = audit.load_baseline_classifications(
        fixture.root, fixture.document, fixture.sources
    )
    assert loaded.errors == []
    assert loaded.rows["src/fixture/module.wasm"]["source_sha256"] == digest(
        b"\x00asm\x01\x00\x00\x00\xff\r\n"
    )
    assert audit.check_evidence(REQUIRED_FINDINGS, [], fixture.sources), (
        "binary source binding cannot certify execution or waive target requirements"
    )


@pytest.mark.parametrize(
    "anchor",
    [
        {"kind": "bytes", "offset": 0, "hex": ""},
        {"kind": "bytes", "offset": 0, "hex": "0"},
        {"kind": "bytes", "offset": 0, "hex": "GG"},
        {"kind": "bytes", "offset": 8, "hex": "FF"},
        {"kind": "bytes", "offset": 0, "hex": "00 61"},
        {"kind": "bytes", "offset": 0, "hex": 0},
        {"kind": "byte", "offset": 0, "hex": "00"},
        {"kind": "bytes", "offset": True, "hex": "00"},
        {"kind": "bytes", "offset": -1, "hex": "00"},
        {"kind": "bytes", "offset": "0", "hex": "00"},
        {"kind": "bytes", "offset": 11, "hex": "00"},
        {"kind": "bytes", "offset": 10, "hex": "0a00"},
        {"kind": "bytes", "offset": 0, "hex": "ff"},
        {"kind": "bytes", "offset": 0, "hex": "00", "status": "PASS"},
    ],
)
def test_reviewed_binary_anchor_rejects_malformed_or_mismatched_evidence(
    reviewed_binary_baseline: ReviewedBaselineFixture, anchor: object
) -> None:
    fixture = reviewed_binary_baseline
    path = "src/fixture/module.wasm"
    fixture.document["files"][path]["source_evidence"] = [anchor]
    loaded = audit.load_baseline_classifications(
        fixture.root, fixture.document, fixture.sources
    )
    assert any(
        f"invalid reviewed classification {path}:" in error for error in loaded.errors
    )


@pytest.mark.parametrize("mutation", ["blob", "sha256", "claim"])
def test_reviewed_binary_anchor_cannot_replace_whole_file_binding_or_schema(
    reviewed_binary_baseline: ReviewedBaselineFixture, mutation: str
) -> None:
    fixture = reviewed_binary_baseline
    path = "src/fixture/module.wasm"
    row = fixture.document["files"][path]
    if mutation == "claim":
        row["status"] = "PASS"
    else:
        row["baseline_blob_oid" if mutation == "blob" else "baseline_sha256"] = "0" * (
            40 if mutation == "blob" else 64
        )
    loaded = audit.load_baseline_classifications(
        fixture.root, fixture.document, fixture.sources
    )
    assert any(
        f"invalid reviewed classification {path}:" in error for error in loaded.errors
    )


@pytest.mark.parametrize("mutation", ["changed", "deleted", "new"])
def test_reviewed_binary_source_changes_remain_red_with_pinned_identity(
    reviewed_binary_baseline: ReviewedBaselineFixture, mutation: str
) -> None:
    fixture = reviewed_binary_baseline
    path = "src/fixture/module.wasm"
    current = dict(fixture.sources)
    if mutation == "changed":
        current[path] = b"\x00asm\x02\x00\x00\x00\xff\r\n"
        expected = f"changed/rerouted source requires run-backed disposition: {path}"
    elif mutation == "deleted":
        del current[path]
        expected = f"deleted source requires run-backed disposition: {path}"
    else:
        current["src/new-module.wasm"] = b"\x00asm\x01\x00\x00\x00"
        expected = "new source requires run-backed disposition: src/new-module.wasm"
    loaded = audit.load_baseline_classifications(
        fixture.root, fixture.document, current
    )
    assert expected in loaded.errors
    assert loaded.rows[path]["source_sha256"] == digest(fixture.sources[path])
    assert fixture.blob_reads == ([] if mutation == "new" else [fixture.objects[path]])


def test_teensy4_native_pending_bits_registry_requires_independent_completion() -> None:
    finding = next((row for row in REQUIRED_FINDINGS if row["issue"] == 5064), None)
    assert finding is not None, (
        "public native LPSPI pending-bit loss #5064 remains required"
    )
    prefix = "src/platforms/arm/teensy/teensy4_common/"
    assert finding["id"] == "teensy4-native-pending-bits"
    assert set(finding["sources"]) == {
        prefix + "fastspi_arm_mxrt1062.h",
        "src/platforms/arm/mxrt1062/fastspi_arm_mxrt1062.h",
    }
    assert set(finding["entrypoints"]) == {
        "Teensy4HardwareSPIOutput::" + method
        for method in (
            "writeBit",
            "writeByte",
            "writeWord",
            "writeBytesValueRaw",
            "writeBytesValue",
            "writeBytes",
            "writePixels",
            "init",
            "select",
            "waitFully",
            "release",
            "endTransaction",
            "finalizeTransmission",
        )
    }
    assert set(finding["dependencies"]) == {
        prefix + "lpspi/lpspi_bus.h",
        prefix + "lpspi/lpspi_hardware.h",
        "src/platforms/arm/mxrt1062/spi_device_proxy.h",
        "src/platforms/spi_output_template.h",
        "src/platforms/arm/teensy/is_teensy.h",
        "src/fl/chipsets/sm16716.h",
        "src/platforms/shared/spi_pixel_writer.h",
    }
    assert set(finding["required_configurations"]) == {
        "both-independent-canonical-and-legacy-headers-real-Teensy-SDK",
        "all-BIT0-7-zero-one-payloads",
        "partial-counts1-7-8-9-and-larger-tails",
        "bit-byte-bit-word-repeat-raw-bulk-and-pixel-start-bits",
        "wait-finalize-release-end-reselect-and-reinitialize",
        "all-supported-LPSPI-indices-or-explicit-rejection",
        "direct-backend-SM16716-two-bit-six-byte-two-bit-header",
        "byte-only-preservation-and-unrelated-peripheral-pin-state",
    }
    assert set(finding["concerns"]) == {
        "public-callable-positive-without-active-proxy-writeBit-producer",
        "independent-header-bodies-are-not-forwarding-aliases",
        "complete-pending-bits-before-select-release-or-explicitly-reject",
        "exact-bit-count-order-drain-and-no-cross-frame-pending-state",
        "restored-no-flush-and-pending-state-bypass-mutations-fail-separately",
        "real-LPSPI-frame-width-ownership-and-SDK-evidence",
    }
    assert check_evidence(
        [finding], [], {path: b"native LPSPI input" for path in finding["sources"]}
    ), "registry rows cannot prove native pending-bit completion"


def test_objectfled_spi_pin_restore_registry_requires_real_gpio_reuse() -> None:
    finding = next((row for row in REQUIRED_FINDINGS if row["issue"] == 5065), None)
    assert finding is not None, (
        "ObjectFLED SPI pin-bank restoration defect #5065 remains required"
    )
    prefix = "src/platforms/arm/teensy/teensy4_common/"
    helper = prefix + "drivers/objectfled/objectfled_spi_mode.cpp.hpp"
    assert finding["id"] == "objectfled-spi-pin-restore"
    assert finding["sources"] == [helper]
    assert set(finding["dependencies"]) == {
        prefix + "drivers/objectfled/objectfled_spi_mode.h",
        prefix + "drivers/objectfled/channel_engine_objectfled.cpp.hpp",
        prefix + "fastpin_arm_mxrt1062.h",
        "src/platforms/arm/teensy/is_teensy.h",
        "src/third_party/object_fled/src/ObjectFLEDDmaManager.h",
        "src/third_party/object_fled/src/ObjectFLEDDmaManager.cpp.hpp",
    }
    assert set(finding["entrypoints"]) == {
        "fl::objectfled_spi_init",
        "fl::objectfled_spi_show",
        "fl::objectfled_spi_wait",
        "fl::objectfled_spi_deinit",
        "AutoResearch::objectfledSpiSelfTest-RPC",
    }
    assert set(finding["required_configurations"]) == {
        "real-Teensy4-SDK-GPR26-and-CORE_PIN14_PORTREG-GPIO6",
        "both-selected-pads-GPIO6-and-other-supported-prior-states",
        "GPR26-mux-pad-GDIR-latch-restore-and-unrelated-bits",
        "actual-init-show-wait-deinit-and-repeated-frames",
        "allocation-failure-partial-init-and-timeout-cleanup",
        "ordinary-pinMode-digitalWrite-and-FastPin-reuse-after-deinit",
        "DMA-drain-before-safe-bank-and-pin-state-restoration",
    }
    assert set(finding["concerns"]) == {
        "actual-AutoResearch-direct-init-show-deinit-producer",
        "channel-canHandle-lookup-is-not-an-emitting-producer",
        "hardware-assisted-GPIO-serializer-not-FIFO-only-exclusion",
        "real-vendor-pinMode-does-not-restore-GPR26",
        "restored-no-GPR-restore-and-omitted-pad-state-mutations-fail-separately",
        "whole-bank-concurrent-writer-question-remains-separate-unproven",
        "SDK-source-configuration-and-producer-identity-bound-no-physical-claim",
    }
    assert all(path.startswith("src/") for path in finding["sources"])
    assert all(path.startswith("src/") for path in finding["dependencies"])
    assert check_evidence(
        [finding], [], {helper: b"actual helper identity needs executable proof"}
    ), "registry metadata cannot prove connected-bank restoration or GPIO reuse"


def test_my9221_ddr_timing_registry_requires_independent_protocol_timing() -> None:
    finding = next((row for row in REQUIRED_FINDINGS if row["issue"] == 5066), None)
    assert finding is not None, (
        "independent direct FastPin MY9221 DDR timing #5066 remains required"
    )
    source = "src/fl/chipsets/my9221.h"
    assert finding["id"] == "my9221-ddr-timing"
    assert finding["sources"] == [source]
    assert set(finding["dependencies"]) == {
        "src/FastLED.h",
        "src/chipsets.h",
        "src/pixeltypes.h",
        "src/fl/system/fastpin.h",
        "src/fl/system/fastpin_base.h",
        "src/platforms/fastpin.h",
        "src/fl/system/delay.h",
        "src/platforms/delay.h",
        "src/platforms/my9221.h",
        "src/platforms/my9221_emitter.h",
        "src/platforms/shared/my9221_typed_emitter.h",
        "src/platforms/esp/8266/my9221_emitter.h",
        "src/platforms/esp/32/my9221_emitter.h",
        "src/platforms/arm/rp/rpcommon/my9221_emitter.h",
        "src/platforms/arm/rp/rp2040/fastpin_arm_rp2040.h",
        "src/platforms/arm/rp/rp2350/fastpin_arm_rp2350.h",
        "src/platforms/esp/fastpin_esp.h",
        "src/platforms/esp/32/core/fastpin_esp32.h",
        "src/platforms/esp/8266/fastpin_esp8266.h",
        "src/platforms/is_platform.h",
        "src/platforms/shared/spi_bitbang/bitbang_delay.h",
        "src/platforms/cpu_frequency.h",
        "src/fl/stl/memory_resource.h",
        "src/fl/stl/memory_resource.cpp.hpp",
        "src/fl/stl/allocator.h",
        "src/fl/stl/allocator.cpp.hpp",
        "src/fl/stl/malloc.h",
    }
    assert set(finding["entrypoints"]) == {
        "MY9221Controller::init",
        "fl::detail::My9221Serializer::writeBit",
        "fl::detail::My9221Serializer::sendWord",
        "fl::detail::My9221Serializer::sendLed",
        "MY9221Controller::latch",
        "MY9221Controller::showPixels",
        "fl::detail::My9221EmitterDispatch::run",
        "CFastLED::addLeds-MY9221-mapping-and-legacy-channel-selection",
    }
    assert set(finding["required_configurations"]) == {
        "both-DDR-data-edges-DI-setup-and-hold-at-least-10ns",
        "DCKI-high-and-low-50ns-to-7200ns-supported-target-CPU-boundaries",
        "four-DI-latch-pulses-high-over-70ns-low-over-230ns",
        "idle-start-prelatch-over-220us-and-poststop-over-200ns-plus-N-times-10ns",
        "valid-chain-bounds-partial-chip-zero-padding-RGB-command-payload-order",
        "repeated-frames-and-empty-boundary-with-independent-timing-oracle",
        "actual-vendor-FastPin-CPU-compiler-codegen-and-unsupported-rejection",
    }
    assert set(finding["concerns"]) == {
        "actual-built-in-and-legacy-channel-producers-use-independent-direct-FastPin-DDR",
        "generic-software-SPI-timing-proof-does-not-cover-MY9221",
        "dual-edge-protocol-preserved-not-single-edge-SPI-substitution",
        "restore-missing-data-waits-latch-pulse-waits-and-poststop-wait-mutations-fail",
        "existing-208-bit-order-golden-model-cannot-certify-phase-timing",
        "intentional-unused-SPI_SPEED-is-not-a-separate-defect",
        "source-missing-guarantees-not-measured-short-pulses-electrical-proof-separate",
    }
    assert check_evidence(
        [finding], [], {source: b"registry identity cannot prove DDR timing"}
    ), "mandatory metadata cannot replace production timing/codegen evidence"


def test_esp32_forced_fastpin_registry_requires_actual_selection_proof() -> None:
    finding = next((row for row in REQUIRED_FINDINGS if row["issue"] == 5067), None)
    assert finding is not None, "ESP32 forced selection #5067 remains required"
    assert finding["id"] == "esp32-forced-fastpin-selection"
    source = "src/platforms/esp/32/core/fastpin_esp32.h"
    assert finding["sources"] == [source]
    assert set(finding["entrypoints"]) == {
        "FastPin<0..63>-explicit-native-specialization-selection",
        "ForcedSoftwarePin::hi-lo-toggle-and-NoPinMap-fallback",
        "GenericSoftwareSPIOutput-production-FastPin-selection",
        "MY9221Controller-production-FastPin-selection",
    }
    assert set(finding["required_configurations"]) == {
        "forced-on-off-with-and-without-FASTLED_NO_PINMAP",
        "actual-Xtensa-and-RISC-V-SDK-type-and-executed-route-proof",
        "valid-invalid-pins-and-user-C++-constant-expression-exclusion-masks",
        "generic-software-SPI-and-independent-MY9221-producers",
        "restore-old-unguarded-native-specializations-mutation-rejected",
    }
    assert set(finding["dependencies"]) == {
        "src/fl/system/fastpin.h",
        "src/fl/system/fastpin_base.h",
        "src/fl/system/forced_software_pin.h",
        "src/platforms/fastpin.h",
        "src/platforms/esp/fastpin_esp.h",
        "src/platforms/generic_pin.h",
        "src/platforms/is_platform.h",
        "src/platforms/esp/is_esp.h",
        "src/platforms/shared/spi_bitbang/generic_software_spi.h",
        "src/fastspi.h",
        "src/FastLED.h",
        "src/fl/chipsets/my9221.h",
        "src/platforms/my9221_emitter.h",
        "src/platforms/shared/my9221_typed_emitter.h",
        "src/platforms/esp/32/my9221_emitter.h",
    }
    assert set(finding["concerns"]) == {
        "explicit-native-specializations-currently-override-forced-primary",
        "NoPinMap-safety-5056-does-not-prove-ESP32-forced-selection",
        "macro-appearance-and-native-IRAM-ELF-do-not-prove-fallback-route",
        "real-selected-type-and-Arduino-fallback-calls-required",
        "forced-provider-cache-placement-and-phase-timing-remain-independent",
        "old-unguarded-restoration-must-fail-selection-oracle-not-compilation-incidental-error",
    }
    assert check_evidence([finding], [], {source: b"unproved actual selection"}), (
        "registry and native IRAM metadata cannot certify forced fallback selection"
    )


INIT_CLOCK_LOW_CASE = (
    "Software SPI production init establishes low clock before first byte"
)


def test_init_clock_low_mutants_change_only_actual_init_order() -> None:
    # Real production bytes, not a copied SPI algorithm or SDK fixture.
    path = audit.ROOT / "src/platforms/shared/spi_bitbang/generic_software_spi.h"
    original = path.read_bytes()
    mutants = audit._init_clock_low_mutants(original)
    low = b"\t\tfl::FastPin<CLOCK_PIN>::lo();\n"
    release = b"\t\trelease();\n"
    assert set(mutants) == {"missing-clock-low", "clock-low-after-release"}
    assert low + release in original
    assert mutants["missing-clock-low"] == original.replace(low + release, release, 1)
    assert mutants["clock-low-after-release"] == original.replace(
        low + release, release + low, 1
    )
    assert path.read_bytes() == original, "mutation preparation must not edit checkout"
    with pytest.raises(ValueError, match="init|mutation|clock"):
        audit._init_clock_low_mutants(original.replace(low + release, release))


def init_clock_low_output(stage: str) -> str:
    # Diagnostic-parser controls ONLY. These strings never authorize evidence.
    prefix = f"FAIL: {INIT_CLOCK_LOW_CASE} at generic_software_spi.cpp:208\n"
    if stage == "missing-clock-low":
        return "REQUIRE FAILED: generic_software_spi.cpp:17: 7 == 8\n"
    if stage == "clock-low-after-release":
        return (
            prefix
            + "  generic_software_spi.cpp:232: observer.clock_was_low\n"
            + "[doctest] test cases: 1 | 0 passed | 1 failed\n"
        )
    return (
        f"PASS: {INIT_CLOCK_LOW_CASE}\n[doctest] test cases: 1 | 1 passed | 0 failed\n"
    )


@pytest.mark.parametrize(
    "stage", ["baseline", "restored", "missing-clock-low", "clock-low-after-release"]
)
def test_init_clock_low_diagnostic_parser_has_nonvacuous_controls(stage: str) -> None:
    code = 1 if stage in {"missing-clock-low", "clock-low-after-release"} else 0
    # Validation returns no executable proof/token/receipt, only accepts the log.
    assert (
        audit._validate_init_clock_low_output(stage, code, init_clock_low_output(stage))
        is None
    )


@pytest.mark.parametrize(
    ("stage", "code", "output"),
    [
        ("missing-clock-low", 1, "fatal error: missing header\n"),
        ("missing-clock-low", 124, "timeout\n"),
        ("missing-clock-low", 1, "[doctest] test cases: 0 | 0 passed | 0 failed\n"),
        ("missing-clock-low", 0, init_clock_low_output("baseline")),
        ("missing-clock-low", 1, init_clock_low_output("clock-low-after-release")),
        (
            "missing-clock-low",
            1,
            init_clock_low_output("missing-clock-low").replace("7 == 8", "6 == 8"),
        ),
        (
            "missing-clock-low",
            1,
            init_clock_low_output("missing-clock-low").replace(
                "generic_software_spi.cpp", "unrelated.cpp"
            ),
        ),
        ("clock-low-after-release", 1, "REQUIRE FAILED: 7 == 8\n"),
        ("baseline", 0, "build succeeded; stale binary not executed\n"),
        ("restored", 1, init_clock_low_output("missing-clock-low")),
        ("unknown-stage", 0, init_clock_low_output("baseline")),
    ],
)
def test_init_clock_low_diagnostic_parser_rejects_incidental_failures(
    stage: str, code: int, output: str
) -> None:
    with pytest.raises(ValueError, match="init|stage|runtime|assertion|case|trace"):
        audit._validate_init_clock_low_output(stage, code, output)


def test_init_clock_low_fixture_diagnostics_cannot_resolve_evidence_slot() -> None:
    finding = next(f for f in REQUIRED_FINDINGS if f["issue"] == 5057)
    claims = [
        {
            "finding": finding["id"],
            "configuration": "normal-same-port",
            "command": ["bash", "test", "generic_software_spi", "--clean", "--debug"],
            "stages": {
                stage: init_clock_low_output(stage)
                for stage in (
                    "baseline",
                    "missing-clock-low",
                    "clock-low-after-release",
                    "restored",
                )
            },
            "compiler": "clang 21.1.5",
            "status": "PASS",
        }
    ]
    errors = check_evidence(
        [finding],
        claims,
        {path: (audit.ROOT / path).read_bytes() for path in finding["sources"]},
    )
    assert any("normal-same-port: unresolved" in error for error in errors)
    assert any("unverified external claim" in error for error in errors)


def test_init_clock_low_host32_claim_cannot_certify_width_or_target_matrix() -> None:
    finding = next(f for f in REQUIRED_FINDINGS if f["issue"] == 5057)
    claim = {
        "finding": finding["id"],
        "configuration": "normal-same-port",
        "scope": "host32-fixture4-only",
        "register_width": 32,
        "vendor_integration": False,
        "stages": {
            stage: init_clock_low_output(stage)
            for stage in (
                "baseline",
                "missing-clock-low",
                "clock-low-after-release",
                "restored",
            )
        },
    }
    errors = check_evidence(
        [finding],
        [claim],
        {path: (audit.ROOT / path).read_bytes() for path in finding["sources"]},
    )
    # Even genuine host32 execution cannot discharge a broader original slot;
    # this hand-authored diagnostic fixture additionally proves no execution.
    for configuration in finding["required_configurations"]:
        assert any(
            f"{finding['id']}/{configuration}: unresolved" in error for error in errors
        ), f"host32 must not waive unproved original obligation: {configuration}"


@pytest.mark.parametrize(
    "incidental",
    [
        "fatal error: fixture_dependency.h: No such file or directory",
        "ERROR: AddressSanitizer: heap-buffer-overflow",
        "runtime error: index 8 out of bounds for type unsigned char[8]",
        "terminate called after throwing an instance of std::runtime_error",
        "Segmentation fault (core dumped)",
        "Traceback (most recent call last):\nRuntimeError: loader failed",
    ],
)
@pytest.mark.parametrize("stage", ["missing-clock-low", "clock-low-after-release"])
def test_init_clock_low_expected_assertion_does_not_mask_incidental_error(
    stage: str, incidental: str
) -> None:
    # Parser-only diagnostic fixture, never production execution evidence.
    transcript = init_clock_low_output(stage) + incidental + "\n"
    with pytest.raises(
        ValueError, match="error|exception|sanitizer|runtime|fatal|trace"
    ):
        audit._validate_init_clock_low_output(stage, 1, transcript)


@pytest.mark.skipif(
    os.environ.get("FASTLED_RUN_SPI_INIT_PROOF") != "1",
    reason="requires four real managed debug builds and isolated source mutations",
)
def test_init_clock_low_real_isolated_runner_binds_execution_and_restores() -> None:
    # This positive control MUST execute real production builds. Unit fixture logs
    # and hand-authored JSON cannot satisfy it. Root runs this explicitly.
    source_name = "src/platforms/shared/spi_bitbang/generic_software_spi.h"
    before = audit._native_input_hashes(audit.ROOT, audit._current_sources(audit.ROOT))
    original = (audit.ROOT / source_name).read_bytes()
    expected_sources = {
        "baseline": original,
        **audit._init_clock_low_mutants(original),
        "restored": original,
    }
    proof = audit._run_init_clock_low_proof(audit.ROOT)
    assert proof.finding == "generic-init-clock-low"
    assert proof.scope == "host32-fixture4-only"
    assert proof.workspace != audit.ROOT
    assert proof.workspace.parent == Path.home() / ".clud" / "worktrees"
    assert proof.workspace.name.startswith("fastled3-wt-spi-init-proof-")
    assert [stage.name for stage in proof.stages] == list(expected_sources)
    assert (proof.workspace / source_name).read_bytes() == original
    assert (
        audit._native_input_hashes(audit.ROOT, audit._current_sources(audit.ROOT))
        == before
    ), "isolated mutation must never alter original tracked/support inputs"
    for stage in proof.stages:
        assert stage.source_sha256 == digest(expected_sources[stage.name])
        assert stage.inputs_before == stage.inputs_after
        assert stage.inputs_before[source_name] == stage.source_sha256
        for name in (
            "tests/platforms/shared/spi_bitbang/generic_software_spi.cpp",
            "tests/platforms/shared/spi_bitbang/generic_software_spi_fixture.h",
            "tests/shared/fl_unittest.cpp",
            "src/platforms/posix/run_unit_test.hpp",
            "src/platforms/run_unit_test.h",
            "tests/meson.build",
            "test",
            "test.py",
        ):
            assert stage.inputs_before[name] == before[name]
        assert stage.tools_before == stage.tools_after
        for tool in ("compiler", "linker", "loader"):
            identity = stage.tools_before[tool]
            assert identity.path.is_absolute()
            assert identity.sha256 == digest(identity.path.read_bytes())
            assert identity.version.strip()
        assert stage.build_returncode == 0
        assert stage.rebuilt_source == source_name
        assert stage.binary_path.is_file()
        assert stage.binary_sha256 == digest(stage.binary_path.read_bytes())
        assert stage.build_log.is_file() and stage.runtime_log.is_file()
        assert stage.build_log_sha256 == digest(stage.build_log.read_bytes())
        assert stage.runtime_log_sha256 == digest(stage.runtime_log.read_bytes())
        assert stage.runtime_command[1] == str(stage.binary_path)
        assert stage.runtime_command[2:] == [
            f"--test-case={INIT_CLOCK_LOW_CASE}",
            "--success",
            "--no-colors",
        ]
        audit._validate_init_clock_low_output(
            stage.name, stage.runtime_returncode, stage.runtime_log.read_text()
        )
    assert len({stage.binary_sha256 for stage in proof.stages[:3]}) == 3
    finding = next(f for f in REQUIRED_FINDINGS if f["issue"] == 5057)
    errors = check_evidence([finding], [], audit._current_sources(audit.ROOT))
    for configuration in finding["required_configurations"]:
        assert any(f"/{configuration}: unresolved" in error for error in errors), (
            "authentic host32 sub-obligation must not waive broader configuration"
        )


def test_init_clock_low_runner_rejects_missing_inputs_before_execution(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    def forbidden_execution(*_args: object, **_kwargs: object) -> None:
        pytest.fail("missing production/test inputs must reject before any process")

    monkeypatch.setattr(audit.RunningProcess, "run", forbidden_execution)
    with pytest.raises(ValueError, match="input|source|fixture|repository"):
        audit._run_init_clock_low_proof(tmp_path)


def test_init_clock_low_external_provenance_never_authenticates_execution(
    tmp_path: Path,
) -> None:
    finding = next(f for f in REQUIRED_FINDINGS if f["issue"] == 5057)
    claim = {
        "finding": finding["id"],
        "configuration": "normal-same-port",
        "scope": "host32-fixture4-only",
        "binary_path": str(tmp_path / "missing-generic_software_spi.so"),
        "binary_sha256": "0" * 64,
        "source_sha256": "1" * 64,
        "tools_before": {"compiler": {"path": "/invented/clang", "sha256": "2" * 64}},
        "tools_after": {"compiler": {"path": "/invented/clang", "sha256": "2" * 64}},
        "status": "PASS",
    }
    errors = check_evidence(
        [finding],
        [claim],
        {path: (audit.ROOT / path).read_bytes() for path in finding["sources"]},
    )
    assert any("unverified external claim" in error for error in errors)
    assert any("normal-same-port: unresolved" in error for error in errors)


def test_init_clock_low_actual_require_abort_needs_no_unreachable_summary() -> None:
    # Verbatim stdout captured by root from the actual selected managed loader.
    # REQUIRE calls _exit(1), so accumulated CHECK/case summaries cannot print.
    captured = (
        "Runner watchdog enabled (60.0 seconds)\n"
        "Pre-initializing CoroutineRunner singleton\n"
        "CoroutineRunner singleton pre-initialized successfully\n"
        "src/fl/channels/manager.cpp.hpp(47): INFO: ChannelManager: Initializing\n"
        "src/fl/channels/manager.cpp.hpp(200): INFO: ChannelManager: Added driver 'BIT_BANG' (priority 0, caps: CLOCKLESS|SPI)\n"
        "REQUIRE FAILED: ../../tests/platforms/shared/spi_bitbang/generic_software_spi.cpp:17: 7 == 8"
    )
    assert (
        audit._validate_init_clock_low_output("missing-clock-low", 1, captured) is None
    )


def test_init_clock_low_actual_late_low_reports_accumulated_observer_failure() -> None:
    # Verbatim failure grammar captured from the selected real managed loader.
    captured = (
        f"FAIL: {INIT_CLOCK_LOW_CASE} at ../../tests/platforms/shared/spi_bitbang/generic_software_spi.cpp:208\n"
        "  ../../tests/platforms/shared/spi_bitbang/generic_software_spi.cpp:232: observer.clock_was_low\n"
        "[doctest] test cases: 1 | 0 passed | 1 failed\n"
    )
    assert (
        audit._validate_init_clock_low_output("clock-low-after-release", 1, captured)
        is None
    )


def init_proof_gitlink_fixture(root: Path, name: str, registered: bool) -> str:
    RunningProcess.run(
        ["git", "init", "--quiet", "--initial-branch=audit-fixture"],
        cwd=root,
        check=True,
        timeout=10,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    oid = "074d09b3d004b54a00210c9231fdc190b3522940"
    if registered:
        (root / ".gitmodules").write_text(
            '[submodule "wiki"]\n\tpath = wiki\n'
            "\turl = https://github.com/fastled/fastled.wiki.git\n"
        )
    RunningProcess.run(
        ["git", "update-index", "--add", "--cacheinfo", f"160000,{oid},{name}"],
        cwd=root,
        check=True,
        timeout=10,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    return oid


def test_init_clock_low_snapshot_preserves_reviewed_docs_gitlink_identity(
    tmp_path: Path,
) -> None:
    from ci import software_spi_init_proof as proof

    oid = init_proof_gitlink_fixture(tmp_path, "wiki", True)
    inputs = proof._snapshot_inputs(tmp_path)
    assert "wiki" not in inputs.files, (
        "Gitlink is identity, never copied as source bytes"
    )
    assert set(inputs.gitlinks) == {"wiki"}
    assert inputs.gitlinks["wiki"].mode == "160000"
    assert inputs.gitlinks["wiki"].oid == oid
    assert "documentation" in inputs.gitlinks["wiki"].reason
    assert ".gitmodules" in inputs.files


@pytest.mark.parametrize(
    "path", ["src/vendor", "tests/vendor", "ci/vendor", "unknown_docs"]
)
def test_init_clock_low_snapshot_rejects_native_or_unreviewed_gitlinks(
    tmp_path: Path, path: str
) -> None:
    from ci import software_spi_init_proof as proof

    init_proof_gitlink_fixture(tmp_path, path, True)
    with pytest.raises(ValueError, match="Gitlink|gitlink|native|unreviewed"):
        proof._snapshot_inputs(tmp_path)


def test_init_clock_low_snapshot_rejects_unregistered_wiki_gitlink(
    tmp_path: Path,
) -> None:
    from ci import software_spi_init_proof as proof

    init_proof_gitlink_fixture(tmp_path, "wiki", False)
    with pytest.raises(ValueError, match="Gitlink|gitlink|registered|documentation"):
        proof._snapshot_inputs(tmp_path)


@pytest.mark.parametrize("changed", ["fixture", "test", "launcher", "tool"])
def test_init_clock_low_runner_rejects_between_stage_oracle_or_tool_drift(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, changed: str
) -> None:
    # Orchestration-negative control only: expensive build/stage boundaries are
    # mocked, and this fixture cannot supply authenticated execution evidence.
    from types import SimpleNamespace

    from ci import software_spi_init_proof as proof

    root, workspace = tmp_path / "root", tmp_path / "workspace"
    source = b"\tvoid init() FL_NO_EXCEPT {\n\t\tfl::FastPin<CLOCK_PIN>::lo();\n\t\trelease();\n\t}\n"
    names = {
        "fixture": "tests/fixture.h",
        "test": proof.TEST,
        "launcher": "ci/launcher.py",
    }
    for base in (root, workspace):
        for name in (proof.SOURCE, *names.values()):
            target = base / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source if name == proof.SOURCE else b"initial input")
    tool = tmp_path / "compiler"
    tool.write_bytes(b"initial tool")

    def inputs(base: Path) -> dict[str, str]:
        return {
            name: digest((base / name).read_bytes())
            for name in (proof.SOURCE, *names.values())
        }

    monkeypatch.setattr(proof, "_snapshot", lambda _root: (workspace, []))
    monkeypatch.setattr(proof, "_inputs", inputs)
    monkeypatch.setattr(
        proof,
        "_tools",
        lambda _workspace: {
            "compiler": proof.ToolIdentity(
                tool, digest(tool.read_bytes()), "fixture-tool"
            )
        },
    )
    monkeypatch.setattr(
        proof,
        "_run",
        lambda *_args, **_kwargs: NativeProcessResult(0, "not execution proof"),
    )
    # This older orchestration-negative fixture exercises input/tool drift only;
    # generated-context semantics are independently tested below.
    monkeypatch.setattr(proof, "_capture_build_context", lambda *_: None)
    monkeypatch.setattr(proof, "_verify_build_context", lambda *_: None)
    monkeypatch.setattr(proof, "_planned_commands", lambda *_: ())
    calls: list[str] = []

    def stage(
        _workspace: Path,
        _artifacts: Path,
        name: str,
        data: bytes,
        *_args: object,
        **_kwargs: object,
    ) -> object:
        calls.append(name)
        (workspace / proof.SOURCE).write_bytes(data)
        if len(calls) == 1:
            target = tool if changed == "tool" else workspace / names[changed]
            target.write_bytes(b"changed between stages, stable during later stages")
        return SimpleNamespace(binary_sha256=digest(name.encode()))

    monkeypatch.setattr(proof, "_stage", stage)
    with pytest.raises(ValueError, match="input|tool|identity|manifest|changed"):
        proof.run_init_clock_low_proof(root)
    assert calls == ["baseline"], "drift must reject before next production stage"
    assert (workspace / proof.SOURCE).read_bytes() == source


def init_clock_low_build_context_fixture(workspace: Path) -> dict[str, str]:
    # Generated-configuration fixture only; it cannot authorize execution proof.
    build = workspace / ".build/meson-debug"
    info = build / "meson-info"
    info.mkdir(parents=True)
    options = [
        {"name": "buildtype", "value": "debug", "type": "combo"},
        {"name": "debug", "value": True, "type": "boolean"},
        {"name": "optimization", "value": "0", "type": "combo"},
        {"name": "b_sanitize", "value": "none", "type": "combo"},
        {"name": "b_pch", "value": True, "type": "boolean"},
        {"name": "native_linker", "value": "/managed/reld", "type": "string"},
    ]
    (info / "intro-buildoptions.json").write_text(json.dumps(options))
    (info / "intro-compilers.json").write_text(
        json.dumps(
            {"host": {"cpp": {"exelist": ["/managed/ctc-clang++"], "id": "clang"}}}
        )
    )
    (info / "intro-machines.json").write_text(json.dumps({"host": {"system": "linux"}}))
    (build / "tu.rsp").write_text("-O0 -g -DKEEP=1")
    command = "/managed/ctc-clang++ @tu.rsp -c ../../tests/platforms/shared/spi_bitbang/generic_software_spi.cpp -o tests/generic.o"
    (build / "compile_commands.json").write_text(
        json.dumps(
            [
                {
                    "directory": str(build),
                    "file": "../../tests/platforms/shared/spi_bitbang/generic_software_spi.cpp",
                    "command": command,
                    "output": "tests/generic.o",
                }
            ]
        )
    )
    (build / "build.ninja").write_text(
        "include rules.ninja\nbuild tests/generic.o: cpp ../../tests/platforms/shared/spi_bitbang/generic_software_spi.cpp\n"
    )
    (build / "rules.ninja").write_text(
        "rule cpp\n command = /managed/ctc-clang++ @tu.rsp -c $in -o $out\n"
    )
    return {
        "PATH": "/managed",
        "CXXFLAGS": "",
        "LD_LIBRARY_PATH": str(build / "ci/meson/native"),
        "FASTLED_TEST_TIMEOUT": "60",
    }


@pytest.mark.parametrize(
    "changed",
    [
        "cpp_args",
        "debug",
        "b_sanitize",
        "b_pch",
        "native_linker",
        "compiler",
        "response",
        "recipe",
        "CXXFLAGS",
        "CPPFLAGS",
        "LDFLAGS",
        "PATH",
        "LD_LIBRARY_PATH",
        "ASAN_OPTIONS",
        "FASTLED_TEST_TIMEOUT",
    ],
)
def test_init_clock_low_build_context_rejects_generated_or_environment_drift(
    tmp_path: Path, changed: str
) -> None:
    from ci import software_spi_init_proof as proof

    environment = init_clock_low_build_context_fixture(tmp_path)
    initial = proof._capture_build_context(tmp_path, environment)
    # The unchanged control validates context only, never a runtime outcome.
    proof._verify_build_context(tmp_path, environment, initial)
    build = tmp_path / ".build/meson-debug"
    option_file = build / "meson-info/intro-buildoptions.json"
    if changed in {"debug", "b_sanitize", "b_pch", "native_linker", "cpp_args"}:
        options = json.loads(option_file.read_text())
        values = {
            "debug": False,
            "b_sanitize": "address",
            "b_pch": False,
            "native_linker": "/other/reld",
            "cpp_args": ["-DDRIFT=1"],
        }
        matching = [row for row in options if row["name"] == changed]
        if matching:
            matching[0]["value"] = values[changed]
        else:
            options.append({"name": changed, "value": values[changed], "type": "array"})
        option_file.write_text(json.dumps(options))
    elif changed == "compiler":
        path = build / "meson-info/intro-compilers.json"
        data = json.loads(path.read_text())
        data["host"]["cpp"]["exelist"] = ["/other/ctc-clang++"]
        path.write_text(json.dumps(data))
    elif changed in {"response", "recipe"}:
        path = build / ("tu.rsp" if changed == "response" else "rules.ninja")
        path.write_text(path.read_text() + " -DDRIFT=1")
    else:
        environment[changed] = "changed between stages, stable during next stage"
    with pytest.raises(
        ValueError, match="context|configuration|environment|recipe|changed"
    ):
        proof._verify_build_context(tmp_path, environment, initial)


@pytest.mark.parametrize(
    "output",
    [
        "[1/1] Compiling C++ object tests/generic.o",
        "/managed/ctc-clang++ @tu.rsp -DDRIFT=1 -c ../../tests/platforms/shared/spi_bitbang/generic_software_spi.cpp -o tests/generic.o",
        "/managed/ctc-clang++ @tu.rsp -c ../../tests/platforms/shared/spi_bitbang/generic_software_spi.cpp -o tests/wrong.o",
    ],
)
def test_init_clock_low_executed_argv_mismatch_rejected(
    tmp_path: Path, output: str
) -> None:
    from ci import software_spi_init_proof as proof

    environment = init_clock_low_build_context_fixture(tmp_path)
    context = proof._capture_build_context(tmp_path, environment)
    with pytest.raises(ValueError, match="executed|command|argv|compilation"):
        proof._validate_executed_compile_command(output, context)


def test_init_clock_low_run_uses_provisioned_environment_and_snapshot_imports(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    # A real Python child checks import isolation only, not SDK/compiler proof.
    import os
    import sys

    from ci import software_spi_init_proof as proof

    snapshot = tmp_path / "snapshot"
    (snapshot / "ci").mkdir(parents=True)
    (snapshot / "ci/__init__.py").write_text("SNAPSHOT_SENTINEL = 'owned snapshot'\n")
    foreign = tmp_path / "foreign"
    (foreign / "ci").mkdir(parents=True)
    (foreign / "ci/__init__.py").write_text(
        "raise RuntimeError('foreign ci imported')\n"
    )
    monkeypatch.setenv("VIRTUAL_ENV", sys.prefix)
    monkeypatch.setenv("UV_PROJECT_ENVIRONMENT", str(tmp_path / "wrong-environment"))
    monkeypatch.setenv("PYTHONPATH", str(foreign))
    (snapshot / ".build/child").mkdir(parents=True)
    code = (
        "import os; os.chdir('.build/child'); import ci,json; print(json.dumps({'ci': ci.__file__, "
        "'sentinel': ci.SNAPSHOT_SENTINEL, 'uv': os.environ['UV_PROJECT_ENVIRONMENT'], "
        "'pythonpath': os.environ['PYTHONPATH']}))"
    )
    result = proof._run(
        [sys.executable, "-c", code], snapshot, tmp_path / "child.log", build=False
    )
    assert result.returncode == 0, result.stdout
    data = json.loads(result.stdout)
    assert Path(data["ci"]).resolve() == (snapshot / "ci/__init__.py").resolve()
    assert data["sentinel"] == "owned snapshot"
    assert Path(data["uv"]).resolve() == Path(sys.prefix).resolve()
    assert data["pythonpath"].split(os.pathsep) == [str(snapshot)]


def test_init_clock_low_build_context_accepts_same_semantic_regeneration(
    tmp_path: Path,
) -> None:
    # Context normalization only: these opaque bytes never constitute proof.
    from ci import software_spi_init_proof as proof

    env = init_clock_low_build_context_fixture(tmp_path)
    build = tmp_path / ".build/meson-debug"
    private = build / "meson-private"
    private.mkdir()
    for name in ("build.dat", "coredata.dat", "meson_test_setup.dat"):
        (private / name).write_bytes(b"opaque serialization A\x00")
    initial = proof._capture_build_context(tmp_path, env)
    for name in ("build.dat", "coredata.dat", "meson_test_setup.dat"):
        (private / name).write_bytes(b"opaque serialization B\x00")
    ninja = build / "build.ninja"
    ninja.write_text("# regenerated serialization comment\n\n" + ninja.read_text())
    proof._verify_build_context(tmp_path, env, initial)


@pytest.mark.parametrize(
    "name", ["pch.ninja", "link.ninja", "meson_native.txt", "config.h"]
)
def test_init_clock_low_build_context_rejects_non_tu_configuration_drift(
    tmp_path: Path, name: str
) -> None:
    from ci import software_spi_init_proof as proof

    env = init_clock_low_build_context_fixture(tmp_path)
    build = tmp_path / ".build/meson-debug"
    path = build / name
    path.write_text(
        "rule pch\n command = compiler -DKEEP=1\n"
        if name.endswith(".ninja")
        else "#define KEEP 1\n"
    )
    initial = proof._capture_build_context(tmp_path, env)
    path.write_text(path.read_text().replace("KEEP", "DRIFT"))
    with pytest.raises(ValueError, match="context|configuration|recipe|changed"):
        proof._verify_build_context(tmp_path, env, initial)


@pytest.mark.parametrize("target", ["runner", "generic_software_spi"])
def test_init_clock_low_build_script_configures_only_bootstrap(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, target: str
) -> None:
    # Executes the actual orchestration script with API boundary doubles, not a
    # compiler/SDK or a purported successful behavioral-proof transcript.
    import sys
    from types import SimpleNamespace

    from ci import software_spi_init_proof as proof
    from ci.meson import build_config
    from ci.meson import compile as compile_mod

    calls: list[str] = []

    def setup(*_args: object, **_kwargs: object) -> bool:
        calls.append("configure")
        return True

    def compile_target(*_args: object, **kwargs: object) -> object:
        calls.append("compile")
        assert kwargs["target"] == target
        assert kwargs["verbose_commands"] is True
        return SimpleNamespace(success=True)

    monkeypatch.chdir(tmp_path)
    monkeypatch.setattr(sys, "argv", ["proof-script", target])
    monkeypatch.setattr(build_config, "setup_meson_build", setup)
    monkeypatch.setattr(compile_mod, "compile_meson", compile_target)
    with pytest.raises(SystemExit) as result:
        exec(proof.BUILD_SCRIPT, {})
    assert result.value.code == 0
    assert calls == (["configure", "compile"] if target == "runner" else ["compile"])


def test_init_clock_low_retains_raw_generated_configuration_for_diagnosis(
    tmp_path: Path,
) -> None:
    from ci import software_spi_init_proof as proof

    workspace, retained = tmp_path / "workspace", tmp_path / "retained"
    init_clock_low_build_context_fixture(workspace)
    private = workspace / ".build/meson-debug/meson-private"
    private.mkdir()
    raw = b"opaque vendor serialization\x00\xff"
    (private / "coredata.dat").write_bytes(raw)
    proof._retain_generated_configuration(workspace, retained)
    assert (retained / "meson-private/coredata.dat").read_bytes() == raw
    assert (retained / "build.ninja").read_bytes() == (
        workspace / ".build/meson-debug/build.ninja"
    ).read_bytes()
    manifest = json.loads((retained / "manifest.json").read_text())
    assert manifest["meson-private/coredata.dat"] == digest(raw)


def init_clock_low_captured_verbose_command() -> tuple[object, str]:
    from ci import software_spi_init_proof as proof

    # Verbatim real selected-TU log + configured argv, retained from failed
    # attempt3m8vb7fs. Grammar fixture only; it cannot authenticate a proof.
    data = json.loads(
        (Path(__file__).parent / "fixtures/spi_init_verbose_command.json").read_text()
    )
    context = proof.BuildContext(
        {}, {}, tuple(data["compile_argv"]), Path(data["directory"])
    )
    return context, data["verbose_line"]


@pytest.mark.parametrize("timestamps", [0, 1, 2])
def test_init_clock_low_actual_verbose_timestamp_prefixes(timestamps: int) -> None:
    from ci import software_spi_init_proof as proof

    context, captured = init_clock_low_captured_verbose_command()
    assert captured.startswith("20.77 20.77 [33/43] ")
    raw = captured.removeprefix("20.77 20.77 ")
    output = "20.77 " * timestamps + raw
    proof._validate_executed_compile_command(output, context)


@pytest.mark.parametrize(
    "changed",
    [
        "arbitrary-prefix",
        "three-timestamps",
        "changed-flags",
        "changed-source",
        "changed-output",
        "duplicate-execution",
    ],
)
def test_init_clock_low_actual_verbose_prefix_does_not_hide_argv_drift(
    changed: str,
) -> None:
    from ci import software_spi_init_proof as proof

    context, captured = init_clock_low_captured_verbose_command()
    if changed == "arbitrary-prefix":
        output = "untrusted-prefix " + captured
    elif changed == "three-timestamps":
        output = "20.77 " + captured
    elif changed == "changed-flags":
        output = captured.replace(" -O0 ", " -O2 ", 1)
    elif changed == "changed-source":
        output = captured.replace("generic_software_spi.cpp", "different_writer.cpp")
    elif changed == "changed-output":
        output = captured.replace(" -o ", " -o wrong-output ", 1)
    else:
        output = captured + "\n" + captured
    with pytest.raises(ValueError, match="executed|command|argv|compilation"):
        proof._validate_executed_compile_command(output, context)


def init_clock_low_schema_only_proof(workspace: Path) -> object:
    # Typed serialization fixture only, returned by a monkeypatched internal
    # runner. This is never accepted by check_evidence as execution proof.
    from ci import software_spi_init_proof as proof

    tool = proof.ToolIdentity(workspace / "compiler", "a" * 64, "schema-fixture")
    context = proof.BuildContext(
        {"native-config": "b" * 64},
        {"PATH": "c" * 64},
        ("compiler", "-c", proof.TEST),
        workspace,
    )
    stages = []
    for name in (
        "baseline",
        "missing-clock-low",
        "clock-low-after-release",
        "restored",
    ):
        stages.append(
            proof.InitStage(
                name=name,
                source_sha256="d" * 64,
                inputs_before={proof.TEST: "e" * 64},
                inputs_after={proof.TEST: "e" * 64},
                tools_before={"compiler": tool},
                tools_after={"compiler": tool},
                build_returncode=0,
                rebuilt_source=proof.SOURCE,
                binary_path=workspace / name / "test.so",
                binary_sha256="f" * 64,
                build_log=workspace / name / "build.log",
                build_log_sha256="1" * 64,
                runtime_log=workspace / name / "runtime.log",
                runtime_log_sha256="2" * 64,
                runtime_command=["loader", "test.so", "--test-case=" + proof.CASE],
                runtime_returncode=0 if name in {"baseline", "restored"} else 1,
                runtime_dependencies={"PCH": "3" * 64, "core.so": "4" * 64},
                build_context=context,
                executed_commands=(("compiler", "-c", proof.TEST),),
            )
        )
    return proof.InitProof(
        "generic-init-clock-low",
        "host32-fixture4-only",
        workspace,
        stages,
        {"wiki": proof.GitlinkInput("160000", "5" * 40, "reviewed docs")},
    )


def test_cli_run_native_invokes_fixed_init_proof_and_serializes_full_provenance(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    configure_tooling_cli(tmp_path, monkeypatch)
    calls: list[Path] = []
    typed = init_clock_low_schema_only_proof(tmp_path / "retained")

    def run(root: Path) -> object:
        calls.append(root)
        return typed

    monkeypatch.setattr(audit, "_run_native_traces", lambda *_: [])
    monkeypatch.setattr(audit, "_run_init_clock_low_proof", run)
    report = tmp_path / "audit.json"
    monkeypatch.setattr(
        sys, "argv", ["spi-audit", "--run-native", "--report", str(report)]
    )
    assert audit.main() == 1
    assert calls == [tmp_path]
    document = json.loads(report.read_text())
    assert document["complete"] is False
    assert any("runner not wired" in error for error in document["errors"])
    from dataclasses import asdict

    actual = document["init_clock_low_proof"]
    assert actual == json.loads(json.dumps(asdict(typed), default=str))
    assert actual["scope"] == "host32-fixture4-only"
    assert actual["workspace"] == str(typed.workspace)
    assert actual["gitlinks"]["wiki"]["mode"] == "160000"
    assert [stage["name"] for stage in actual["stages"]] == [
        stage.name for stage in typed.stages
    ]
    for saved, stage in zip(actual["stages"], typed.stages):
        assert saved["inputs_before"] == stage.inputs_before
        assert saved["inputs_after"] == stage.inputs_after
        assert (
            saved["tools_before"]["compiler"]["sha256"]
            == stage.tools_before["compiler"].sha256
        )
        assert saved["tools_after"] == saved["tools_before"]
        assert (
            saved["build_context"]["configuration"] == stage.build_context.configuration
        )
        assert saved["build_context"]["environment"] == stage.build_context.environment
        assert saved["executed_commands"] == [
            list(command) for command in stage.executed_commands
        ]
        assert saved["runtime_command"] == stage.runtime_command
        assert saved["runtime_dependencies"] == stage.runtime_dependencies
        for key in ("binary_path", "build_log", "runtime_log"):
            assert saved[key] == str(getattr(stage, key))
        for key in (
            "binary_sha256",
            "build_log_sha256",
            "runtime_log_sha256",
            "source_sha256",
        ):
            assert saved[key] == getattr(stage, key)


@pytest.mark.parametrize("failure", ["compile failure", "timeout", "restore failure"])
def test_cli_init_proof_failure_stays_red_and_preserves_retained_path(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    failure: str,
) -> None:
    configure_tooling_cli(tmp_path, monkeypatch)
    monkeypatch.setattr(audit, "_run_native_traces", lambda *_: [])
    retained = tmp_path / "owned-retained-attempt"

    def run(_root: Path) -> None:
        raise ValueError(f"init proof {failure}; retained {retained}")

    monkeypatch.setattr(audit, "_run_init_clock_low_proof", run)
    report = tmp_path / "failed.json"
    monkeypatch.setattr(
        sys, "argv", ["spi-audit", "--run-native", "--report", str(report)]
    )
    assert audit.main() == 1
    document = json.loads(report.read_text())
    assert document["complete"] is False
    assert not document.get("init_clock_low_proof")
    assert any(
        failure in error and str(retained) in error for error in document["errors"]
    )


@pytest.mark.parametrize("cached", [False, True])
def test_native_trace_requires_uncached_test_execution_not_compiler_cache_misses(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    cached: bool,
) -> None:
    sources = prepare_native_inputs(tmp_path)
    output = (
        "1/1 CACHED (previous test result)\n"
        if cached
        else "zccache cache hit\n1/1 PASS\n"
    )
    patch_native_runner(
        monkeypatch, lambda *_args, **_kwargs: NativeProcessResult(0, output)
    )
    monkeypatch.setattr(audit, "_current_sources", lambda _root: sources)
    records = _run_native_traces(tmp_path, sources)
    assert len(records) == 3
    assert all("--clean" in record["command"] for record in records)
    assert all("--no-fingerprint" not in record["command"] for record in records)
    if cached:
        assert all(
            record["kind"] != "host_production_gpio_trace"
            or record.get("fresh_execution") is False
            for record in records
        ), "cached test summaries are not fresh production traces"
    else:
        assert all(record["kind"] == "host_production_gpio_trace" for record in records)


# MY9221 adapter scaffolding contracts. These pure/helper tests are not runtime
# evidence; only the opt-in acceptance below may execute the actual fixed runner.
MY_SCHEDULE_CONTROLLER = "src/fl/chipsets/my9221.h"
MY_SCHEDULE_SERIALIZER = "src/platforms/shared/my9221_typed_emitter.h"
MY_SCHEDULE_CASE = (
    "MY9221 public controller observes DDR and four timed DI latch pulses"
)
MY_SCHEDULE_CHAIN_CASE = (
    "MY9221 partial BGR chains repeat with final HIGH and cascade stop"
)
MY_SCHEDULE_CASES = (
    MY_SCHEDULE_CASE,
    MY_SCHEDULE_CHAIN_CASE,
    "MY9221 real dynamic reserve failure emits no partial frame and retries",
    "MY9221 public emission guards edges and restores prior interrupt state",
)
MY_SCHEDULE_MUTATIONS = {
    "missing-latch-high-wait": (
        MY_SCHEDULE_CONTROLLER,
        b"Timing::template waitNanoseconds<80>();",
    ),
    "missing-latch-low-wait": (
        MY_SCHEDULE_CONTROLLER,
        b"Timing::template waitNanoseconds<240>();",
    ),
    "missing-data-setup-wait": (
        MY_SCHEDULE_SERIALIZER,
        b"Timing::template waitNanoseconds<10>();",
    ),
    "missing-data-hold-phase-wait": (
        MY_SCHEDULE_SERIALIZER,
        b"Timing::template waitNanoseconds<50>();",
    ),
    "missing-cascade-stop-wait": (
        MY_SCHEDULE_CONTROLLER,
        b"Timing::waitMicroseconds(mStopDelayUs);",
    ),
}
MY_SCHEDULE_STAGES = ("baseline", *MY_SCHEDULE_MUTATIONS, "restored")


def my_schedule_sources() -> dict[str, bytes]:
    return {
        path: (audit.ROOT / path).read_bytes()
        for path in (MY_SCHEDULE_CONTROLLER, MY_SCHEDULE_SERIALIZER)
    }


def test_my9221_managed_schedule_mutants_touch_only_prescribed_production_call() -> (
    None
):
    original = my_schedule_sources()
    before = dict(original)
    mutants = audit._my9221_schedule_mutants(original)
    assert original == before, "mutation preparation cannot alter caller source bytes"
    assert tuple(mutants) == tuple(MY_SCHEDULE_MUTATIONS)
    for stage, (changed_path, call) in MY_SCHEDULE_MUTATIONS.items():
        assert set(mutants[stage]) == set(original)
        assert original[changed_path].count(call) == 1
        for path, source in original.items():
            expected = source.replace(call, b"", 1) if path == changed_path else source
            assert mutants[stage][path] == expected


@pytest.mark.parametrize("defect", ["missing", "duplicate", "unexpected-source"])
def test_my9221_managed_schedule_mutation_preparation_rejects_ambiguous_source(
    defect: str,
) -> None:
    original = my_schedule_sources()
    call = MY_SCHEDULE_MUTATIONS["missing-data-setup-wait"][1]
    if defect == "missing":
        original[MY_SCHEDULE_SERIALIZER] = original[MY_SCHEDULE_SERIALIZER].replace(
            call, b""
        )
    elif defect == "duplicate":
        original[MY_SCHEDULE_SERIALIZER] += b"\n" + call
    else:
        original["src/untrusted.h"] = b"caller-selected mutation target"
    with pytest.raises(ValueError, match="source|pattern|mutation|call"):
        audit._my9221_schedule_mutants(original)


@pytest.mark.parametrize(
    "names",
    [
        MY_SCHEDULE_STAGES[:-1],
        MY_SCHEDULE_STAGES[1:],
        (*MY_SCHEDULE_STAGES[:2], MY_SCHEDULE_STAGES[1], *MY_SCHEDULE_STAGES[3:]),
        (*MY_SCHEDULE_STAGES[:-1], "caller-approved"),
        tuple(reversed(MY_SCHEDULE_STAGES)),
    ],
)
def test_my9221_managed_schedule_rejects_missing_corrupt_or_reordered_stages(
    names: tuple[str, ...],
) -> None:
    with pytest.raises(ValueError, match="stage|sequence|mutation|restor"):
        audit._validate_my9221_stage_sequence(names)


def test_my9221_managed_schedule_exact_stage_catalog_is_validation_only() -> None:
    audit._validate_my9221_stage_sequence(MY_SCHEDULE_STAGES)
    finding = next(row for row in REQUIRED_FINDINGS if row["issue"] == 5066)
    errors = check_evidence([finding], [], audit._current_sources(audit.ROOT))
    for configuration in finding["required_configurations"]:
        assert any(f"/{configuration}: unresolved" in error for error in errors)


def test_my9221_managed_schedule_runner_rejects_missing_inputs_before_process(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    def forbidden(*_args: object, **_kwargs: object) -> None:
        pytest.fail("missing source/oracle must reject before managed execution")

    monkeypatch.setattr(audit.RunningProcess, "run", forbidden)
    with pytest.raises(ValueError, match="source|input|fixture|repository"):
        audit._run_my9221_host_schedule_proof(tmp_path)


def test_my9221_managed_schedule_external_claim_cannot_clear_any_full_group() -> None:
    finding = next(row for row in REQUIRED_FINDINGS if row["issue"] == 5066)
    claims = [
        {
            "finding": finding["id"],
            "configuration": configuration,
            "scope": "host92-93-requested-schedule-and-public-controller-only",
            "status": "PASS",
            "stages": list(MY_SCHEDULE_STAGES),
        }
        for configuration in finding["required_configurations"]
    ]
    errors = check_evidence([finding], claims, audit._current_sources(audit.ROOT))
    assert any("external" in error for error in errors)
    for configuration in finding["required_configurations"]:
        assert any(f"/{configuration}: unresolved" in error for error in errors)


@pytest.mark.skipif(
    os.environ.get("FASTLED_RUN_MY9221_PROOF") != "1",
    reason="requires seven actual isolated managed debug builds and runtime mutants",
)
def test_my9221_managed_host_schedule_real_acceptance() -> None:
    from ci import software_spi_init_proof as native

    original = my_schedule_sources()
    before = native._inputs(audit.ROOT)
    expected = {
        "baseline": original,
        **audit._my9221_schedule_mutants(original),
        "restored": original,
    }
    proof = audit._run_my9221_host_schedule_proof(audit.ROOT)
    assert proof.finding == "my9221-ddr-timing"
    assert proof.scope == "host92-93-requested-schedule-and-public-controller-only"
    assert proof.workspace.parent == Path.home() / ".clud" / "worktrees"
    assert proof.workspace.name.startswith("fastled3-wt-spi-my9221-proof-")
    assert proof.workspace != audit.ROOT
    assert [stage.name for stage in proof.stages] == list(MY_SCHEDULE_STAGES)
    for stage in proof.stages:
        assert stage.source_sha256s == {
            path: digest(raw) for path, raw in expected[stage.name].items()
        }
        assert stage.inputs_before == stage.inputs_after
        expected_inputs = dict(before)
        expected_inputs.update(stage.source_sha256s)
        assert stage.inputs_before == expected_inputs
        assert stage.tools_before == proof.stages[0].tools_before
        assert stage.build_context == proof.stages[0].build_context
        if stage is not proof.stages[0]:
            native._validate_my9221_dependencies(
                proof.workspace, proof.stages[proof.stages.index(stage) - 1], stage
            )
        for path, raw in expected[stage.name].items():
            assert stage.inputs_before[path] == digest(raw)
        assert (
            stage.inputs_before["tests/fl/channels/spi_legacy_golden.cpp"]
            == before["tests/fl/channels/spi_legacy_golden.cpp"]
        )
        assert stage.tools_before == stage.tools_after
        assert stage.build_returncode == 0
        assert stage.rebuilt_source == "tests/fl/channels/spi_legacy_golden.cpp"
        assert stage.executed_commands and stage.build_context.compile_argv
        assert stage.binary_sha256 == digest(stage.binary_path.read_bytes())
        assert stage.build_log_sha256 == digest(stage.build_log.read_bytes())
        assert stage.runtime_dependencies
        cases = (
            MY_SCHEDULE_CASES
            if stage.name in {"baseline", "restored"}
            else (
                MY_SCHEDULE_CHAIN_CASE
                if stage.name == "missing-cascade-stop-wait"
                else MY_SCHEDULE_CASE,
            )
        )
        assert tuple(run.case for run in stage.executions) == cases
        for run in stage.executions:
            assert run.command[1] == str(stage.binary_path)
            assert run.command[2:] == [
                f"--test-case={run.case}",
                "--success",
                "--no-colors",
            ]
            assert run.log_sha256 == digest(run.log.read_bytes())
            assert run.returncode == (
                0 if stage.name in {"baseline", "restored"} else 1
            )
            audit._validate_my9221_schedule_output(
                stage.name, run.case, run.returncode, run.log.read_text()
            )
        for identity in stage.tools_before.values():
            assert identity.path.is_absolute() and identity.version.strip()
            assert identity.sha256 == digest(identity.path.read_bytes())
    assert len({stage.binary_sha256 for stage in proof.stages[:-1]}) == 6
    for path, raw in original.items():
        assert (proof.workspace / path).read_bytes() == raw
    assert native._inputs(audit.ROOT) == before
    finding = next(row for row in REQUIRED_FINDINGS if row["issue"] == 5066)
    errors = check_evidence([finding], [], audit._current_sources(audit.ROOT))
    for configuration in finding["required_configurations"]:
        assert any(f"/{configuration}: unresolved" in error for error in errors), (
            "host virtual schedule cannot certify complete target/configuration groups"
        )


@pytest.mark.parametrize(
    ("stage", "case", "diagnostic"),
    [
        ("missing-latch-high-wait", MY_SCHEDULE_CASE, "1 > 70"),
        ("missing-latch-low-wait", MY_SCHEDULE_CASE, "1 > 230"),
        ("missing-data-setup-wait", MY_SCHEDULE_CASE, "1 >= 10"),
        ("missing-data-hold-phase-wait", MY_SCHEDULE_CASE, "12 >= 50"),
        ("missing-cascade-stop-wait", MY_SCHEDULE_CHAIN_CASE, "240 > 250"),
    ],
)
@pytest.mark.parametrize(
    "incidental", ["AddressSanitizer: bad access", "fatal error: compilation failed"]
)
def test_my9221_managed_schedule_intended_diagnostic_cannot_mask_incidental_failure(
    stage: str,
    case: str,
    diagnostic: str,
    incidental: str,
) -> None:
    # Parser rejection fixtures only: expressions come from real manual runtime
    # observations, but this hand-written transcript grants NO execution proof.
    transcript = (
        f"FAIL: {case} at ../../tests/fl/channels/spi_legacy_golden.cpp:870\n"
        f"  ../../tests/fl/channels/spi_legacy_golden.cpp:160: {diagnostic}\n"
        "[doctest] test cases: 1 | 0 passed | 1 failed\n" + incidental
    )
    with pytest.raises(ValueError, match="error|sanitizer|fatal|incidental|diagnostic"):
        audit._validate_my9221_schedule_output(stage, case, 1, transcript)


@pytest.mark.parametrize(
    "diagnostic", ["240 > 210", "0 == 1040", "240 > 250\n  other.cpp:1: false"]
)
def test_my9221_managed_schedule_stop_requires_only_intended_five_chip_failure(
    diagnostic: str,
) -> None:
    transcript = (
        f"FAIL: {MY_SCHEDULE_CHAIN_CASE} at ../../tests/fl/channels/spi_legacy_golden.cpp:909\n"
        f"  ../../tests/fl/channels/spi_legacy_golden.cpp:165: {diagnostic}\n"
        "[doctest] test cases: 1 | 0 passed | 1 failed\n"
    )
    with pytest.raises(
        ValueError, match="assertion|diagnostic|failure|bound|five|oracle"
    ):
        audit._validate_my9221_schedule_output(
            "missing-cascade-stop-wait", MY_SCHEDULE_CHAIN_CASE, 1, transcript
        )


@pytest.mark.parametrize("failure", ["compile failure", "timeout", "restore failure"])
def test_my9221_managed_schedule_cli_records_failed_internal_attempt_without_scope_grant(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    failure: str,
) -> None:
    # This mocks orchestration, not execution or authenticated proof creation.
    configure_tooling_cli(tmp_path, monkeypatch)
    monkeypatch.setattr(audit, "_run_native_traces", lambda *_: [])
    monkeypatch.setattr(
        audit,
        "_run_init_clock_low_proof",
        lambda *_: init_clock_low_schema_only_proof(tmp_path),
    )
    calls: list[Path] = []
    retained = tmp_path / "retained-my9221-attempt"

    def failed(root: Path) -> None:
        calls.append(root)
        raise ValueError(f"MY9221 proof {failure}; retained {retained}")

    monkeypatch.setattr(audit, "_run_my9221_host_schedule_proof", failed, raising=False)
    report = tmp_path / "my9221-cli.json"
    monkeypatch.setattr(
        sys, "argv", ["spi-audit", "--run-native", "--report", str(report)]
    )
    assert audit.main() == 1
    assert calls == [tmp_path], (
        "--run-native must invoke only the fixed internal MY adapter"
    )
    document = json.loads(report.read_text())
    assert document["complete"] is False
    assert not document.get("my9221_host_schedule_proof")
    assert any(
        failure in error and str(retained) in error for error in document["errors"]
    )


# Runner validation controls only: these synthetic stage records are never proof.
def test_my9221_managed_rebuilt_pch_is_source_bound_validation_only(
    tmp_path: Path,
) -> None:
    from types import SimpleNamespace

    from ci import software_spi_init_proof as native

    pch = str(tmp_path / ".build/meson-debug/tests/test_pch.h.pch")
    library = str(tmp_path / ".build/meson-debug/ci/meson/native/fastled.so")
    external = str(tmp_path / "external/runtime.so")
    common = dict(
        executed_commands=(
            (
                "compiler",
                "-x",
                "c++-header",
                "-o",
                "test_pch.h.pch",
                "-Werror=invalid-pch",
                "-verify-pch",
                "-fpch-validate-input-files-content",
            ),
        ),
        tools_before={"compiler": "fixed"},
        build_context="fixed",
    )
    before = SimpleNamespace(
        runtime_dependencies={
            pch: "old-pch",
            library: "fixed-lib",
            external: "fixed-external",
        },
        source_sha256s={"src/fl/chipsets/my9221.h": "original"},
        **common,
    )
    after = SimpleNamespace(
        runtime_dependencies={
            pch: "rebuilt-pch",
            library: "fixed-lib",
            external: "fixed-external",
        },
        source_sha256s={"src/fl/chipsets/my9221.h": "prescribed-mutant"},
        **common,
    )
    native._validate_my9221_dependencies(tmp_path, before, after)
    # A legitimate derived-byte delta must not grant any configuration coverage.
    finding = next(row for row in REQUIRED_FINDINGS if row["issue"] == 5066)
    assert check_evidence([finding], [], audit._current_sources(audit.ROOT))
    for field in ("external", "missing", "commands", "tools", "context"):
        bad = SimpleNamespace(**vars(after))
        bad.runtime_dependencies = dict(after.runtime_dependencies)
        if field == "external":
            bad.runtime_dependencies[external] = "drift"
        elif field == "missing":
            del bad.runtime_dependencies[pch]
        elif field == "commands":
            bad.executed_commands = ()
        elif field == "tools":
            bad.tools_before = {"compiler": "drift"}
        else:
            bad.build_context = "drift"
        with pytest.raises(ValueError):
            native._validate_my9221_dependencies(tmp_path, before, bad)


def test_my9221_managed_runtime_dependency_drift_is_never_a_rebuild(
    tmp_path: Path,
) -> None:
    from types import SimpleNamespace

    from ci import software_spi_init_proof as native

    pch = str(tmp_path / ".build/meson-debug/tests/test_pch.h.pch")
    common = dict(
        source_sha256s={"src/fl/chipsets/my9221.h": "same"},
        executed_commands=(
            (
                "compiler",
                "-x",
                "c++-header",
                "-o",
                "test_pch.h.pch",
                "-Werror=invalid-pch",
                "-verify-pch",
                "-fpch-validate-input-files-content",
            ),
        ),
        tools_before={"compiler": "fixed"},
        build_context="fixed",
    )
    before = SimpleNamespace(runtime_dependencies={pch: "before"}, **common)
    after = SimpleNamespace(runtime_dependencies={pch: "after"}, **common)
    with pytest.raises(ValueError, match="dependenc"):
        native._validate_my9221_dependencies(tmp_path, before, after)


@pytest.mark.parametrize("failure", ["write", "read"])
def test_my9221_managed_restore_attempts_both_headers_after_first_error(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, failure: str
) -> None:
    from ci import software_spi_init_proof as native

    original = {
        "controller.h": b"controller-original",
        "serializer.h": b"serializer-original",
    }
    for path in original:
        (tmp_path / path).write_bytes(b"mutated")
    write = Path.write_bytes
    read = Path.read_bytes
    attempted = []

    def fail_write(path: Path, data: bytes) -> int:
        attempted.append(path.name)
        if failure == "write" and path.name == "controller.h":
            raise OSError("controller write blocked")
        return write(path, data)

    def fail_read(path: Path) -> bytes:
        if failure == "read" and path.name == "controller.h":
            raise OSError("controller verification blocked")
        return read(path)

    monkeypatch.setattr(Path, "write_bytes", fail_write)
    monkeypatch.setattr(Path, "read_bytes", fail_read)
    errors = native._restore_sources(tmp_path, original)
    assert attempted == ["controller.h", "serializer.h"]
    assert read(tmp_path / "serializer.h") == original["serializer.h"]
    assert errors and "controller.h" in str(errors)
    assert "blocked" in str(errors)
    assert not (tmp_path / "proof.json").exists()


@pytest.mark.parametrize(
    "issue,source",
    [
        (5068, "src/platforms/esp/8266/fastpin_esp8266.h"),
        (5069, "src/platforms/arm/nrf51/fastpin_arm_nrf51.h"),
        (5070, "src/platforms/arm/nrf52/fastpin_arm_nrf52_variants.h"),
        (5071, "src/platforms/arm/giga/armpin.h"),
        (5072, "src/platforms/arm/mgm240/fastpin_arm_mgm240.h"),
        (5073, "src/platforms/arm/lpc/fastpin_arm_lpc.h"),
        (5074, "src/platforms/arm/lpc/fastpin_arm_lpc11_legacy.h"),
    ],
)
def test_additional_forced_selection_findings_remain_required(
    issue: int, source: str
) -> None:
    row = next(item for item in REQUIRED_FINDINGS if item["issue"] == issue)
    assert source in row["sources"]
    assert "src/fl/system/forced_software_pin.h" in row["dependencies"]
    assert "force-on/NO_PINMAP Arduino fallback" in row["required_configurations"]
    assert (
        "restored unguarded specialization must fail the selected-type/route oracle"
        in row["required_configurations"]
    )
    assert check_evidence([row], [], audit._current_sources(audit.ROOT))


def my9221_actual_setup_transcript() -> str:
    fixture = audit.ROOT / "ci/fixtures/software_spi_audit/my9221_setup_runtime.json"
    data = json.loads(fixture.read_text())
    assert digest(data["output"].encode()) == data["sha256"]
    return data["output"]


def test_my9221_managed_actual_setup_preamble_and_changes_are_intended() -> None:
    output = my9221_actual_setup_transcript()
    assert output.count(":179: 4 >= 10") == 1
    assert output.count(":179: 1 >= 10") == 18
    audit._validate_my9221_schedule_output(
        "missing-data-setup-wait", MY_SCHEDULE_CASE, 1, output
    )


@pytest.mark.parametrize(
    "old,new",
    [
        (":179: 4 >= 10", ":179: 5 >= 10"),
        (":179: 4 >= 10", ":179: 4 >= 9"),
        (":179: 4 >= 10", ":180: 4 >= 10"),
        (":179: 1 >= 10", ":179: 2 >= 10"),
        (":179: 1 >= 10", ":179: 1 >= 9"),
        (":179: 1 >= 10", ":146: 1 >= 10"),
    ],
)
def test_my9221_managed_setup_capture_rejects_other_values_bounds_and_lines(
    old: str, new: str
) -> None:
    with pytest.raises(ValueError):
        audit._validate_my9221_schedule_output(
            "missing-data-setup-wait",
            MY_SCHEDULE_CASE,
            1,
            my9221_actual_setup_transcript().replace(old, new, 1),
        )


@pytest.mark.parametrize(
    "tail", ["", "fatal error: unrelated failure", "AddressSanitizer: bad access"]
)
def test_my9221_managed_setup_preamble_cannot_replace_required_one_ns_failure(
    tail: str,
) -> None:
    output = my9221_actual_setup_transcript()
    output = "\n".join(
        line for line in output.splitlines() if ":179: 1 >= 10" not in line
    )
    with pytest.raises(ValueError):
        audit._validate_my9221_schedule_output(
            "missing-data-setup-wait", MY_SCHEDULE_CASE, 1, output + tail
        )


def test_my9221_managed_hold_derived_phase_and_datahold_families_only() -> None:
    # Derived from unchanged real oracle lines169/180 and exact selected payload;
    # parser validation only, pending actual captured hold-mutant execution.
    output = (
        f"FAIL: {MY_SCHEDULE_CASE} at ../../tests/fl/channels/spi_legacy_golden.cpp:870\n"
        f"  ../../tests/fl/channels/spi_legacy_golden.cpp:169: 12 >= 50\n"
        f"  ../../tests/fl/channels/spi_legacy_golden.cpp:180: 1 >= 10\n"
        "[doctest] test cases: 1 | 0 passed | 1 failed\n"
    )
    audit._validate_my9221_schedule_output(
        "missing-data-hold-phase-wait", MY_SCHEDULE_CASE, 1, output
    )
    for old, new in [
        (":169:", ":146:"),
        ("12 >= 50", "12 >= 49"),
        (":180:", ":179:"),
        ("1 >= 10", "2 >= 10"),
    ]:
        with pytest.raises(ValueError):
            audit._validate_my9221_schedule_output(
                "missing-data-hold-phase-wait",
                MY_SCHEDULE_CASE,
                1,
                output.replace(old, new, 1),
            )


@pytest.mark.parametrize(
    "changed",
    [
        "compiler",
        "compiler-launcher",
        "linker",
        "bridge-linker",
        "cache-launcher",
        "python",
        "ninja",
        "loader",
    ],
)
@pytest.mark.parametrize("when", ["before", "after"])
def test_my9221_managed_tool_drift_retains_all_identities_before_rejection(
    tmp_path: Path, changed: str, when: str
) -> None:
    from ci import software_spi_init_proof as proof

    # Diagnostic serialization/control only: constructed identities confer no proof.
    names = (
        "compiler",
        "compiler-launcher",
        "linker",
        "bridge-linker",
        "cache-launcher",
        "python",
        "ninja",
        "loader",
    )
    expected = {
        name: proof.ToolIdentity(
            tmp_path / name, digest(name.encode()), "fixture-version"
        )
        for name in names
    }
    before = dict(expected)
    after = dict(expected)
    drifted = before if when == "before" else after
    drifted[changed] = proof.ToolIdentity(
        tmp_path / changed, digest(b"changed"), "changed-version"
    )
    with pytest.raises(ValueError, match="tool"):
        proof._verify_tool_identities(
            tmp_path, "runtime-0-after", expected, before, after
        )
    record = json.loads((tmp_path / "tool-identities-runtime-0-after.json").read_text())
    assert record["phase"] == "runtime-0-after"
    assert record["changed_names"] == [changed]
    for field, identities in (
        ("expected", expected),
        ("before", before),
        ("after", after),
    ):
        assert set(record[field]) == set(names)
        for name, identity in identities.items():
            assert record[field][name] == {
                "path": str(identity.path),
                "sha256": identity.sha256,
                "version": identity.version,
            }
    assert set(record) == {
        "phase",
        "changed_names",
        "changed_fields",
        "expected",
        "before",
        "after",
    }
    assert record["changed_fields"] == {changed: ["sha256", "version"]}
    assert not (tmp_path / "proof.json").exists()


def test_my9221_managed_bootstrap_tool_identities_retained_without_proof(
    tmp_path: Path,
) -> None:
    from ci import software_spi_init_proof as proof

    names = (
        "compiler",
        "compiler-launcher",
        "linker",
        "bridge-linker",
        "cache-launcher",
        "python",
        "ninja",
        "loader",
    )
    identities = {
        name: proof.ToolIdentity(
            tmp_path / name, digest(name.encode()), "fixture-version"
        )
        for name in names
    }
    proof._verify_tool_identities(
        tmp_path, "bootstrap", identities, identities, identities
    )
    record = json.loads((tmp_path / "tool-identities-bootstrap.json").read_text())
    assert record["changed_names"] == []
    assert record["expected"] == record["before"] == record["after"]
    assert set(record["expected"]) == set(names)
    assert not (tmp_path / "proof.json").exists()


RP2040_MY_KERNEL = "src/platforms/arm/rp/rpcommon/my9221_emitter.h"


def test_my9221_rp2040_elf_mutant_changes_only_kernel_sram_boundary() -> None:
    original = (audit.ROOT / RP2040_MY_KERNEL).read_bytes()
    marker = b"__no_inline_not_in_flash_func(my9221EmitRp)"
    assert original.count(marker) == 2  # mutually exclusive GCC compiler branches
    mutant = audit._rp2040_kernel_mutant(original)
    assert mutant == original.replace(marker, b"__attribute__((noinline)) my9221EmitRp")
    assert mutant.count(b"__no_inline_not_in_flash_func(my9221RpPin") == original.count(
        b"__no_inline_not_in_flash_func(my9221RpPin"
    )
    assert (audit.ROOT / RP2040_MY_KERNEL).read_bytes() == original


@pytest.mark.parametrize(
    "source",
    [
        b"",
        b"__no_inline_not_in_flash_func(my9221EmitRp)",
        b"__no_inline_not_in_flash_func(my9221EmitRp)" * 3,
    ],
)
def test_my9221_rp2040_elf_mutant_rejects_changed_boundary_shape(source: bytes) -> None:
    with pytest.raises(ValueError):
        audit._rp2040_kernel_mutant(source)


@pytest.mark.parametrize(
    "names",
    [
        (),
        ("baseline", "restored"),
        ("kernel-xip-mutant", "baseline", "restored"),
        ("baseline", "kernel-xip-mutant", "restored", "extra"),
    ],
)
def test_my9221_rp2040_elf_rejects_missing_reordered_or_extra_stages(
    names: tuple[str, ...],
) -> None:
    with pytest.raises(ValueError):
        audit._validate_rp2040_elf_stage_sequence(names)


def test_my9221_rp2040_elf_closed_stage_catalog_does_not_supply_target_proof() -> None:
    audit._validate_rp2040_elf_stage_sequence(
        ("baseline", "kernel-xip-mutant", "restored")
    )
    row = next(row for row in REQUIRED_FINDINGS if row["issue"] == 5066)
    assert check_evidence([row], [], audit._current_sources(audit.ROOT))


def test_my9221_rp2040_elf_runner_rejects_missing_checkout_before_process(
    tmp_path: Path,
) -> None:
    with pytest.raises(ValueError, match="input|source|checkout|missing"):
        audit._run_my9221_rp2040_elf_proof(tmp_path)


@pytest.mark.skipif(
    os.environ.get("FASTLED_RUN_MY9221_RP2040_ELF_PROOF") != "1",
    reason="requires three real fixed RP2040 vendor builds; ELF-property only",
)
def test_my9221_rp2040_elf_real_three_stage_acceptance() -> None:
    from ci import software_spi_init_proof as native

    original = (audit.ROOT / RP2040_MY_KERNEL).read_bytes()
    before = native._inputs(audit.ROOT)
    proof = audit._run_my9221_rp2040_elf_proof(audit.ROOT)
    assert proof.scope == "rp2040-native-gpio2-3-elf-sram-property-only"
    assert proof.public_api_scope == "direct-controller-init-setLeds-showLeds"
    assert proof.global_fastled_dispatch_proven is False
    assert proof.complete_executed_argv is False
    assert [stage.name for stage in proof.stages] == [
        "baseline",
        "kernel-xip-mutant",
        "restored",
    ]
    assert proof.workspace.parent == Path.home() / ".clud" / "worktrees"
    assert proof.workspace != audit.ROOT
    for stage in proof.stages:
        assert stage.build_returncode == 0, "compile failure cannot satisfy XIP mutant"
        assert stage.inputs_before == stage.inputs_after
        assert stage.tools_before == stage.tools_after == proof.stages[0].tools_before
        assert stage.configuration == proof.stages[0].configuration
        assert stage.raw_database_scope == "planned-argv-not-complete-executed-receipt"
        for path, expected in stage.artifact_sha256s.items():
            assert digest(Path(path).read_bytes()) == expected
        if stage.name == "kernel-xip-mutant":
            assert stage.property_errors == ["kernel-not-in-sdk-sram"]
        else:
            assert stage.property_errors == []
            assert stage.public_producer_reachable is True
            assert stage.callback_arguments_resolved is True
            assert stage.masked_callee_literal_closure is True
    assert (proof.workspace / RP2040_MY_KERNEL).read_bytes() == original
    assert native._inputs(audit.ROOT) == before
    row = next(row for row in REQUIRED_FINDINGS if row["issue"] == 5066)
    errors = check_evidence([row], [], audit._current_sources(audit.ROOT))
    for configuration in row["required_configurations"]:
        assert any(f"/{configuration}: unresolved" in error for error in errors)


def rp2040_local_elf_capture() -> dict[str, object]:
    # Actual fresh SDK output/raw ELF bytes; these data are parser fixtures,
    # never proof receipts or a source of runtime/configuration acceptance.
    path = audit.ROOT / "ci/fixtures/software_spi_audit/rp2040_local_elf_capture.json"
    return json.loads(path.read_text())


def rp2040_capture_word(capture: dict[str, object], address: int, value: int) -> None:
    for section in capture["section_bytes"].values():
        raw = bytearray.fromhex(section["bytes_hex"])
        offset = address - section["vma"]
        if 0 <= offset <= len(raw) - 4:
            prior = int.from_bytes(raw[offset : offset + 4], "little")
            raw[offset : offset + 4] = value.to_bytes(4, "little")
            section["bytes_hex"] = raw.hex()
            old = f"{prior:08x} \t.word\t0x{prior:08x}"
            new = f"{value:08x} \t.word\t0x{value:08x}"
            for key in ("text_disassembly", "ram_disassembly"):
                capture[key] = capture[key].replace(old, new)
            return
    raise AssertionError("literal fixture address not backed by captured ELF section")


def test_my9221_rp2040_elf_actual_capture_allows_pre_guard_flash_literals_and_flash_lma() -> (
    None
):
    capture = rp2040_local_elf_capture()
    assert "0x200000c0 0x10013140" in capture["elf_layout"]
    assert "10003584" in capture["text_disassembly"]
    assert "#208" in capture["ram_disassembly"]  # SIO/MMIO scalar, not a code pointer.
    assert audit._classify_rp2040_elf_capture(capture) == []
    row = next(row for row in REQUIRED_FINDINGS if row["issue"] == 5066)
    assert check_evidence([row], [], audit._current_sources(audit.ROOT))


@pytest.mark.parametrize(
    "word,error",
    [
        (0x200000C0, "callback-thumb-target-invalid"),
        (0x200000C3, "callback-target-mismatch"),
        (0x100000C1, "callback-target-mismatch"),
    ],
)
def test_my9221_rp2040_elf_callback_words_resolve_exact_thumb_entrypoints(
    word: int, error: str
) -> None:
    capture = rp2040_local_elf_capture()
    rp2040_capture_word(capture, 0x10003584, word)
    assert error in audit._classify_rp2040_elf_capture(capture)


@pytest.mark.parametrize(
    "change,error",
    [
        ("missing-root", "public-producer-unreachable"),
        ("wrong-vptr-slot", "public-controller-vptr-unbound"),
        ("section-end", "sram-symbol-range-missing"),
        ("load-end", "sram-load-range-missing"),
        ("no-executable-load", "sram-load-range-missing"),
        ("unknown-indirect", "unsupported-kernel-shape"),
        ("unknown-opcode", "unsupported-kernel-shape"),
        ("flash-literal-in-kernel", "unsupported-kernel-shape"),
        ("bad-return-token", "unsupported-kernel-shape"),
        ("malformed-memory", "sdk-memory-range-invalid"),
    ],
)
def test_my9221_rp2040_elf_rejects_actual_capture_graph_range_and_instruction_corruption(
    change: str, error: str
) -> None:
    capture = rp2040_local_elf_capture()
    if change == "missing-root":
        capture["text_disassembly"] = capture["text_disassembly"].replace(
            "<my9221_rp2040_public_producer>", "<unused_public_producer>"
        )
    elif change == "wrong-vptr-slot":
        capture["text_disassembly"] = capture["text_disassembly"].replace(
            "str\tr3, [sp, #0]", "str\tr3, [sp, #4]", 1
        )
    elif change == "section-end":
        capture["elf_layout"] = capture["elf_layout"].replace(
            "0150c0 000e34", "0150c0 000100", 1
        )
    elif change == "load-end":
        capture["elf_layout"] = capture["elf_layout"].replace(
            "0x00e34 0x00e34 R E", "0x00100 0x00100 R E", 1
        )
    elif change == "no-executable-load":
        capture["elf_layout"] = capture["elf_layout"].replace(
            "0x00e34 0x00e34 R E", "0x00e34 0x00e34 RW ", 1
        )
    elif change == "unknown-indirect":
        capture["ram_disassembly"] = capture["ram_disassembly"].replace(
            "blx\tr7", "blx\tr0", 1
        )
    elif change == "unknown-opcode":
        capture["ram_disassembly"] = capture["ram_disassembly"].replace(
            "mrs\tr3, PRIMASK", "udf\t#0", 1
        )
    elif change == "flash-literal-in-kernel":
        capture["ram_disassembly"] = capture["ram_disassembly"].replace(
            "movs\tr3, #2", "ldr\tr3, [pc, #0] @ (10000000)", 1
        )
    elif change == "bad-return-token":
        capture["ram_disassembly"] = capture["ram_disassembly"].replace(
            "bx\tr3", "bx\tr0", 1
        )
    else:
        capture["memory_map"] = capture["memory_map"].replace(
            "0x00040000", "0x00000000", 1
        )
    assert error in audit._classify_rp2040_elf_capture(capture)


def test_my9221_rp2040_elf_veneer_cannot_target_unrelated_flash_code() -> None:
    capture = rp2040_local_elf_capture()
    rp2040_capture_word(capture, 0x1001223C, 0x10003001)
    assert "kernel-veneer-target-invalid" in audit._classify_rp2040_elf_capture(capture)


@pytest.mark.parametrize(
    "key,old,new",
    [
        ("ram_disassembly", "bcs.n\t2000014c", "bcs.n\t2000014e"),
        ("ram_disassembly", "bhi.n\t20000134", "bne.n\t20000134"),
        (
            "text_disassembly",
            "bl\t10003266 <fl::CLEDController::showLeds",
            "bl\t10003268 <fl::CLEDController::showLeds",
        ),
        (
            "text_disassembly",
            "bl\t10012230 <___ZN2fl6detailL12my9221EmitRp",
            "bl\t10012232 <___ZN2fl6detailL12my9221EmitRp",
        ),
        ("text_disassembly", "ldr\tr3, [pc, #104]", "ldr\tr3, [pc, #100]"),
        ("text_disassembly", "str\tr6, [sp, #24]", "str\tr6, [sp, #28]"),
        ("ram_disassembly", "47b8      \tblx\tr7", "4780      \tblx\tr7"),
    ],
)
def test_my9221_rp2040_elf_normalization_cannot_hide_actual_target_or_operand_drift(
    key: str, old: str, new: str
) -> None:
    capture = rp2040_local_elf_capture()
    assert old in capture[key], "negative must change a real captured instruction"
    capture[key] = capture[key].replace(old, new, 1)
    assert audit._classify_rp2040_elf_capture(capture), (
        "same symbol annotation cannot authorize wrong raw target/opcode/operand"
    )


def test_my9221_rp2040_elf_unused_producer_symbol_cannot_replace_actual_main_edge() -> (
    None
):
    capture = rp2040_local_elf_capture()
    old = "bl\t10003304 <my9221_rp2040_public_producer>"
    assert old in capture["text_disassembly"]
    capture["text_disassembly"] = capture["text_disassembly"].replace(
        old, "bl\t10003306 <my9221_rp2040_public_producer>", 1
    )
    assert "public-producer-unreachable" in audit._classify_rp2040_elf_capture(capture)


def rp2040_capture_instruction(
    capture: dict[str, object],
    key: str,
    address: int,
    words: str,
    old_operand: str,
    new_operand: str,
) -> None:
    import re

    pattern = rf"^{address:x}:\s+([0-9a-f ]+?)\s+\t(.+)$"
    match = re.search(pattern, capture[key], re.MULTILINE)
    assert match and old_operand in match[2]
    original = b"".join(
        int(word, 16).to_bytes(2, "little") for word in match[1].split()
    )
    replacement = b"".join(
        int(word, 16).to_bytes(2, "little") for word in words.split()
    )
    assert len(original) == len(replacement)
    changed = f"{address:x}:\\t{words} \\t{match[2].replace(old_operand, new_operand, 1)}".replace(
        "\\t", "\t"
    )
    capture[key] = capture[key][: match.start()] + changed + capture[key][match.end() :]
    for section in capture["section_bytes"].values():
        offset = address - section["vma"]
        raw = bytearray.fromhex(section["bytes_hex"])
        if 0 <= offset <= len(raw) - len(original):
            assert raw[offset : offset + len(original)] == original
            raw[offset : offset + len(original)] = replacement
            section["bytes_hex"] = raw.hex()
            return
    raise AssertionError("instruction not in actual captured ELF storage")


@pytest.mark.parametrize(
    "key,address,words,old,new,error",
    [
        (
            "text_disassembly",
            0x1000338A,
            "f7ff ffbc",
            "bl\t10003304",
            "bl\t10003306",
            "public-producer-unreachable",
        ),
        (
            "ram_disassembly",
            0x2000014E,
            "d2fe",
            "bcs.n\t2000014c",
            "bcs.n\t2000014e",
            "unsupported-kernel-shape",
        ),
        (
            "text_disassembly",
            0x10003552,
            "9607",
            "str\tr6, [sp, #24]",
            "str\tr6, [sp, #28]",
            "callback-target-mismatch",
        ),
    ],
)
def test_my9221_rp2040_elf_consistent_bad_target_or_slot_cannot_hide_behind_annotation(
    key: str, address: int, words: str, old: str, new: str, error: str
) -> None:
    capture = rp2040_local_elf_capture()
    rp2040_capture_instruction(capture, key, address, words, old, new)
    assert error in audit._classify_rp2040_elf_capture(capture)


def test_my9221_rp2040_elf_decoy_named_writer_target_cannot_certify_unused_kernel() -> (
    None
):
    capture = rp2040_local_elf_capture()
    old = "bl\t10012230 <___ZN2fl6detailL12my9221EmitRpEPKtjNS0_12My9221RpPinsE_veneer>"
    new = "bl\t10003266 <decoy_my9221EmitRp>"
    rp2040_capture_instruction(
        capture, "text_disassembly", 0x1000356A, "f7ff fe7c", old, new
    )
    capture["text_function_headers"] += "10003266 <decoy_my9221EmitRp>:\n"
    assert "kernel-veneer-target-invalid" in audit._classify_rp2040_elf_capture(capture)


@pytest.mark.parametrize("kind", ["section", "load"])
def test_my9221_rp2040_elf_rejects_ambiguous_executable_extents(kind: str) -> None:
    capture = rp2040_local_elf_capture()
    lines = capture["elf_layout"].splitlines()
    needle = (
        ".data             PROGBITS" if kind == "section" else "0x200000c0 0x10013140"
    )
    matches = [line for line in lines if needle in line]
    assert len(matches) == 1
    capture["elf_layout"] += matches[0] + "\n"
    assert "sram-extent-ambiguous" in audit._classify_rp2040_elf_capture(capture)


@pytest.mark.parametrize("field", ["tools", "sdk", "configuration", "environment"])
def test_my9221_rp2040_elf_frozen_profile_rejects_any_identity_drift(
    tmp_path: Path, field: str
) -> None:
    import copy

    from ci import software_spi_my9221_rp2040_proof as vendor

    # Frozen-state comparison only: hand-written state never certifies a build.
    expected = {
        "tools": {"compiler": "initial"},
        "sdk": {"core/header.h": "initial"},
        "configuration": {"cxx_flags": ["-mcpu=cortex-m0plus"]},
        "environment": {"PATH": "hash-only"},
    }
    observed = copy.deepcopy(expected)
    observed[field] = {"drift": "changed"}
    with pytest.raises(ValueError, match="identity|profile|changed"):
        vendor._verify_profile_state(tmp_path, "mutant-before", expected, observed)
    diagnostic = json.loads((tmp_path / "profile-mutant-before.json").read_text())
    assert diagnostic["changed_fields"] == [field]
    assert diagnostic["expected"] == expected
    assert diagnostic["observed"] == observed
    assert not (tmp_path / "proof.json").exists()


@dataclass(slots=True)
class Rp2040StagedProject:
    workspace: Path
    project: Path
    expected: dict[str, str]


def rp2040_staged_project_fixture(
    tmp_path: Path, unsafe: bool = False
) -> Rp2040StagedProject:
    from ci import software_spi_my9221_rp2040_runner as vendor
    from ci.compiler.source_manager import generate_main_cpp

    workspace, project = tmp_path / "workspace", tmp_path / "project"
    source = workspace / "src/fl/model.h"
    source.parent.mkdir(parents=True)
    source.write_bytes(b"actual fixture source identity")
    example = workspace / "examples/AutoResearch/AutoResearch.ino"
    example.parent.mkdir(parents=True)
    example.write_bytes(vendor.PRODUCER)
    metadata = (audit.ROOT / "library.json").read_bytes()
    (workspace / "library.json").write_bytes(metadata)
    staged = project / "lib/FastLED/fl/model.h"
    staged.parent.mkdir(parents=True)
    if unsafe:
        try:
            staged.symlink_to(source)
        except OSError as error:
            pytest.skip(f"real symlink capability unavailable: {error}")
    else:
        staged.write_bytes(source.read_bytes())
    (project / "lib/FastLED/library.json").write_bytes(metadata)
    sketch = project / "src/sketch/AutoResearch.ino"
    sketch.parent.mkdir(parents=True)
    sketch.write_bytes(vendor.PRODUCER)
    (project / "src/main.cpp").write_bytes(
        generate_main_cpp(["AutoResearch.ino"]).encode()
    )
    return Rp2040StagedProject(
        workspace, project, {"src/fl/model.h": digest(source.read_bytes())}
    )


@pytest.mark.parametrize(
    "kind",
    [
        "extra-library",
        "extra-project",
        "extra-sketch",
        "source-symlink",
        "directory-symlink",
    ],
)
def test_my9221_rp2040_elf_rejects_extra_or_unsafe_staged_inputs(
    tmp_path: Path, kind: str
) -> None:
    from ci import software_spi_my9221_rp2040_runner as vendor

    fixture = rp2040_staged_project_fixture(tmp_path, kind == "source-symlink")
    workspace, project, expected = fixture.workspace, fixture.project, fixture.expected
    if kind == "extra-library":
        (project / "lib/FastLED/injected.cpp").write_text("#error extra library source")
    elif kind == "extra-project":
        (project / "src/injected.cpp").write_text("#error extra project source")
    elif kind == "extra-sketch":
        (project / "src/sketch/Injected.h").write_text("#error extra sketch header")
    elif kind == "directory-symlink":
        try:
            (project / "src/sketch/external").symlink_to(
                workspace / "src", target_is_directory=True
            )
        except OSError as error:
            pytest.skip(f"real directory symlink capability unavailable: {error}")
    with pytest.raises(ValueError, match="staged|unsafe|universe|source"):
        vendor._check_staged_sources(workspace, expected, project)


def test_my9221_rp2040_elf_exact_staged_project_control(tmp_path: Path) -> None:
    from ci import software_spi_my9221_rp2040_runner as vendor

    fixture = rp2040_staged_project_fixture(tmp_path)
    workspace, project, expected = fixture.workspace, fixture.project, fixture.expected
    vendor._check_staged_sources(workspace, expected, project)


def test_my9221_rp2040_elf_child_environment_excludes_secrets_and_binds_reviewed_inputs(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from ci import software_spi_my9221_rp2040_runner as vendor

    for name in (
        "GITHUB_TOKEN",
        "AWS_SECRET_ACCESS_KEY",
        "UNREVIEWED_PRIVATE_VALUE",
        "HTTPS_PROXY",
    ):
        monkeypatch.setenv(name, "secret-must-not-inherit-or-fingerprint")
    monkeypatch.setenv("CXXFLAGS", "-DREVIEWED_PROFILE_FLAG=1")
    environment = vendor._environment(tmp_path)
    assert all(
        name not in environment
        for name in (
            "GITHUB_TOKEN",
            "AWS_SECRET_ACCESS_KEY",
            "UNREVIEWED_PRIVATE_VALUE",
            "HTTPS_PROXY",
        )
    )
    assert environment["CXXFLAGS"] == "-DREVIEWED_PROFILE_FLAG=1"
    assert environment["UV_PROJECT_ENVIRONMENT"] == sys.prefix
    assert environment["PYTHONPATH"] == str(tmp_path.resolve())
    assert environment["UV_NO_SYNC"] == "1"
    assert environment["FBUILD_DEV_MODE"] == "1"


@pytest.mark.parametrize(
    "changed", ["pid", "start", "cache", "running-image", "health-pid"]
)
def test_my9221_rp2040_elf_serving_daemon_drift_cannot_be_disk_binary_proof(
    tmp_path: Path, changed: str
) -> None:
    import copy

    from ci import software_spi_my9221_rp2040_runner as vendor

    # Identity comparison fixtures only; no hand-written daemon record can
    # authenticate a build or clear target evidence.
    baseline = {
        "pid": 42,
        "start_ticks": "123",
        "started_at": "fixture-start",
        "cache_identity": "fixture-cache",
        "image_sha256": "a" * 64,
        "health_pid": 42,
        "image_pid": 42,
    }
    observed = copy.deepcopy(baseline)
    observed[
        {
            "pid": "pid",
            "start": "start_ticks",
            "cache": "cache_identity",
            "running-image": "image_sha256",
            "health-pid": "health_pid",
        }[changed]
    ] = "drift"
    with pytest.raises(ValueError, match="daemon|engine|identity"):
        vendor._verify_serving_engine(tmp_path, "stage-after", baseline, observed)
    assert not (tmp_path / "proof.json").exists()


def test_my9221_rp2040_elf_inspection_targets_retained_immutable_elf(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from ci import software_spi_my9221_rp2040_runner as vendor

    saved = tmp_path / "saved"
    saved.mkdir()
    (saved / "firmware.elf").write_bytes(b"retained ELF identity")
    info = {
        "prog_path": str(tmp_path / "mutable/firmware.elf"),
        "objdump_path": "/bound/sdk/objdump",
        "readelf_path": "/bound/sdk/readelf",
    }
    commands = []

    def stop_at_inspection(
        command: list[str], *_args: object, **_kwargs: object
    ) -> None:
        commands.append(command)
        raise RuntimeError("inspection stopped by orchestration-negative fixture")

    monkeypatch.setattr(vendor.native, "_run", stop_at_inspection)
    with pytest.raises(RuntimeError, match="inspection stopped"):
        vendor._capture(tmp_path, saved, info, {})
    assert commands[0][-1] == str(saved / "firmware.elf")


def test_my9221_rp2040_elf_daemon_startup_env_filters_credentials_and_binds_build_flags() -> (
    None
):
    from ci import software_spi_my9221_rp2040_runner as vendor

    raw = (
        b"GITHUB_TOKEN=never-hash-this-secret\0UNKNOWN_PRIVATE_VALUE=also-secret\0"
        b"HTTPS_PROXY=https://user:password@example.invalid\0"
        b"PATH=/bound/tool/bin\0CXXFLAGS=-DSTARTUP_PROFILE=1\0FBUILD_DEV_MODE=1\0"
    )
    observed = vendor._startup_environment_hashes(raw)
    assert observed == {
        "PATH": digest(b"/bound/tool/bin"),
        "CXXFLAGS": digest(b"-DSTARTUP_PROFILE=1"),
        "FBUILD_DEV_MODE": digest(b"1"),
    }
    assert digest(b"never-hash-this-secret") not in observed.values()
    assert digest(b"https://user:password@example.invalid") not in observed.values()


def test_my9221_rp2040_elf_daemon_startup_compiler_environment_drift_rejects(
    tmp_path: Path,
) -> None:
    from ci import software_spi_my9221_rp2040_runner as vendor

    baseline = {
        "pid": 42,
        "health_pid": 42,
        "image_pid": 42,
        "startup_environment": vendor._startup_environment_hashes(
            b"CXXFLAGS=-DPROFILE=1\0"
        ),
    }
    observed = {
        **baseline,
        "startup_environment": vendor._startup_environment_hashes(
            b"CXXFLAGS=-DPROFILE=2\0"
        ),
    }
    assert baseline["startup_environment"] != observed["startup_environment"]
    with pytest.raises(ValueError, match="daemon|engine|identity"):
        vendor._verify_serving_engine(tmp_path, "after", baseline, observed)
    assert not (tmp_path / "proof.json").exists()


@pytest.mark.skipif(
    sys.platform != "linux",
    reason="serving-image discovery is the explicitly Linux /proc profile",
)
def test_my9221_rp2040_elf_daemon_version_comes_from_verified_serving_identity(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from ci import software_spi_my9221_rp2040_runner as vendor

    # Tool-discovery orchestration only; mock observations cannot authenticate
    # a vendor build or publish any proof.
    info = {"aliases": {}}
    for key in (
        "cc_path",
        "cxx_path",
        "ar_path",
        "objdump_path",
        "readelf_path",
        "nm_path",
    ):
        path = tmp_path / key
        path.write_bytes(key.encode())
        info[key] = str(path)
    linker = tmp_path / "ld"
    linker.write_bytes(b"linker fixture identity")
    info["aliases"]["ld"] = str(linker)
    commands = []
    health_calls = []

    def version_command(
        command: list[str], *_args: object, **_kwargs: object
    ) -> NativeProcessResult:
        assert Path(command[0]).name != "fbuild-daemon", (
            "daemon CLI has no --version; never start/probe the service executable"
        )
        commands.append(command)
        return NativeProcessResult(0, "fixture tool version")

    def observed_daemon(
        _workspace: Path, _saved: Path, phase: str, _environment: dict, tools: dict
    ) -> dict:
        health_calls.append(phase)
        return {
            "version": "2.5.52",
            "build_id": "verified-health-build",
            "image_sha256": tools["fbuild-daemon"].sha256,
        }

    monkeypatch.setattr(vendor.native, "_run", version_command)
    monkeypatch.setattr(vendor, "_daemon_identity", observed_daemon)
    tools = vendor._tool_identities(
        info, tmp_path, {"PATH": os.environ["PATH"]}, tmp_path / "saved"
    )
    assert commands and health_calls
    assert tools["fbuild-daemon"].version == "2.5.52 (verified-health-build)"
    assert not (tmp_path / "saved/proof.json").exists()


@pytest.mark.skipif(
    sys.platform != "linux",
    reason="serving-image discovery is the explicitly Linux /proc profile",
)
def test_my9221_rp2040_elf_health_version_never_waives_running_image_identity(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from ci import software_spi_init_proof as native
    from ci import software_spi_my9221_rp2040_runner as vendor

    # A matching reported version cannot replace the independently observed
    # /proc loaded-image SHA check. This is a parser negative, not a real API receipt.
    observation = {
        "version": "2.5.52",
        "build_id": "matching-version",
        "image_sha256": "b" * 64,
    }
    monkeypatch.setattr(
        vendor.native,
        "_run",
        lambda *_args, **_kwargs: NativeProcessResult(0, json.dumps(observation)),
    )
    tools = {
        "fbuild-daemon": native.ToolIdentity(tmp_path / "daemon", "a" * 64, "2.5.52")
    }
    with pytest.raises(ValueError, match="image|binary"):
        vendor._daemon_identity(tmp_path, tmp_path / "saved", "bootstrap", {}, tools)
    assert not (tmp_path / "saved/proof.json").exists()


def test_my9221_rp2040_elf_build_session_hashes_before_ensure_and_captures_immediate_after(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from ci import software_spi_my9221_rp2040_runner as vendor

    # Orchestration only: no fake SDK execution/property proof is supplied.
    events = []
    engine = {
        "pid": 42,
        "health_pid": 42,
        "image_pid": 42,
        "start_ticks": "initial",
        "image_sha256": "same",
        "startup_environment": {"CXXFLAGS": "same"},
    }
    monkeypatch.setattr(
        vendor,
        "_state",
        lambda *_args, **_kwargs: events.append("hash") or {"fixed": "profile"},
    )
    monkeypatch.setattr(
        vendor,
        "_ensure_stage_engine",
        lambda *_args, **_kwargs: events.append("ensure"),
        raising=False,
    )
    monkeypatch.setattr(
        vendor,
        "_daemon_identity",
        lambda *_args, **_kwargs: events.append("capture") or engine,
    )
    monkeypatch.setattr(
        vendor,
        "_compile",
        lambda *_args, **_kwargs: events.append("build") or {"same": "info"},
    )
    monkeypatch.setattr(
        vendor.native,
        "_verify_inputs",
        lambda *_args, **_kwargs: events.append("inputs") or {},
    )
    monkeypatch.setattr(
        vendor,
        "_check_staged_sources",
        lambda *_args, **_kwargs: events.append("staged"),
    )
    monkeypatch.setattr(
        vendor.property_check, "_verify_profile_state", lambda *_args, **_kwargs: None
    )
    vendor._build_session(
        tmp_path,
        tmp_path / "saved",
        {},
        {},
        (),
        {"fixed": "profile"},
        engine,
        {"same": "info"},
        {},
    )
    assert events == [
        "hash",
        "ensure",
        "capture",
        "build",
        "capture",
        "hash",
        "inputs",
        "staged",
    ]


def test_my9221_rp2040_elf_new_verified_instance_between_builds_preserves_profile(
    tmp_path: Path,
) -> None:
    from ci import software_spi_my9221_rp2040_runner as vendor

    baseline = {
        "pid": 42,
        "health_pid": 42,
        "image_pid": 42,
        "start_ticks": "old",
        "started_at": 1,
        "exe_device": 1,
        "exe_inode": 10,
        "image_sha256": "same",
        "version": "same",
        "cache_identity": "same",
        "startup_environment": {"CXXFLAGS": "same"},
    }
    later = {
        **baseline,
        "pid": 43,
        "health_pid": 43,
        "image_pid": 43,
        "start_ticks": "new",
        "started_at": 2,
        "exe_inode": 11,
    }
    vendor._verify_engine_profile(tmp_path, "next-build", baseline, later)
    for key in (
        "image_sha256",
        "cache_identity",
        "startup_environment",
        "not_a_pid_field",
    ):
        bad = {**later, key: "changed"}
        with pytest.raises(ValueError, match="engine|profile|identity"):
            vendor._verify_engine_profile(tmp_path, "next-build", baseline, bad)
    assert not (tmp_path / "proof.json").exists()


@pytest.mark.parametrize("field", ["pid", "image_sha256", "startup_environment"])
def test_my9221_rp2040_elf_build_window_never_accepts_instance_or_provider_drift(
    tmp_path: Path, field: str
) -> None:
    from ci import software_spi_my9221_rp2040_runner as vendor

    baseline = {
        "pid": 42,
        "health_pid": 42,
        "image_pid": 42,
        "start_ticks": "same",
        "image_sha256": "same",
        "startup_environment": {"CXXFLAGS": "same"},
    }
    with pytest.raises(ValueError, match="engine|identity"):
        vendor._verify_serving_engine(
            tmp_path, "build-after", baseline, {**baseline, field: "changed"}
        )


def test_my9221_rp2040_elf_unqualified_online_engine_blocks_normal_ensure(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from ci import software_spi_my9221_rp2040_runner as vendor

    expected = {
        "pid": 42,
        "health_pid": 42,
        "image_pid": 42,
        "image_sha256": "approved",
        "startup_environment": {"CXXFLAGS": "same"},
    }
    monkeypatch.setattr(
        vendor,
        "_probe_existing_engine",
        lambda *_args, **_kwargs: {**expected, "image_sha256": "unqualified"},
        raising=False,
    )
    commands = []
    monkeypatch.setattr(
        vendor.native, "_run", lambda *args, **_kwargs: commands.append(args)
    )
    with pytest.raises(ValueError, match="engine|profile|identity"):
        vendor._ensure_stage_engine(tmp_path, tmp_path / "saved", {}, {}, expected)
    assert commands == [], "normal ensure cannot replace an unqualified online engine"


@pytest.mark.skipif(
    sys.platform != "linux",
    reason="actual serving/client-discovery profile uses provisioned Linux venv",
)
def test_my9221_rp2040_elf_normal_ensure_uses_same_bound_uv_launch_as_wrapper(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    import shutil

    from ci import software_spi_init_proof as native
    from ci import software_spi_my9221_rp2040_runner as vendor

    environment = vendor._environment(tmp_path)
    paths = {
        "python": Path(sys.executable).absolute(),
        "fbuild-daemon": Path(sys.executable).absolute().parent / "fbuild-daemon",
        "fbuild-extension": Path(
            __import__("fbuild._native", fromlist=["__file__"]).__file__
        ),
        "uv": Path(shutil.which("uv", path=environment["PATH"])),
    }
    tools = {
        name: native.ToolIdentity(path, digest(path.read_bytes()), "fixture identity")
        for name, path in paths.items()
    }
    monkeypatch.setattr(
        vendor, "_probe_existing_engine", lambda *_args, **_kwargs: None
    )
    commands = []
    monkeypatch.setattr(
        vendor.native,
        "_run",
        lambda command, *_args, **_kwargs: (
            commands.append(command) or NativeProcessResult(0, "{}")
        ),
    )
    vendor._ensure_stage_engine(tmp_path, tmp_path / "saved", environment, tools, {})
    assert commands[0][:5] == [
        str(tools["uv"].path),
        "run",
        "--no-sync",
        str(tools["python"].path),
        "-c",
    ]
    assert not (tmp_path / "saved/proof.json").exists()


def test_my9221_rp2040_irq_mutant_changes_only_saved_state_restore_operand() -> None:
    source = (audit.ROOT / "src/platforms/my9221.h").read_bytes()
    marker = b"restore_interrupts(mSaved);"
    assert source.count(marker) == 1
    assert audit._rp2040_irq_mutant(source) == source.replace(
        marker, b"restore_interrupts(0);"
    )
    assert (audit.ROOT / "src/platforms/my9221.h").read_bytes() == source


@pytest.mark.parametrize(
    "source", [b"", b"restore_interrupts(mSaved);\nrestore_interrupts(mSaved);"]
)
def test_my9221_rp2040_irq_mutant_rejects_ambiguous_or_missing_restore(
    source: bytes,
) -> None:
    with pytest.raises(ValueError, match="restore|boundary|source"):
        audit._rp2040_irq_mutant(source)


def rp2040_irq_capture(profile: str = "baseline") -> dict[str, object]:
    path = audit.ROOT / "ci/fixtures/software_spi_audit/rp2040_irq_elf_captures.json"
    return json.loads(path.read_text())[profile]


def rp2040_irq_replace_span(
    capture: dict[str, object], start: int, end: int, replacement: str
) -> None:
    import re

    pattern = r"^([0-9a-f]+):\s+([0-9a-f ]+?)\s+\t([^\n]+)\n?"
    selected = [
        match
        for match in re.finditer(pattern, capture["ram_disassembly"], re.MULTILINE)
        if start <= int(match[1], 16) < end
    ]
    assert selected and int(selected[0][1], 16) == start
    old = b"".join(
        int(word, 16).to_bytes(2, "little")
        for match in selected
        for word in match[2].split()
    )
    revised = list(re.finditer(pattern, replacement, re.MULTILINE))
    new = b"".join(
        int(word, 16).to_bytes(2, "little")
        for match in revised
        for word in match[2].split()
    )
    assert len(old) == len(new) == end - start
    capture["ram_disassembly"] = (
        capture["ram_disassembly"][: selected[0].start()]
        + replacement
        + capture["ram_disassembly"][selected[-1].end() :]
    )
    for section in capture["section_bytes"].values():
        raw = bytearray.fromhex(section["bytes_hex"])
        offset = start - section["vma"]
        if 0 <= offset <= len(raw) - len(old):
            assert raw[offset : offset + len(old)] == old
            raw[offset : offset + len(old)] = new
            section["bytes_hex"] = raw.hex()
            return
    raise AssertionError("IRQ control must alter actual captured ELF storage")


def test_my9221_rp2040_irq_actual_baseline_has_nonempty_saved_state_returns_for_both_entries() -> (
    None
):
    result = audit._classify_rp2040_irq_capture(rp2040_irq_capture())
    assert result.errors == []
    for entry in (0, 1):
        assert result.normal_returns[entry], "no vacuous success for an entry PRIMASK"
        for outcome in result.normal_returns[entry]:
            assert outcome.entry_primask == outcome.exit_primask == entry
            assert outcome.sp_delta == 0
            assert outcome.return_token == "entry-lr"
            assert outcome.final_parked is True
    row = next(row for row in REQUIRED_FINDINGS if row["issue"] == 5066)
    assert check_evidence([row], [], audit._current_sources(audit.ROOT))


def test_my9221_rp2040_irq_actual_source_mutant_reaches_intended_semantic_counterexample() -> (
    None
):
    result = audit._classify_rp2040_irq_capture(
        rp2040_irq_capture("unconditional_enable")
    )
    assert result.errors == ["saved-primask-restoration-lost"], (
        "generic placement/hash rejection is not the control"
    )
    assert result.normal_returns[0] and result.normal_returns[1]
    assert any(
        outcome.entry_primask == 1
        and outcome.exit_primask == 0
        and outcome.sp_delta == 0
        and outcome.return_token == "entry-lr"
        for outcome in result.counterexamples
    )


def test_my9221_rp2040_irq_mrs_reads_current_primask_after_cpsid() -> None:
    capture = rp2040_irq_capture()
    rp2040_irq_replace_span(
        capture,
        0x20000102,
        0x20000108,
        "20000102:\tb672      \tcpsid\ti\n20000104:\tf3ef 8310 \tmrs\tr3, PRIMASK\n",
    )
    result = audit._classify_rp2040_irq_capture(capture)
    assert "saved-primask-restoration-lost" in result.errors
    assert any(
        outcome.entry_primask == 0 and outcome.exit_primask == 1
        for outcome in result.counterexamples
    ), "MRS after CPSID cannot mint an entry-P0 token"


def test_my9221_rp2040_irq_any_gpio_after_restore_rejects_even_if_entry_disabled() -> (
    None
):
    capture = rp2040_irq_capture()
    rp2040_irq_replace_span(
        capture,
        0x2000011E,
        0x2000012A,
        "2000011e:\t9b09      \tldr\tr3, [sp, #36]\n"
        "20000120:\tf383 8810 \tmsr\tPRIMASK, r3\n"
        "20000124:\t9b05      \tldr\tr3, [sp, #20]\n"
        "20000126:\t4798      \tblx\tr3\n"
        "20000128:\t47b8      \tblx\tr7\n",
    )
    result = audit._classify_rp2040_irq_capture(capture)
    assert "irq-gpio-after-restore" in result.errors


@pytest.mark.parametrize("slot,opcode", [(8, "9b02"), (12, "9b03")])
def test_my9221_rp2040_irq_terminal_hi_or_toggle_invalidates_prior_low_evidence(
    slot: int, opcode: str
) -> None:
    capture = rp2040_irq_capture()
    rp2040_capture_instruction(
        capture,
        "ram_disassembly",
        0x2000011E,
        opcode,
        "ldr\tr3, [sp, #20]",
        f"ldr\tr3, [sp, #{slot}]",
    )
    result = audit._classify_rp2040_irq_capture(capture)
    assert "irq-final-park-suffix-missing" in result.errors


@pytest.mark.parametrize(
    "address,opcode,old,new,error",
    [
        (
            0x20000108,
            "9009",
            "str\tr3, [sp, #36]",
            "str\tr0, [sp, #36]",
            "irq-restoration-unresolved",
        ),
        (
            0x20000108,
            "600b",
            "str\tr3, [sp, #36]",
            "str\tr3, [r1, #0]",
            "irq-stack-alias-unresolved",
        ),
        (0x2000010A, "4780", "blx\tr7", "blx\tr0", "irq-callback-target-unresolved"),
        (0x20000132, "4700", "bx\tr3", "bx\tr0", "irq-return-token-unresolved"),
        (0x20000134, "de00", "movs\tr5, #16", "udf\t#0", "irq-unsupported-instruction"),
    ],
)
def test_my9221_rp2040_irq_consistent_unknown_or_clobbered_semantics_reject(
    address: int, opcode: str, old: str, new: str, error: str
) -> None:
    capture = rp2040_irq_capture()
    rp2040_capture_instruction(capture, "ram_disassembly", address, opcode, old, new)
    assert error in audit._classify_rp2040_irq_capture(capture).errors


def test_my9221_rp2040_irq_no_normal_return_cannot_pass_vacuously() -> None:
    capture = rp2040_irq_capture()
    rp2040_capture_instruction(
        capture,
        "ram_disassembly",
        0x20000132,
        "e7fe",
        "bx\tr3",
        "b.n\t20000132 <fl::detail::my9221EmitRp(unsigned short const*, unsigned int, fl::detail::My9221RpPins)+0x4a>",
    )
    result = audit._classify_rp2040_irq_capture(capture)
    assert "irq-normal-return-unproved" in result.errors
    assert not result.normal_returns.get(0) and not result.normal_returns.get(1)


@pytest.mark.parametrize(
    "operation,register,words",
    [
        ("mrs", "sp", "f3ef 8d10"),
        ("mrs", "pc", "f3ef 8f10"),
        ("msr", "sp", "f38d 8810"),
        ("msr", "pc", "f38f 8810"),
    ],
)
def test_my9221_rp2040_irq_special_register_forms_reject_during_decode(
    operation: str, register: str, words: str
) -> None:
    capture = rp2040_irq_capture()
    if operation == "mrs":
        rp2040_capture_instruction(
            capture,
            "ram_disassembly",
            0x20000102,
            words,
            "mrs\tr3, PRIMASK",
            f"mrs\t{register}, PRIMASK",
        )
    else:
        rp2040_capture_instruction(
            capture,
            "ram_disassembly",
            0x20000126,
            words,
            "msr\tPRIMASK, r3",
            f"msr\tPRIMASK, {register}",
        )
    result = audit._classify_rp2040_irq_capture(capture)
    assert result.authentication_errors == []
    assert result.decode_errors == ["irq-unsupported-register"]
    assert not result.counterexamples, (
        "unsupported SP/PC semantics cannot be concrete IRQ evidence"
    )
    assert not result.normal_returns.get(0) and not result.normal_returns.get(1)


def test_my9221_rp2040_irq_managed_dispatch_uses_semantics_not_placement_shape(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from ci import software_spi_my9221_rp2040_runner as vendor

    monkeypatch.setattr(
        vendor.property_check,
        "classify_capture",
        lambda *_args: pytest.fail(
            "IRQ mutant cannot be screened by placement RAW shape"
        ),
    )
    assessed = vendor._assess_property(
        rp2040_irq_capture("unconditional_enable"),
        vendor._PropertyKind.IRQ,
        "unconditional-enable-mutant",
    )
    assert assessed.irq_analysis is not None
    assert assessed.irq_analysis.authentication_errors == []
    assert assessed.irq_analysis.decode_errors == []
    assert assessed.errors == ["saved-primask-restoration-lost"]
    assert any(
        outcome.entry_primask == 1 and outcome.exit_primask == 0
        for outcome in assessed.irq_analysis.counterexamples
    )


def test_my9221_rp2040_irq_stage_validator_requires_actual_intended_analysis() -> None:
    baseline = audit._classify_rp2040_irq_capture(rp2040_irq_capture())
    mutant = audit._classify_rp2040_irq_capture(
        rp2040_irq_capture("unconditional_enable")
    )
    audit._validate_rp2040_irq_stage_analysis("baseline", baseline)
    audit._validate_rp2040_irq_stage_analysis("restored", baseline)
    audit._validate_rp2040_irq_stage_analysis("unconditional-enable-mutant", mutant)
    for stage, result in [
        ("baseline", mutant),
        ("restored", mutant),
        ("unconditional-enable-mutant", baseline),
        ("unknown", baseline),
    ]:
        with pytest.raises(ValueError):
            audit._validate_rp2040_irq_stage_analysis(stage, result)
    row = next(row for row in REQUIRED_FINDINGS if row["issue"] == 5066)
    assert check_evidence([row], [], audit._current_sources(audit.ROOT))


@pytest.mark.parametrize(
    "corruption",
    [
        "authentication",
        "decode",
        "empty-returns",
        "missing-counterexample",
        "wrong-counterexample",
        "counterexample-not-in-returns",
        "sp",
        "lr",
        "park",
        "unknown-mask",
        "extra-error",
    ],
)
def test_my9221_rp2040_irq_stage_validator_rejects_incomplete_or_mislabelled_control(
    corruption: str,
) -> None:
    from dataclasses import replace

    result = audit._classify_rp2040_irq_capture(
        rp2040_irq_capture("unconditional_enable")
    )
    if corruption == "authentication":
        result = replace(result, authentication_errors=["missing actual route"])
    elif corruption == "decode":
        result = replace(result, decode_errors=["unknown opcode"])
    elif corruption == "empty-returns":
        result = replace(result, normal_returns={0: [], 1: []})
    elif corruption == "missing-counterexample":
        result = replace(result, counterexamples=[])
    elif corruption == "wrong-counterexample":
        result = replace(
            result, counterexamples=[replace(result.counterexamples[0], exit_primask=1)]
        )
    elif corruption == "counterexample-not-in-returns":
        witness = result.counterexamples[0]
        result = replace(
            result,
            counterexamples=[replace(witness, restore_pc=witness.restore_pc + 2)],
        )
    elif corruption == "extra-error":
        result = replace(result, errors=result.errors + ["irq-restoration-unresolved"])
    else:
        field, value = {
            "sp": ("sp_delta", 4),
            "lr": ("return_token", "unknown"),
            "park": ("final_parked", False),
            "unknown-mask": ("exit_primask", None),
        }[corruption]
        returns = {
            entry: list(outcomes) for entry, outcomes in result.normal_returns.items()
        }
        returns[1][0] = replace(returns[1][0], **{field: value})
        result = replace(result, normal_returns=returns)
    with pytest.raises(ValueError):
        audit._validate_rp2040_irq_stage_analysis("unconditional-enable-mutant", result)


def test_my9221_rp2040_irq_managed_runner_rejects_missing_inputs_before_process(
    tmp_path: Path,
) -> None:
    with pytest.raises(ValueError, match="source|input|missing|checkout"):
        audit._run_my9221_rp2040_irq_proof(tmp_path)


@pytest.mark.skipif(
    os.environ.get("FASTLED_RUN_MY9221_RP2040_IRQ_PROOF") != "1",
    reason="requires three actual fixed RP2040 SDK builds; static IRQ property only",
)
def test_my9221_rp2040_irq_managed_real_three_stage_acceptance() -> None:
    from ci import software_spi_init_proof as native

    guard = "src/platforms/my9221.h"
    original = (audit.ROOT / guard).read_bytes()
    expected = {
        "baseline": original,
        "unconditional-enable-mutant": audit._rp2040_irq_mutant(original),
        "restored": original,
    }
    before = native._inputs(audit.ROOT)
    proof = audit._run_my9221_rp2040_irq_proof(audit.ROOT)
    assert proof.scope == "rp2040-native-gpio2-3-saved-primask-property-only"
    assert proof.public_api_scope == "direct-controller-init-setLeds-showLeds"
    assert proof.global_fastled_dispatch_proven is False
    assert proof.complete_executed_argv is False
    assert [stage.name for stage in proof.stages] == list(expected)
    assert proof.workspace.parent == Path.home() / ".clud" / "worktrees"
    for stage in proof.stages:
        assert stage.build_returncode == 0
        assert stage.inputs_before == stage.inputs_after
        assert stage.inputs_before[guard] == digest(expected[stage.name])
        assert stage.tools_before == stage.tools_after == proof.stages[0].tools_before
        assert stage.configuration == proof.stages[0].configuration
        assert stage.serving_engine_before == stage.serving_engine_after
        assert stage.raw_database_scope == "planned-argv-not-complete-executed-receipt"
        assert stage.irq_analysis is not None
        audit._validate_rp2040_irq_stage_analysis(stage.name, stage.irq_analysis)
        for path, sha256 in stage.artifact_sha256s.items():
            assert digest(Path(path).read_bytes()) == sha256
        assert stage.public_producer_reachable and stage.callback_arguments_resolved
        assert stage.masked_callee_literal_closure
    assert (proof.workspace / guard).read_bytes() == original
    assert native._inputs(audit.ROOT) == before
    row = next(row for row in REQUIRED_FINDINGS if row["issue"] == 5066)
    errors = check_evidence([row], [], audit._current_sources(audit.ROOT))
    for configuration in row["required_configurations"]:
        assert any(f"/{configuration}: unresolved" in error for error in errors)


@pytest.mark.parametrize(
    "kind,relative",
    [
        ("IRQ", "platforms/my9221.h"),
        ("PLACEMENT", "platforms/arm/rp/rpcommon/my9221_emitter.h"),
    ],
)
def test_my9221_rp2040_controlled_source_retention_uses_production_layout(
    tmp_path: Path,
    kind: str,
    relative: str,
) -> None:
    """Real filesystem layout contract only; no build/proof authentication."""
    from ci import software_spi_my9221_rp2040_runner as vendor

    project = tmp_path / "project"
    staged = project / "lib" / "FastLED" / relative
    staged.parent.mkdir(parents=True)
    source = b"// exact controlled source bytes\n"
    staged.write_bytes(source)
    saved = tmp_path / "retained"
    saved.mkdir()
    vendor._retain_controlled_source(
        project, saved, getattr(vendor._PropertyKind, kind), source
    )
    assert (saved / "staged-controlled-source.h").read_bytes() == source
    staged.write_bytes(b"// stale staged source\n")
    with pytest.raises(ValueError, match="controlled staged vendor source differs"):
        vendor._retain_controlled_source(
            project, saved, getattr(vendor._PropertyKind, kind), source
        )
    assert (saved / "staged-controlled-source.h").read_bytes() == source


@pytest.mark.parametrize("ensure", [False, True])
def test_my9221_rp2040_launcher_uses_official_environment_in_bound_child(
    ensure: bool,
) -> None:
    """Source-level orchestration contract, not an authenticated engine proof."""
    from ci import software_spi_my9221_rp2040_runner as vendor

    script = vendor._launcher_script(ensure)
    assert "from ci.util.fbuild_runner import fbuild_child_env" in script
    assert "fbuild_child_env()" in script
    assert "_startup_environment_hashes" in script
    assert "adjacent_daemon_sha256" in script and "client_extension_sha256" in script
    if ensure:
        assert script.index("os.environ.update(") < script.index(
            "Daemon.ensure_running()"
        )
    else:
        assert "Daemon.ensure_running()" not in script
        assert "os.environ.update(" not in script
    assert "dict(os.environ)" not in script, (
        "do not serialize arbitrary startup secrets"
    )


@pytest.mark.parametrize("online", ["foreign", "matching", "absent"])
def test_my9221_rp2040_prebootstrap_launcher_contract_controls_service_reuse(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    online: str,
) -> None:
    """Mock orchestration only; no synthetic record grants property acceptance."""
    from ci import software_spi_my9221_rp2040_runner as vendor

    desired = {
        "startup_environment": {"PATH": "official-helper-path", "UV_NO_SYNC": "one"}
    }
    engine = {
        "pid": 42,
        "health_pid": 42,
        "image_pid": 42,
        "image_sha256": "approved",
        **desired,
    }
    observed = None if online == "absent" else dict(engine)
    if online == "foreign":
        observed["startup_environment"] = {"PATH": "unprepared-path"}
    events = []
    monkeypatch.setattr(
        vendor,
        "_probe_existing_engine",
        lambda *args: events.append("preflight") or observed,
    )
    monkeypatch.setattr(
        vendor, "_ensure_stage_engine", lambda *args: events.append("ensure")
    )
    monkeypatch.setattr(
        vendor,
        "_daemon_identity",
        lambda *args: events.append("verified-after") or engine,
    )
    monkeypatch.setattr(
        vendor,
        "_compile",
        lambda *args: pytest.fail("prebootstrap qualification must not compile"),
    )
    if online == "foreign":
        with pytest.raises(ValueError, match="startup|launcher|environment"):
            vendor._prepare_bootstrap_engine(
                tmp_path, tmp_path / "saved", {}, {}, desired
            )
        assert events == ["preflight"], "foreign daemon must remain untouched"
    else:
        result = vendor._prepare_bootstrap_engine(
            tmp_path, tmp_path / "saved", {}, {}, desired
        )
        assert result == engine
        assert events == (
            ["preflight", "verified-after"]
            if online == "matching"
            else ["preflight", "ensure", "verified-after"]
        )
        assert result["startup_environment"] == desired["startup_environment"]


def test_my9221_rp2040_bootstrap_cannot_adopt_wrong_postensure_environment(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from ci import software_spi_my9221_rp2040_runner as vendor

    monkeypatch.setattr(vendor, "_probe_existing_engine", lambda *args: None)
    monkeypatch.setattr(vendor, "_ensure_stage_engine", lambda *args: None)
    monkeypatch.setattr(
        vendor,
        "_daemon_identity",
        lambda *args: {"startup_environment": {"PATH": "wrong"}},
    )
    with pytest.raises(ValueError, match="startup|launcher|environment"):
        vendor._prepare_bootstrap_engine(
            tmp_path,
            tmp_path / "saved",
            {},
            {},
            {"startup_environment": {"PATH": "desired"}},
        )
