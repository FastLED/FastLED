"""Can the emitter matrix be *built* in fixed point, not just inverted?

FastLED#4043 (P9) item 2 is "fixed-point `EmitterProfile` + bind-time
derivation". #4275 measured one half of its precondition -- that a Q16 3x3
inverse is accurate enough to replace the float one -- and it is: worst
0.0064 dE2000 across the shipped primaries, against A1's 0.5.

That measurement kept the *forward* matrix float. It quantised a matrix that
float had already built from float chromaticities. The half it did not price
is the one item 2 actually needs: `EmitterProfile` stores `float xy_r[2]`,
and a float-free bind path has to hold those as Q16 and run `xyY_to_XYZ` in
Q16 too.

That is not the same question, and there is a specific reason to doubt it.
`xyY_to_XYZ` is `X = Y*x/y`, `Z = Y*(1-x-y)/y` -- a division by `y`, and blue's
`y` is about 0.06. One Q16 step is 1/65536, so quantising `y` there is a
relative perturbation of 2.5e-04 on the divisor, and it lands on the column
the inverse is least able to absorb. Measuring it before anyone converts the
profile is the same discipline #4275 applied to the inverse: if it does not
hold, item 2 as scoped is not effort, it is impossible, and that is worth
knowing first.

    uv run python ci/color_fixed_profile_study.py
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass

from typeguard import typechecked

from ci.color_fixed_inverse_study import (
    D65_WHITE,
    Primaries,
    _matvec,
    clamp_drives,
    corpus_primaries,
    delta_e2000,
    emitter_matrix,
    emitter_matrix_f32,
    f32,
    from_q16,
    invert3x3_f32,
    invert3x3_q16,
    lab_safe,
    quantize_rows,
    rounded_div,
    solve_q16,
    sweep_drives,
    to_q16,
    xyz_to_lab,
)
from ci.color_reference import Matrix3, Xyz


kQ16One = 1 << 16


@typechecked
@dataclass(frozen=True, slots=True)
class ProfileError:
    """What one device costs when its profile is held in Q16.

    `coefficient_ulps` is on the inverse the solve uses; `drive_error` is in
    normalized drive units; `delta_e` is the perceptual figure A1 is written
    in and the one that decides the question.
    """

    name: str
    delta_e: float
    drive_error: float
    coefficient_ulps: int


# Per-emitter luminances to price alongside the unit case.
#
# `EmitterProfile` carries `lum_r`, `lum_g` and `lum_b` and does not require
# them to be 1. The first version of this study fixed them at unity, which
# left the question it exists to answer only half priced: quantising a
# luminance is a second perturbation, and it lands on the same columns the
# divisor sensitivity already stresses.
@typechecked
@dataclass(frozen=True, slots=True)
class EmitterLuminances:
    """`EmitterProfile`'s `lum_r`, `lum_g` and `lum_b`, with a name."""

    label: str
    red: float
    green: float
    blue: float

    def per_emitter(self) -> list[float]:
        """The three, in the column order the emitter matrix uses."""

        return [self.red, self.green, self.blue]


kUnitLuminance = EmitterLuminances(label="unit", red=1.0, green=1.0, blue=1.0)

kLuminanceSets: tuple[EmitterLuminances, ...] = (
    kUnitLuminance,
    # A warm-white part: green carries most of the flux, blue least.
    EmitterLuminances(label="typical", red=1.0, green=0.8, blue=0.4),
    # Deliberately lopsided, to find where the derivation stops holding.
    EmitterLuminances(label="lopsided", red=0.25, green=1.0, blue=0.1),
    EmitterLuminances(label="dim-blue", red=1.0, green=1.0, blue=0.05),
)


@typechecked
def emitter_matrix_q16(
    primaries: Primaries, luminances: EmitterLuminances
) -> list[list[int]] | None:
    """The emitter columns built entirely in Q16, from Q16 chromaticities.

    Mirrors `xyY_to_XYZ`'s operation order -- divide by `y`, then scale --
    because that order is what decides where a fixed-point path rounds.

    Returns None when a `y` or a luminance quantises to zero, which is the
    degenerate case the shipped `isUsableSolveChromaticity` and
    `isUsableLuminance` already refuse.
    """

    columns: list[list[int]] = []
    for (x_float, y_float), luminance_float in zip(primaries, luminances.per_emitter()):
        x = to_q16(x_float)
        y = to_q16(y_float)
        luminance = to_q16(luminance_float)
        if y <= 0 or luminance <= 0:
            return None
        # z = 1 - x - y, in Q16, from the *quantised* x and y rather than
        # from the floats: a fixed-point profile has no float to go back to.
        z = kQ16One - x - y
        # X = Y*x/y and Z = Y*z/y. Divide first and scale second, matching
        # `xyY_to_XYZ`, so this models the shipped path rather than a
        # rearrangement of it.
        #
        # The order is visible but small: scaling first instead moves 30 of
        # the corpus's 144 coefficients, by at most 10 raw units (1.5e-04 of
        # a unit), and moves no dE2000 in this study enough to matter.
        # Recorded so nobody reads the choice as load-bearing for the budget
        # -- it is load-bearing for *fidelity to the shipped code*, which is
        # a different claim.
        columns.append(
            [
                rounded_div(rounded_div(x * kQ16One, y) * luminance, kQ16One),
                luminance,
                rounded_div(rounded_div(z * kQ16One, y) * luminance, kQ16One),
            ]
        )
    return [
        [columns[0][0], columns[1][0], columns[2][0]],
        [columns[0][1], columns[1][1], columns[2][1]],
        [columns[0][2], columns[1][2], columns[2][2]],
    ]


@typechecked
def emitter_matrix_f32_lum(
    primaries: Primaries, luminances: EmitterLuminances
) -> list[list[float]]:
    """`emitter_matrix_f32` with per-emitter luminance, the shipped order."""

    columns: list[list[float]] = []
    for (x, y), luminance in zip(primaries, luminances.per_emitter()):
        inv_y = f32(1.0 / f32(y))
        columns.append(
            [
                f32(f32(f32(x) * f32(luminance)) * inv_y),
                f32(luminance),
                f32(f32(f32(1.0 - f32(x) - f32(y)) * f32(luminance)) * inv_y),
            ]
        )
    return [
        [columns[0][0], columns[1][0], columns[2][0]],
        [columns[0][1], columns[1][1], columns[2][1]],
        [columns[0][2], columns[1][2], columns[2][2]],
    ]


@typechecked
def compare_profile_quantization(
    name: str,
    primaries: Primaries,
    steps: int,
    luminances: EmitterLuminances = kUnitLuminance,
) -> ProfileError:
    """Worst dE2000, drive error and coefficient ULPs for one device.

    The baseline is the shipped derivation at its own precision -- float32
    forward, float32 inverse, quantised at the end -- exactly as #4275
    models it. The candidate quantises the chromaticities first and never
    reaches float again.
    """

    forward_f32 = emitter_matrix_f32_lum(primaries, luminances)
    inverse_f32 = invert3x3_f32(forward_f32)
    if inverse_f32 is None:
        raise ValueError(f"{name}: float32 inverse reported singular")
    shipped = quantize_rows(inverse_f32)

    forward_q16 = emitter_matrix_q16(primaries, luminances)
    if forward_q16 is None:
        raise ValueError(f"{name}: a chromaticity quantised to an unusable y")
    proposed = invert3x3_q16(forward_q16)
    if proposed is None:
        raise ValueError(f"{name}: fixed-point inverse reported singular")

    worst_ulps = 0
    for row in range(3):
        for col in range(3):
            difference = abs(shipped[row][col] - proposed[row][col])
            if difference > worst_ulps:
                worst_ulps = difference

    # Rendering stays float64: it stands in for the device, and modelling it
    # at either candidate's precision would credit one of them with the
    # emulator's rounding.
    # Rendering in float64, and with the same luminances -- otherwise the two
    # candidates would be scored against a device neither was built for.
    # Each column is one emitter, so scaling a column scales that emitter.
    unit = emitter_matrix(primaries)
    red, green, blue = luminances.per_emitter()

    def scaled(row: Xyz) -> Xyz:
        return (row[0] * red, row[1] * green, row[2] * blue)

    forward = Matrix3(
        row0=scaled(unit.row0), row1=scaled(unit.row1), row2=scaled(unit.row2)
    )

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

        shipped_xyz = lab_safe(_matvec(forward, clamp_drives(shipped_drives)))
        proposed_xyz = lab_safe(_matvec(forward, clamp_drives(proposed_drives)))
        difference = delta_e2000(
            xyz_to_lab(shipped_xyz, D65_WHITE), xyz_to_lab(proposed_xyz, D65_WHITE)
        )
        if difference > worst_delta_e:
            worst_delta_e = difference

    return ProfileError(name, worst_delta_e, worst_drive_error, worst_ulps)


@typechecked
def quantization_residual(primaries: Primaries) -> float:
    """Largest relative error the Q16 grid puts on a chromaticity itself.

    Reported alongside the dE2000, because it is the input to everything
    above and it is where the divisor sensitivity shows: blue's `y` is the
    smallest number in the profile and so the most perturbed in relative
    terms.
    """

    worst = 0.0
    for x_float, y_float in primaries:
        for value in (x_float, y_float):
            if value == 0.0:
                continue
            residual = abs(from_q16(to_q16(value)) - value) / abs(value)
            if residual > worst:
                worst = residual
    return worst


@typechecked
def main(argv: list[str]) -> int:
    """Print the table and return non-zero when A1 would be breached."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--steps",
        type=int,
        default=9,
        help="drive sweep resolution per axis (default 9, i.e. 729 targets)",
    )
    args = parser.parse_args(argv)

    print("Building the emitter matrix in Q16, from Q16 chromaticities.")
    print("Baseline: the shipped float32 forward + float32 inverse.")
    print()
    print(
        f"{'primaries':<14}{'luminances':<11}{'chroma q16 err':>16}{'coef ULP':>10}"
        f"{'drive err':>12}{'worst dE2000':>14}"
    )

    worst_overall = 0.0
    worst_where = ""
    for name, primaries in corpus_primaries():
        residual = quantization_residual(primaries)
        for luminances in kLuminanceSets:
            measured = compare_profile_quantization(
                name, primaries, args.steps, luminances
            )
            print(
                f"{name:<14}{luminances.label:<11}{residual:>16.2e}"
                f"{measured.coefficient_ulps:>10d}"
                f"{measured.drive_error:>12.2e}{measured.delta_e:>14.4f}"
            )
            if measured.delta_e > worst_overall:
                worst_overall = measured.delta_e
                worst_where = f"{name}/{luminances.label}"

    print()
    print(f"worst case: {worst_where}")
    # A1 allows 0.5 dE2000 end to end, and the gamut mapper already spends
    # about 0.15 of it, so a derivation change has roughly 0.35 to work in.
    print(f"worst across the corpus: {worst_overall:.4f} dE2000 (A1 budget 0.5)")
    if worst_overall > 0.35:
        print("OVER the derivation's share of the budget")
        return 1
    print("inside the derivation's share of the budget")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
