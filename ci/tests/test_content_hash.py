"""Tests for the shared content-hash helper (issue #3773, follows #3761).

The three metadata caches (src/, tests/, examples/) used to key on
``path:mtime:size``. ``git checkout``, ``git worktree add`` and branch switches
rewrite mtimes with byte-identical content, so every one of those invalidated
the cache and forced the rediscovery the cache exists to avoid. #3761 fixed the
examples cache; this covers the extracted helper all three now share.
"""

from __future__ import annotations

import os
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest import mock

from ci.meson.cache_utils import compute_files_content_hash


def _tree(root: Path) -> list[Path]:
    (root / "pkg").mkdir(parents=True)
    a = root / "pkg" / "a.cpp"
    b = root / "pkg" / "b.h"
    a.write_text("int a() { return 1; }\n")
    b.write_text("#pragma once\n")
    return [a, b]


class TestContentHash(unittest.TestCase):
    def test_mtime_churn_does_not_change_the_hash(self) -> None:
        """The whole point: a git checkout must not invalidate the cache."""
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            files = _tree(root)
            before = compute_files_content_hash(files, root)

            for f in files:
                st = f.stat()
                os.utime(f, (st.st_atime + 10_000, st.st_mtime + 10_000))

            self.assertEqual(before, compute_files_content_hash(files, root))

    def test_content_edit_changes_the_hash(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            files = _tree(root)
            before = compute_files_content_hash(files, root)
            files[0].write_text("int a() { return 2; }\n")
            self.assertNotEqual(before, compute_files_content_hash(files, root))

    def test_rename_changes_the_hash(self) -> None:
        """Paths participate, so identical content under a new name differs."""
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            files = _tree(root)
            before = compute_files_content_hash(files, root)
            renamed = files[0].with_name("renamed.cpp")
            files[0].rename(renamed)
            self.assertNotEqual(
                before, compute_files_content_hash([renamed, files[1]], root)
            )

    def test_order_is_significant_so_callers_must_sort(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            files = _tree(root)
            self.assertNotEqual(
                compute_files_content_hash(files, root),
                compute_files_content_hash(list(reversed(files)), root),
            )

    def test_dropping_a_file_changes_the_hash(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            files = _tree(root)
            self.assertNotEqual(
                compute_files_content_hash(files, root),
                compute_files_content_hash(files[:1], root),
            )

    def test_missing_files_are_skipped_not_fatal(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            files = _tree(root)
            expected = compute_files_content_hash(files, root)
            with_ghost = files + [root / "pkg" / "does_not_exist.cpp"]
            self.assertEqual(expected, compute_files_content_hash(with_ghost, root))

    def test_file_outside_root_still_contributes(self) -> None:
        """The examples cache hashes its discovery script, which lives outside."""
        with TemporaryDirectory() as tmp:
            root = Path(tmp) / "inside"
            root.mkdir()
            files = _tree(root)
            outside = Path(tmp) / "outside.py"
            outside.write_text("# discovery script\n")

            base = compute_files_content_hash(files, root)
            with_outside = compute_files_content_hash(files + [outside], root)
            self.assertNotEqual(base, with_outside)

            # And its *content* matters, not just its presence.
            outside.write_text("# discovery script, changed\n")
            self.assertNotEqual(
                with_outside, compute_files_content_hash(files + [outside], root)
            )

    def test_relative_path_outside_root_cannot_collide_with_one_inside(self) -> None:
        """An outside-root file is keyed by its ABSOLUTE path.

        Keying it by the relative path it was passed as lets it collide with an
        identically-named path inside *root* whenever the contents match, and a
        collision means the external file's edits are invisible to the cache.
        """
        with TemporaryDirectory() as tmp:
            work = Path(tmp) / "work"
            root = work / "inside"
            root.mkdir(parents=True)
            files = _tree(root)  # root/pkg/a.cpp, root/pkg/b.h

            # Same relative path, same content, but OUTSIDE root.
            (work / "pkg").mkdir()
            (work / "pkg" / "a.cpp").write_bytes(files[0].read_bytes())

            # The collision only exists when the caller passes a *relative*
            # path, because Path.as_posix() on an absolute path is already
            # absolute. Reproducing it therefore requires the cwd to be `work`
            # so that "pkg/a.cpp" names the outside file -- os.path.relpath()
            # from the ambient cwd yields a long "../.." form that collides
            # with nothing and would make this test vacuous.
            #
            # cwd is restored in finally. Note this is process-global: it is
            # safe under pytest's default in-process serial execution and under
            # xdist (separate worker processes), but do not make this class
            # thread-parallel without revisiting it.
            prev = os.getcwd()
            os.chdir(work)
            try:
                outside_rel = Path("pkg") / "a.cpp"
                self.assertFalse(outside_rel.is_absolute())
                self.assertNotEqual(
                    compute_files_content_hash([files[0]], root),
                    compute_files_content_hash([outside_rel], root),
                )
            finally:
                os.chdir(prev)

    def test_unreadable_file_is_distinct_from_an_empty_one(self) -> None:
        """An unreadable file must not hash the same as a genuinely empty one.

        If it did, a file locked by AV or an indexer would hash identically to
        an empty file, and a later edit made while it was still unlocked-but-
        empty-looking would read as "unchanged". Drives the helper against a
        real read failure rather than asserting against a hand-built digest,
        so it would catch the helper skipping the file or hashing it as b"".
        """
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "pkg").mkdir()
            target = root / "pkg" / "empty.cpp"
            target.write_text("")

            empty_hash = compute_files_content_hash([target], root)

            real_read_bytes = Path.read_bytes

            def deny(self: Path) -> bytes:
                if self == target:
                    raise PermissionError(13, "locked by another process")
                return real_read_bytes(self)

            with mock.patch.object(Path, "read_bytes", deny):
                unreadable_hash = compute_files_content_hash([target], root)

            self.assertNotEqual(
                empty_hash,
                unreadable_hash,
                "an unreadable file must not hash like an empty one",
            )

            # And it must still be hashed, not silently dropped: a skipped file
            # would collapse to the digest of an empty file list.
            self.assertNotEqual(unreadable_hash, compute_files_content_hash([], root))


if __name__ == "__main__":
    unittest.main()
