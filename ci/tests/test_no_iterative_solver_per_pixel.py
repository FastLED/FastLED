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
# Without these, `if (...) { ... }` parses as a function named `if`, every
# branch in the tree becomes a call edge, and the fixpoint swallows the whole
# namespace -- measured 230 names including `blur1d` and `fill_solid`.
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

_TRAILER = r"(?:\s|const|noexcept|FL_NO_EXCEPT|override|final|mutable|volatile)*"
# A constructor initialiser list, narrowly: `: name(...), name(...)`. Anchored
# on the colon and on `name(` entries rather than "anything up to the brace".
# The permissive form was tried and is wrong -- a ternary `f(x) : g(y)` then
# matches and runs to a distant brace, swallowing the real definition that
# follows. Measured: the closure collapsed from 21 names to 5 and lost
# `project_to_hull` itself.
# One level of nesting inside an entry, because the interesting case is
# exactly `: ok(solve_wx_overdrive(o))` -- a solver reached from an
# initialiser and never mentioned in the body. Without the nesting the entry
# does not match, the whole definition does not match, and the constructor
# contributes no edges at all; a mutation covers this.
_INIT_ARG = r"[^;{}()]*(?:\([^;{}()]*\)[^;{}()]*)*"
# `: member(expr)` and `: member{expr}` both initialise, so both are entries.
_INIT_ENTRY = r"[A-Za-z_]\w*\s*(?:\(" + _INIT_ARG + r"\)|\{" + _INIT_ARG + r"\})"
_CTOR_INIT = r"(?::\s*" + _INIT_ENTRY + r"(?:\s*,\s*" + _INIT_ENTRY + r")*\s*)?"

DEFINITION = re.compile(
    r"\b(?P<name>[A-Za-z_]\w*)\s*\((?P<args>[^;{}()]*)\)"
    + _TRAILER
    + r"(?P<init>"
    + _CTOR_INIT
    + r")"
    + r"\{"
)
CALLED_NAME = re.compile(r"\b([A-Za-z_]\w*)\s*\(")

# Definition syntax `DEFINITION` cannot parse. A function written this way
# contributes no call edges, so a solver wrapper spelled
# `auto wrap(...) -> bool { nnls3(...); }` would never enter the derived set
# and a stage could call it freely. The vacuity floor below does not catch
# that -- one missed definition out of hundreds moves no count. So the forms
# are detected and the test fails, rather than silently losing coverage:
# extend `DEFINITION` to cover the form, then drop it from here.
UNSUPPORTED_DEFINITION_SYNTAX = (
    ("trailing return type", re.compile(r"\)" + _TRAILER + r"->[^;{}]*\{")),
    ("requires clause", re.compile(r"\)" + _TRAILER + r"requires\b[^;{}]*\{")),
    ("attribute before body", re.compile(r"\)" + _TRAILER + r"\[\[[^\]]*\]\]\s*\{")),
)

# Measured 461 on the tree that introduced this check. A parser change that
# drops most definitions would make the fixpoint find nothing and the scan
# pass vacuously, so the floor is asserted rather than trusted.
MIN_PARSED_DEFINITIONS = 300


def scanned_sources() -> "list[Path]":
    """The translation units the call graph is built from."""

    return sorted(GFX.glob("*.cpp.hpp")) + sorted(GFX.glob("*.h"))


def strip_comments_and_literals(text: str) -> str:
    """Blank out comments and string/char literals, preserving offsets loosely.

    Necessary, not cosmetic. Prose is full of things that read as code: the
    arrow diagram `(R,0,0)->(R,0,0,0)` in `rgbw_colorimetric.h` made the
    trailing-return detector fire on a file that has no trailing-return
    definition, and a comment in `lookup_lut` mentioning `build_lut()` put
    `lookup_lut` in the forbidden set -- a per-pixel LUT lookup, which A3
    explicitly permits ("matrix + clamp/compress, optionally LUT-backed").
    Forbidding it would have blocked the very thing P7 asks for.
    """

    out: list[str] = []
    index = 0
    length = len(text)
    while index < length:
        char = text[index]
        if char == "/" and index + 1 < length and text[index + 1] == "*":
            close = text.find("*/", index + 2)
            index = length if close < 0 else close + 2
            out.append(" ")
        elif char == "/" and index + 1 < length and text[index + 1] == "/":
            close = text.find("\n", index)
            index = length if close < 0 else close
            out.append(" ")
        elif char == '"' or char == "'":
            quote = char
            index += 1
            while index < length:
                if text[index] == "\\":
                    index += 2
                    continue
                if text[index] == quote:
                    index += 1
                    break
                index += 1
            out.append('""')
        else:
            out.append(char)
            index += 1
    return "".join(out)


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
    for source in scanned_sources():
        text = strip_comments_and_literals(source.read_text(encoding="utf-8"))
        for match in DEFINITION.finditer(text):
            name = match.group("name")
            if name in NOT_FUNCTIONS:
                continue
            open_index = text.index("{", match.end() - 1)
            body = text[open_index : _end_of_block(text, open_index)]
            # The initialiser list counts as part of the constructor: a call
            # in `Foo(x) : mResult(solve_rgbcct(...)) {}` runs every time the
            # object is built, and scanning only from the brace would miss it.
            initialiser = match.group("init") or ""
            called = edges.setdefault(name, set())
            for candidate in CALLED_NAME.findall(initialiser + body):
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
    `nnls3 /* why */ (args)`. One known limit:

    Callers pass text with comments and literals already stripped, so
    prose mentioning a symbol cannot trip it either way. One known limit
    remains: preprocessor tricks that split the identifier would evade
    it, and anyone doing that is deliberately defeating the guard, not
    tripping over it.
    """

    gap = r"(?:\s|/\*.*?\*/|//[^\n]*\n)*"
    # `Name(` covers a call and a temporary. The optional identifier covers a
    # variable declaration, `InitProbe probe(args)`, which is how a
    # constructor is usually reached and which `Name(` alone does not match --
    # found by a mutation that the first version of this guard let through.
    declarator = r"(?:[A-Za-z_]\w*" + gap + r")?"
    # `(` or `{`: `InitProbe probe{args}` constructs exactly as
    # `InitProbe probe(args)` does, and matching only the paren let a stage
    # build a forbidden type unchallenged.
    #
    # `symbol` comes from the derived set -- names parsed out of this repo's
    # own sources -- not from anything a caller supplies, and it is escaped
    # regardless. The surrounding parts are fixed literals.
    return re.compile(
        r"\b" + re.escape(symbol) + r"(?:\s+|" + gap + r")" + declarator + r"[({]",
        re.S,
    )


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

    def test_every_definition_form_present_is_parseable(
        self: "TestNoIterativeSolverPerPixel",
    ) -> None:
        # A definition the parser cannot see contributes no call edges, so a
        # solver wrapper written that way never joins the forbidden set and a
        # stage may call it freely. The vacuity floor does not catch this --
        # one missed definition out of hundreds moves no count. Fail loudly
        # instead of losing coverage quietly.
        for label, pattern in UNSUPPORTED_DEFINITION_SYNTAX:
            for source in scanned_sources():
                text = strip_comments_and_literals(source.read_text(encoding="utf-8"))
                with self.subTest(form=label, source=source.name):
                    self.assertIsNone(
                        pattern.search(text),
                        f"{source.name} defines a function using a {label}, "
                        "which DEFINITION cannot parse -- its call edges are "
                        "invisible to the forbidden-set derivation. Extend "
                        "DEFINITION to cover the form, then remove it from "
                        "UNSUPPORTED_DEFINITION_SYNTAX.",
                    )

    def test_no_iterative_solver_on_the_per_pixel_path(
        self: "TestNoIterativeSolverPerPixel",
    ) -> None:
        forbidden = sorted(forbidden_symbols())
        for name in PER_PIXEL_STAGES:
            text = strip_comments_and_literals((GFX / name).read_text(encoding="utf-8"))
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
