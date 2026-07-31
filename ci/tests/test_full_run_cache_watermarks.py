"""Regression tests for the full-run cache (issue #3783).

`runner()` has two branches that run unit tests -- unit-only and mixed
(`--cpp`, bare `bash test`). Only the mixed branch persists
`.full_run_cache.json`, but `_watermarks` was captured inside the *unit-only*
branch, which returns before the mixed one is reached. So the value was
computed where nothing used it and unbound where it was needed, and every
`bash test --cpp` raised

    NameError: name '_watermarks' is not defined

straight into a bare `except Exception: pass`. The cache file was never created
on a fresh checkout and the ultra-early-exit fast path it gates could never
fire -- silently, for as long as the bug existed.

Fixing that makes the write live, which puts real weight on two guards that
were previously unreachable and are tested here: a narrowed run must never be
persisted as a full-suite pass, and a unit-only entry must never be replayed to
a caller that asked for examples. Both would be stale greens.
"""

from __future__ import annotations

import ast
import json
import tempfile
import unittest
from pathlib import Path

from ci.early_exit_cache import full_run_cache
from ci.meson.cache_utils import (
    _WATERMARK_KEYS,
    capture_source_watermarks,
    get_full_run_cache_file,
    save_full_run_result,
)
from ci.util.paths import PROJECT_ROOT


def _runner_ast() -> ast.FunctionDef:
    """The `runner` function of ci/util/test_runner.py, parsed."""
    source = (PROJECT_ROOT / "ci" / "util" / "test_runner.py").read_text(
        encoding="utf-8"
    )
    for node in ast.parse(source).body:
        if isinstance(node, ast.FunctionDef) and node.name == "runner":
            return node
    raise AssertionError("runner() not found in ci/util/test_runner.py")


def _has_save_call(node: ast.AST) -> bool:
    return any(
        isinstance(c, ast.Call)
        and isinstance(c.func, ast.Name)
        and c.func.id == "save_full_run_result"
        for c in ast.walk(node)
    )


def _innermost_trys_containing_save(node: ast.AST) -> "list[ast.Try]":
    """Every tightest `try` wrapping a save_full_run_result call.

    There are two -- the writer and the cache refresh -- and both must report
    rather than swallow. Deliberately not every Try in the walk: runner()'s
    giant outer try also *contains* the calls, and auditing its handlers would
    fail this test the day someone adds a legitimate top-level
    `except Exception` there.
    """
    out: list[ast.Try] = []
    for child in ast.walk(node):
        if not isinstance(child, ast.Try) or not _has_save_call(child):
            continue
        nested = any(
            isinstance(g, ast.Try) and g is not child and _has_save_call(g)
            for g in ast.walk(child)
        )
        if not nested:
            out.append(child)
    return out


class TestWatermarksAreBoundOnEveryPath(unittest.TestCase):
    def test_watermarks_is_assigned_at_function_scope(self) -> None:
        """The assignment must sit in `runner`'s top-level body.

        Nested inside a branch it binds on some paths and not others -- exactly
        how #3783 happened.
        """
        runner = _runner_ast()
        top_level: set[str] = set()
        for stmt in runner.body:
            targets = []
            if isinstance(stmt, ast.Assign):
                targets = stmt.targets
            elif isinstance(stmt, ast.AnnAssign) and stmt.value is not None:
                targets = [stmt.target]
            for t in targets:
                if isinstance(t, ast.Name):
                    top_level.add(t.id)

        self.assertIn(
            "_watermarks",
            top_level,
            "_watermarks must be assigned in runner()'s top-level body; a "
            "branch-local assignment leaves it unbound on the --cpp path "
            "(#3783)",
        )

    def test_the_name_is_actually_read(self) -> None:
        """Guards the test above from passing vacuously against dead code."""
        reads = [
            n
            for n in ast.walk(_runner_ast())
            if isinstance(n, ast.Name)
            and n.id == "_watermarks"
            and isinstance(n.ctx, ast.Load)
        ]
        self.assertTrue(reads, "expected runner() to read _watermarks")


class TestCacheWriteReportsFailures(unittest.TestCase):
    def test_handler_does_not_silently_pass(self) -> None:
        """The write may fail without failing the run -- but not in silence.

        A bare `pass` here hid the #3783 NameError for the life of the bug. The
        handler stays broad on purpose (a malformed cache file can raise almost
        anything), so the safety comes from reporting, not from narrowing.
        """
        nodes = _innermost_trys_containing_save(_runner_ast())
        self.assertEqual(
            len(nodes), 2, "expected the writer and the refresh call sites"
        )

        for node in nodes:
            for handler in node.handlers:
                if handler.type is None:
                    self.fail("bare `except:` around save_full_run_result (#3783)")
                # KeyboardInterrupt re-raises; only the catch-all must report.
                if (
                    isinstance(handler.type, ast.Name)
                    and handler.type.id == "Exception"
                ):
                    body_is_only_pass = all(
                        isinstance(st, ast.Pass) for st in handler.body
                    )
                    self.assertFalse(
                        body_is_only_pass,
                        "`except Exception: pass` around save_full_run_result "
                        "swallows programming errors -- it hid #3783 "
                        "completely. Stay tolerant, but report.",
                    )


class TestNarrowedRunsAreNeverCachedAsFullPasses(unittest.TestCase):
    """The entry replays as an unconditional green for the WHOLE suite."""

    def test_save_is_gated_on_full_scope(self) -> None:
        source = (PROJECT_ROOT / "ci" / "util" / "test_runner.py").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "_full_scope_save = not (test_name or test_file_filter)",
            source,
            "a name/file-filtered run must not persist .full_run_cache.json: "
            "its counts are a subset but its watermarks cover the whole tree, "
            "so the next `bash test --cpp` would exit 0 having run nothing",
        )

    def test_gate_actually_guards_the_save_call(self) -> None:
        """An `if` testing the gate must contain a save call -- otherwise the
        constant exists but guards nothing."""
        gated = [
            n
            for n in ast.walk(_runner_ast())
            if isinstance(n, ast.If)
            and any(
                isinstance(sub, ast.Name) and sub.id == "_full_scope_save"
                for sub in ast.walk(n.test)
            )
            and any(_has_save_call(st) for st in n.body)
        ]
        self.assertTrue(
            gated,
            "no `if` testing _full_scope_save encloses a save_full_run_result "
            "call, so a filtered run can still be cached as a full pass",
        )


class TestFullRunCacheRoundTrip(unittest.TestCase):
    def _write(self, build_dir: Path, **kw: object) -> None:
        marks = {k: 0.0 for k in _WATERMARK_KEYS}
        (build_dir / "build.ninja").write_text("", encoding="utf-8")
        save_full_run_result(build_dir, 357, 357, 12.5, watermarks=marks, **kw)

    def test_attribution_is_persisted_and_read_back(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            self._write(
                build_dir,
                num_examples_passed=83,
                num_examples_run=83,
                examples_included=True,
            )
            entry = json.loads(get_full_run_cache_file(build_dir).read_text())
            self.assertEqual(entry["num_examples_run"], 83)
            self.assertIs(entry["examples_included"], True)

    def test_omitted_attribution_stays_absent_rather_than_zero(self) -> None:
        """Absent must not become "0 examples" -- that reads as an
        authoritative "examples were cached" claim (#3779)."""
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            self._write(build_dir)
            entry = json.loads(get_full_run_cache_file(build_dir).read_text())
            self.assertNotIn("num_examples_run", entry)
            self.assertNotIn("examples_included", entry)

    def test_capture_supplies_every_required_key(self) -> None:
        """save_full_run_result fails closed on a partial watermark set and
        returns WITHOUT persisting -- a capture producing the wrong keys would
        reproduce #3783's symptom (no cache file) with no error at all."""
        marks = capture_source_watermarks(PROJECT_ROOT)
        self.assertTrue(
            _WATERMARK_KEYS <= marks.keys(),
            f"capture is missing {_WATERMARK_KEYS - marks.keys()}",
        )

    def test_none_watermarks_skip_persisting_rather_than_raise(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            save_full_run_result(build_dir, 357, 357, 1.0, watermarks=None)
            self.assertFalse(get_full_run_cache_file(build_dir).exists())


class TestUnitOnlyEntryIsNotServedToExamples(unittest.TestCase):
    def test_examples_run_rejects_a_unit_only_entry(self) -> None:
        """Mtime gates alone would replay a unit-only pass as covering
        examples. The reader must fail closed on recorded coverage."""
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            (build_dir / "build.ninja").write_text("", encoding="utf-8")
            save_full_run_result(
                build_dir,
                274,
                274,
                10.0,
                watermarks={k: 0.0 for k in _WATERMARK_KEYS},
                examples_included=False,
            )
            entry = json.loads(get_full_run_cache_file(build_dir).read_text())
            self.assertIs(entry["examples_included"], False)

            source = (PROJECT_ROOT / "ci" / "early_exit_cache.py").read_text(
                encoding="utf-8"
            )
            self.assertIn(
                'saved.get("examples_included") is False',
                source,
                "full_run_cache must reject a unit-only entry when the caller "
                "asked for examples",
            )

    def test_full_run_cache_returns_the_raw_entry(self) -> None:
        """CASE 2 describes the run from what the ENTRY recorded, so the entry
        itself has to come back with the counts."""
        self.assertIn(
            "tuple[int, int, float, dict]",
            (PROJECT_ROOT / "ci" / "early_exit_cache.py").read_text(encoding="utf-8"),
        )
        self.assertTrue(callable(full_run_cache))


if __name__ == "__main__":
    unittest.main()
