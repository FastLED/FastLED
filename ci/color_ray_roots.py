"""Exact feasible chroma along one ray, by polynomial roots (P7, #4041).

`docs/color-gamut-algorithm-selection.md` records that the shipped eight-halving
chroma bisection is only a valid *search* if the feasible chroma along a
fixed-lightness, fixed-hue ray is a single interval, and that the claim rests
on sampling: 1.9 million feasibility evaluations on the three-emitter device
and 7.5 million more across the wide hulls, none of which found a disconnected
ray. The report names what is missing:

    What none of it supplies is option 2, an analytic bound. The claim
    remains empirical.

Sampling cannot supply it, and not merely for want of density. The report's own
method note explains why an answer is available without sampling at all: an
inverse-OKLab fixed-lightness, fixed-hue ray is *cubic* in chroma. Writing that
out, each emitter drive along the ray is

    d_i(c) = sum_j K_ij * (p_j + q_j * c)^3

with `p_j` fixed by the lightness, `q_j` by the hue, and `K` the product of the
inverse emitter matrix with the inverse OKLab LMS matrix. Expanding the cube
gives an ordinary cubic in `c` per drive, so `{c : 0 <= d_i(c) <= 1}` is decided
by the real roots of `d_i` and `d_i - 1` rather than by how finely `c` was
walked. The feasible set is then exact for that ray: no gap can hide between
two samples, however narrow.

That is what this module computes. It answers the ray exactly and leaves the
lightness/hue grid sampled, which is the honest split -- and it is enough,
because the thing the sampled sweeps could not see turns out to be there.

Two limits on "exact", stated here rather than left to be discovered:

* it is exact *given* that `real_roots` returns every real root. A closed-form
  cubic loses accuracy on a near-double root, and can merge two roots that are
  closer together than the polished result can separate. `feasible_runs`
  therefore unions the roots with a uniform guard grid, so the worst this can
  degrade to is the resolution of a sampled scan at that grid's density --
  never worse, which is the point of carrying it;
* an exact tangency -- a drive touching a bound with even multiplicity -- is
  not resolvable in floating point at all. Horner at a double root returns
  something of order 1e-18 with an arbitrary sign. `runs_from_cubics`
  classifies cut points so that such a touch is reported where the arithmetic
  is exact, but a grid does not land on one anyway: tangency is measure-zero
  in lightness and hue, and what a grid meets is the near-tangency beside it,
  which is an ordinary narrow interval and is exactly how this study's wedge
  presents.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

from typeguard import typechecked

from ci.color_reference import Matrix3, _invert_3x3
from ci.color_wide_hull_study import ChromaInterval, RayScan


# The two matrices `color_reference._xyz_from_oklab` inverts on every call.
# Named here because the cubic factorisation needs their inverses as
# coefficients rather than as a function; `test_the_factorisation_matches_the
# _reference_conversion` pins the two against each other so a change to one
# cannot drift from the other silently.
_OKLAB_FROM_LMS_ROOT = Matrix3(
    (0.2104542553, 0.7936177850, -0.0040720468),
    (1.9779984951, -2.4285922050, 0.4505937099),
    (0.0259040371, 0.7827717662, -0.8086757660),
)
_LMS_FROM_XYZ = Matrix3(
    (0.8190224432164319, 0.3619062562801221, -0.12887378261216414),
    (0.0329836671980271, 0.9292868468965546, 0.03614466816999844),
    (0.048177199566046255, 0.26423952494422764, 0.6335478258136937),
)

# A cubic whose leading coefficient is this much smaller than its largest is
# solved as a quadratic instead. Cardano's formula divides by the leading
# coefficient, so keeping a numerically-zero one loses accuracy in the roots
# that do exist rather than finding an extra one.
_DEGENERACY_RATIO = 1e-12

# Roots are polished by this many Newton steps. The closed form is accurate to
# a few ULP on well-separated roots and much worse on near-double ones, which
# is exactly the case this study is about.
_POLISH_STEPS = 4


@typechecked
@dataclass(frozen=True, slots=True)
class RayCubic:
    """One emitter drive as a cubic in chroma: `cube*c^3 + ... + constant`."""

    cube: float
    square: float
    linear: float
    constant: float


@typechecked
@dataclass(frozen=True, slots=True)
class DisconnectedRay:
    """A ray whose feasible chroma is more than one interval.

    `gap` is what a bisection bracketed at the neutral axis gives up: it stops
    at the end of the first interval and never sees the rest.
    """

    lightness: float
    hue_degrees: float
    intervals: tuple[ChromaInterval, ...]
    gap: float


def _rows(matrix: Matrix3) -> tuple[tuple[float, ...], ...]:
    return (matrix.row0, matrix.row1, matrix.row2)


# Inverted once. `ray_cubics` is called a few hundred thousand times by a
# sweep, and inverting these two per call dominated everything else in it.
_LMS_ROOT_FROM_OKLAB = _rows(_invert_3x3(_OKLAB_FROM_LMS_ROOT))
_XYZ_FROM_LMS = _rows(_invert_3x3(_LMS_FROM_XYZ))


@typechecked
def ray_cubics(
    inverse: Matrix3, lightness: float, hue_degrees: float
) -> tuple[RayCubic, ...]:
    """The three drive polynomials for one fixed-lightness, fixed-hue ray."""

    lms_root_from_oklab = _LMS_ROOT_FROM_OKLAB
    xyz_from_lms = _XYZ_FROM_LMS
    drive_from_xyz = _rows(inverse)

    radians = math.radians(hue_degrees)
    cosine = math.cos(radians)
    sine = math.sin(radians)

    # The LMS cube roots are affine in chroma: constant part from lightness,
    # slope from the hue direction.
    offsets: list[float] = []
    slopes: list[float] = []
    for axis in range(3):
        row = lms_root_from_oklab[axis]
        offsets.append(row[0] * lightness)
        slopes.append(row[1] * cosine + row[2] * sine)

    # Fold the two linear stages after the cube into one matrix so the cube is
    # the only nonlinearity between chroma and drive.
    mixing: list[list[float]] = []
    for drive in range(3):
        row: list[float] = []
        for axis in range(3):
            total = 0.0
            for middle in range(3):
                total += drive_from_xyz[drive][middle] * xyz_from_lms[middle][axis]
            row.append(total)
        mixing.append(row)

    cubics: list[RayCubic] = []
    for drive in range(3):
        cube = 0.0
        square = 0.0
        linear = 0.0
        constant = 0.0
        for axis in range(3):
            weight = mixing[drive][axis]
            offset = offsets[axis]
            slope = slopes[axis]
            cube += weight * slope * slope * slope
            square += weight * 3.0 * offset * slope * slope
            linear += weight * 3.0 * offset * offset * slope
            constant += weight * offset * offset * offset
        cubics.append(RayCubic(cube, square, linear, constant))
    return tuple(cubics)


@typechecked
def evaluate(cubic: RayCubic, chroma: float) -> float:
    """Horner evaluation of one drive polynomial."""

    value = cubic.cube
    value = value * chroma + cubic.square
    value = value * chroma + cubic.linear
    value = value * chroma + cubic.constant
    return value


def _polish(cubic: RayCubic, offset: float, root: float) -> float:
    value = root
    for _ in range(_POLISH_STEPS):
        residual = evaluate(cubic, value) - offset
        slope = (3.0 * cubic.cube * value + 2.0 * cubic.square) * value + cubic.linear
        if slope == 0.0:
            break
        value -= residual / slope
    return value


def _quadratic_roots(square: float, linear: float, constant: float) -> list[float]:
    if square == 0.0:
        if linear == 0.0:
            return []
        return [-constant / linear]
    discriminant = linear * linear - 4.0 * square * constant
    if discriminant < 0.0:
        return []
    root = math.sqrt(discriminant)
    return [(-linear + root) / (2.0 * square), (-linear - root) / (2.0 * square)]


@typechecked
def real_roots(cubic: RayCubic, offset: float) -> tuple[float, ...]:
    """Every real `c` with `cubic(c) == offset`, closed form then polished."""

    magnitudes = (
        abs(cubic.cube),
        abs(cubic.square),
        abs(cubic.linear),
        abs(cubic.constant - offset),
    )
    largest = max(magnitudes)
    if largest == 0.0:
        return ()
    if abs(cubic.cube) < _DEGENERACY_RATIO * largest:
        found = _quadratic_roots(cubic.square, cubic.linear, cubic.constant - offset)
        polished: list[float] = []
        for root in found:
            polished.append(_polish(cubic, offset, root))
        return tuple(polished)

    square = cubic.square / cubic.cube
    linear = cubic.linear / cubic.cube
    constant = (cubic.constant - offset) / cubic.cube
    shift = square / 3.0
    depressed_linear = linear - square * shift
    depressed_constant = constant - shift * linear + 2.0 * shift * shift * shift
    half = depressed_constant / 2.0
    third = depressed_linear / 3.0
    discriminant = half * half + third * third * third

    raw: list[float] = []
    if discriminant > 0.0:
        root = math.sqrt(discriminant)
        first = -half + root
        second = -half - root
        raw.append(
            math.copysign(abs(first) ** (1.0 / 3.0), first)
            + math.copysign(abs(second) ** (1.0 / 3.0), second)
            - shift
        )
    elif discriminant == 0.0:
        single = math.copysign(abs(-half) ** (1.0 / 3.0), -half)
        raw.append(2.0 * single - shift)
        raw.append(-single - shift)
    else:
        radius = math.sqrt(-third * third * third)
        angle = math.acos(max(-1.0, min(1.0, -half / radius)))
        scale = 2.0 * math.sqrt(-third)
        for turn in range(3):
            raw.append(scale * math.cos((angle + 2.0 * math.pi * turn) / 3.0) - shift)

    polished = []
    for root in raw:
        polished.append(_polish(cubic, offset, root))
    return tuple(polished)


@typechecked
def feasible_runs(
    inverse: Matrix3,
    lightness: float,
    hue_degrees: float,
    max_chroma: float,
    guard_cells: int,
) -> RayScan:
    """Every maximal feasible chroma interval on one ray, from the roots.

    The partition is the union of the polynomial roots and a uniform grid of
    `guard_cells` cells. The roots are what makes this exact; the grid is
    defensive, so that a root the closed form loses on a near-double case can
    only cost this the resolution of a sampled scan, never more.
    """

    if max_chroma <= 0.0:
        raise ValueError("max_chroma must be positive")
    if guard_cells <= 0:
        raise ValueError("guard_cells must be positive")

    cubics = ray_cubics(inverse, lightness, hue_degrees)
    cuts = {0.0, max_chroma}
    for cell in range(1, guard_cells):
        cuts.add(max_chroma * cell / guard_cells)
    for cubic in cubics:
        for offset in (0.0, 1.0):
            for root in real_roots(cubic, offset):
                if 0.0 < root < max_chroma:
                    cuts.add(root)

    return RayScan(lightness, hue_degrees, runs_from_cubics(cubics, sorted(cuts)))


def _inside(cubics: tuple[RayCubic, ...], chroma: float) -> bool:
    for cubic in cubics:
        drive = evaluate(cubic, chroma)
        if drive < 0.0 or drive > 1.0:
            return False
    return True


@typechecked
def runs_from_cubics(
    cubics: tuple[RayCubic, ...], ordered: list[float]
) -> tuple[ChromaInterval, ...]:
    """Maximal feasible runs over a partition, cut points and open cells alike.

    The cut points are classified as well as the cells between them because a
    drive with an even-multiplicity root at a bound touches the boundary
    without crossing it: the ray meets the hull at exactly one chroma, and that
    chroma is feasible while every neighbourhood of it is not. Testing
    midpoints alone drops that point, and `RayScan` promises maximal runs
    rather than runs wider than zero.

    Such a touch is measure-zero in lightness and hue, so no grid finds one by
    landing on it. What a grid does find is the near-tangency beside it, which
    is a genuinely narrow interval -- and that is exactly how the wedge this
    module was written to find presents itself.
    """

    if not ordered:
        return ()

    # Point, cell, point, cell, ..., point -- each carrying the chroma range it
    # stands for, so a merged run reports the union without special cases.
    atoms: list[tuple[float, float, bool]] = []
    for index in range(len(ordered)):
        edge = ordered[index]
        atoms.append((edge, edge, _inside(cubics, edge)))
        if index + 1 < len(ordered):
            middle = (edge + ordered[index + 1]) / 2.0
            atoms.append((edge, ordered[index + 1], _inside(cubics, middle)))

    intervals: list[ChromaInterval] = []
    low: float | None = None
    high = 0.0
    for atom_low, atom_high, feasible in atoms:
        if feasible:
            if low is None:
                low = atom_low
            high = atom_high
        elif low is not None:
            intervals.append(ChromaInterval(low, high))
            low = None
    if low is not None:
        intervals.append(ChromaInterval(low, high))
    return tuple(intervals)


@typechecked
def sweep_disconnected(
    inverse: Matrix3,
    lightness_steps: int,
    hue_steps: int,
    max_chroma: float,
    guard_cells: int,
) -> list[DisconnectedRay]:
    """Every ray on a lightness x hue grid whose feasible chroma is not one run."""

    if lightness_steps <= 0 or hue_steps <= 0:
        raise ValueError("lightness_steps and hue_steps must be positive")

    broken: list[DisconnectedRay] = []
    for lightness_step in range(1, lightness_steps):
        lightness = lightness_step / lightness_steps
        for hue_step in range(hue_steps):
            hue_degrees = 360.0 * hue_step / hue_steps
            scan = feasible_runs(
                inverse, lightness, hue_degrees, max_chroma, guard_cells
            )
            if scan.is_connected:
                continue
            broken.append(
                DisconnectedRay(
                    lightness,
                    hue_degrees,
                    scan.intervals,
                    scan.intervals[1].low - scan.intervals[0].high,
                )
            )
    return broken
