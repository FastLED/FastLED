"""Regression tests for the cached Node.js lint-tool bootstrap."""

import importlib.util
import os
import shutil
from pathlib import Path
from types import ModuleType

import pytest
from running_process import PIPE, RunningProcess


def _load_setup_module() -> ModuleType:
    script = Path("ci/setup-js-linting-fast.py")
    spec = importlib.util.spec_from_file_location("setup_js_linting_fast", script)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_js_tools_cache_is_shared_by_linked_worktrees(tmp_path: Path) -> None:
    module = _load_setup_module()
    repository = tmp_path / "fastled"
    common_git_dir = repository / ".git"
    common_git_dir.mkdir(parents=True)

    worktree = tmp_path / "fastled-wt-feature"
    worktree.mkdir()
    worktree_git_dir = common_git_dir / "worktrees" / "feature"
    worktree_git_dir.mkdir(parents=True)
    (worktree_git_dir / "commondir").write_text("../..\n", encoding="utf-8")
    (worktree / ".git").write_text(f"gitdir: {worktree_git_dir}\n", encoding="utf-8")

    expected = repository / ".cache" / "js-tools"
    assert module.repository_tools_dir(repository) == expected
    assert module.repository_tools_dir(worktree) == expected


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


def test_node_setup_creates_missing_cache_parent(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    module = _load_setup_module()
    tools_dir = tmp_path / ".cache" / "js-tools"
    node_dir = tools_dir / "node"

    monkeypatch.setattr(module, "TOOLS_DIR", tools_dir)
    monkeypatch.setattr(module, "NODE_DIR", node_dir)
    monkeypatch.setattr(
        module,
        "get_node_download_info",
        lambda: ("https://example.invalid/node.tar.xz", "node-test.tar.xz", False),
    )
    monkeypatch.setattr(module.platform, "system", lambda: "Linux")
    monkeypatch.setattr(module.platform, "machine", lambda: "x86_64")

    def fake_download(_url: str, destination: Path, _filename: str) -> None:
        destination.write_bytes(b"cached node archive")

    def fake_extract(
        _archive: Path, destination: Path, _is_zip: bool, _arch: str
    ) -> None:
        (destination / "bin").mkdir(parents=True)
        (destination / "bin" / "node").touch()

    monkeypatch.setattr(module, "_download_archive", fake_download)
    monkeypatch.setattr(module, "_extract", fake_extract)

    assert not tools_dir.parent.exists()
    module.download_and_extract_node()
    assert (node_dir / "bin" / "node").exists()


@pytest.mark.skipif(os.name == "nt", reason="uses a POSIX fake uv executable")
def test_install_fails_in_ci_when_js_setup_fails(tmp_path: Path) -> None:
    install_script = Path("install").resolve()
    fake_bin = tmp_path / "bin"
    fake_bin.mkdir()
    fake_uv = fake_bin / "uv"
    fake_uv.write_text(
        "#!/bin/sh\n"
        'if [ "$1" = "run" ] && [ "$2" = "ci/setup-js-linting-fast.py" ]; then\n'
        "    exit 17\n"
        "fi\n"
        "exit 0\n",
        encoding="utf-8",
    )
    fake_uv.chmod(0o755)

    env = os.environ.copy()
    env["PATH"] = f"{fake_bin}{os.pathsep}{env['PATH']}"
    env["CI"] = "true"
    env["FASTLED_DOCKER"] = ""
    result = RunningProcess.run(
        ["bash", str(install_script)],
        cwd=tmp_path,
        env=env,
        stdout=PIPE,
        stderr=PIPE,
        encoding="utf-8",
        errors="replace",
    )

    assert result.returncode != 0
    assert "JavaScript linter setup failed in CI" in result.stderr
