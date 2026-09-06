"""Float64 host primitives for P5 color-pipeline golden vectors.

References:
* IEC 61966-2-1:1999, sRGB transfer.
* ICC.1:2022, Annex E, linear Bradford chromatic adaptation.
* ISO/CIE 11664-6:2022, CIEDE2000.

This is host-only reference code. It deliberately preserves signed, wide XYZ
intermediates; gamut mapping and native-code quantization belong to later P5
work, not these primitives.
"""

from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass
from enum import Enum
from itertools import combinations, product
from math import atan2, copysign, cos, exp, hypot, isfinite, pi, radians, sin, sqrt
from typing import TypeAlias


Rgb: TypeAlias = tuple[float, float, float]
Xyz: TypeAlias = tuple[float, float, float]
Lab: TypeAlias = tuple[float, float, float]


@dataclass(frozen=True, slots=True)
class Matrix3:
    """A named 3×3 float64 matrix used only by the host reference."""

    row0: Xyz
    row1: Xyz
    row2: Xyz


@dataclass(frozen=True, slots=True)
class Oklch:
    """Oklab's lightness/chroma/hue coordinates for the host mapper."""

    lightness: float
    chroma: float
    hue_degrees: float


@dataclass(frozen=True, slots=True)
class Emitter:
    """Measured diode chromaticity and normalized full-drive luminance."""

    name: str
    chromaticity: Chromaticity
    capacity: float
    is_white: bool

    def __post_init__(self) -> None:
        if not self.name:
            raise ValueError("emitter name must not be empty")
        if not isfinite(self.capacity) or not 0.0 < self.capacity <= 1.0:
            raise ValueError("emitter capacity must be finite and in (0, 1]")


@dataclass(frozen=True, slots=True)
class DeviceProfile:
    """RGB/RGBW/RGBWW capacities relative to a selected rendering white."""

    emitters: tuple[Emitter, ...]
    rendering_white: Chromaticity

    def __post_init__(self) -> None:
        if len(self.emitters) not in (3, 4, 5):
            raise ValueError("device profile requires three, four, or five emitters")
        if len({emitter.name for emitter in self.emitters}) != len(self.emitters):
            raise ValueError("device emitter names must be unique")


@dataclass(frozen=True, slots=True)
class MappingSolution:
    """Mapped target and deterministic device drives."""

    mapped_xyz: Xyz
    drives: tuple[float, ...]
    target_oklch: Oklch
    mapped_oklch: Oklch


@dataclass(frozen=True, slots=True)
class _GamutConstraint:
    normal: Xyz
    minimum: float
    maximum: float


@dataclass(frozen=True, slots=True)
class _Cubic:
    cubic: float
    quadratic: float
    linear: float
    constant: float


class TransferFunction(Enum):
    """Named source transfer functions aligned with P2's public terminology."""

    LINEAR = "linear"
    SRGB = "srgb"
    BT709 = "bt709"


@dataclass(frozen=True)
class Chromaticity:
    """CIE 1931 xy chromaticity."""

    x: float
    y: float

    def __post_init__(self) -> None:
        if not isfinite(self.x) or not isfinite(self.y):
            raise ValueError("chromaticity must be finite")
        if self.x <= 0.0 or self.y <= 0.0 or self.x + self.y > 1.0:
            raise ValueError("chromaticity must be inside the CIE xy simplex")


@dataclass(frozen=True)
class RgbPrimaries:
    """Source RGB primaries and reference white."""

    red: Chromaticity
    green: Chromaticity
    blue: Chromaticity
    white: Chromaticity

    @classmethod
    def bt709(cls) -> RgbPrimaries:
        return cls(
            red=Chromaticity(0.6400, 0.3300),
            green=Chromaticity(0.3000, 0.6000),
            blue=Chromaticity(0.1500, 0.0600),
            white=Chromaticity(0.3127, 0.3290),
        )


@dataclass(frozen=True)
class SourceProfile:
    """Source primaries plus its independently declared transfer function."""

    primaries: RgbPrimaries
    transfer: TransferFunction

    @classmethod
    def linear_srgb(cls) -> SourceProfile:
        return cls(RgbPrimaries.bt709(), TransferFunction.LINEAR)

    @classmethod
    def srgb_bt709(cls) -> SourceProfile:
        return cls(RgbPrimaries.bt709(), TransferFunction.SRGB)


def _require_finite(values: Sequence[float], label: str) -> None:
    if len(values) != 3:
        raise ValueError(f"{label} requires exactly three components")
    if not all(isfinite(value) for value in values):
        raise ValueError(f"{label} must contain only finite values")


def _matvec(matrix: Matrix3, vector: Xyz) -> Xyz:
    return (
        sum(cell * value for cell, value in zip(matrix.row0, vector)),
        sum(cell * value for cell, value in zip(matrix.row1, vector)),
        sum(cell * value for cell, value in zip(matrix.row2, vector)),
    )


def _invert_3x3(matrix: Matrix3) -> Matrix3:
    a, b, c = matrix.row0
    d, e, f = matrix.row1
    g, h, i = matrix.row2
    determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
    if not isfinite(determinant) or abs(determinant) < 1e-15:
        raise ValueError("primary matrix is singular")
    inverse_determinant = 1.0 / determinant
    return Matrix3(
        row0=(
            (e * i - f * h) * inverse_determinant,
            (c * h - b * i) * inverse_determinant,
            (b * f - c * e) * inverse_determinant,
        ),
        row1=(
            (f * g - d * i) * inverse_determinant,
            (a * i - c * g) * inverse_determinant,
            (c * d - a * f) * inverse_determinant,
        ),
        row2=(
            (d * h - e * g) * inverse_determinant,
            (b * g - a * h) * inverse_determinant,
            (a * e - b * d) * inverse_determinant,
        ),
    )


def _xy_to_xyz(chromaticity: Chromaticity) -> Xyz:
    return (
        chromaticity.x / chromaticity.y,
        1.0,
        (1.0 - chromaticity.x - chromaticity.y) / chromaticity.y,
    )


def _scale_row(row: Xyz, scales: Xyz) -> Xyz:
    return (
        row[0] * scales[0],
        row[1] * scales[1],
        row[2] * scales[2],
    )


def _source_matrix(primaries: RgbPrimaries) -> Matrix3:
    red, green, blue, white = (
        _xy_to_xyz(primaries.red),
        _xy_to_xyz(primaries.green),
        _xy_to_xyz(primaries.blue),
        _xy_to_xyz(primaries.white),
    )
    unscaled = Matrix3(
        row0=(red[0], green[0], blue[0]),
        row1=(red[1], green[1], blue[1]),
        row2=(red[2], green[2], blue[2]),
    )
    scales = _matvec(_invert_3x3(unscaled), white)
    return Matrix3(
        row0=_scale_row(unscaled.row0, scales),
        row1=_scale_row(unscaled.row1, scales),
        row2=_scale_row(unscaled.row2, scales),
    )


def decode_rgb8(encoded: Sequence[int], profile: SourceProfile) -> Rgb:
    """Decode RGB8 to linear RGB without clamping or intermediate quantization."""

    if len(encoded) != 3:
        raise ValueError("RGB8 requires exactly three components")
    if any(
        isinstance(component, bool) or not isinstance(component, int)
        for component in encoded
    ):
        raise ValueError("RGB8 components must be integers")
    if any(component < 0 or component > 255 for component in encoded):
        raise ValueError("RGB8 components must be in [0, 255]")

    def decode(component: int) -> float:
        normalized = component / 255.0
        if profile.transfer is TransferFunction.LINEAR:
            return normalized
        if profile.transfer is TransferFunction.SRGB:
            return (
                normalized / 12.92
                if normalized <= 0.04045
                else ((normalized + 0.055) / 1.055) ** 2.4
            )
        if profile.transfer is TransferFunction.BT709:
            return (
                normalized / 4.5
                if normalized < 0.081
                else ((normalized + 0.099) / 1.099) ** (1.0 / 0.45)
            )
        raise ValueError(f"unsupported transfer {profile.transfer!r}")

    return (decode(encoded[0]), decode(encoded[1]), decode(encoded[2]))


def source_rgb_to_xyz(linear_rgb: Rgb, profile: SourceProfile) -> Xyz:
    """Convert already-linear source RGB to its profile-relative XYZ."""

    _require_finite(linear_rgb, "linear RGB")
    return _matvec(_source_matrix(profile.primaries), linear_rgb)


def bradford_adaptation(
    xyz: Xyz, source_white: Chromaticity, destination_white: Chromaticity
) -> Xyz:
    """Adapt XYZ between whites using ICC's linear Bradford method."""

    _require_finite(xyz, "XYZ")
    bradford = Matrix3(
        row0=(0.8951, 0.2664, -0.1614),
        row1=(-0.7502, 1.7135, 0.0367),
        row2=(0.0389, -0.0685, 1.0296),
    )
    source_cones = _matvec(bradford, _xy_to_xyz(source_white))
    destination_cones = _matvec(bradford, _xy_to_xyz(destination_white))
    if any(abs(value) < 1e-15 for value in source_cones):
        raise ValueError("Bradford source white has a zero cone response")
    scaled: Xyz = (
        destination_cones[0] / source_cones[0],
        destination_cones[1] / source_cones[1],
        destination_cones[2] / source_cones[2],
    )
    cones = _matvec(bradford, xyz)
    return _matvec(
        _invert_3x3(bradford),
        _scale_row(cones, scaled),
    )


def xyz_to_lab(xyz: Xyz, reference_white: Xyz) -> Lab:
    """Convert physically realizable XYZ to CIELAB relative to its white."""

    _require_finite(xyz, "XYZ")
    _require_finite(reference_white, "reference white")
    if any(value < 0.0 for value in xyz) or any(
        value <= 0.0 for value in reference_white
    ):
        raise ValueError(
            "CIELAB requires non-negative XYZ and positive reference white"
        )
    epsilon = 216.0 / 24389.0
    kappa = 24389.0 / 27.0

    def transform(value: float) -> float:
        return (
            value ** (1.0 / 3.0) if value > epsilon else (kappa * value + 16.0) / 116.0
        )

    fx, fy, fz = (
        transform(value / white) for value, white in zip(xyz, reference_white)
    )
    return (116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz))


def delta_e2000(first: Lab, second: Lab) -> float:
    """Return CIEDE2000 for two CIELAB coordinates with unit weights."""

    _require_finite(first, "first Lab")
    _require_finite(second, "second Lab")
    l1, a1, b1 = first
    l2, a2, b2 = second
    average_lightness = (l1 + l2) / 2.0
    chroma1 = hypot(a1, b1)
    chroma2 = hypot(a2, b2)
    average_chroma = (chroma1 + chroma2) / 2.0
    g = 0.5 * (1.0 - sqrt(average_chroma**7 / (average_chroma**7 + 25.0**7)))
    adjusted_a1, adjusted_a2 = (1.0 + g) * a1, (1.0 + g) * a2
    adjusted_chroma1, adjusted_chroma2 = hypot(adjusted_a1, b1), hypot(adjusted_a2, b2)

    def hue(a: float, b: float) -> float:
        return (atan2(b, a) * 180.0 / pi) % 360.0 if a != 0.0 or b != 0.0 else 0.0

    hue1, hue2 = hue(adjusted_a1, b1), hue(adjusted_a2, b2)
    delta_lightness = l2 - l1
    delta_chroma = adjusted_chroma2 - adjusted_chroma1
    hue_difference = hue2 - hue1
    if adjusted_chroma1 * adjusted_chroma2 == 0.0:
        delta_hue = 0.0
    elif hue_difference > 180.0:
        delta_hue = hue_difference - 360.0
    elif hue_difference < -180.0:
        delta_hue = hue_difference + 360.0
    else:
        delta_hue = hue_difference
    delta_hue_term = (
        2.0 * sqrt(adjusted_chroma1 * adjusted_chroma2) * sin(radians(delta_hue / 2.0))
    )

    if adjusted_chroma1 * adjusted_chroma2 == 0.0:
        average_hue = hue1 + hue2
    elif abs(hue_difference) <= 180.0:
        average_hue = (hue1 + hue2) / 2.0
    elif hue1 + hue2 < 360.0:
        average_hue = (hue1 + hue2 + 360.0) / 2.0
    else:
        average_hue = (hue1 + hue2 - 360.0) / 2.0

    t = (
        1.0
        - 0.17 * cos(radians(average_hue - 30.0))
        + 0.24 * cos(radians(2.0 * average_hue))
        + 0.32 * cos(radians(3.0 * average_hue + 6.0))
        - 0.20 * cos(radians(4.0 * average_hue - 63.0))
    )
    delta_theta = 30.0 * exp(-(((average_hue - 275.0) / 25.0) ** 2))
    lightness_scale = 1.0 + 0.015 * (average_lightness - 50.0) ** 2 / sqrt(
        20.0 + (average_lightness - 50.0) ** 2
    )
    chroma_mean = (adjusted_chroma1 + adjusted_chroma2) / 2.0
    # R_C is defined over the *adjusted* chroma mean C-bar-prime, not the raw
    # mean used for G above. The two coincide at high chroma (G -> 0) and at
    # low chroma (R_C -> 0), which is why the usual Sharma pairs do not
    # separate them; they disagree in the band around C ~ 15-25 where G is
    # still large and R_C is turning on.
    chroma_scale = 2.0 * sqrt(chroma_mean**7 / (chroma_mean**7 + 25.0**7))
    saturation_scale = 1.0 + 0.045 * chroma_mean
    hue_scale = 1.0 + 0.015 * chroma_mean * t
    rotation = -sin(radians(2.0 * delta_theta)) * chroma_scale
    return sqrt(
        (delta_lightness / lightness_scale) ** 2
        + (delta_chroma / saturation_scale) ** 2
        + (delta_hue_term / hue_scale) ** 2
        + rotation * (delta_chroma / saturation_scale) * (delta_hue_term / hue_scale)
    )


_D65 = Chromaticity(0.3127, 0.3290)
_MAPPING_TOLERANCE = 1e-12


def _dot(left: Xyz, right: Xyz) -> float:
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2]


def _cross(left: Xyz, right: Xyz) -> Xyz:
    return (
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    )


def _emitter_xyz(emitter: Emitter) -> Xyz:
    unit = _xy_to_xyz(emitter.chromaticity)
    return (unit[0] * emitter.capacity, emitter.capacity, unit[2] * emitter.capacity)


def emitted_xyz(profile: DeviceProfile, drives: Sequence[float]) -> Xyz:
    """Return rendering-white XYZ for drives without clipping signed math."""

    if len(drives) != len(profile.emitters):
        raise ValueError("drive count must match device emitters")
    if not all(isfinite(drive) for drive in drives):
        raise ValueError("drives must be finite")
    columns = tuple(_emitter_xyz(emitter) for emitter in profile.emitters)
    return (
        sum(column[0] * drive for column, drive in zip(columns, drives)),
        sum(column[1] * drive for column, drive in zip(columns, drives)),
        sum(column[2] * drive for column, drive in zip(columns, drives)),
    )


def _oklab_from_xyz(xyz: Xyz) -> Lab:
    _require_finite(xyz, "XYZ")
    lms = (
        0.8190224432164319 * xyz[0]
        + 0.3619062562801221 * xyz[1]
        - 0.12887378261216414 * xyz[2],
        0.0329836671980271 * xyz[0]
        + 0.9292868468965546 * xyz[1]
        + 0.03614466816999844 * xyz[2],
        0.048177199566046255 * xyz[0]
        + 0.26423952494422764 * xyz[1]
        + 0.6335478258136937 * xyz[2],
    )
    lms_root = tuple(copysign(abs(value) ** (1.0 / 3.0), value) for value in lms)
    return (
        0.2104542553 * lms_root[0]
        + 0.7936177850 * lms_root[1]
        - 0.0040720468 * lms_root[2],
        1.9779984951 * lms_root[0]
        - 2.4285922050 * lms_root[1]
        + 0.4505937099 * lms_root[2],
        0.0259040371 * lms_root[0]
        + 0.7827717662 * lms_root[1]
        - 0.8086757660 * lms_root[2],
    )


def _xyz_from_oklab(lab: Lab) -> Xyz:
    _require_finite(lab, "OKLab")
    lms_root = _matvec(
        _invert_3x3(
            Matrix3(
                (0.2104542553, 0.7936177850, -0.0040720468),
                (1.9779984951, -2.4285922050, 0.4505937099),
                (0.0259040371, 0.7827717662, -0.8086757660),
            )
        ),
        lab,
    )
    lms: Xyz = (lms_root[0] ** 3, lms_root[1] ** 3, lms_root[2] ** 3)
    return _matvec(
        _invert_3x3(
            Matrix3(
                (0.8190224432164319, 0.3619062562801221, -0.12887378261216414),
                (0.0329836671980271, 0.9292868468965546, 0.03614466816999844),
                (0.048177199566046255, 0.26423952494422764, 0.6335478258136937),
            )
        ),
        lms,
    )


def _oklch_from_xyz(xyz: Xyz) -> Oklch:
    lightness, a_value, b_value = _oklab_from_xyz(xyz)
    return Oklch(
        lightness, hypot(a_value, b_value), atan2(b_value, a_value) * 180.0 / pi % 360.0
    )


def _is_neutral_relative_to_white(xyz: Xyz, white_direction: Xyz) -> bool:
    """Return whether XYZ is on its declared rendering-white ray."""

    scale = xyz[1]
    return all(
        abs(component - direction * scale)
        <= _MAPPING_TOLERANCE * max(1.0, abs(component), abs(direction * scale))
        for component, direction in zip(xyz, white_direction)
    )


def _adapted_columns(profile: DeviceProfile) -> tuple[Xyz, ...]:
    if profile.rendering_white == _D65:
        return tuple(_emitter_xyz(emitter) for emitter in profile.emitters)
    return tuple(
        bradford_adaptation(_emitter_xyz(emitter), profile.rendering_white, _D65)
        for emitter in profile.emitters
    )


def _solve3(matrix: Matrix3, vector: Xyz) -> Xyz:
    return _matvec(_invert_3x3(matrix), vector)


def _solve_white_preferred(
    target: Xyz, columns: tuple[Xyz, ...], profile: DeviceProfile
) -> tuple[float, ...] | None:
    count = len(columns)
    candidates: list[tuple[float, ...]] = []
    for free in combinations(range(count), 3):
        active = tuple(index for index in range(count) if index not in free)
        for active_values in product((0.0, 1.0), repeat=len(active)):
            residual = list(target)
            for index, value in zip(active, active_values):
                for component in range(3):
                    residual[component] -= columns[index][component] * value
            matrix = Matrix3(
                (columns[free[0]][0], columns[free[1]][0], columns[free[2]][0]),
                (columns[free[0]][1], columns[free[1]][1], columns[free[2]][1]),
                (columns[free[0]][2], columns[free[1]][2], columns[free[2]][2]),
            )
            try:
                free_values = _solve3(matrix, (residual[0], residual[1], residual[2]))
            except ValueError:
                continue
            if any(
                value < -_MAPPING_TOLERANCE or value > 1.0 + _MAPPING_TOLERANCE
                for value in free_values
            ):
                continue
            drives = [0.0] * count
            for index, value in zip(active, active_values):
                drives[index] = value
            for index, value in zip(free, free_values):
                drives[index] = round(min(1.0, max(0.0, value)), 15)
            reproduced = tuple(
                sum(column[component] * drive for column, drive in zip(columns, drives))
                for component in range(3)
            )
            if (
                max(
                    abs(actual - expected)
                    for actual, expected in zip(reproduced, target)
                )
                <= _MAPPING_TOLERANCE
            ):
                candidates.append(tuple(drives))
    if not candidates:
        return None
    white_indices = tuple(
        index for index, emitter in enumerate(profile.emitters) if emitter.is_white
    )
    rgb_indices = tuple(
        index for index, emitter in enumerate(profile.emitters) if not emitter.is_white
    )
    return min(
        candidates,
        key=lambda drives: (
            -sum(drives[index] for index in white_indices),
            sum(drives[index] ** 2 for index in rgb_indices),
            drives,
        ),
    )


def _zonotope_constraints(columns: tuple[Xyz, ...]) -> tuple[_GamutConstraint, ...]:
    constraints: list[_GamutConstraint] = []
    for left, right in combinations(columns, 2):
        normal = _cross(left, right)
        if _dot(normal, normal) <= _MAPPING_TOLERANCE:
            continue
        projections = tuple(_dot(normal, column) for column in columns)
        constraints.append(
            _GamutConstraint(
                normal,
                sum(min(0.0, value) for value in projections),
                sum(max(0.0, value) for value in projections),
            )
        )
    if not constraints:
        raise ValueError("device gamut is degenerate")
    return tuple(constraints)


def _cubic_roots_on_interval(coefficients: _Cubic, upper: float) -> tuple[float, ...]:
    a3, a2, a1, a0 = (
        coefficients.cubic,
        coefficients.quadratic,
        coefficients.linear,
        coefficients.constant,
    )
    derivative_roots: list[float] = []
    if abs(a3) > _MAPPING_TOLERANCE:
        discriminant = (2.0 * a2) ** 2 - 4.0 * (3.0 * a3) * a1
        if discriminant >= 0.0:
            derivative_roots.extend(
                (
                    (-2.0 * a2 - sqrt(discriminant)) / (6.0 * a3),
                    (-2.0 * a2 + sqrt(discriminant)) / (6.0 * a3),
                )
            )
    elif abs(a2) > _MAPPING_TOLERANCE:
        derivative_roots.append(-a1 / (2.0 * a2))
    boundaries = sorted(
        {0.0, upper, *(root for root in derivative_roots if 0.0 < root < upper)}
    )

    def value(chroma: float) -> float:
        return ((a3 * chroma + a2) * chroma + a1) * chroma + a0

    roots = [point for point in boundaries if abs(value(point)) <= _MAPPING_TOLERANCE]
    for lower, high in zip(boundaries, boundaries[1:]):
        low_value, high_value = value(lower), value(high)
        if low_value * high_value >= 0.0:
            continue
        for _ in range(80):
            midpoint = (lower + high) / 2.0
            if value(lower) * value(midpoint) <= 0.0:
                high = midpoint
            else:
                lower = midpoint
        roots.append((lower + high) / 2.0)
    return tuple(sorted({round(root, 14) for root in roots}))


def _ray_feasible_intervals(
    lightness: float, hue_degrees: float, columns: tuple[Xyz, ...]
) -> tuple[tuple[float, float], ...]:
    constraints = _zonotope_constraints(columns)
    # A finite cap derived from the normalized device zonotope bounds every
    # Oklab coordinate reachable by a legal drive vector.  It is deliberately
    # conservative; it is not a sampling horizon.
    maximum_xyz = tuple(
        sum(abs(column[index]) for column in columns) for index in range(3)
    )
    lms_bound = (
        0.8190224432164319 * maximum_xyz[0]
        + 0.3619062562801221 * maximum_xyz[1]
        + 0.12887378261216414 * maximum_xyz[2],
        0.0329836671980271 * maximum_xyz[0]
        + 0.9292868468965546 * maximum_xyz[1]
        + 0.03614466816999844 * maximum_xyz[2],
        0.048177199566046255 * maximum_xyz[0]
        + 0.26423952494422764 * maximum_xyz[1]
        + 0.6335478258136937 * maximum_xyz[2],
    )
    root_bound = tuple(value ** (1.0 / 3.0) for value in lms_bound)
    a_bound = (
        1.9779984951 * root_bound[0]
        + 2.4285922050 * root_bound[1]
        + 0.4505937099 * root_bound[2]
    )
    b_bound = (
        0.0259040371 * root_bound[0]
        + 0.7827717662 * root_bound[1]
        + 0.8086757660 * root_bound[2]
    )
    chroma_cap = hypot(a_bound, b_bound)
    boundaries = {0.0, chroma_cap}
    for constraint in constraints:
        samples = tuple(
            _dot(
                constraint.normal,
                _xyz_from_oklab(
                    (
                        lightness,
                        chroma * cos(radians(hue_degrees)),
                        chroma * sin(radians(hue_degrees)),
                    )
                ),
            )
            for chroma in range(4)
        )
        a0 = samples[0]
        a3 = (samples[3] - 3.0 * samples[2] + 3.0 * samples[1] - samples[0]) / 6.0
        a2 = (samples[2] - 2.0 * samples[1] + samples[0] - 6.0 * a3) / 2.0
        a1 = samples[1] - a0 - a2 - a3
        for bound in (constraint.minimum, constraint.maximum):
            boundaries.update(
                _cubic_roots_on_interval(_Cubic(a3, a2, a1, a0 - bound), chroma_cap)
            )
    ordered = sorted(boundaries)
    intervals: list[tuple[float, float]] = []

    def feasible(chroma: float) -> bool:
        xyz = _xyz_from_oklab(
            (
                lightness,
                chroma * cos(radians(hue_degrees)),
                chroma * sin(radians(hue_degrees)),
            )
        )
        return all(
            constraint.minimum - _MAPPING_TOLERANCE
            <= _dot(constraint.normal, xyz)
            <= constraint.maximum + _MAPPING_TOLERANCE
            for constraint in constraints
        )

    # Tangent or capacity-endpoint intersections can be feasible singleton
    # sets.  Retain them rather than treating only open intervals as gamut.
    intervals.extend((point, point) for point in ordered if feasible(point))
    for lower, upper in zip(ordered, ordered[1:]):
        midpoint = (lower + upper) / 2.0
        if feasible(midpoint):
            intervals.append((lower, upper))
    return tuple(intervals)


def map_and_solve(target_xyz: Xyz, profile: DeviceProfile) -> MappingSolution:
    """Map in D65 OKLCh and solve deterministically in rendering-white XYZ."""

    _require_finite(target_xyz, "target XYZ")
    original_columns = tuple(_emitter_xyz(emitter) for emitter in profile.emitters)
    direct_drives = _solve_white_preferred(target_xyz, original_columns, profile)
    target_d65 = (
        target_xyz
        if profile.rendering_white == _D65
        else bradford_adaptation(target_xyz, profile.rendering_white, _D65)
    )
    target_oklch = _oklch_from_xyz(target_d65)
    if direct_drives is not None:
        # Feasible inputs are an identity operation, including their exact
        # rendering-white XYZ representation.
        return MappingSolution(target_xyz, direct_drives, target_oklch, target_oklch)

    columns = _adapted_columns(profile)
    rendering_white_direction = _xy_to_xyz(profile.rendering_white)
    target_is_neutral = _is_neutral_relative_to_white(
        target_xyz, rendering_white_direction
    )
    white_direction = (
        rendering_white_direction
        if profile.rendering_white == _D65
        else bradford_adaptation(
            _xy_to_xyz(profile.rendering_white), profile.rendering_white, _D65
        )
    )
    constraints = _zonotope_constraints(columns)
    neutral_scale = min(
        (
            constraint.maximum / projection
            if projection > _MAPPING_TOLERANCE
            else constraint.minimum / projection
        )
        for constraint in constraints
        if abs(projection := _dot(constraint.normal, white_direction))
        > _MAPPING_TOLERANCE
    )
    neutral_lightness = _oklch_from_xyz(
        (
            white_direction[0] * neutral_scale,
            white_direction[1] * neutral_scale,
            white_direction[2] * neutral_scale,
        )
    ).lightness
    if target_is_neutral:
        # Neutrality is geometric in the profile's XYZ frame.  Routing it
        # through an OKLab hue after a lightness clamp would turn tiny matrix
        # residuals into an arbitrary colored ray.
        neutral_target_scale = min(max(target_xyz[1], 0.0), neutral_scale)
        mapped_d65: Xyz = (
            white_direction[0] * neutral_target_scale,
            white_direction[1] * neutral_target_scale,
            white_direction[2] * neutral_target_scale,
        )
    else:
        lightness = min(max(target_oklch.lightness, 0.0), neutral_lightness)
        intervals = _ray_feasible_intervals(
            lightness, target_oklch.hue_degrees, columns
        )
        # Gamut mapping may reduce chroma to reach the device gamut, but it
        # must never invent saturation.  The intervals need not be monotonic,
        # so retain only their portions at or below the requested chroma.
        reduced_intervals = tuple(
            (lower, min(upper, target_oklch.chroma))
            for lower, upper in intervals
            if lower <= target_oklch.chroma + _MAPPING_TOLERANCE
        )
        if not reduced_intervals:
            mapped_d65 = (0.0, 0.0, 0.0)
        else:
            chroma = max(interval[1] for interval in reduced_intervals)
            mapped_d65 = _xyz_from_oklab(
                (
                    lightness,
                    chroma * cos(radians(target_oklch.hue_degrees)),
                    chroma * sin(radians(target_oklch.hue_degrees)),
                )
            )
    mapped_xyz = (
        mapped_d65
        if profile.rendering_white == _D65
        else bradford_adaptation(mapped_d65, _D65, profile.rendering_white)
    )
    drives = _solve_white_preferred(mapped_xyz, original_columns, profile)
    if drives is None:
        raise ValueError("mapped target is outside device feasibility tolerance")
    # Return the reproducible device point, avoiding a Bradford round-trip
    # discrepancy between the advertised mapped target and solved drives.
    mapped_xyz = emitted_xyz(profile, drives)
    mapped_d65 = (
        mapped_xyz
        if profile.rendering_white == _D65
        else bradford_adaptation(mapped_xyz, profile.rendering_white, _D65)
    )
    return MappingSolution(
        mapped_xyz, drives, target_oklch, _oklch_from_xyz(mapped_d65)
    )


def apply_brightness_then_inverse_response(
    solved_light: Sequence[float],
    brightness: float,
    response_exponents: Sequence[float],
) -> tuple[float, ...]:
    """Apply the sole brightness scalar in light before nonlinear inversion."""

    if len(solved_light) != len(response_exponents):
        raise ValueError("light and response counts must match")
    if not isfinite(brightness) or brightness < 0.0 or brightness > 1.0:
        raise ValueError("brightness must be finite and in [0, 1]")
    if not all(isfinite(light) and light >= 0.0 for light in solved_light):
        raise ValueError("solved light must be finite and non-negative")
    if not all(
        isfinite(exponent) and exponent > 0.0 for exponent in response_exponents
    ):
        raise ValueError("response exponents must be finite and positive")
    return tuple(
        (light * brightness) ** (1.0 / exponent)
        for light, exponent in zip(solved_light, response_exponents)
    )
