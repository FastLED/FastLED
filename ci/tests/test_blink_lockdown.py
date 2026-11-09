"""Tests for the Blink.ino lockdown lint and its PreToolUse hook."""

import io
import json
from pathlib import Path

import pytest

from ci.hooks import protect_example_ino
from ci.lint import blink_lockdown


ROOT = Path(__file__).resolve().parent.parent.parent


def _hook(
    monkeypatch: pytest.MonkeyPatch, tool: str, rel_path: str, content: str = ""
) -> int:
    payload = {
        "tool_name": tool,
        "tool_input": {"file_path": str(ROOT / rel_path), "content": content},
    }
    monkeypatch.delenv(protect_example_ino.OVERRIDE_ENV_VAR, raising=False)
    monkeypatch.setattr("sys.stdin", io.StringIO(json.dumps(payload)))
    return protect_example_ino.main()


def test_blink_matches_frozen_hash() -> None:
    assert blink_lockdown.run()


def test_changed_blink_fails(tmp_path: Path) -> None:
    probe = tmp_path / "Blink.ino"
    probe.write_text("void setup() {}\nvoid loop() {}\n", encoding="utf-8")
    assert blink_lockdown.blink_sha256(probe) != blink_lockdown.FROZEN_BLINK_SHA256


def test_crlf_does_not_change_hash(tmp_path: Path) -> None:
    crlf = tmp_path / "Blink.ino"
    crlf.write_bytes(blink_lockdown.BLINK_PATH.read_bytes().replace(b"\n", b"\r\n"))
    assert blink_lockdown.blink_sha256(crlf) == blink_lockdown.FROZEN_BLINK_SHA256


def test_hook_blocks_write_and_edit_on_blink(monkeypatch: pytest.MonkeyPatch) -> None:
    blink = "examples/Blink/Blink.ino"
    assert _hook(monkeypatch, "Write", blink) == 2
    assert _hook(monkeypatch, "Edit", blink) == 2
    directive = "// FL_AGENT_ALLOW_NEW_EXAMPLE\n"
    assert _hook(monkeypatch, "Write", blink, directive) == 2


def test_hook_still_allows_other_existing_sketches(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    assert _hook(monkeypatch, "Edit", "examples/AutoResearch/AutoResearch.ino") == 0
