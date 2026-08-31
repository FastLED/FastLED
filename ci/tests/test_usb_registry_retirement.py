"""Regression tests for retiring FastLED's local USB identity tables."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_legacy_usb_identity_tables_are_gone() -> None:
    """Runtime USB selection and labels must come from fbuild's registry."""
    port_utils = (ROOT / "ci/util/port_utils.py").read_text(encoding="utf-8")
    serial_probe = (ROOT / "ci/util/serial_probe.py").read_text(encoding="utf-8")

    assert "ENVIRONMENT_TO_VCOM_VID_PIDS" not in port_utils
    assert "ENVIRONMENT_TO_VCOM_VID_PID" not in port_utils
    assert "BOARD_FINGERPRINTS" not in serial_probe


def test_audit_is_a_registry_query_tool_not_a_retirement_checklist() -> None:
    """Keep the query implementation needed by #3996, but drop old literals."""
    audit = (ROOT / "ci/util/audit_usb_registry.py").read_text(encoding="utf-8")

    assert "AUDITED_LITERALS: tuple[Literal, ...] = ()" in audit
    assert "fetch_artifact" in audit
    assert "decode_registry" in audit


def test_autoresearch_defers_known_board_port_selection_to_fbuild() -> None:
    """Known environments, including LPC845, must not be locally fingerprinted."""
    phases = (ROOT / "ci/autoresearch/phases.py").read_text(encoding="utf-8")

    assert "defer_port_selection_to_fbuild" in phases
