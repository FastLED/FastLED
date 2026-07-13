"""Regression checks for clean WASM frontend dependency installation."""

from __future__ import annotations

import hashlib
import importlib
from pathlib import Path


def test_frontend_dependency_hash_tracks_both_manifests(monkeypatch, tmp_path: Path) -> None:
    module = importlib.import_module("ci.esbuild_frontend")
    (tmp_path / "package.json").write_text('{"dependencies":{}}', encoding="utf-8")
    (tmp_path / "package-lock.json").write_text('{"lockfileVersion":3}', encoding="utf-8")
    monkeypatch.setattr(module, "FRONTEND_DIR", tmp_path)

    actual = module._frontend_dependency_hash()
    expected = hashlib.sha256(
        b'package.json{"dependencies":{}}package-lock.json{"lockfileVersion":3}'
    ).hexdigest()
    assert actual == expected


def test_frontend_dependencies_run_npm_ci_once_per_manifest_hash(monkeypatch, tmp_path: Path) -> None:
    module = importlib.import_module("ci.esbuild_frontend")
    (tmp_path / "package.json").write_text('{"dependencies":{}}', encoding="utf-8")
    lockfile = tmp_path / "package-lock.json"
    lockfile.write_text('{"lockfileVersion":3}', encoding="utf-8")
    monkeypatch.setattr(module, "FRONTEND_DIR", tmp_path)

    calls: list[tuple[list[str], str]] = []

    def fake_run(args: list[str], cwd: str):
        calls.append((args, cwd))
        (tmp_path / "node_modules").mkdir(exist_ok=True)
        return type("Result", (), {"returncode": 0})()

    monkeypatch.setattr(module.subprocess, "run", fake_run)
    module.ensure_frontend_dependencies()
    module.ensure_frontend_dependencies()
    lockfile.write_text('{"lockfileVersion":3,"changed":true}', encoding="utf-8")
    module.ensure_frontend_dependencies()

    assert calls == [
        (["npm", "ci", "--ignore-scripts"], str(tmp_path)),
        (["npm", "ci", "--ignore-scripts"], str(tmp_path)),
    ]
