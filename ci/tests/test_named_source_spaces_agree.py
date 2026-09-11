"""The named source spaces must mean the same thing in C++ and in the reference.

`src/fl/gfx/color_profile.h` hard-codes the chromaticities for sRGB/BT.709,
Display P3 and BT.2020. `ci/color_reference.py` and
`ci/color_reference_corpus.py` hard-code them again, and the golden corpus --
the thing P6's ΔE budget is measured against -- is generated from *those*
copies.

Nothing checked the two agreed. A divergence would not fail: both sides stay
internally consistent, the corpus is generated for one set of primaries, the
pipeline renders with another, and the budget case goes on passing while
comparing against the wrong reference. That is worse than the drift in
FastLED#4330 / #4335 / #4337, where a guard merely stopped covering something.

#4034 calls these names a registry ("source-color names owned by the canonical
ledmapper spec"), so this pins the registry rather than either copy: the
values below are the standard-defined ones, and both implementations are
checked against them.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

from ci.color_reference import RgbPrimaries


PROJECT_ROOT = Path(__file__).resolve().parents[2]
HEADER = PROJECT_ROOT / "src" / "fl" / "gfx" / "color_profile.h"

# Standard-defined, to four decimal places, from the specifications rather
# than from either implementation:
#   sRGB / BT.709 -- IEC 61966-2-1, ITU-R BT.709-6
#   Display P3    -- SMPTE RP 431-2 primaries with a D65 white
#   BT.2020       -- ITU-R BT.2020-2
D65 = (0.3127, 0.3290)
EXPECTED = {
    "srgb": ((0.6400, 0.3300), (0.3000, 0.6000), (0.1500, 0.0600), D65),
    "display_p3": ((0.6800, 0.3200), (0.2650, 0.6900), (0.1500, 0.0600), D65),
    "bt2020": ((0.7080, 0.2920), (0.1700, 0.7970), (0.1310, 0.0460), D65),
}

# `Chromaticity(.680f, .320f)` and `Chromaticity(0.6800, 0.3200)` both.
kChromaticity = re.compile(r"Chromaticity\(\s*(-?[0-9.]+)f?\s*,\s*(-?[0-9.]+)f?\s*\)")


def _pairs(text: str) -> list[tuple[float, float]]:
    return [(float(a), float(b)) for a, b in kChromaticity.findall(text)]


def _cxx_space(name: str) -> list[tuple[float, float]]:
    """The chromaticities belonging to one named space's definition.

    Anchored on the `constexpr` declaration and read forward, because the
    values sit on the `return` line below it -- and because every one of those
    return lines also mentions `d65()`, so matching on the name alone finds
    whichever definition comes first in the file rather than the one asked
    for.
    """

    lines = HEADER.read_text(encoding="utf-8").splitlines()
    for index, line in enumerate(lines):
        if name in line and "constexpr" in line:
            window = "\n".join(lines[index : index + 4])
            found = _pairs(window)
            if found:
                return found
    raise AssertionError(f"no definition of {name} in {HEADER.name}")


class TestNamedSourceSpacesAgree(unittest.TestCase):
    def test_cxx_named_spaces_match_the_standards(self) -> None:
        # srgbPrimaries() carries only r/g/b; its white comes from d65().
        for cxx_name, key in (
            ("srgbPrimaries()", "srgb"),
            ("displayP3()", "display_p3"),
            ("bt2020()", "bt2020"),
        ):
            with self.subTest(space=cxx_name):
                found = _cxx_space(cxx_name)
                self.assertEqual(len(found), 3, msg=f"{cxx_name}: expected r/g/b")
                self.assertEqual(tuple(found), EXPECTED[key][:3])

    def test_cxx_d65_matches(self) -> None:
        # All three spaces take their white by calling `d65()`, so checking it
        # once covers every white in the header. Only the first pair in the
        # window is d65's own -- it is a one-liner, and the window runs into
        # the definition below it.
        self.assertEqual(_cxx_space("d65()")[0], D65)

    def test_reference_named_spaces_match_the_standards(self) -> None:
        # The corpus generator is what the golden artifact is built from, so
        # this is the copy the ΔE budget is actually measured against.
        bt709 = RgbPrimaries.bt709()
        self.assertEqual((bt709.red.x, bt709.red.y), EXPECTED["srgb"][0])
        self.assertEqual((bt709.green.x, bt709.green.y), EXPECTED["srgb"][1])
        self.assertEqual((bt709.blue.x, bt709.blue.y), EXPECTED["srgb"][2])
        self.assertEqual((bt709.white.x, bt709.white.y), D65)

        corpus = (PROJECT_ROOT / "ci" / "color_reference_corpus.py").read_text(
            encoding="utf-8"
        )
        block = corpus[corpus.index("def _source_profiles") :]
        for key in ("display_p3", "bt2020"):
            with self.subTest(space=key):
                start = block.index(f'"{key}"')
                found = _pairs(block[start : start + 400])[:3]
                self.assertEqual(tuple(found), EXPECTED[key][:3])


if __name__ == "__main__":
    unittest.main()
