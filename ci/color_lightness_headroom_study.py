"""Why the lightness clamp loses reachable brightness, and what recovers it (#4245).

The shipped mapper clamps lightness to the brightest reachable *neutral* and
then compresses chroma. `docs/color-gamut-algorithm-selection.md` explains why
the clamp is there and not a shortcut: above that cap the neutral is
infeasible, so a chroma bisection bracketed at `[0, C]` starts with an
infeasible low end, its invariant is inverted, and it converges on zero.

That reasoning is correct and the conclusion drawn from it was too strong.
Two mapper shapes were tried and measured failing before this one:

* compressing chroma at the target's own lightness -- refuted, because it is
  the same inverted bisection;
* a line search from the neutral-at-cap toward the target -- refuted here,
  and worth recording: the anchor sits *on* the hull boundary, so the segment
  leaves immediately and the search collapses chroma to zero. It is worse
  than the shipped path on most targets.

What was missing is the shape of the feasible set, which this measures. Below
the cap the feasible chroma along a fixed-lightness, fixed-hue ray is
`[0, Cmax]`. Above it, the ray is still a single interval -- but `[Cmin, Cmax]`
with `Cmin > 0`. Connected, and simply not anchored at zero.

Everything follows from that: a bisection that assumes zero is feasible cannot
work above the cap, and one that finds both edges can keep the target's own
lightness whenever any chroma is feasible there.
"""

from __future__ import annotations

import math
from collections.abc import Callable
from dataclasses import dataclass

from typeguard import typechecked

from ci.color_gamut_study import attainable_lightness, is_feasible
from ci.color_reference import Matrix3, Xyz, _oklch_from_xyz, _xyz_from_oklab


# Chroma beyond which nothing is reachable on the devices studied here. Used
# as the probe ceiling; a target past it is compressed like any other.
PROBE_CEILING = 0.8


@typechecked
@dataclass(frozen=True, slots=True)
class ChromaInterval:
    """Feasible chroma along one fixed-lightness, fixed-hue ray."""

    low: float
    high: float

    @property
    def anchored_at_zero(self: "ChromaInterval") -> bool:
        """True below the neutral cap, false above it.

        The whole of #4245 is in this bit: a bisection bracketed at zero is
        valid exactly when it holds.
        """

        return self.low == 0.0


@typechecked
def target_xyz(lightness: float, hue_degrees: float, chroma: float) -> Xyz:
    radians = math.radians(hue_degrees)
    return _xyz_from_oklab(
        (lightness, chroma * math.cos(radians), chroma * math.sin(radians))
    )


@typechecked
def feasible_intervals(
    inverse: Matrix3, lightness: float, hue_degrees: float, samples: int
) -> list[ChromaInterval]:
    """Every maximal feasible run along the ray, by sampling.

    Sampled rather than bisected on purpose: bisection is the thing under
    test, so establishing the shape has to be done without assuming it.
    """

    runs: list[ChromaInterval] = []
    start: float | None = None
    last = 0.0
    for index in range(samples + 1):
        chroma = PROBE_CEILING * index / samples
        if is_feasible(inverse, target_xyz(lightness, hue_degrees, chroma)):
            if start is None:
                start = chroma
            last = chroma
        elif start is not None:
            runs.append(ChromaInterval(start, last))
            start = None
    if start is not None:
        runs.append(ChromaInterval(start, last))
    return runs


@typechecked
def map_clamp_then_chroma(
    inverse: Matrix3,
    lightness: float,
    hue_degrees: float,
    chroma: float,
    halvings: int,
) -> tuple[float, float]:
    """The shipped shape: clamp lightness to the neutral cap, bisect chroma."""

    if is_feasible(inverse, target_xyz(lightness, hue_degrees, chroma)):
        return lightness, chroma
    clamped = attainable_lightness(inverse, lightness)
    low, high = 0.0, chroma
    for _ in range(halvings):
        middle = (low + high) / 2.0
        if is_feasible(inverse, target_xyz(clamped, hue_degrees, middle)):
            low = middle
        else:
            high = middle
    return clamped, low


@typechecked
def map_interval_clamp(
    inverse: Matrix3,
    lightness: float,
    hue_degrees: float,
    chroma: float,
    probes: int,
    halvings: int,
) -> tuple[float, float]:
    """Keep the lightness; clamp chroma into the feasible interval there.

    Above the cap that interval does not contain zero, so a seed inside it has
    to be found before either edge can be bisected -- which is why this needs
    a fixed number of probes as well as the halvings. Both counts are
    compile-time constants, so nothing here is an iterative solver in the
    A3/B11 sense.

    Falls back to the shipped path when no chroma is feasible at this
    lightness, so it is never worse.
    """

    if is_feasible(inverse, target_xyz(lightness, hue_degrees, chroma)):
        return lightness, chroma
    if chroma <= 0.0:
        return map_clamp_then_chroma(inverse, lightness, hue_degrees, chroma, halvings)

    seed: float | None = None
    for index in range(1, probes + 1):
        candidate = PROBE_CEILING * index / probes
        if is_feasible(inverse, target_xyz(lightness, hue_degrees, candidate)):
            seed = candidate
            break
    if seed is None:
        return map_clamp_then_chroma(inverse, lightness, hue_degrees, chroma, halvings)

    low, high = 0.0, seed
    for _ in range(halvings):
        middle = (low + high) / 2.0
        if is_feasible(inverse, target_xyz(lightness, hue_degrees, middle)):
            high = middle
        else:
            low = middle
    lower_edge = high

    low, high = seed, PROBE_CEILING
    for _ in range(halvings):
        middle = (low + high) / 2.0
        if is_feasible(inverse, target_xyz(lightness, hue_degrees, middle)):
            low = middle
        else:
            high = middle
    upper_edge = low

    return lightness, min(max(chroma, lower_edge), upper_edge)


@typechecked
def neutral_cap(inverse: Matrix3) -> float:
    """Brightest reachable neutral -- the lightness the shipped mapper clamps to."""

    return attainable_lightness(inverse, 5.0)


@typechecked
@dataclass(frozen=True, slots=True)
class BrightestReachable:
    """The highest feasible lightness, and where on the hull it is."""

    lightness: float
    hue_degrees: float
    chroma: float


@typechecked
def brightest_reachable(
    inverse: Matrix3, hue_step_degrees: int, chroma_steps: int, lightness_steps: int
) -> BrightestReachable:
    """Highest feasible lightness anywhere, with the hue and chroma reaching it."""

    cap = neutral_cap(inverse)
    best = BrightestReachable(cap, 0.0, 0.0)
    for hue_degrees in range(0, 360, hue_step_degrees):
        for chroma_index in range(1, chroma_steps + 1):
            chroma = PROBE_CEILING * chroma_index / chroma_steps
            for step in range(lightness_steps):
                lightness = cap + (cap * 0.6) * step / (lightness_steps - 1)
                if lightness <= best.lightness:
                    continue
                if is_feasible(
                    inverse, target_xyz(lightness, float(hue_degrees), chroma)
                ):
                    best = BrightestReachable(lightness, float(hue_degrees), chroma)
    return best


@typechecked
def hue_of(xyz: Xyz) -> float:
    return _oklch_from_xyz(xyz).hue_degrees % 360.0
