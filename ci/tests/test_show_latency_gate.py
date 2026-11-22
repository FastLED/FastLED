"""Tests for the AutoResearch show() latency gate (FastLED#5356).

The RPC needs a board; the verdict and the budget file do not, and a wrong
verdict would either hide a #2994-class regression or fail good runs.
"""

from __future__ import annotations

import pytest

from ci.autoresearch.show_latency import (
    WIRE_US,
    evaluate,
    load_budgets,
    parse_budgets,
)


def _result(mean_us: float, count: int = 500) -> dict[str, object]:
    return {
        "success": True,
        "show_count": count,
        "show_total_us": int(mean_us * count),
        "show_min_us": int(mean_us) - 10,
        "show_max_us": int(mean_us) + 300,
    }


def test_mean_within_budget_passes() -> None:
    verdict = evaluate(_result(215.0), budget_us=280)
    assert verdict.ok
    assert "215.0 us" in verdict.message


def test_mean_over_budget_fails() -> None:
    """#5357's 357 us encode regression must trip a 280 us budget."""
    verdict = evaluate(_result(357.0), budget_us=280)
    assert not verdict.ok
    assert "over budget" in verdict.message


def test_show_blocking_on_the_wire_fails_even_under_a_loose_budget() -> None:
    """#2994: show() tracking wire time means it waited for the transfer."""
    verdict = evaluate(_result(1687.0), budget_us=5000)
    assert not verdict.ok
    assert "blocks on the transfer" in verdict.message
    assert WIRE_US == pytest.approx(1050.0)


def test_missing_timing_fails() -> None:
    verdict = evaluate({"success": True, "show_count": 0}, budget_us=280)
    assert not verdict.ok


def test_firmware_failure_fails() -> None:
    verdict = evaluate({"success": False, "error": "bad pin"}, budget_us=280)
    assert not verdict.ok
    assert "bad pin" in verdict.message


def test_budget_lines_parse_with_comments() -> None:
    budgets = parse_budgets(
        "# header\n\nesp32c6 rmt 280  # trailing comment\nesp32s3 PARLIO 150\n"
    )
    assert budgets == {("esp32c6", "RMT"): 280, ("esp32s3", "PARLIO"): 150}


def test_malformed_budget_line_is_rejected() -> None:
    with pytest.raises(ValueError, match="line 1"):
        parse_budgets("esp32c6 RMT\n")


def test_checked_in_budget_file_parses() -> None:
    budgets = load_budgets()
    assert budgets[("esp32c6", "RMT")] > 0


def test_gate_applies_only_to_single_driver_legacy_runs() -> None:
    """Only --legacy binds the legacy path timingDriftTest uses to the driver."""
    from ci.autoresearch.show_latency import gate_key

    assert gate_key("esp32c6", ["rmt"], legacy=True) == ("esp32c6", "RMT")
    assert gate_key("esp32c6", ["RMT"], legacy=False) is None
    assert gate_key("esp32c6", ["RMT", "PARLIO"], legacy=True) is None
    assert gate_key(None, ["RMT"], legacy=True) is None


def test_firmware_failure_message_is_reported() -> None:
    verdict = evaluate(
        {"success": False, "error": "InvalidPin", "message": "pin out of range"},
        budget_us=280,
    )
    assert "pin out of range" in verdict.message


def test_instruction_counter_is_reported_when_present() -> None:
    """#5376: the C6 reports instructions per show(), immune to code layout."""
    result = _result(215.0, count=500)
    result.update(
        {"show_instr_total": 500 * 12400, "show_instr_count": 500, "cpu_mhz": 160}
    )
    verdict = evaluate(result, budget_us=280)
    assert verdict.ok
    assert "12400 instr/show" in verdict.message
    assert "34400 cycles/show" in verdict.message
    assert "IPC 0.36" in verdict.message


def test_instruction_counter_absent_on_other_boards() -> None:
    verdict = evaluate(_result(215.0), budget_us=280)
    assert "instr/show" not in verdict.message


def test_zero_mean_does_not_divide_by_zero() -> None:
    result = _result(0.0, count=5)
    result.update({"show_instr_total": 5 * 100, "show_instr_count": 5, "cpu_mhz": 160})
    verdict = evaluate(result, budget_us=280)
    assert "100 instr/show" in verdict.message
    assert "IPC" not in verdict.message
