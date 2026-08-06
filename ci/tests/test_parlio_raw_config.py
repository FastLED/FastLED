"""Regression tests for the AutoResearch raw PARLIO TX diagnostic."""

from pathlib import Path


SOURCE = (
    Path(__file__).resolve().parents[2]
    / "examples"
    / "AutoResearch"
    / "AutoResearchRemoteSystemMethods.cpp"
)


def test_parlio_raw_tx_uses_the_tx_shift_edge_enum() -> None:
    source = SOURCE.read_text(encoding="utf-8")

    assert "cfg.shift_edge = PARLIO_SHIFT_EDGE_POS;" in source
    assert "cfg.shift_edge = PARLIO_SAMPLE_EDGE_POS;" not in source
