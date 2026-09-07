"""White-preferred allocation for devices with a white emitter (C3, #4041).

`docs/color-gamut-algorithm-selection.md` covers only the three-emitter
device. A white emitter makes the emitter matrix wide: the preimage of a
target is no longer unique, and C3 asks for the white-preferred one -- as
much light from the white emitter as the target allows, since it is the
efficient one and usually the one with the best colour rendering.

The P5 reference finds that by enumerating vertices: every way of choosing
three free emitters and pinning the rest to 0 or 1, solved and filtered.
That is bounded but not small, and A3/B11 forbid anything resembling a
search on the per-pixel path.

This harness asks whether the enumeration is necessary. For one white
emitter it is not -- see `allocate_one_white`. For two it is; the reduction
that looks like it should work is measured failing here rather than left for
someone to try.
"""

from __future__ import annotations

import itertools
from dataclasses import dataclass

from ci.color_reference import Matrix3, Xyz, _matvec


@dataclass(frozen=True, slots=True)
class WhiteLevelRange:
    """The white drives that keep every RGB drive inside [0, 1]."""

    lowest: float
    highest: float


@dataclass(frozen=True, slots=True)
class WhitePreferredDrives:
    """One device's drives under the C3 white-preferred policy."""

    red: float
    green: float
    blue: float
    white: float


@dataclass(frozen=True, slots=True)
class TwoWhiteLevels:
    """Drives for a pair of white emitters."""

    first: float
    second: float

    @property
    def total(self: "TwoWhiteLevels") -> float:
        return self.first + self.second


# Slack on the drive bounds. The allocation is compared against a float64
# reference, so this only has to absorb float64 rounding.
DRIVE_TOLERANCE = 1e-9


def emitter_column(x: float, y: float) -> Xyz:
    """An emitter's XYZ at unit luminance, from its chromaticity."""

    return (x / y, 1.0, (1.0 - x - y) / y)


def rgb_matrix(columns: list[Xyz]) -> Matrix3:
    """The 3x3 matrix whose columns are the three RGB emitters."""

    return Matrix3(
        row0=(columns[0][0], columns[1][0], columns[2][0]),
        row1=(columns[0][1], columns[1][1], columns[2][1]),
        row2=(columns[0][2], columns[1][2], columns[2][2]),
    )


def white_level_range(
    inverse: Matrix3, target: Xyz, white: Xyz
) -> WhiteLevelRange | None:
    """The interval of white drives that keeps the RGB drives in [0, 1].

    This is the whole trick. With the white emitter at drive w, the RGB
    drives that make up the difference are

        d(w) = M^-1 . target  -  w * (M^-1 . white)

    which is *affine in w*. So each of the six bounds on the three RGB
    drives is a single inequality in w, and the feasible set is one
    interval -- computed directly, with no search over vertices.

    None when the intersection is empty, which means no white level lets the
    RGB emitters cover the remainder: the target is outside the hull.
    """

    at_zero = _matvec(inverse, target)
    per_white = _matvec(inverse, white)
    low, high = 0.0, 1.0
    for index in range(3):
        slope = per_white[index]
        if slope > 1e-15:
            high = min(high, at_zero[index] / slope)
            low = max(low, (at_zero[index] - 1.0) / slope)
        elif slope < -1e-15:
            low = max(low, at_zero[index] / slope)
            high = min(high, (at_zero[index] - 1.0) / slope)
        elif (
            at_zero[index] < -DRIVE_TOLERANCE or at_zero[index] > 1.0 + DRIVE_TOLERANCE
        ):
            # White cannot move this drive at all, and it is already out.
            return None
    if low > high + 1e-12:
        return None
    return WhiteLevelRange(low, high)


def allocate_one_white(
    inverse: Matrix3, target: Xyz, white: Xyz
) -> WhitePreferredDrives | None:
    """White-preferred drives for an RGB + one white device.

    None when the target is outside the hull.

    White-preferred means the top of the interval above, so this is closed
    form: one matrix multiply for the target, one for the white column (which
    a real implementation would precompute at bind time), then six compares.
    """

    span = white_level_range(inverse, target, white)
    if span is None:
        return None
    level = span.highest
    at_zero = _matvec(inverse, target)
    per_white = _matvec(inverse, white)
    clamped: list[float] = []
    for index in range(3):
        drive = at_zero[index] - level * per_white[index]
        if drive < -DRIVE_TOLERANCE or drive > 1.0 + DRIVE_TOLERANCE:
            return None
        clamped.append(min(max(drive, 0.0), 1.0))
    return WhitePreferredDrives(clamped[0], clamped[1], clamped[2], level)


def most_white_two(
    inverse: Matrix3, target: Xyz, first: Xyz, second: Xyz
) -> TwoWhiteLevels | None:
    """Largest total white for two white emitters, by exact 2D enumeration.

    With two whites the problem stops being one-dimensional: maximizing
    w1 + w2 subject to the RGB drives staying in [0, 1] is a linear program
    over a polygon. This solves it the slow, certain way -- every pair of
    constraints intersected, infeasible points discarded -- so that cheaper
    candidates have something to be wrong against.
    """

    at_zero = _matvec(inverse, target)
    from_first = _matvec(inverse, first)
    from_second = _matvec(inverse, second)
    # Each constraint is a*w1 + b*w2 <= c.
    constraints: list[tuple[float, float, float]] = []
    for index in range(3):
        constraints.append((from_first[index], from_second[index], at_zero[index]))
        constraints.append(
            (-from_first[index], -from_second[index], 1.0 - at_zero[index])
        )
    constraints.extend(
        [(-1.0, 0.0, 0.0), (1.0, 0.0, 1.0), (0.0, -1.0, 0.0), (0.0, 1.0, 1.0)]
    )

    def inside(w1: float, w2: float) -> bool:
        return all(a * w1 + b * w2 <= c + 1e-7 for a, b, c in constraints)

    best: TwoWhiteLevels | None = None
    for (a1, b1, c1), (a2, b2, c2) in itertools.combinations(constraints, 2):
        determinant = a1 * b2 - a2 * b1
        if abs(determinant) < 1e-12:
            continue
        w1 = (c1 * b2 - c2 * b1) / determinant
        w2 = (a1 * c2 - a2 * c1) / determinant
        if not inside(w1, w2):
            continue
        if best is None or w1 + w2 > best.total + 1e-12:
            best = TwoWhiteLevels(w1, w2)
    return best


def best_single_white(
    inverse: Matrix3, target: Xyz, first: Xyz, second: Xyz
) -> float | None:
    """Most white obtainable while using only one of the two white emitters.

    The tempting reduction: run the one-white closed form twice and keep the
    better answer. `ci/tests/test_color_rgbw_study.py` measures how badly
    this loses, and why.
    """

    best: float | None = None
    for white in (first, second):
        span = white_level_range(inverse, target, white)
        if span is None:
            continue
        if best is None or span.highest > best:
            best = span.highest
    return best
