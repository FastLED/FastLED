"""Ray connectivity and map-then-allocate scoring on wide matrices (P7, #4041).

`docs/color-gamut-algorithm-selection.md` establishes two things for the
three-emitter device and explicitly declines to claim either for wider ones:

* the feasible chroma ray is a single interval, which is what makes the
  shipped eight-halving bisection a valid search rather than a guess. The
  report's sweep "says nothing about the >=4-emitter case where the zonotope
  gains a redundant generator";
* how the mapper actually scores once a white emitter is in play. The report
  lists that under "Not covered", because mapping and allocation were each
  characterized alone and a target that is out of gamut *and* on a
  white-emitter device goes through both.

Neither carries over by assumption. With a white emitter the preimage of a
target stops being unique, so "feasible" stops meaning "the one solution
lands in the box" and starts meaning "some solution does" -- a different
predicate, and the one the embedded mapper actually consults through
`mapAndAllocateRgbwQ16` / `mapAndAllocateRgbwwQ16`.

This harness measures both against the same devices the corpus ships.
"""

from __future__ import annotations

import math
from collections.abc import Callable
from dataclasses import dataclass

from typeguard import typechecked

from ci.color_reference import (
    Matrix3,
    Xyz,
    _invert_3x3,
    _matvec,
    _oklch_from_xyz,
    _xyz_from_oklab,
)
from ci.color_rgbw_study import (
    allocate_one_white,
    allocate_two_white,
    emitter_column,
    most_white_two,
    rgb_matrix,
    white_level_range,
)


# A device-hull membership test: does *some* set of drives in [0, 1] produce
# this XYZ? For three emitters that is the unique preimage landing in the
# box; for four or five it is a genuine feasibility question.
FeasibilityTest = Callable[[Xyz], bool]


@typechecked
@dataclass(frozen=True, slots=True)
class ChromaInterval:
    """A maximal run of feasible chroma along one fixed-lightness ray."""

    low: float
    high: float


@typechecked
@dataclass(frozen=True, slots=True)
class Realization:
    """What a device actually produced for a target, and how.

    `realized` is re-rendered from `drives` rather than copied from the
    target, so comparing the two is what catches an allocation that reports
    success while producing something else.
    """

    realized: Xyz
    drives: tuple[float, ...]


@typechecked
@dataclass(frozen=True, slots=True)
class RayScan:
    """Every feasible chroma interval found along one ray."""

    lightness: float
    hue_degrees: float
    intervals: tuple[ChromaInterval, ...]

    @property
    def is_connected(self: "RayScan") -> bool:
        """Bisection is only a valid search when this holds.

        Zero intervals counts as connected: a ray with no feasible chroma at
        all has nothing for the search to discard.
        """

        return len(self.intervals) <= 1

    @property
    def starts_at_zero(self: "RayScan") -> bool:
        """The interval reaches the neutral axis.

        The shipped search seeds its bracket at chroma zero, so an interval
        that is connected but floats away from the axis would defeat it just
        as thoroughly as a disconnected one.
        """

        if not self.intervals:
            return False
        return self.intervals[0].low == 0.0


def one_white_feasible(inverse: Matrix3, white: Xyz) -> FeasibilityTest:
    """Hull membership for RGB + one white, via the closed-form interval."""

    def test(target: Xyz) -> bool:
        return white_level_range(inverse, target, white) is not None

    return test


def two_white_feasible(inverse: Matrix3, first: Xyz, second: Xyz) -> FeasibilityTest:
    """Hull membership for RGB + two whites, via vertex enumeration.

    Deliberately the slow oracle rather than `allocate_two_white`: a sweep
    built on the closed form could only ever confirm the closed form's own
    idea of the hull, which is the assumption under test.
    """

    def test(target: Xyz) -> bool:
        return most_white_two(inverse, target, first, second) is not None

    return test


def two_white_allocatable(inverse: Matrix3, first: Xyz, second: Xyz) -> FeasibilityTest:
    """Hull membership as the *embedded* path decides it.

    `mapAndAllocateRgbwwQ16` takes every feasibility decision through the
    closed-form allocation, so this is the predicate its halvings actually
    see. It differs from `two_white_feasible` only in tolerance --
    `DRIVE_TOLERANCE` against `ENUMERATION_TOLERANCE`, two orders of
    magnitude apart -- which matters only for targets sitting on the hull
    boundary, and is why a composition scored with the oracle in front and
    the closed form behind reports failures that neither method commits on
    its own.
    """

    def test(target: Xyz) -> bool:
        return allocate_two_white(inverse, target, first, second) is not None

    return test


@typechecked
def scan_ray(
    test: FeasibilityTest,
    lightness: float,
    hue_degrees: float,
    max_chroma: float,
    samples: int,
) -> RayScan:
    """Sample one fixed-lightness, fixed-hue ray and group the feasible runs."""

    radians = math.radians(hue_degrees)
    cosine = math.cos(radians)
    sine = math.sin(radians)
    intervals: list[ChromaInterval] = []
    start: float | None = None
    last_feasible = 0.0
    for index in range(samples + 1):
        chroma = max_chroma * index / samples
        point = _xyz_from_oklab((lightness, chroma * cosine, chroma * sine))
        if test(point):
            if start is None:
                start = chroma
            last_feasible = chroma
        elif start is not None:
            intervals.append(ChromaInterval(start, last_feasible))
            start = None
    if start is not None:
        intervals.append(ChromaInterval(start, last_feasible))
    return RayScan(lightness, hue_degrees, tuple(intervals))


@typechecked
def scan_rays(
    test: FeasibilityTest,
    lightness_steps: int,
    hue_step_degrees: int,
    max_chroma: float,
    chroma_samples: int,
) -> list[RayScan]:
    """Every ray on a lightness x hue grid, one scan each."""

    scans: list[RayScan] = []
    for lightness_step in range(1, lightness_steps):
        lightness = lightness_step / lightness_steps
        for hue_degrees in range(0, 360, hue_step_degrees):
            scans.append(
                scan_ray(
                    test, lightness, float(hue_degrees), max_chroma, chroma_samples
                )
            )
    return scans


@typechecked
def disconnected(scans: list[RayScan]) -> list[RayScan]:
    """The rays a chroma bisection would get wrong."""

    broken: list[RayScan] = []
    for scan in scans:
        if not scan.is_connected:
            broken.append(scan)
    return broken


@typechecked
def attainable_lightness(
    test: FeasibilityTest, lightness: float, halvings: int
) -> float:
    """Largest neutral lightness at or below `lightness` that the hull holds.

    The wide-matrix twin of `color_gamut_study.attainable_lightness`. Same
    reason it has to run first: chroma compression cannot rescue a target
    that is too bright, because at zero chroma it is still outside.
    """

    if test(_xyz_from_oklab((lightness, 0.0, 0.0))):
        return lightness
    low, high = 0.0, lightness
    for _ in range(halvings):
        middle = (low + high) / 2.0
        if test(_xyz_from_oklab((middle, 0.0, 0.0))):
            low = middle
        else:
            high = middle
    return low


@typechecked
def map_oklch(test: FeasibilityTest, xyz: Xyz, halvings: int) -> Xyz:
    """Hue-preserving chroma compression against a wide hull.

    Mirrors what `mapAndAllocateRgbwQ16` does: the same fixed halving count
    as the three-emitter path, with every feasibility decision taken through
    the allocation rather than through a 3x3 inverse.
    """

    if test(xyz):
        return xyz
    polar = _oklch_from_xyz(xyz)
    hue = math.radians(polar.hue_degrees)
    lightness = attainable_lightness(test, polar.lightness, halvings)
    low, high = 0.0, polar.chroma
    for _ in range(halvings):
        chroma = (low + high) / 2.0
        candidate = _xyz_from_oklab(
            (lightness, chroma * math.cos(hue), chroma * math.sin(hue))
        )
        if test(candidate):
            low = chroma
        else:
            high = chroma
    return _xyz_from_oklab((lightness, low * math.cos(hue), low * math.sin(hue)))


@typechecked
def realize_one_white(
    forward: Matrix3, inverse: Matrix3, white: Xyz, target: Xyz
) -> Realization | None:
    """Allocate `target` on an RGB+W device and re-render the drives to XYZ.

    Returning the re-rendered XYZ rather than trusting the drives is the
    point: it is what catches an allocation that reports success while
    producing something else.
    """

    drives = allocate_one_white(inverse, target, white)
    if drives is None:
        return None
    rgb = (drives.red, drives.green, drives.blue)
    rendered = _matvec(forward, rgb)
    realized: Xyz = (
        rendered[0] + drives.white * white[0],
        rendered[1] + drives.white * white[1],
        rendered[2] + drives.white * white[2],
    )
    return Realization(realized, (drives.red, drives.green, drives.blue, drives.white))


@typechecked
def realize_two_white(
    forward: Matrix3, inverse: Matrix3, first: Xyz, second: Xyz, target: Xyz
) -> Realization | None:
    """The same, for RGB + two whites."""

    levels = allocate_two_white(inverse, target, first, second)
    if levels is None:
        return None
    at_zero = _matvec(inverse, target)
    from_first = _matvec(inverse, first)
    from_second = _matvec(inverse, second)
    rgb: list[float] = []
    for index in range(3):
        drive = (
            at_zero[index]
            - levels.first * from_first[index]
            - levels.second * from_second[index]
        )
        rgb.append(min(max(drive, 0.0), 1.0))
    rendered = _matvec(forward, (rgb[0], rgb[1], rgb[2]))
    realized: Xyz = (
        rendered[0] + levels.first * first[0] + levels.second * second[0],
        rendered[1] + levels.first * first[1] + levels.second * second[1],
        rendered[2] + levels.first * first[2] + levels.second * second[2],
    )
    return Realization(realized, (rgb[0], rgb[1], rgb[2], levels.first, levels.second))


@typechecked
def hue_drift_degrees(before: Xyz, after: Xyz) -> float:
    """Absolute OKLab hue change across the mapping, in degrees.

    Degenerate by construction near the neutral axis, where hue is not
    defined; callers screen on chroma before reading this.
    """

    first = _oklch_from_xyz(before)
    second = _oklch_from_xyz(after)
    difference = abs(first.hue_degrees - second.hue_degrees) % 360.0
    return min(difference, 360.0 - difference)


@typechecked
@dataclass(frozen=True, slots=True)
class WideDevice:
    """One emitter set from the corpus, with both hull predicates built.

    `hull` is the oracle -- vertex enumeration for two whites, the closed-form
    interval for one -- and answers "is this target reachable at all". `solve`
    is what the embedded mapper consults. They differ only in tolerance, and
    `scan_rays` deliberately runs against `hull`: a connectivity sweep built
    on the closed form could only confirm the closed form's own idea of the
    hull, which is the assumption under test.
    """

    name: str
    forward: Matrix3
    inverse: Matrix3
    whites: tuple[Xyz, ...]
    hull: FeasibilityTest
    solve: FeasibilityTest

    def realize(self: "WideDevice", target: Xyz) -> Realization | None:
        """Allocate and re-render, so the drives are checked rather than trusted."""

        if len(self.whites) == 1:
            allocated = realize_one_white(
                self.forward, self.inverse, self.whites[0], target
            )
        else:
            allocated = realize_two_white(
                self.forward, self.inverse, self.whites[0], self.whites[1], target
            )
        return allocated


def _build_device(name: str, columns: list[Xyz], whites: tuple[Xyz, ...]) -> WideDevice:
    forward = rgb_matrix(columns)
    inverse = _invert_3x3(forward)
    if len(whites) == 1:
        hull = one_white_feasible(inverse, whites[0])
        solve = hull
    else:
        hull = two_white_feasible(inverse, whites[0], whites[1])
        solve = two_white_allocatable(inverse, whites[0], whites[1])
    return WideDevice(name, forward, inverse, whites, hull, solve)


@typechecked
def corpus_devices() -> list[WideDevice]:
    """The white-emitter devices `ci/color_reference_corpus.py` ships.

    Kept in step with that module by `test_color_wide_hull_study.py` rather
    than imported from it: the corpus builds `DeviceProfile` objects for the
    reference, and this study needs the raw columns.
    """

    unit_rgb = [
        emitter_column(0.6400, 0.3300, 1.0),
        emitter_column(0.3000, 0.6000, 1.0),
        emitter_column(0.1500, 0.0600, 1.0),
    ]
    # Primaries split the way sRGB splits luminance, so the strip can only
    # just exceed the white it renders and bright neutrals need both whites.
    split_rgb = [
        emitter_column(0.6400, 0.3300, 0.22),
        emitter_column(0.3000, 0.6000, 0.60),
        emitter_column(0.1500, 0.0600, 0.08),
    ]
    d65 = emitter_column(0.3127, 0.3290, 1.0)
    d50 = emitter_column(0.3457, 0.3585, 1.0)
    return [
        _build_device("rgbw", unit_rgb, (d65,)),
        _build_device("non_d65_white", unit_rgb, (d50,)),
        _build_device("rgbww", unit_rgb, (d65, d50)),
        _build_device(
            "rgbww_two_white",
            split_rgb,
            (
                emitter_column(0.3127, 0.3290, 0.35),
                emitter_column(0.3457, 0.3585, 0.35),
            ),
        ),
    ]
