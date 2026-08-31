"""Regression tests for the cached Node.js lint-tool bootstrap."""

import importlib.util
import shutil
from pathlib import Path
from types import ModuleType

import pytest


def _load_setup_module() -> ModuleType:
    script = Path("ci/setup-js-linting-fast.py")
    spec = importlib.util.spec_from_file_location("setup_js_linting_fast", script)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_node_archive_is_reused_after_extracted_tree_is_removed(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    module = _load_setup_module()
    tools_dir = tmp_path / "js-tools"
    node_dir = tools_dir / "node"
    archive_name = "node-test.tar.xz"
    archive_path = tools_dir / archive_name
    download_calls: list[Path] = []

    monkeypatch.setattr(module, "TOOLS_DIR", tools_dir)
    monkeypatch.setattr(module, "NODE_DIR", node_dir)
    monkeypatch.setattr(
        module,
        "get_node_download_info",
        lambda: ("https://example.invalid/node.tar.xz", archive_name, False),
    )
    monkeypatch.setattr(module.platform, "system", lambda: "Linux")
    monkeypatch.setattr(module.platform, "machine", lambda: "x86_64")

    def fake_download(_url: str, destination: Path, _filename: str) -> None:
        download_calls.append(destination)
        destination.write_bytes(b"cached node archive")

    def fake_extract(
        _archive: Path, destination: Path, _is_zip: bool, _arch: str
    ) -> None:
        (destination / "bin").mkdir(parents=True, exist_ok=True)
        (destination / "bin" / "node").touch()

    monkeypatch.setattr(module, "_download_archive", fake_download)
    monkeypatch.setattr(module, "_extract", fake_extract)

    module.download_and_extract_node()
    assert archive_path.read_bytes() == b"cached node archive"

    shutil.rmtree(node_dir)
    module.download_and_extract_node()

    assert download_calls == [archive_path]
    assert archive_path.read_bytes() == b"cached node archive"
    assert (node_dir / "bin" / "node").exists()
