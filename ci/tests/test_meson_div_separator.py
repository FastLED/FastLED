"""Pin the separator behaviour of Meson's ``/`` operator.

Two places in this repo explain why derived paths are composed with literal
``+ '/' +`` rather than Meson's ``/``: ``ci/meson/shared/meson.build`` and
``ci/lint_meson/path_norm_join_checker.py``. Both used to assert flatly that
``/`` "emits the native separator", which was true when written and is no
longer true -- Meson added a ``.replace('\\\\', '/')`` inside ``_op_div``.

A stale explanation is how a convention gets deleted by someone who checks it
and finds it false. These tests record what the installed Meson actually does,
so the reasoning stays anchored to something executable instead of to a
comment nobody can verify.

The convention itself does not depend on which way this goes: literal
concatenation is correct under both behaviours. What matters is that a change
here is *noticed*.
"""

from __future__ import annotations

import os
import unittest


class TestMesonDivSeparator(unittest.TestCase):
    def _op_div(self):
        from mesonbuild.interpreter.primitives.string import (  # noqa: PLC0415
            StringHolder,
        )

        return StringHolder._op_div

    def test_div_normalizes_backslashes_in_the_result(self) -> None:
        """Current Meson scrubs separators; older Meson did not.

        If this ever fails, `/` has gone back to emitting native separators and
        the `+ '/' +` convention is load-bearing again rather than merely
        version-proof. Do not "fix" the test -- read the two comments it backs.
        """
        self.assertEqual(self._op_div()("C:/a", "b"), "C:/a/b")

    def test_div_also_scrubs_a_backslashed_left_operand(self) -> None:
        """It normalizes the whole join, not just the seam it introduces."""
        self.assertEqual(self._op_div()("C:\\a\\b", "c"), "C:/a/b/c")

    def test_os_path_join_alone_would_reintroduce_the_bug(self) -> None:
        """Why the scrub is what matters, not the join.

        This is the exact shape reported on 2026-06-02: one native separator
        injected mid-path, producing `C:/Users/.../fastled\\src`.
        """
        joined = os.path.join("C:/Users/x/fastled", "src")
        if os.sep == "\\":
            self.assertIn("\\", joined)
            self.assertNotEqual(joined, "C:/Users/x/fastled/src")
        else:  # POSIX hosts never had the hazard
            self.assertEqual(joined, "C:/Users/x/fastled/src")

    def test_literal_concatenation_is_unconditional(self) -> None:
        """The convention the two comments mandate, on any host."""
        self.assertEqual("C:/Users/x/fastled" + "/" + "src", "C:/Users/x/fastled/src")


if __name__ == "__main__":
    unittest.main()
