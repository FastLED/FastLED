"""Can the bind-time solve matrix be inverted in fixed point? (P9, #4043)

#4043's survey names the one item on that phase that is blocked on nothing but
effort:

    What still uses float is the *derivation* that runs once per profile ...
    B11 permits that per-pixel, but the TINY-tier criterion is about *linked
    symbols*, so bind-time float still counts against it.

and then names the reason it is not a small change:

    `EmitterProfile` itself stores `float xy_r[2]`, `lum_r` and so on. It is
    P2's public type and the whole legacy RGBW stack consumes it. A float-free
    bind-time derivation needs a fixed-point profile representation, which is a
    cross-cutting P2/P9 change rather than a rewrite of one 129-line file.

That is about the *profile*. It leaves a narrower question unanswered, and the
narrow one has to be settled first because the wide one depends on it:
**is a Q16 3x3 inverse accurate enough to replace the float one at all?**

If it is not, converting `EmitterProfile` buys nothing -- the derivation would
still have to reach float to stay inside the A1 budget, and P9 item 2 is not
"effort" but "impossible as scoped". If it is, the profile conversion is the
only thing left in the way and can be scoped as such.

The two paths compared here:

    shipped   xyY -> XYZ in float -> invert in float -> quantize to Q16
    proposed  xyY -> XYZ -> quantize to Q16 -> invert in Q16 (i64 intermediates)

Both end in Q16, so this is not a question of the *output* format. It is a
question of where the arithmetic happens: float32 carries a 24-bit mantissa
that follows the magnitude, while Q16 carries a fixed 1/65536 absolute step
and, with 64-bit intermediates, far more headroom than float32 around one.
Neither dominates a priori -- which is why this measures rather than argues.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import TypeAlias

from typeguard import typechecked

from ci.color_gamut_study import emitter_matrix
from ci.color_reference import (
    Matrix3,
    Rgb,
    Xyz,
    _invert_3x3,
    _matvec,
    delta_e2000,
    xyz_to_lab,
)


# The three CIE xy pairs a three-emitter device is described by.
Primaries: TypeAlias = tuple[tuple[float, float], ...]

# s16.16, the working domain the pipeline already uses.
Q16_ONE = 65536

# D65 at unit luminance, the white the reference measures against.
D65_WHITE: Xyz = (0.9504559, 1.0, 1.0890578)


@typechecked
@dataclass(frozen=True, slots=True)
class InverseError:
    """How far a fixed-point inverse lands from the float one it replaces."""

    device: str
    worst_delta_e: float
    worst_drive_error: float
    worst_coefficient_ulps: int


@typechecked
def to_q16(value: float) -> int:
    """Round to the nearest s16.16 step, halves away from zero."""

    if value >= 0.0:
        return int(value * Q16_ONE + 0.5)
    return -int(-value * Q16_ONE + 0.5)


@typechecked
def from_q16(value: int) -> float:
    return value / Q16_ONE


def _rows(matrix: Matrix3) -> tuple[tuple[float, ...], ...]:
    return (matrix.row0, matrix.row1, matrix.row2)


@typechecked
def quantize_matrix(matrix: Matrix3) -> list[list[int]]:
    """Every entry rounded to s16.16."""

    out: list[list[int]] = []
    for row in _rows(matrix):
        quantized: list[int] = []
        for value in row:
            quantized.append(to_q16(value))
        out.append(quantized)
    return out


@typechecked
def rounded_div(numerator: int, denominator: int) -> int:
    """Nearest integer, halves away from zero, for signed operands.

    Truncation would bias every coefficient toward zero, and the bias does not
    cancel across a row -- a solve is a sum of three of them.
    """

    if denominator == 0:
        raise ValueError("denominator must be non-zero")
    if denominator < 0:
        numerator = -numerator
        denominator = -denominator
    if numerator >= 0:
        return (numerator + denominator // 2) // denominator
    return -((-numerator + denominator // 2) // denominator)


@typechecked
def invert3x3_q16(matrix: list[list[int]]) -> list[list[int]] | None:
    """Inverse of an s16.16 matrix, in s16.16, with exact intermediates.

    Python's integers are unbounded, so the cofactors and determinant here are
    exact; a C++ implementation needs i64 for the cofactors (Q32) and the
    determinant (Q48) and a staged or 128-bit divide for the last step. That
    is the cost this study is pricing, so the arithmetic is written the way
    that implementation would have to do it rather than in floats.

    Returns None on a singular matrix, matching `invert3x3`'s contract.
    """

    a, b, c = matrix[0]
    d, e, f = matrix[1]
    g, h, i = matrix[2]

    # Cofactors are products of two Q16 values: Q32.
    cof = [
        [e * i - f * h, c * h - b * i, b * f - c * e],
        [f * g - d * i, a * i - c * g, c * d - a * f],
        [d * h - e * g, b * g - a * h, a * e - b * d],
    ]
    # Determinant is a Q16 times a Q32: Q48.
    determinant = a * cof[0][0] + b * cof[1][0] + c * cof[2][0]
    if determinant == 0:
        return None

    # inverse = cofactor / determinant is Q32 / Q48 = Q-16, so shift by 32 to
    # land back in Q16.
    out: list[list[int]] = []
    for row in range(3):
        scaled: list[int] = []
        for col in range(3):
            scaled.append(rounded_div(cof[row][col] << 32, determinant))
        out.append(scaled)
    return out


@typechecked
def solve_q16(inverse: list[list[int]], xyz: Xyz) -> Rgb:
    """Drives from an s16.16 inverse, the way the embedded solve does it."""

    drives: list[float] = []
    for row in range(3):
        total = 0
        for col in range(3):
            total += inverse[row][col] * to_q16(xyz[col])
        # Q16 * Q16 = Q32; back to Q16, then to float for comparison.
        drives.append(from_q16((total + (1 << 15)) >> 16))
    return (drives[0], drives[1], drives[2])


@typechecked
def clamp_drives(drives: Rgb) -> Rgb:
    """Into [0, 1], the way `clampGamutDrives` does before emission."""

    clamped: list[float] = []
    for value in drives:
        if value < 0.0:
            clamped.append(0.0)
        elif value > 1.0:
            clamped.append(1.0)
        else:
            clamped.append(value)
    return (clamped[0], clamped[1], clamped[2])


@typechecked
def lab_safe(xyz: Xyz) -> Xyz:
    """Float noise around zero clipped away, so CIELAB will accept the point.

    Display P3's red primary has `1 - x - y` equal to zero, which in float is
    5.55e-17, so re-rendering a pure-red drive lands its Z component at about
    -2e-17. That is the summation, not a colour with negative Z, and
    `xyz_to_lab` refuses the whole point over it. Clipped rather than
    tolerated at a threshold: any *real* negative here would be a defect in
    the matrices, and clipping to zero leaves such a value visible as a
    ridiculous ratio rather than hiding it behind an epsilon.
    """

    return (
        xyz[0] if xyz[0] > 0.0 else 0.0,
        xyz[1] if xyz[1] > 0.0 else 0.0,
        xyz[2] if xyz[2] > 0.0 else 0.0,
    )


@typechecked
def sweep_drives(steps: int) -> list[Rgb]:
    """A grid of in-gamut drive vectors, including the corners."""

    if steps <= 0:
        raise ValueError("steps must be positive")

    grid: list[Rgb] = []
    for r in range(steps + 1):
        for g in range(steps + 1):
            for b in range(steps + 1):
                grid.append((r / steps, g / steps, b / steps))
    return grid


@typechecked
def compare_inverses(name: str, primaries: Primaries, steps: int) -> InverseError:
    """Worst-case disagreement between the two derivations on one device.

    Driven from drives rather than from XYZ: a drive vector is what the device
    is actually asked for, so re-rendering it gives a target that is in gamut
    by construction and the recovered drives can be compared against the
    values that produced it.
    """

    forward = emitter_matrix(primaries)
    float_inverse = _invert_3x3(forward)
    shipped = quantize_matrix(float_inverse)
    proposed = invert3x3_q16(quantize_matrix(forward))
    if proposed is None:
        raise ValueError(f"{name}: fixed-point inverse reported singular")

    worst_ulps = 0
    for row in range(3):
        for col in range(3):
            difference = abs(shipped[row][col] - proposed[row][col])
            if difference > worst_ulps:
                worst_ulps = difference

    worst_delta_e = 0.0
    worst_drive_error = 0.0
    for drives in sweep_drives(steps):
        xyz = _matvec(forward, drives)
        shipped_drives = solve_q16(shipped, xyz)
        proposed_drives = solve_q16(proposed, xyz)

        for index in range(3):
            error = abs(shipped_drives[index] - proposed_drives[index])
            if error > worst_drive_error:
                worst_drive_error = error

        # Re-rendered through the clamp the device applies. A drive a rounding
        # step below zero is emitted as zero, and comparing the unclamped
        # value would measure a colour no hardware produces.
        shipped_xyz = lab_safe(_matvec(forward, clamp_drives(shipped_drives)))
        proposed_xyz = lab_safe(_matvec(forward, clamp_drives(proposed_drives)))
        difference = delta_e2000(
            xyz_to_lab(shipped_xyz, D65_WHITE), xyz_to_lab(proposed_xyz, D65_WHITE)
        )
        if difference > worst_delta_e:
            worst_delta_e = difference

    return InverseError(name, worst_delta_e, worst_drive_error, worst_ulps)


@typechecked
def collapsed_primaries(fraction: float) -> Primaries:
    """sRGB with green moved `fraction` of the way onto the red-blue line.

    Conditioning is what decides where a fixed-point inverse gives out, so the
    limit has to be approached rather than asserted. At `fraction` 1.0 the
    three primaries are collinear and the matrix is singular.
    """

    if fraction < 0.0 or fraction > 1.0:
        raise ValueError("fraction must lie in [0, 1]")

    red = (0.6400, 0.3300)
    blue = (0.1500, 0.0600)
    midpoint_x = 0.5 * (red[0] + blue[0])
    midpoint_y = 0.5 * (red[1] + blue[1])
    green_x = 0.3000 + fraction * (midpoint_x - 0.3000)
    green_y = 0.6000 + fraction * (midpoint_y - 0.6000)
    return (red, (green_x, green_y), blue)


@typechecked
def corpus_primaries() -> list[tuple[str, Primaries]]:
    """The primary sets the rest of the colour studies measure.

    The last one is deliberately near-degenerate: a well-conditioned matrix
    tells you nothing about where a fixed-point inverse gives out, and the
    determinant is what decides that.
    """

    return [
        ("srgb", ((0.6400, 0.3300), (0.3000, 0.6000), (0.1500, 0.0600))),
        ("bt2020", ((0.7080, 0.2920), (0.1700, 0.7970), (0.1310, 0.0460))),
        ("display_p3", ((0.6800, 0.3200), (0.2650, 0.6900), (0.1500, 0.0600))),
        ("narrow", ((0.6400, 0.3300), (0.5000, 0.4200), (0.4400, 0.4000))),
    ]
