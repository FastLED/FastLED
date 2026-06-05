"""Tests for ci/clean-worktrees.py safety classification.

Exercises classify_worktree() against a freshly-created git repo so we can
verify the helper does not delete worktrees with uncommitted changes or
unpushed commits.
"""

import importlib.util
import os
import subprocess
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from types import ModuleType


def _load_helper() -> ModuleType:
    """Load ci/clean-worktrees.py as a module (filename contains a hyphen)."""
    project_root = Path(__file__).resolve().parent.parent.parent
    src = project_root / "ci" / "clean-worktrees.py"
    spec = importlib.util.spec_from_file_location("_clean_worktrees", src)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    # Make the module discoverable by sys.modules so dataclass works.
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


def _git(args: list[str], cwd: Path) -> str:
    return subprocess.check_output(
        ["git", *args], cwd=str(cwd), text=True, encoding="utf-8", errors="replace"
    )


def _init_repo(root: Path) -> None:
    _git(["init", "--initial-branch=master"], root)
    _git(["config", "user.email", "test@example.com"], root)
    _git(["config", "user.name", "Test"], root)
    (root / "README.md").write_text("hi\n")
    _git(["add", "README.md"], root)
    _git(["commit", "-m", "init"], root)


class TestClassifyWorktree(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = TemporaryDirectory()
        self.tmp = Path(self._tmp.name)
        self.helper = _load_helper()

    def tearDown(self) -> None:
        # On Windows, .git often has read-only objects. Clear them first.
        for root, dirs, files in os.walk(self.tmp):
            for name in dirs + files:
                p = Path(root) / name
                try:
                    p.chmod(0o700)
                except KeyboardInterrupt:
                    import _thread

                    _thread.interrupt_main()
                    raise
                except OSError:
                    pass
        self._tmp.cleanup()

    def test_orphan_directory_is_safe(self) -> None:
        orphan = self.tmp / "orphan"
        orphan.mkdir()
        status = self.helper.classify_worktree(orphan)
        self.assertTrue(status.safe)
        self.assertIn("orphan", status.reason)

    def test_clean_worktree_no_upstream_no_origin_is_unsafe(self) -> None:
        # Without origin/master to compare against, the helper has no way to
        # prove the branch has no extra commits — must err on the side of
        # safety and refuse to delete.
        parent = self.tmp / "parent"
        parent.mkdir()
        _init_repo(parent)
        wt_path = self.tmp / "wt-clean"
        _git(["worktree", "add", "-b", "feature", str(wt_path)], parent)

        status = self.helper.classify_worktree(wt_path)
        self.assertFalse(status.safe)
        self.assertIn("origin/master", status.reason)

    def test_clean_worktree_with_upstream_is_safe(self) -> None:
        # Build a parent repo, add a fake origin remote pointing to a bare
        # clone, push, and create a worktree that tracks the upstream.
        parent = self.tmp / "parent"
        parent.mkdir()
        _init_repo(parent)
        origin = self.tmp / "origin.git"
        subprocess.check_call(
            ["git", "init", "--bare", str(origin)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        _git(["remote", "add", "origin", str(origin)], parent)
        _git(["push", "-u", "origin", "master"], parent)
        wt_path = self.tmp / "wt-tracking"
        _git(
            ["worktree", "add", "--track", "-b", "feature", str(wt_path), "master"],
            parent,
        )
        # Set upstream for the new branch.
        _git(["push", "-u", "origin", "feature"], wt_path)

        status = self.helper.classify_worktree(wt_path)
        self.assertTrue(
            status.safe, f"expected safe, got unsafe: reason={status.reason!r}"
        )

    def test_uncommitted_changes_is_unsafe(self) -> None:
        parent = self.tmp / "parent"
        parent.mkdir()
        _init_repo(parent)
        wt_path = self.tmp / "wt-dirty"
        _git(["worktree", "add", "-b", "feature", str(wt_path)], parent)
        (wt_path / "NEW.txt").write_text("dirty\n")

        status = self.helper.classify_worktree(wt_path)
        self.assertFalse(status.safe)
        self.assertIn("uncommitted", status.reason)

    def test_unpushed_commits_no_upstream_is_unsafe(self) -> None:
        parent = self.tmp / "parent"
        parent.mkdir()
        _init_repo(parent)
        wt_path = self.tmp / "wt-unpushed"
        _git(["worktree", "add", "-b", "feature", str(wt_path)], parent)
        # Create a commit beyond origin/master. There is no origin in this
        # test repo, so classify_worktree should treat that as unsafe.
        (wt_path / "f.txt").write_text("x\n")
        _git(["add", "f.txt"], wt_path)
        _git(["commit", "-m", "extra"], wt_path)

        status = self.helper.classify_worktree(wt_path)
        self.assertFalse(status.safe)


class TestFormatBytes(unittest.TestCase):
    def test_bytes(self) -> None:
        helper = _load_helper()
        self.assertEqual(helper.format_bytes(0), "0 B")
        self.assertEqual(helper.format_bytes(512), "512 B")
        self.assertTrue(helper.format_bytes(2048).endswith(" KB"))
        self.assertTrue(helper.format_bytes(5 * 1024 * 1024).endswith(" MB"))


if __name__ == "__main__":
    unittest.main()
