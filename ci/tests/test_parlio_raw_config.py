"""Regression tests for the AutoResearch raw PARLIO TX diagnostic."""

from pathlib import Path


SOURCE = (
    Path(__file__).resolve().parents[2]
    / "examples"
    / "AutoResearch"
    / "AutoResearchRemoteSystemMethods.cpp"
)


def test_parlio_raw_tx_sets_the_edge_field_the_idf_has() -> None:
    """IDF 5.5.5 names the TX edge `shift_edge`; 5.5.1 only has `sample_edge`.

    The diagnostic must compile on both, so the field is picked by member
    detection -- and the two enums must never be mixed (their POS values
    differ: SAMPLE_EDGE_POS is 1, SHIFT_EDGE_POS is 0).
    """
    source = SOURCE.read_text(encoding="utf-8")

    assert "setParlioRawTxEdge(cfg, 0);" in source
    assert "-> decltype(cfg.shift_edge, void())" in source
    assert "cfg.sample_edge = PARLIO_SAMPLE_EDGE_POS;" in source
    assert "shift_edge = PARLIO_SAMPLE_EDGE_POS" not in source
    assert "sample_edge = PARLIO_SHIFT_EDGE_POS" not in source
