"""Tests for the standardized smoke-sketch manifest reader.

Issue: https://github.com/FastLED/FastLED/issues/2411 (Part A).
"""

import tempfile
from pathlib import Path

import pytest

from ci.standardized_smoke_sketches import (
    MANIFEST_RELATIVE_PATH,
    SmokeSketch,
    load_smoke_sketches,
    load_smoke_sketches_detailed,
    manifest_path,
)


# ---------------------------------------------------------------------------
# Live manifest sanity checks — these run against the actual file checked into
# the repo so a malformed commit fails CI before the manifest is consumed.
# ---------------------------------------------------------------------------


def test_live_manifest_exists() -> None:
    """The canonical manifest must exist at the documented path."""
    assert manifest_path(None).is_file(), (
        f"Manifest missing at {manifest_path(None)} (expected at {MANIFEST_RELATIVE_PATH})"
    )


def test_live_manifest_contains_canonical_sketches() -> None:
    """The four canonical sketches from #2411 must be present.

    If a future change replaces one of these, this test should be updated in
    the same commit so the contract change is explicit.
    """
    names = load_smoke_sketches(None)
    expected = {"Blink", "XYMatrix", "Apa102", "AudioFftParity"}
    missing = expected - set(names)
    assert not missing, (
        f"Canonical smoke sketches missing from manifest: {sorted(missing)}. "
        f"Got: {names}"
    )


def test_live_manifest_entries_resolve_to_existing_examples() -> None:
    """Every entry must point at an existing examples/<name>/ directory."""
    root = Path(__file__).resolve().parent.parent.parent
    examples_dir = root / "examples"
    for entry in load_smoke_sketches_detailed(None):
        example_dir = examples_dir / entry.name
        assert example_dir.is_dir(), (
            f"Smoke-sketch '{entry.name}' (manifest line {entry.manifest_line}) "
            f"does not resolve to an existing example directory: {example_dir}"
        )


# ---------------------------------------------------------------------------
# Parser behaviour — driven against synthetic manifests in tmp dirs so we can
# exercise edge cases without touching the live file.
# ---------------------------------------------------------------------------


def _write_manifest(tmp_root: Path, body: str) -> Path:
    manifest = tmp_root / MANIFEST_RELATIVE_PATH
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text(body, encoding="utf-8")
    return manifest


def test_parser_skips_blank_lines_and_comments() -> None:
    body = """# leading comment
# another comment

Blink

  # indented comment
XYMatrix
"""
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        _write_manifest(root, body)
        assert load_smoke_sketches(root) == ["Blink", "XYMatrix"]


def test_parser_strips_inline_comments_and_whitespace() -> None:
    body = "Blink  # basic clockless\n  XYMatrix\t# 2D matrix\n"
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        _write_manifest(root, body)
        assert load_smoke_sketches(root) == ["Blink", "XYMatrix"]


def test_parser_records_manifest_line_numbers() -> None:
    body = "# header\n\nBlink\nXYMatrix\n"
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        _write_manifest(root, body)
        entries = load_smoke_sketches_detailed(root)
        assert entries == [
            SmokeSketch(name="Blink", manifest_line=3),
            SmokeSketch(name="XYMatrix", manifest_line=4),
        ]


def test_parser_rejects_duplicates() -> None:
    body = "Blink\nXYMatrix\nBlink\n"
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        _write_manifest(root, body)
        with pytest.raises(ValueError, match="Duplicate smoke sketch 'Blink'"):
            load_smoke_sketches(root)


def test_parser_raises_when_manifest_missing() -> None:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        # Note: no manifest written.
        with pytest.raises(FileNotFoundError):
            load_smoke_sketches(root)
