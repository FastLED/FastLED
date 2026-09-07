"""Algorithm-selection study for the P7 gamut mapper (#4041).

#4041 asks for a comparison of clipping, max-normalization, OKLCh
compression and a constrained solve, to "pick the smallest algorithm meeting
the A1 budget on embedded hardware". This is the harness behind that
comparison; `docs/color-gamut-algorithm-selection.md` records the result.

Each candidate maps an out-of-gamut XYZ target onto the device hull. They are
scored by CIEDE2000 against the P5 reference's own mapped result, so the
number answers "how far from the reference objective does this land", not
"is this a pleasing colour".
"""

from __future__ import annotations

import math
from dataclasses import dataclass

from ci.color_reference import (
    Matrix3,
    Xyz,
    _invert_3x3,
    _matvec,
    _oklch_from_xyz,
    _xyz_from_oklab,
    delta_e2000,
    xyz_to_lab,
)


D65_WHITE: Xyz = (0.9504559270516716, 1.0, 1.0890577507598784)
FEASIBILITY_TOLERANCE = 1e-12


@dataclass(frozen=True, slots=True)
class CandidateScore:
    """One algorithm's agreement with the reference over a vector set."""

    name: str
    worst_delta_e: float
    mean_delta_e: float
    vector_count: int


def emitter_matrix(primaries: tuple[tuple[float, float], ...]) -> Matrix3:
    """Columns are each emitter's XYZ at unit luminance."""

    columns: list[Xyz] = []
    for x, y in primaries:
        columns.append((x / y, 1.0, (1.0 - x - y) / y))
    return Matrix3(
        row0=(columns[0][0], columns[1][0], columns[2][0]),
        row1=(columns[0][1], columns[1][1], columns[2][1]),
        row2=(columns[0][2], columns[1][2], columns[2][2]),
    )


def is_feasible(inverse: Matrix3, xyz: Xyz) -> bool:
    """True when every emitter drive lies within [0, 1].

    Both bounds matter. Checking only the lower one accepts a target that
    needs drives above full scale -- too bright rather than too saturated --
    which no device can produce, and the mapper would then return it
    unchanged. The corpus's out-of-gamut vectors all fail on the lower bound
    (max drive observed: 0.81), so this does not move the recorded scores;
    it stops the harness from being wrong on a corpus that does contain them.
    """

    drives = _matvec(inverse, xyz)
    for drive in drives:
        if drive < -FEASIBILITY_TOLERANCE or drive > 1.0 + FEASIBILITY_TOLERANCE:
            return False
    return True


def map_clip(forward: Matrix3, inverse: Matrix3, xyz: Xyz) -> Xyz:
    """Clamp negative drives to zero. The cheapest thing that can be done."""

    drives = _matvec(inverse, xyz)
    clamped: Xyz = (
        max(drives[0], 0.0),
        max(drives[1], 0.0),
        max(drives[2], 0.0),
    )
    return _matvec(forward, clamped)


def map_max_normalize(forward: Matrix3, inverse: Matrix3, xyz: Xyz) -> Xyz:
    """Clamp, then scale so the largest drive is at most full scale."""

    drives = _matvec(inverse, xyz)
    clamped: Xyz = (
        max(drives[0], 0.0),
        max(drives[1], 0.0),
        max(drives[2], 0.0),
    )
    largest = max(clamped)
    if largest > 1.0:
        clamped = (clamped[0] / largest, clamped[1] / largest, clamped[2] / largest)
    return _matvec(forward, clamped)


def map_desaturate_to_neutral(forward: Matrix3, inverse: Matrix3, xyz: Xyz) -> Xyz:
    """Pull the target toward the equal-luminance neutral until it fits.

    A plausible cheap mapper that is *not* hue-preserving: the straight line
    to neutral in XYZ is not a line of constant hue.
    """

    if is_feasible(inverse, xyz):
        return xyz
    luminance = xyz[1]
    neutral: Xyz = (
        D65_WHITE[0] * luminance,
        D65_WHITE[1] * luminance,
        D65_WHITE[2] * luminance,
    )
    low, high = 0.0, 1.0
    for _ in range(40):
        middle = (low + high) / 2.0
        candidate: Xyz = (
            neutral[0] + middle * (xyz[0] - neutral[0]),
            neutral[1] + middle * (xyz[1] - neutral[1]),
            neutral[2] + middle * (xyz[2] - neutral[2]),
        )
        if is_feasible(inverse, candidate):
            low = middle
        else:
            high = middle
    return (
        neutral[0] + low * (xyz[0] - neutral[0]),
        neutral[1] + low * (xyz[1] - neutral[1]),
        neutral[2] + low * (xyz[2] - neutral[2]),
    )


def map_oklch_bisect(forward: Matrix3, inverse: Matrix3, xyz: Xyz) -> Xyz:
    """Hold OKLab lightness and hue; bisect chroma down until feasible.

    This is the reference's objective with the simplest possible search. The
    reference itself searches the zonotope globally because feasibility along
    the chroma ray is not guaranteed monotonic; bisection assumes it is.
    """

    if is_feasible(inverse, xyz):
        return xyz
    polar = _oklch_from_xyz(xyz)
    hue = math.radians(polar.hue_degrees)
    low, high = 0.0, polar.chroma
    for _ in range(30):
        chroma = (low + high) / 2.0
        candidate = _xyz_from_oklab(
            (polar.lightness, chroma * math.cos(hue), chroma * math.sin(hue))
        )
        if is_feasible(inverse, candidate):
            low = chroma
        else:
            high = chroma
    return _xyz_from_oklab((polar.lightness, low * math.cos(hue), low * math.sin(hue)))


CANDIDATES = {
    "clip": map_clip,
    "max-normalize": map_max_normalize,
    "desaturate-to-neutral": map_desaturate_to_neutral,
    "oklch-bisect": map_oklch_bisect,
}


def score_candidate(
    name: str,
    forward: Matrix3,
    inverse: Matrix3,
    cases: list[tuple[Xyz, Xyz]],
) -> CandidateScore:
    """Score one candidate against (target, reference_mapped) pairs."""

    worst = 0.0
    total = 0.0
    for target, reference in cases:
        mapped = CANDIDATES[name](forward, inverse, target)
        # CIEDE2000 is only defined for non-negative XYZ. Raise rather than
        # skip: silently dropping a case shrinks the denominator, and a
        # candidate that fails often would then be scored on the subset it
        # happens to handle and look better than one that handles everything.
        if min(mapped) < 0.0:
            raise ValueError(
                f"candidate {name!r} produced negative XYZ {mapped} for "
                f"target {target}; it cannot be scored on a partial corpus"
            )
        if min(reference) < 0.0:
            raise ValueError(
                f"reference mapped result {reference} is negative for target "
                f"{target}; the corpus is outside the CIELAB domain here"
            )
        delta = delta_e2000(
            xyz_to_lab(mapped, D65_WHITE), xyz_to_lab(reference, D65_WHITE)
        )
        total += delta
        if delta > worst:
            worst = delta
    counted = len(cases)
    mean = total / counted if counted else 0.0
    return CandidateScore(name, worst, mean, counted)
