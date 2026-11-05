"""Fail-closed source/evidence contract for the software SPI audit (#5031).

These tests use small source universes to exercise gate mutations. They are not
hardware or production-writer evidence; the C++ production-template traces and
real target configuration receipts must be collected separately.
"""

import hashlib
import json
import sys
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

import pytest
from running_process import RunningProcess

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
    ) | {5055, 5056, 5057, 5059, 5060}
    assert len(REQUIRED_FINDINGS) == 34
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

    patch_native_runner(monkeypatch, run)
    monkeypatch.setattr("ci.software_spi_audit._current_sources", lambda _root: sources)
    records = _run_native_traces(tmp_path, sources)
    assert calls == [
        ["bash", "test", "generic_software_spi"],
        ["bash", "test", "generic_software_spi_inverted", "--debug"],
        ["bash", "test", "forced_software_pin", "--debug"],
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
    assert actual["baseline_files"] == 2986
    assert "src/fastspi.h" in actual["source_accounting"]
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
