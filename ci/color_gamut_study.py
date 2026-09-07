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

from typeguard import typechecked

from ci.color_reference import (
    Matrix3,
    Oklch,
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
    """Clamp each drive into [0, 1]. The cheapest thing that can be done.

    Both bounds, not just the lower one: a target can be outside the hull for
    being too bright as well as too saturated, and a mapper that returns an
    over-unity drive has not mapped anything.
    """

    drives = _matvec(inverse, xyz)
    clamped: Xyz = (
        min(max(drives[0], 0.0), 1.0),
        min(max(drives[1], 0.0), 1.0),
        min(max(drives[2], 0.0), 1.0),
    )
    return _matvec(forward, clamped)


def map_max_normalize(forward: Matrix3, inverse: Matrix3, xyz: Xyz) -> Xyz:
    """Clamp negatives, then scale so the largest drive is at most full scale.

    The normalization makes the upper clamp unnecessary here: scaling by the
    largest drive already brings everything to at most 1.
    """

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
    pulled: Xyz = (
        neutral[0] + low * (xyz[0] - neutral[0]),
        neutral[1] + low * (xyz[1] - neutral[1]),
        neutral[2] + low * (xyz[2] - neutral[2]),
    )
    # Pulling toward an equal-luminance neutral cannot fix an over-bright
    # target, since the neutral is over-bright too. Clamp the drives so the
    # candidate still satisfies the mapper contract.
    pulled_drives = _matvec(inverse, pulled)
    bounded: Xyz = (
        min(max(pulled_drives[0], 0.0), 1.0),
        min(max(pulled_drives[1], 0.0), 1.0),
        min(max(pulled_drives[2], 0.0), 1.0),
    )
    return _matvec(forward, bounded)


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
    lightness = attainable_lightness(inverse, polar.lightness)
    low, high = 0.0, polar.chroma
    for _ in range(30):
        chroma = (low + high) / 2.0
        candidate = _xyz_from_oklab(
            (lightness, chroma * math.cos(hue), chroma * math.sin(hue))
        )
        if is_feasible(inverse, candidate):
            low = chroma
        else:
            high = chroma
    return _xyz_from_oklab((lightness, low * math.cos(hue), low * math.sin(hue)))


@typechecked
def attainable_lightness(inverse: Matrix3, lightness: float) -> float:
    """Largest neutral lightness the device can reach, at or below `lightness`.

    Chroma compression cannot rescue a target that is too *bright*: at zero
    chroma the point is still outside the hull, so the bisection converges on
    an infeasible answer. The reference clamps lightness to the attainable
    neutral interval before compressing chroma, and this is that clamp.
    """

    if is_feasible(inverse, _xyz_from_oklab((lightness, 0.0, 0.0))):
        return lightness
    low, high = 0.0, lightness
    for _ in range(30):
        middle = (low + high) / 2.0
        if is_feasible(inverse, _xyz_from_oklab((middle, 0.0, 0.0))):
            low = middle
        else:
            high = middle
    return low


Q16_ONE = 65536


@typechecked
def integer_cube_root(value: int) -> int:
    """Largest integer whose cube does not exceed `value`.

    Mirrors `fl::icbrt64` in `src/fl/math/fixed_point/icbrt.h`, which is what
    the embedded mapper calls. Modelled here so the accuracy claim behind that
    implementation is measured rather than asserted.
    """

    if value < 0:
        raise ValueError(f"cube root domain is non-negative, got {value}")
    if value == 0:
        return 0
    root = 1 << ((value.bit_length() + 2) // 3)
    while True:
        lower = (2 * root + value // (root * root)) // 3
        if lower >= root:
            return root
        root = lower


@typechecked
def cbrt_q16(value: float, ulp_error: int = 0) -> float:
    """Signed cube root as the s16.16 path computes it.

    For a Q16 raw `r` standing for r/2^16, the root `y` satisfies
    (y/2^16)^3 = r/2^16, so y^3 = r * 2^32 -- the root is the integer cube
    root of the raw value shifted left by 32. `ulp_error` perturbs the result
    to measure how much accuracy the mapper actually needs.
    """

    sign = -1.0 if value < 0.0 else 1.0
    raw = round(abs(value) * Q16_ONE)
    root = integer_cube_root(raw << 32) + ulp_error
    return sign * max(root, 0) / Q16_ONE


_LMS_FROM_XYZ = (
    (0.8190224432164319, 0.3619062562801221, -0.12887378261216414),
    (0.0329836671980271, 0.9292868468965546, 0.03614466816999844),
    (0.048177199566046255, 0.26423952494422764, 0.6335478258136937),
)
_OKLAB_FROM_LMS_ROOT = (
    (0.2104542553, 0.7936177850, -0.0040720468),
    (1.9779984951, -2.4285922050, 0.4505937099),
    (0.0259040371, 0.7827717662, -0.8086757660),
)


@typechecked
def oklch_q16(xyz: Xyz, ulp_error: int = 0) -> Oklch:
    """(lightness, chroma, hue degrees) with every stage quantized to Q16.

    The precision study previously reached OKLCh through float64 cube roots
    even while quantizing everything around them, which measured the stages
    on either side of the root and not the root itself. This closes that gap.
    """

    def quantize(value: float) -> float:
        return round(value * Q16_ONE) / Q16_ONE

    lms = [
        quantize(sum(_LMS_FROM_XYZ[i][j] * xyz[j] for j in range(3))) for i in range(3)
    ]
    root = [quantize(cbrt_q16(value, ulp_error)) for value in lms]
    lab = [
        quantize(sum(_OKLAB_FROM_LMS_ROOT[i][j] * root[j] for j in range(3)))
        for i in range(3)
    ]
    return Oklch(
        lab[0],
        math.hypot(lab[1], lab[2]),
        math.degrees(math.atan2(lab[2], lab[1])) % 360.0,
    )


@typechecked
def map_oklch_bounded(
    forward: Matrix3, inverse: Matrix3, xyz: Xyz, iterations: int
) -> Xyz:
    """OKLCh chroma compression with a fixed iteration count.

    The distinction from an iterative *solver* matters for A3/B11: this runs
    a constant number of halvings and returns, rather than looping until a
    convergence criterion is met. Its cost is known at compile time.
    """

    if is_feasible(inverse, xyz):
        return xyz
    polar = _oklch_from_xyz(xyz)
    hue = math.radians(polar.hue_degrees)
    # Lightness first: reducing chroma cannot bring an over-bright target back
    # into the hull, so without this the bisection converges on an infeasible
    # answer -- at zero chroma the point is still outside.
    lightness = attainable_lightness(inverse, polar.lightness)
    low, high = 0.0, polar.chroma
    for _ in range(iterations):
        chroma = (low + high) / 2.0
        candidate = _xyz_from_oklab(
            (lightness, chroma * math.cos(hue), chroma * math.sin(hue))
        )
        if is_feasible(inverse, candidate):
            low = chroma
        else:
            high = chroma
    return _xyz_from_oklab((lightness, low * math.cos(hue), low * math.sin(hue)))


def map_oklch_bisect_8(forward: Matrix3, inverse: Matrix3, xyz: Xyz) -> Xyz:
    """The selected embedded algorithm: eight halvings, no table."""

    return map_oklch_bounded(forward, inverse, xyz, 8)


CANDIDATES = {
    "clip": map_clip,
    "max-normalize": map_max_normalize,
    "desaturate-to-neutral": map_desaturate_to_neutral,
    "oklch-bisect": map_oklch_bisect,
    "oklch-bisect-8": map_oklch_bisect_8,
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
