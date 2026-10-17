"""Native objects must survive PCH timestamp replay without hiding real edits."""

import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

from running_process import PIPE, RunningProcess

from ci.compile_pch import reconcile_pch_inputs
from ci.meson.cache_utils import check_ninja_skip, save_ninja_skip_state
from ci.meson.native_input_guard import prepare_native_inputs, save_native_inputs
from ci.tests.test_pch_e2e import COMPILE_PCH, CXX, PCH_BUILD_FLAGS


@unittest.skipUnless(
    CXX and shutil.which("ninja"), "native compiler and Ninja required"
)
class NativeInputGuardTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.run_command(["git", "init", "-q", str(self.root)])
        (self.root / "src").mkdir()
        self.header = self.root / "src" / "value.h"
        self.source = self.root / "src" / "value.cpp"
        self.header.write_text("#pragma once\n#define VALUE 1\n")
        self.source.write_text('#include "value.h"\nint main() { return VALUE; }\n')
        self.a = self.root / "build-a"
        self.b = self.root / "build-b"
        for build in (self.a, self.b):
            build.mkdir()
            compiler = str(CXX).replace("\\", "/")
            source = self.source.as_posix()
            (build / "build.ninja").write_text(
                f'rule compile\n  command = "{compiler}" -MD -MF $out.d -c "$in" -o $out\n'
                "  depfile = $out.d\n  deps = gcc\n"
                f'rule link\n  command = "{compiler}" value.o -o value.exe\n'
                f"build value.o: compile {source}\nbuild value.exe: link value.o\n"
                "default value.exe\n"
            )

    def run_command(self, args: list[str], *, expected: int = 0) -> str:
        result = RunningProcess.run(
            args,
            stdout=PIPE,
            stderr=PIPE,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        return result.stdout

    def compile(self, build: Path) -> None:
        state = prepare_native_inputs(build, self.root)
        self.run_command(["ninja", "-C", str(build)])
        save_native_inputs(state)

    def test_other_build_rebuilds_objects_after_unchanged_pch_replay(self) -> None:
        pch = self.a / "value.h.pch"
        self.run_command(
            [
                sys.executable,
                str(COMPILE_PCH),
                str(CXX),
                *PCH_BUILD_FLAGS,
                str(self.header),
                "-o",
                str(pch),
                "-MD",
                "-MF",
                str(self.a / "value.h.d"),
            ]
        )
        pch_stat = pch.stat().st_mtime_ns
        self.compile(self.a)
        self.header.write_text("#pragma once\n#define VALUE 2\n")
        self.compile(self.b)
        self.run_command([str(self.b / "value.exe")], expected=2)
        self.header.write_text("#pragma once\n#define VALUE 1\n")
        self.assertTrue(reconcile_pch_inputs(pch))
        self.compile(self.b)
        self.run_command([str(self.b / "value.exe")], expected=1)
        self.assertEqual(pch.stat().st_mtime_ns, pch_stat)

    def test_unchanged_content_preserves_ordinary_objects(self) -> None:
        self.compile(self.b)
        obj = self.b / "value.o"
        before = obj.stat().st_mtime_ns
        os.utime(self.header, ns=(1_000_000_000, 1_000_000_000))
        state = prepare_native_inputs(self.b, self.root)
        self.assertEqual(state.invalidated, 0)
        self.assertEqual(obj.stat().st_mtime_ns, before)

    def test_normal_newer_edit_leaves_objects_for_ninja(self) -> None:
        self.compile(self.b)
        self.header.write_text("#pragma once\n#define VALUE 2\n")
        newer = (self.b / "value.o").stat().st_mtime_ns + 10_000_000_000
        os.utime(self.header, ns=(newer, newer))
        state = prepare_native_inputs(self.b, self.root)
        self.assertEqual(state.invalidated, 0)
        self.assertTrue((self.b / "value.o").exists())

    def test_object_metadata_drift_preserves_identical_content(self) -> None:
        self.compile(self.b)
        obj = self.b / "value.o"
        before = obj.read_bytes()
        os.link(obj, self.root / "cached-object.o")
        state = prepare_native_inputs(self.b, self.root)
        self.assertEqual(state.invalidated, 0)
        self.assertEqual(obj.read_bytes(), before)

    def test_missing_and_corrupt_baselines_discard_unverifiable_objects(self) -> None:
        for manifest in (None, "broken json"):
            with self.subTest(manifest=manifest):
                self.compile(self.b)
                marker = self.b / ".native-inputs.json"
                if manifest is None:
                    marker.rename(self.b / "previous-inputs.json")
                else:
                    marker.write_text(manifest)
                state = prepare_native_inputs(self.b, self.root)
                self.assertEqual(state.invalidated, 1)
                self.assertFalse((self.b / "value.o").exists())

    def test_removed_input_invalidates_objects(self) -> None:
        extra = self.root / "src" / "removed.h"
        extra.write_text("#define UNUSED 1\n")
        self.compile(self.b)
        extra.rename(self.root / "removed.h")
        state = prepare_native_inputs(self.b, self.root)
        self.assertEqual(state.invalidated, 1)

    def test_failed_build_does_not_publish_new_baseline(self) -> None:
        self.compile(self.b)
        marker = self.b / ".native-inputs.json"
        baseline = marker.read_bytes()
        self.header.write_text("#pragma once\n#define VALUE 2\n")
        prepare_native_inputs(self.b, self.root)
        self.assertEqual(marker.read_bytes(), baseline)

    def test_partial_build_does_not_certify_untouched_objects(self) -> None:
        other_source = self.root / "src" / "other.cpp"
        other_source.write_text(self.source.read_text())
        ninja_file = self.b / "build.ninja"
        ninja_file.write_text(
            ninja_file.read_text()
            + f"build other.o: compile {other_source.as_posix()}\n"
        )
        state = prepare_native_inputs(self.b, self.root)
        self.run_command(["ninja", "-C", str(self.b), "value.exe", "other.o"])
        save_native_inputs(state)
        original_mtime = self.header.stat().st_mtime_ns
        other_before = (self.b / "other.o").stat().st_mtime_ns
        self.header.write_text("#pragma once\n#define VALUE 2\n")
        newer = other_before + 10_000_000_000
        os.utime(self.header, ns=(newer, newer))
        state = prepare_native_inputs(self.b, self.root)
        self.run_command(["ninja", "-C", str(self.b)])
        # Cache hardlink materialization updates ctime without compiling the
        # other object. A successful partial build must retain its old inputs.
        os.link(self.b / "other.o", self.root / "cached-other.o")
        save_native_inputs(state)
        self.assertEqual((self.b / "other.o").stat().st_mtime_ns, other_before)
        pending = prepare_native_inputs(self.b, self.root)
        self.assertTrue(pending.objects["other.o"].pending)
        # This target was successfully rebuilt with the current content.
        # Unvisited objects still need protection, but must not globally
        # disable a valid content-aware target cache.
        outputs = self.b / "tests"
        outputs.mkdir()
        shutil.copy2(self.b / "value.exe", outputs / "value.exe")
        original_cwd = Path.cwd()
        try:
            os.chdir(self.root)
            save_ninja_skip_state(self.b, "value")
            self.assertTrue(check_ninja_skip(self.b, "value"))
            self.assertFalse(pending.changed)
        finally:
            os.chdir(original_cwd)
        self.assertEqual(pending.invalidated, 0)
        updated_object = (self.b / "value.o").stat().st_mtime_ns
        # Another build's byte-identical PCH restores the same header's old
        # timestamp. The untouched object still contains VALUE=1.
        os.utime(self.header, ns=(original_mtime, original_mtime))
        state = prepare_native_inputs(self.b, self.root)
        self.assertEqual(state.invalidated, 1)
        self.assertTrue(state.changed)
        self.assertFalse((self.b / "other.o").exists())
        self.assertEqual((self.b / "value.o").stat().st_mtime_ns, updated_object)
