from pathlib import Path

from ci.util import audit_usb_registry


REGISTRY = {
    0x1234: ("Example Vendor", {0x5678: "Example Product"}),
}


def test_lookup_resolves_published_pair(monkeypatch, capsys) -> None:
    monkeypatch.setattr(audit_usb_registry, "fetch_artifact", lambda: b"registry")
    monkeypatch.setattr(audit_usb_registry, "decode_registry", lambda raw: REGISTRY)

    assert audit_usb_registry.main(["--lookup", "1234:5678"]) == 0
    assert (
        "FOUND 1234:5678  Example Vendor / Example Product" in capsys.readouterr().out
    )


def test_lookup_fails_for_absent_pair(monkeypatch, capsys) -> None:
    monkeypatch.setattr(audit_usb_registry, "fetch_artifact", lambda: b"registry")
    monkeypatch.setattr(audit_usb_registry, "decode_registry", lambda raw: REGISTRY)

    assert audit_usb_registry.main(["--lookup", "1234:9999"]) == 1
    assert "MISSING 1234:9999" in capsys.readouterr().out


def test_registry_docs_use_exact_lookup_command() -> None:
    expected = "audit_usb_registry.py --lookup <VID:PID>"
    doc_paths = (
        Path("agents/docs/hardware-autoresearch.md"),
        Path("agents/docs/usb-vid-pid-registry.md"),
    )

    for doc_path in doc_paths:
        assert expected in doc_path.read_text()
