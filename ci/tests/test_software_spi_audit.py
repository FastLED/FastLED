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
    ) | {5055, 5056, 5057, 5059, 5060, 5061, 5062, 5063, 5064, 5065, 5066, 5067}
    assert len(REQUIRED_FINDINGS) == 41
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
