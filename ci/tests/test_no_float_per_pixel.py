"""B11: the per-pixel path carries no floating point (P9, #4043).

The TINY tier is specified to link no float symbols when colour management is
in its minimal configuration. The stages already meet that on the streaming
path -- P6 and P7 built them in s16.16 -- but nothing enforced it, and the
cost of losing it is invisible until someone links for an AVR and finds the
soft-float library pulled in.

The check is per *function*, not per file, and that distinction is the whole
point. `device_solve.cpp.hpp` legitimately contains float: its bind-time
`buildRgbSolveMatrixQ16` derives the inverse emitter matrix in float and
quantizes the result once, which B11 permits and `source_xyz.h` documents.
A file-level "no float here" test would either fail on that or have to
exempt the file, and exempting the file would stop guarding the per-pixel
function inside it -- which is the one that matters.

What this does NOT claim: that a linked TINY binary contains no float
symbols. That is a link-time property needing `bash bloat` on a real target,
and it remains P9's open half. This is the source-level precondition for it.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
GFX = PROJECT_ROOT / "src" / "fl" / "gfx"

# Functions that run once per pixel inside show(), by the file that defines
# them. Bind-time builders are deliberately absent: they run once per profile
# and are allowed float.
PER_PIXEL_FUNCTIONS: dict[str, tuple[str, ...]] = {
    "transfer.cpp.hpp": ("decodeTransferU16",),
    "source_xyz.cpp.hpp": ("linearRgbToXyzQ16",),
    "chromatic_adaptation.cpp.hpp": ("adaptXyzQ16",),
    "device_solve.cpp.hpp": ("solveRgbDrivesQ16",),
    "flux_scalar.cpp.hpp": ("applyFluxScalar",),
    "oklab_q16.cpp.hpp": ("xyzToOklabQ16", "oklabToXyzQ16"),
    "gamut_map.cpp.hpp": (
        "mapAndSolveDrivesQ16",
        "mapAndAllocateRgbwQ16",
        "mapAndAllocateRgbwwQ16",
    ),
    "white_allocation.cpp.hpp": (
        "allocateEmitterDrivesQ16",
        "allocateTwoWhiteDrivesQ16",
    ),
}

FLOAT_TOKEN = re.compile(r"\b(?:float|double)\b")

# Comments and string/char literals, in one alternation so a `//` inside a
# string is not treated as a comment and a quote inside a comment is not
# treated as a literal. Both are blanked before anything counts a brace: a
# `}` inside `"}"` truncated a function body and hid everything after it,
# which is a guard silently scanning the wrong text.
NOISE = re.compile(
    r"//[^\n]*"
    r"|/\*.*?\*/"
    r"|\"(?:\\.|[^\"\\\n])*\""
    r"|'(?:\\.|[^'\\\n])*'",
    re.S,
)

# What may sit between a definition's parameter list and its opening brace:
# nothing but qualifier words such as `FL_NO_EXCEPT`, `const`, `noexcept`.
# A call in a condition -- `if (f()) { ... }` -- has a `)` there instead, and
# without this check its block was accepted as f()'s body.
DEFINITION_TAIL = re.compile(r"^\s*(?:[A-Za-z_][A-Za-z0-9_]*\s*)*\{")


def blank_noise(text: str) -> str:
    """Replace comments and literals with spaces, preserving length and lines.

    Length-preserving so every offset computed afterwards still indexes the
    original text, and newline-preserving so reported line numbers hold.
    """

    def _blank(match: re.Match[str]) -> str:
        return "".join("\n" if ch == "\n" else " " for ch in match.group(0))

    return NOISE.sub(_blank, text)


# Kept for the cases that only need comments gone.
COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)


def definition_body(text: str, name: str) -> str | None:
    """The braced body of `name`'s definition, or None if not found.

    Scans for the definition rather than any mention, so a call site does not
    masquerade as one: the match must be followed by a parameter list and then
    an opening brace, with nothing but the return-type/qualifier text between.
    """

    for match in re.finditer(r"\b" + re.escape(name) + r"\s*\(", text):
        depth = 0
        index = match.end() - 1
        while index < len(text):
            if text[index] == "(":
                depth += 1
            elif text[index] == ")":
                depth -= 1
                if depth == 0:
                    break
            index += 1
        else:
            continue
        tail = text[index + 1 :]
        # A declaration ends in `;`, a call sits inside another expression,
        # and only a definition reaches `{` through qualifier words alone.
        tail_match = DEFINITION_TAIL.match(tail)
        if tail_match is None:
            continue
        depth = 0
        start = index + 1 + tail_match.end() - 1
        for cursor in range(start, len(text)):
            if text[cursor] == "{":
                depth += 1
            elif text[cursor] == "}":
                depth -= 1
                if depth == 0:
                    return text[start : cursor + 1]
    return None


class TestNoFloatPerPixel(unittest.TestCase):
    def test_every_named_function_exists(self: "TestNoFloatPerPixel") -> None:
        """Without this the suite passes when a rename orphans an entry.

        A misspelled or moved function silently drops out of the sweep, and
        the guard reports success over a shrinking set -- the failure mode
        that makes a structural test worse than none.
        """

        for file_name, functions in PER_PIXEL_FUNCTIONS.items():
            path = GFX / file_name
            with self.subTest(file=file_name):
                self.assertTrue(path.is_file(), f"{path} is missing")
                text = blank_noise(path.read_text(encoding="utf-8"))
                for function in functions:
                    self.assertIsNotNone(
                        definition_body(text, function),
                        msg=f"{file_name}: no definition of {function}()",
                    )

    def test_no_float_in_any_per_pixel_body(self: "TestNoFloatPerPixel") -> None:
        offenders: list[str] = []
        for file_name, functions in PER_PIXEL_FUNCTIONS.items():
            text = blank_noise((GFX / file_name).read_text(encoding="utf-8"))
            for function in functions:
                body = definition_body(text, function)
                if body is None:
                    continue  # reported by the case above
                if FLOAT_TOKEN.search(body):
                    offenders.append(f"{file_name}:{function}")
        self.assertEqual(
            offenders,
            [],
            msg=(
                "floating point on the per-pixel path, which B11 forbids for "
                "the TINY tier: " + ", ".join(offenders)
            ),
        )

    def test_bind_time_float_is_still_allowed(self: "TestNoFloatPerPixel") -> None:
        """The boundary this test draws, asserted from the other side.

        `buildRgbSolveMatrixQ16` derives the inverse emitter matrix in float
        and quantizes once. If that ever became float-free the guard above
        would still pass, but this case would fail and prompt whoever did it
        to say so here -- which is how P9's remaining half gets noticed when
        it lands rather than silently.
        """

        text = blank_noise((GFX / "device_solve.cpp.hpp").read_text(encoding="utf-8"))
        body = definition_body(text, "buildRgbSolveMatrixQ16")
        self.assertIsNotNone(body)
        assert body is not None
        self.assertRegex(
            body,
            FLOAT_TOKEN,
            msg=(
                "buildRgbSolveMatrixQ16 no longer uses float. If the bind-time "
                "derivation was converted to fixed point, that is P9's "
                "remaining half -- update this test and say so."
            ),
        )

    def test_the_detector_finds_float_when_it_is_there(
        self: "TestNoFloatPerPixel",
    ) -> None:
        # A positive control: without it the sweep would pass on a detector
        # that never matches.
        source = "void f(int a) {\n    float x = 1.0f;\n    (void)x;\n}\n"
        body = definition_body(source, "f")
        self.assertIsNotNone(body)
        assert body is not None
        self.assertTrue(FLOAT_TOKEN.search(body))

    def test_a_declaration_is_not_mistaken_for_a_definition(
        self: "TestNoFloatPerPixel",
    ) -> None:
        # Otherwise a header-style declaration would yield an empty body and
        # every function would trivially pass.
        source = "void f(int a);\nvoid g() {\n    float y = 2.0;\n}\n"
        self.assertIsNone(definition_body(source, "f"))

    def test_a_call_site_is_not_mistaken_for_a_definition(
        self: "TestNoFloatPerPixel",
    ) -> None:
        source = "void caller() {\n    f(1);\n}\nvoid f(int a) {\n    int b = a;\n}\n"
        body = definition_body(source, "f")
        self.assertIsNotNone(body)
        assert body is not None
        self.assertIn("int b = a;", body)

    def test_a_control_flow_brace_is_not_mistaken_for_a_definition(
        self: "TestNoFloatPerPixel",
    ) -> None:
        """`if (f()) { ... }` used to be accepted as f()'s body.

        The consequence is the one that matters: the guard then scanned the
        `if` block and reported no float while the real body had one. A test
        that silently examines the wrong text is worse than no test.
        """

        source = (
            "void caller() {\n"
            "    if (f()) { int q = 0; }\n"
            "}\n"
            "void f(int a) {\n"
            "    float sneaky = 1.0f;\n"
            "}\n"
        )
        body = definition_body(blank_noise(source), "f")
        self.assertIsNotNone(body)
        assert body is not None
        self.assertIsNotNone(FLOAT_TOKEN.search(body))

    def test_a_brace_inside_a_string_does_not_truncate_the_body(
        self: "TestNoFloatPerPixel",
    ) -> None:
        # Counting raw characters stopped at the `}` in `"}"`, hiding
        # everything after it -- including the float.
        source = (
            'void f(int a) {\n    const char* s = "}";\n    float sneaky = 1.0f;\n}\n'
        )
        body = definition_body(blank_noise(source), "f")
        self.assertIsNotNone(body)
        assert body is not None
        self.assertIsNotNone(FLOAT_TOKEN.search(body))

    def test_a_qualifier_between_the_parameters_and_the_brace_is_allowed(
        self: "TestNoFloatPerPixel",
    ) -> None:
        # Every function under guard is declared `FL_NO_EXCEPT`, so rejecting
        # qualifier words would reject all of them.
        source = "void f(int a) FL_NO_EXCEPT {\n    int b = a;\n}\n"
        self.assertIsNotNone(definition_body(blank_noise(source), "f"))

    def test_blanking_preserves_offsets_and_lines(
        self: "TestNoFloatPerPixel",
    ) -> None:
        source = 'int a;  // note\nconst char* s = "xy";\n/* two\n   lines */\nint b;\n'
        blanked = blank_noise(source)
        self.assertEqual(len(blanked), len(source))
        self.assertEqual(blanked.count("\n"), source.count("\n"))
        self.assertNotIn("note", blanked)
        self.assertNotIn("xy", blanked)

    def test_comments_mentioning_float_do_not_count(
        self: "TestNoFloatPerPixel",
    ) -> None:
        # Several of these functions carry comments explaining why they avoid
        # float; reading those as usage would make the guard unusable.
        source = "void f() {\n    // deliberately no float here\n    int a = 1;\n}\n"
        stripped = COMMENT.sub(" ", source)
        body = definition_body(stripped, "f")
        self.assertIsNotNone(body)
        assert body is not None
        self.assertIsNone(FLOAT_TOKEN.search(body))


if __name__ == "__main__":
    unittest.main()
