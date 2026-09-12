"""A3/B11: iterative solvers must never run on the per-pixel path.

`nnls3` is a 500-iteration projected-gradient solve. It is legitimate at
profile/cache build time and catastrophic per pixel, and #4041 asks for a
structural test rather than a convention. This asserts the streaming stages
never reference it, so a future edit that wires one in fails here rather than
in a frame-rate report on someone's bench.

The forbidden set is **derived, not listed**. It used to be three names --
`nnls3`, `solve_rgb_colorimetric`, `build_rgb_colorimetric_cache` -- and
naming the solver was the only way to trip it. But `nnls3` is private
(`static bool project_to_hull` at `rgbw_colorimetric.cpp.hpp:183` is its only
caller), and the way anyone actually reaches it is through the *public*
wrappers over that: `solve_strict_subgamut_xy`, `solve_wx_overdrive` and
`solve_rgbcct`, all declared in `rgbw_colorimetric.h`. None of those three was
on the list. A stage calling `solve_rgbcct(...)` ran a 500-iteration solve per
pixel and this guard passed it.

So the seeds below name the iterative primitives, and the check forbids every
function that can reach one of them -- computed by walking call edges across
`src/fl/gfx` to a fixpoint. Wrapping a solver in a new name now extends the
forbidden set instead of escaping it.

This is FastLED#4335's lesson one level down. That issue was "the guard's list
of files was itself a thing nothing checked"; this was the guard's list of
*symbols*, with the same shape and the same fix -- derive it, then assert the
derivation is not vacuous.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
GFX = PROJECT_ROOT / "src" / "fl" / "gfx"

# The stages that execute once per pixel inside show().
#
# This list used to omit `pipeline.cpp.hpp`, `gamut_map.cpp.hpp` and
# `white_allocation.cpp.hpp`, which is most of the point: `processPixelQ16`
# calls `mapAndSolveDrivesQ16(pipeline.gamut, ...)` per pixel, so the gamut
# mapper is exactly where an iterative solve would be reached for -- and it is
# the stage A3 names when it says iterative solves never run per-pixel. The
# guard was blind there. Verified by injecting an `nnls3` call into
# `gamut_map.cpp.hpp`: the old list passed, this one fails (FastLED#4041).
#
# Keep in step with `test_no_rgb8_intermediate.py`, which scans the same eight
# and is where the missing three were noticed.
PER_PIXEL_STAGES = (
    "pipeline.cpp.hpp",
    "transfer.cpp.hpp",
    "source_xyz.cpp.hpp",
    "chromatic_adaptation.cpp.hpp",
    "device_solve.cpp.hpp",
    "gamut_map.cpp.hpp",
    "white_allocation.cpp.hpp",
    "flux_scalar.cpp.hpp",
)

# The iterative primitives themselves. Everything that can reach one of these
# is derived from them; this tuple is the only hand-maintained part.
ITERATIVE_PRIMITIVES = (
    "nnls3",
    "solve_rgb_colorimetric",
    "build_rgb_colorimetric_cache",
)

# Public wrappers that reach `nnls3` and were absent from the old hand-written
# list. Asserted to be in the derived set, so the derivation cannot quietly
# stop finding them -- which is the whole defect this file now guards against.
KNOWN_REACHING_WRAPPERS = (
    "project_to_hull",
    "solve_strict_subgamut_xy",
    "solve_wx_overdrive",
    "solve_rgbcct",
)

# C++ constructs that look like a call or a definition to a regex and are not.
# Without these, `if (...) { ... }` parses as a function named `if` whose body
# is the block, every branch in the tree becomes a call edge, and the fixpoint
# swallows the whole namespace -- measured 230 names including `blur1d` and
# `fill_solid` before this list existed.
NOT_FUNCTIONS = frozenset(
    (
        "if",
        "for",
        "while",
        "switch",
        "catch",
        "return",
        "sizeof",
        "do",
        "else",
        "operator",
        "decltype",
        "static_cast",
        "reinterpret_cast",
        "const_cast",
        "dynamic_cast",
        "alignof",
        "noexcept",
        "throw",
        "new",
        "delete",
        "and",
        "or",
        "not",
        "typeid",
        "constexpr",
        "static_assert",
    )
)

# `name(params)` followed by trailing specifiers and an opening brace. Ctor
# initialiser lists and trailing return types are not matched on purpose: a
# definition this misses simply contributes no call edges, which loses
# coverage rather than inventing it. The vacuity test below is what keeps that
# honest.
_TRAILER = r"(?:\s|const|noexcept|FL_NO_EXCEPT|override|final|mutable|volatile)*"
DEFINITION = re.compile(
    r"\b(?P<name>[A-Za-z_]\w*)\s*\((?P<args>[^;{}()]*)\)" + _TRAILER + r"\{"
)
CALLED_NAME = re.compile(r"\b([A-Za-z_]\w*)\s*\(")

# Measured 455 on the tree that introduced this check. A parser change that
# drops most definitions would make the fixpoint find nothing and the scan
# pass vacuously, so the floor is asserted rather than trusted.
MIN_PARSED_DEFINITIONS = 300


def _end_of_block(text: str, open_index: int) -> int:
    """Index of the brace closing the one at `open_index`."""

    depth = 0
    index = open_index
    while index < len(text):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return index
        index += 1
    return len(text)


def call_edges() -> "dict[str, set[str]]":
    """Map every function defined under `src/fl/gfx` to the names it calls."""

    edges: dict[str, set[str]] = {}
    sources = sorted(GFX.glob("*.cpp.hpp")) + sorted(GFX.glob("*.h"))
    for source in sources:
        text = source.read_text(encoding="utf-8")
        for match in DEFINITION.finditer(text):
            name = match.group("name")
            if name in NOT_FUNCTIONS:
                continue
            open_index = text.index("{", match.end() - 1)
            body = text[open_index : _end_of_block(text, open_index)]
            called = edges.setdefault(name, set())
            for candidate in CALLED_NAME.findall(body):
                if candidate not in NOT_FUNCTIONS:
                    called.add(candidate)
    return edges


def forbidden_symbols() -> "set[str]":
    """Every name that can reach an iterative primitive, primitives included."""

    reaching = set(ITERATIVE_PRIMITIVES)
    edges = call_edges()
    grew = True
    while grew:
        grew = False
        for name, called in edges.items():
            if name not in reaching and (called & reaching):
                reaching.add(name)
                grew = True
    return reaching


def call_pattern(symbol: str) -> re.Pattern[str]:
    """Match a call to `symbol`, tolerating comments between name and paren.

    Deliberately a regex rather than a C++ token scan, and the trade is worth
    stating. A full tokenizer has to get raw strings, character literals and
    line continuations right; getting *that* wrong silently weakens the very
    guard it implements, and it is a lot of machinery for a check on five
    files we control.

    So this matches the identifier followed by an open paren, allowing
    whitespace and block or line comments in between -- which covers
    `nnls3 /* why */ (args)`. Two known limits:

    * A comment or string literal that itself contains `nnls3(` fails the
      test. That is a false positive, so it fails closed; the fix is obvious
      to whoever hits it.
    * Preprocessor tricks that split the identifier would evade it. Anyone
      doing that is deliberately defeating the guard, not tripping over it.

    Prose naming the symbol without a following paren -- as the comments in
    these files do -- does not match.
    """

    gap = r"(?:\s|/\*.*?\*/|//[^\n]*\n)*"
    return re.compile(r"\b" + re.escape(symbol) + gap + r"\(", re.S)


class TestNoIterativeSolverPerPixel(unittest.TestCase):
    def test_stage_files_exist(self: "TestNoIterativeSolverPerPixel") -> None:
        # Guards against the list silently going stale if a file is renamed:
        # a missing file would otherwise make this suite vacuously pass.
        for name in PER_PIXEL_STAGES:
            with self.subTest(stage=name):
                self.assertTrue((GFX / name).is_file(), f"{name} not found in {GFX}")

    def test_the_call_graph_is_not_vacuous(
        self: "TestNoIterativeSolverPerPixel",
    ) -> None:
        # A parser that matches nothing forbids nothing and passes everything.
        edges = call_edges()
        self.assertGreaterEqual(
            len(edges),
            MIN_PARSED_DEFINITIONS,
            f"only {len(edges)} function definitions parsed under {GFX}; the "
            "scan below would pass vacuously",
        )

    def test_the_derived_set_reaches_the_public_wrappers(
        self: "TestNoIterativeSolverPerPixel",
    ) -> None:
        # The defect this file exists to close. `nnls3` is private; these are
        # the names a caller can actually write, and the old hand-written list
        # had none of them.
        forbidden = forbidden_symbols()
        for symbol in ITERATIVE_PRIMITIVES:
            with self.subTest(symbol=symbol):
                self.assertIn(symbol, forbidden)
        for symbol in KNOWN_REACHING_WRAPPERS:
            with self.subTest(symbol=symbol):
                self.assertIn(
                    symbol,
                    forbidden,
                    f"{symbol} reaches an iterative solver and must be "
                    "forbidden on the per-pixel path; if it genuinely no "
                    "longer does, remove it from KNOWN_REACHING_WRAPPERS and "
                    "say why in the same commit",
                )

    def test_no_iterative_solver_on_the_per_pixel_path(
        self: "TestNoIterativeSolverPerPixel",
    ) -> None:
        forbidden = sorted(forbidden_symbols())
        for name in PER_PIXEL_STAGES:
            text = (GFX / name).read_text(encoding="utf-8")
            for symbol in forbidden:
                with self.subTest(stage=name, symbol=symbol):
                    self.assertIsNone(
                        call_pattern(symbol).search(text),
                        f"{name} calls {symbol} on the per-pixel path; "
                        "iterative solves belong at profile/cache build time "
                        "(A3/B11).",
                    )


if __name__ == "__main__":
    unittest.main()
