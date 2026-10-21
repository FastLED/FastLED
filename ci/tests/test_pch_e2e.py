"""End-to-end PCH staleness regression tests against the real clang toolchain.

These drive ``ci/compile_pch.py`` (the wrapper every native and WASM PCH
build goes through) with the compiler FastLED itself builds with, and then
compile a consumer TU exactly the way the test/example builds do. No mocks.

The bug they pin down: clang, given ``-fpch-validate-input-files-content``,
accepts a PCH whose input header is byte-identical but has a newer mtime --
and then fails to tie that header to its PCH copy, so ``#pragma once`` lets it
be parsed a second time ("redefinition of ..."). zccache supplies private
input snapshots; shared source timestamps must remain untouched.
See agents/docs/testing-commands.md -> "Precompiled Header Invariant".
"""

from __future__ import annotations

import os
import shutil
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Any
from unittest.mock import patch

from running_process import PIPE, RunningProcess

import ci.wasm_compile_native as wasm_native
from ci.compile_pch import (
    fix_depfile,
    hash_compile_flags,
    hash_input_files,
    invalidate_stale_pch,
    is_stale_pch_error,
    parse_depfile_inputs,
    purge_pch_artifacts,
)
from ci.meson.native_launchers import wrap_compiler_with_zccache
from ci.wasm_compile_pch import check_dependencies_changed, compute_flags_hash


ROOT = Path(__file__).resolve().parent.parent.parent
COMPILE_PCH = ROOT / "ci" / "compile_pch.py"


def _find_cxx() -> str | None:
    """Locate the C++ driver the native build uses (meson's native file)."""
    suffix = ".exe" if sys.platform == "win32" else ""
    launcher = ROOT / ".cached" / "clang-native" / f"ctc-clang++{suffix}"
    if launcher.is_file():
        return str(launcher)
    bindir = Path(sys.prefix) / ("Scripts" if sys.platform == "win32" else "bin")
    wrapper = bindir / f"clang-tool-chain-cpp{suffix}"
    if wrapper.is_file():
        return str(wrapper)
    return shutil.which("clang-tool-chain-cpp")


CXX = _find_cxx()

# Mirrors fastled_pch / test_pch in ci/meson/native/meson.build and
# tests/meson.build: the PCH producer and consumer validation flags.
PCH_BUILD_FLAGS = [
    "-c",
    "-Wno-unused-command-line-argument",
    "-x",
    "c++-header",
    "-std=gnu++17",
    "-Werror=invalid-pch",
    "-verify-pch",
    "-fpch-validate-input-files-content",
    "-fpch-instantiate-templates",
]
PCH_USE_FLAGS = [
    "-std=gnu++17",
    "-Werror=invalid-pch",
    "-fpch-validate-input-files-content",
]
if sys.platform != "win32":
    PCH_BUILD_FLAGS.append("-fPIC")
    PCH_USE_FLAGS.append("-fPIC")

HEADER_V1 = "#pragma once\ntemplate <class T = int> T ident(T x) { return x; }\n"
HEADER_V2 = HEADER_V1 + "inline int added_in_v2() { return 2; }\n"


def _run(cmd: list[str], **kw: Any) -> Any:
    """Run ``cmd`` capturing stdout and stderr separately as text."""
    return RunningProcess.run(
        cmd,
        stdout=PIPE,
        stderr=PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
        **kw,
    )


def _bump_mtime(path: Path, seconds: int = 10) -> None:
    st = path.stat()
    later = st.st_mtime_ns + seconds * 1_000_000_000
    os.utime(path, ns=(later, later))


@unittest.skipIf(
    CXX is None and not os.environ.get("CI"),
    "FastLED clang toolchain not installed (run ./install)",
)
class TestPchEndToEnd(unittest.TestCase):
    def setUp(self) -> None:
        if CXX is None:
            self.fail("CI must have the FastLED clang toolchain; refusing to skip")
        self._tmp = TemporaryDirectory()
        self.dir = Path(self._tmp.name)
        self.header = self.dir / "a.h"
        self.pch_src = self.dir / "pch.h"
        self.pch = self.dir / "pch.h.pch"
        self.depfile = self.dir / "pch.h.d"
        self.header.write_text(HEADER_V1, encoding="utf-8")
        self.pch_src.write_text(f'#include "{self.header}"\n', encoding="utf-8")
        self._build_pch()

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def _build_pch(self) -> str:
        """Run ci/compile_pch.py the way the meson custom_target does."""
        assert CXX is not None
        env = dict(os.environ, PYTHONPATH=str(ROOT))
        cmd = [
            sys.executable,
            str(COMPILE_PCH),
            CXX,
            *PCH_BUILD_FLAGS,
            str(self.pch_src),
            "-o",
            str(self.pch),
            "-MD",
            "-MF",
            str(self.depfile),
        ]
        r = _run(cmd, cwd=self.dir, env=env)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return r.stderr

    def _compile_tu(self, body: str, *, cached: bool = True) -> Any:
        assert CXX is not None
        tu = self.dir / "tu.cpp"
        tu.write_text(f'#include "{self.header}"\n{body}\n', encoding="utf-8")
        cmd = [
            CXX,
            *PCH_USE_FLAGS,
            "-include-pch",
            str(self.pch),
            "-c",
            str(tu),
            "-o",
            str(self.dir / "tu.o"),
        ]
        return _run(wrap_compiler_with_zccache(cmd) if cached else cmd, cwd=self.dir)

    def _assert_compiles(self, body: str = "int main() { return ident(0); }") -> None:
        r = self._compile_tu(body)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    # -- the reported bug ---------------------------------------------------

    def test_touched_header_reuses_pch_via_wrapper(self) -> None:
        """Same content: private inputs preserve both source and PCH mtimes."""
        before = (self.pch.stat().st_mtime_ns, self.pch.read_bytes())
        _bump_mtime(self.header)
        touched = self.header.stat().st_mtime_ns
        self.assertIn("skipping compilation", self._build_pch())
        self.assertEqual(self.header.stat().st_mtime_ns, touched)
        self.assertEqual(before, (self.pch.stat().st_mtime_ns, self.pch.read_bytes()))
        self._assert_compiles()
        self.assertEqual(self.header.stat().st_mtime_ns, touched)

    def test_touched_header_preserved_by_precompile_pass(self) -> None:
        """Same, through compile.py's pre-ninja invalidate_stale_pch pass."""
        _bump_mtime(self.header)
        invalidate_stale_pch(self.pch)
        self.assertTrue(self.pch.exists(), "identical input must preserve the PCH")
        self._build_pch()
        self._assert_compiles()

    def test_backwards_mtime_reuses_pch(self) -> None:
        before = (self.pch.stat().st_mtime_ns, self.pch.read_bytes())
        _bump_mtime(self.header, seconds=-10)
        touched = self.header.stat().st_mtime_ns
        invalidate_stale_pch(self.pch)
        self.assertEqual(self.header.stat().st_mtime_ns, touched)
        self.assertIn("skipping compilation", self._build_pch())
        self.assertEqual(before, (self.pch.stat().st_mtime_ns, self.pch.read_bytes()))
        self._assert_compiles()
        self.assertEqual(self.header.stat().st_mtime_ns, touched)

    def test_same_size_edit_with_restored_mtime_rebuilds(self) -> None:
        recorded = self.header.stat().st_mtime_ns
        self.header.write_text(
            HEADER_V1.replace("return x;", "return 7;"), encoding="utf-8"
        )
        os.utime(self.header, ns=(recorded, recorded))
        invalidate_stale_pch(self.pch)
        self.assertFalse(self.pch.exists())
        self._build_pch()
        self._assert_compiles()

    # -- siblings -----------------------------------------------------------

    def test_content_change_rebuilds_pch(self) -> None:
        self.header.write_text(HEADER_V2, encoding="utf-8")
        _bump_mtime(self.header)
        invalidate_stale_pch(self.pch)
        self._build_pch()
        self._assert_compiles("int main() { return ident(0) + added_in_v2(); }")

    def test_no_change_does_not_rebuild(self) -> None:
        """No rebuild loop: an untouched PCH is reused byte-for-byte."""
        before = (self.pch.stat().st_mtime_ns, self.pch.read_bytes())
        invalidate_stale_pch(self.pch)
        self.assertTrue(self.pch.exists())
        stderr = self._build_pch()
        self.assertIn("skipping compilation", stderr)
        self.assertEqual(before, (self.pch.stat().st_mtime_ns, self.pch.read_bytes()))
        self._assert_compiles()

    def test_invalid_dependency_tracking_rebuilds(self) -> None:
        Path(str(self.depfile) + ".cache").write_text(
            "invalid depfile", encoding="utf-8"
        )
        invalidate_stale_pch(self.pch)
        self.assertFalse(self.pch.exists())
        self._build_pch()
        self._assert_compiles()

    def test_flag_change_rebuilds_pch(self) -> None:
        before = self.pch.stat().st_mtime_ns
        PCH_BUILD_FLAGS.append("-DFL_PCH_E2E_FLAG=1")
        try:
            stderr = self._build_pch()
        finally:
            PCH_BUILD_FLAGS.remove("-DFL_PCH_E2E_FLAG=1")
        self.assertNotIn("skipping compilation", stderr)
        self.assertNotEqual(before, self.pch.stat().st_mtime_ns)

    def test_object_compile_through_wrapper_is_never_skipped(self) -> None:
        """wasm_build_library.py routes .o compiles through the wrapper too."""
        assert CXX is not None
        env = dict(os.environ, PYTHONPATH=str(ROOT))
        tu = self.dir / "obj.cpp"
        tu.write_text(f'#include "{self.header}"\nint f() {{ return 1; }}\n')
        obj = self.dir / "obj.obj"
        cmd = [
            sys.executable,
            str(COMPILE_PCH),
            CXX,
            *PCH_USE_FLAGS,
            "-c",
            str(tu),
            "-o",
            str(obj),
            "-MD",
            "-MF",
            str(self.dir / "obj.d"),
        ]
        for _ in range(2):
            r = _run(cmd, cwd=self.dir, env=env)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertNotIn("skipping compilation", r.stderr)
            self.assertFalse(Path(str(obj) + ".input_hash").exists())
            self.assertFalse(Path(str(obj) + ".inputs.json").exists())

    # -- fail-safe detection ------------------------------------------------

    def test_stale_pch_error_is_recognised_and_purged(self) -> None:
        """Produce the real clang failure, then check the fail-safe sees it."""
        _bump_mtime(self.header)  # bypass the guards: use the stale PCH as-is
        r = self._compile_tu("int main() { return ident(0); }", cached=False)
        self.assertNotEqual(r.returncode, 0, "clang bug no longer reproduces?")
        self.assertTrue(is_stale_pch_error(r.stderr, self.dir), r.stderr)
        self.assertGreaterEqual(purge_pch_artifacts(self.dir), 1)
        self.assertFalse(self.pch.exists())
        self.assertFalse(Path(str(self.pch) + ".input_hash").exists())

    def test_legacy_chained_pch_migrates_through_bounded_recovery(self) -> None:
        """An unchanged legacy chain must rebuild when private provenance is absent."""
        assert CXX is not None
        base_pch = self.pch
        top_header = self.dir / "top.h"
        top_header.write_text('#include "a.h"\n', encoding="utf-8")
        top_pch = self.dir / "top.h.pch"
        depfile = self.dir / "top.h.d"
        args = [
            CXX,
            *PCH_BUILD_FLAGS,
            "-include-pch",
            str(base_pch),
            str(top_header),
            "-o",
            str(top_pch),
            "-MD",
            "-MF",
            str(depfile),
        ]
        # Simulate a producer predating zccache's chained-PCH provenance.
        raw = _run(args, cwd=self.dir)
        self.assertEqual(raw.returncode, 0, raw.stdout + raw.stderr)
        fix_depfile(depfile, top_pch)
        saved = Path(str(depfile) + ".cache")
        saved.write_bytes(depfile.read_bytes())
        Path(str(top_pch) + ".input_hash").write_text(
            hash_input_files(parse_depfile_inputs(saved)), encoding="utf-8"
        )
        Path(str(top_pch) + ".flags_hash").write_text(
            hash_compile_flags(args), encoding="utf-8"
        )
        provenance = Path(str(top_pch) + ".zccache-imports.json")
        self.assertFalse(provenance.exists())
        _bump_mtime(self.header)
        touched = self.header.stat().st_mtime_ns
        command = [sys.executable, str(COMPILE_PCH), *args]
        env = dict(os.environ, PYTHONPATH=str(ROOT))
        skipped = _run(command, cwd=self.dir, env=env)
        self.assertEqual(skipped.returncode, 0, skipped.stdout + skipped.stderr)
        self.assertIn("skipping compilation", skipped.stderr)
        self.pch = top_pch
        rejected = self._compile_tu("int f() { return ident(0); }")
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("PCH input validation failed; rebuild the PCH:", rejected.stderr)
        self.assertTrue(is_stale_pch_error(rejected.stderr, self.dir), rejected.stderr)
        # This is the same purge-and-one-retry sequence used by both runners.
        self.assertGreater(purge_pch_artifacts(self.dir), 0)
        self.pch = base_pch
        self._build_pch()
        rebuilt = _run(command, cwd=self.dir, env=env)
        self.assertEqual(rebuilt.returncode, 0, rebuilt.stdout + rebuilt.stderr)
        self.assertTrue(provenance.exists())
        self.pch = top_pch
        self._assert_compiles()
        self.assertEqual(self.header.stat().st_mtime_ns, touched)

    def test_redefinition_outside_pch_is_not_matched(self) -> None:
        out = f"{self.dir / 'other.h'}:3:5: error: redefinition of 'x'\n"
        self.assertFalse(is_stale_pch_error(out, self.dir))


# -- WASM PCH reuse paths (same rule, no compiler needed) -------------------


def _set_mtime(path: Path, seconds: int) -> None:
    ns = seconds * 1_000_000_000
    os.utime(path, ns=(ns, ns))


class TestWasmPchDependencies(unittest.TestCase):
    def _setup(self, tmp: str) -> None:
        d = Path(tmp)
        header = d / "a.h"
        header.write_text("#pragma once\n", encoding="utf-8")
        depfile = d / "wasm_pch.h.d"
        depfile.write_text(f"wasm_pch.h.pch: {header}\n", encoding="utf-8")
        pch = d / "wasm_pch.h.pch"
        pch.write_bytes(b"pch")
        # Real build order: depfile is written before the PCH is finished.
        _set_mtime(header, 1)
        _set_mtime(depfile, 2)
        _set_mtime(pch, 3)
        Path(str(pch) + ".input_hash").write_text(
            hash_input_files([header.resolve()]), encoding="utf-8"
        )
        self.header, self.depfile, self.pch = header, depfile, pch

    def test_touched_dependency_preserves_mtime_even_with_older_depfile(self) -> None:
        with TemporaryDirectory() as tmp:
            self._setup(tmp)
            _set_mtime(self.header, 4)
            changed, reason = check_dependencies_changed(self.depfile, self.pch)
            self.assertFalse(changed, reason)
            self.assertEqual(self.header.stat().st_mtime_ns, 4_000_000_000)

    def test_backwards_dependency_mtime_is_preserved(self) -> None:
        with TemporaryDirectory() as tmp:
            self._setup(tmp)
            _set_mtime(self.header, 0)
            self.assertEqual(
                check_dependencies_changed(self.depfile, self.pch), (False, "")
            )
            self.assertEqual(self.header.stat().st_mtime_ns, 0)

    def test_real_dependency_edit_with_old_mtime_rebuilds(self) -> None:
        with TemporaryDirectory() as tmp:
            self._setup(tmp)
            self.header.write_text("#pragma once\nint edited;\n", encoding="utf-8")
            _set_mtime(self.header, 1)
            changed, reason = check_dependencies_changed(self.depfile, self.pch)
            self.assertTrue(changed, reason)

    def test_missing_dependency_rebuilds(self) -> None:
        with TemporaryDirectory() as tmp:
            self._setup(tmp)
            self.depfile.write_text(
                f"wasm_pch.h.pch: {self.header} {Path(tmp) / 'missing.h'}\n",
                encoding="utf-8",
            )
            changed, reason = check_dependencies_changed(self.depfile, self.pch)
            self.assertTrue(changed)
            self.assertIn("dependency missing", reason)

    def test_untouched_dependencies_are_fresh(self) -> None:
        with TemporaryDirectory() as tmp:
            self._setup(tmp)
            self.assertEqual(
                check_dependencies_changed(self.depfile, self.pch), (False, "")
            )


class TestWasmFlagsHash(unittest.TestCase):
    def test_flag_order_changes_the_hash(self) -> None:
        a: dict[str, list[str]] = {"defines": ["-DA=1", "-DA=2"], "compiler_flags": []}
        b: dict[str, list[str]] = {"defines": ["-DA=2", "-DA=1"], "compiler_flags": []}
        self.assertNotEqual(compute_flags_hash(a), compute_flags_hash(b))

    def test_project_root_is_part_of_the_hash(self) -> None:
        flags: dict[str, list[str]] = {"defines": [], "compiler_flags": ["-O1"]}
        here = compute_flags_hash(flags)
        with patch("ci.wasm_compile_pch.PROJECT_ROOT", Path("/elsewhere")):
            self.assertNotEqual(here, compute_flags_hash(flags))


class TestWasmSketchObjectFollowsPch(unittest.TestCase):
    def test_object_older_than_pch_recompiles(self) -> None:
        with TemporaryDirectory() as tmp:
            d = Path(tmp)
            src, obj, pch = d / "s.cpp", d / "s.o", d / "wasm_pch.h.pch"
            for f in (src, obj, pch):
                f.write_bytes(b"x")
            _set_mtime(src, 1)
            _set_mtime(obj, 2)
            _set_mtime(pch, 3)
            with patch.object(wasm_native, "BUILD_DIR", d):
                self.assertTrue(wasm_native.needs_compilation(src, obj))
                _set_mtime(pch, 1)
                self.assertFalse(wasm_native.needs_compilation(src, obj))


if __name__ == "__main__":
    unittest.main()
