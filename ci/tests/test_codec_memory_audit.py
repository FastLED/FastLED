from __future__ import annotations

import importlib.util
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "ci" / "codec_memory" / "audit.py"
SPEC = importlib.util.spec_from_file_location("codec_memory_audit", MODULE_PATH)
assert SPEC and SPEC.loader
AUDIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)


def test_checked_in_ledger_is_machine_readable_and_green() -> None:
    summary, tables = AUDIT.ledger_values()
    AUDIT.check_ledger(dict(summary), dict(tables))


def test_regression_over_two_percent_fails_closed() -> None:
    summary, tables = AUDIT.ledger_values()
    current = dict(summary)
    baseline = summary[("minimp3-float", "scratch")]
    current[("minimp3-float", "scratch")] = int(baseline * 1.02) + 1
    current[("minimp3-float", "codec-core")] = (
        current[("minimp3-float", "decoder-state")]
        + current[("minimp3-float", "scratch")]
    )
    with pytest.raises(RuntimeError, match="changed"):
        AUDIT.check_ledger(current, dict(tables))


def test_new_static_table_requires_ledger_update() -> None:
    summary, tables = AUDIT.ledger_values()
    current_tables = dict(tables)
    current_tables[("minimp3-float", "new_table")] = 4
    with pytest.raises(RuntimeError, match="missing from ledger"):
        AUDIT.check_ledger(dict(summary), current_tables)


def test_callgraph_uses_new_reachable_callees() -> None:
    frames = {"decode": 100, "stage": 200, "new_stage": 300}
    original = {"decode": {"stage"}, "stage": set(), "new_stage": set()}
    changed = {
        "decode": {"stage"},
        "stage": {"new_stage"},
        "new_stage": set(),
    }
    assert AUDIT.longest_stack_path(frames, original, "decode") == 300
    assert AUDIT.longest_stack_path(frames, changed, "decode") == 600


def test_callgraph_requires_root_and_reachable_frames() -> None:
    with pytest.raises(RuntimeError, match="root missing"):
        AUDIT.longest_stack_path({}, {}, "decode")
    with pytest.raises(RuntimeError, match="frame missing"):
        AUDIT.longest_stack_path(
            {"decode": 100}, {"decode": {"stage"}, "stage": set()}, "decode"
        )


def test_callgraph_root_resolution_is_unique() -> None:
    graph = {"_mangled_MP3Decode_signature": set()}
    assert (
        AUDIT.resolve_callgraph_root(graph, "MP3Decode")
        == "_mangled_MP3Decode_signature"
    )
    with pytest.raises(RuntimeError, match="expected one"):
        AUDIT.resolve_callgraph_root({}, "MP3Decode")


def test_massif_peak_is_attributed_to_codec_allocator(tmp_path: Path) -> None:
    massif = tmp_path / "massif.out"
    massif.write_text(
        "snapshot=0\n"
        "n2: 1234 0x1: unrelated()\n"
        " n4: 20000 0x2: fl::third_party::Mp3MemoryAllocate(unsigned long)\n"
        " n4: 4524 0x3: fl::third_party::Mp3MemoryAllocate(unsigned long)\n"
        "snapshot=1\n"
        " n4: 100 0x2: fl::third_party::Mp3MemoryAllocate(unsigned long)\n",
        encoding="utf-8",
    )
    assert AUDIT.parse_massif_codec_peak(massif) == 24524


def test_working_ram_budget_includes_stream_buffer() -> None:
    summary, tables = AUDIT.ledger_values()
    current = dict(summary)
    current[("minimp3-float", "working-ram")] = AUDIT.RAM_LIMIT + 1
    with pytest.raises(RuntimeError, match="working RAM exceeds"):
        AUDIT.check_ledger(current, dict(tables))


def test_missing_summary_metric_fails_closed() -> None:
    summary, tables = AUDIT.ledger_values()
    current = dict(summary)
    del current[("minimp3-float", "working-ram")]
    with pytest.raises(RuntimeError, match="missing current metric"):
        AUDIT.check_ledger(current, dict(tables))
