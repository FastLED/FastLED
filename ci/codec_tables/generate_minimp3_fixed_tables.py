#!/usr/bin/env python3
"""Generate the fixed-point constant tables for minimp3's integer DSP path.

Every value here is derived from the ISO 11172-3 / 13818-3 formula it comes
from, evaluated in Python's double precision and rounded once into the target
Q format. Nothing is transcribed from another decoder: the point of Phase 3
(FastLED/FastLED#4054) is a CC0 fixed-point path, so the RPSL-licensed Helix
tree is not a source here even for values that are mathematically forced.

The float tables already in `minimp3.h` are CC0 and *are* used, but only as a
cross-check: `--check` verifies that each generated constant agrees with the
corresponding upstream float literal to within the precision the Q format can
represent. That catches a misread formula, which is the failure mode that would
otherwise show up as "PSNR is 60 dB on one corpus file" much later.

Usage:
    uv run python ci/codec_tables/generate_minimp3_fixed_tables.py --write
    uv run python ci/codec_tables/generate_minimp3_fixed_tables.py --check
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "src" / "third_party" / "minimp3" / "minimp3_fixed_tables.h"

# Q format of the sample pipeline. Kept in sync with MINIMP3_FRAC_BITS in
# minimp3.h; the generator does not emit it, it only documents the intent.
FRAC_BITS = 26


def q(value: float, bits: int) -> int:
    """Round `value` into a Q`bits` fixed-point integer, half away from zero.

    Deliberately a different rule from the decoder's runtime rounding, which is
    half-toward-+infinity because an arithmetic shift is what implements it.
    This runs once at build time, where symmetry around zero is worth more than
    matching a shift, and a constant's sign should not bias its rounding.
    """
    scaled = value * (1 << bits)
    result = math.floor(scaled + 0.5) if scaled >= 0 else math.ceil(scaled - 0.5)
    if not -(1 << 31) <= result <= (1 << 31) - 1:
        raise ValueError(f"Q{bits} overflow for {value!r}: {result}")
    return int(result)


def normalize(value: float) -> tuple[int, int]:
    """Split `value` into a Q30-normalised mantissa and a power-of-two exponent.

    Returns `(mantissa, exponent)` with `value == mantissa * 2**(exponent - 30)`
    and `abs(mantissa)` in `[2**30, 2**31)` for non-zero inputs. This is how the
    decoder carries quantities whose dynamic range does not fit a single Q
    format -- scalefactor gains span roughly 2**-181 to 2**10, and x**(4/3)
    reaches 165000 -- while still multiplying them with plain 32x32->64 integer
    arithmetic.
    """
    if value == 0.0:
        return 0, 0
    exponent = math.floor(math.log2(abs(value))) + 1
    mantissa = q(value / (2.0**exponent), 30)
    # Rounding can carry the mantissa up to exactly 2**30 in magnitude after
    # the divide (value just under a power of two); renormalise if so.
    if abs(mantissa) >= (1 << 31):
        mantissa //= 2
        exponent += 1
    return mantissa, exponent


# --------------------------------------------------------------------------
# Table definitions. Each returns (c_type, name, values, comment).
# --------------------------------------------------------------------------


def pow43_tables() -> tuple[list[int], list[int]]:
    """x**(4/3), laid out exactly like upstream's `g_pow43`.

    Indices 0..15 hold `-pow43(i)` and 16..144 hold `+pow43(x)` for x in
    [0, 128], so the Huffman inner loop can keep folding the sign bit into the
    table index instead of branching.
    """
    mant: list[int] = []
    exp: list[int] = []
    for i in range(16):
        m, e = normalize(-(float(i) ** (4.0 / 3.0)))
        mant.append(m)
        exp.append(e)
    for x in range(129):
        m, e = normalize(float(x) ** (4.0 / 3.0))
        mant.append(m)
        exp.append(e)
    return mant, exp


def expfrac_q30() -> list[int]:
    """2**(r/4) for r in [0, 3] -- the fractional part of every gain exponent.

    Scalefactor gains are 2**(k/4) for integer k, so splitting k into
    `4*a + r` leaves four possible mantissas and an integer shift.
    """
    return [q(2.0 ** (r / 4.0), 30) for r in range(4)]


def antialias_q31() -> tuple[list[int], list[int]]:
    """Alias-reduction butterfly coefficients, ISO 11172-3 2.4.3.4.10.1.

    cs = 1/sqrt(1+c^2), ca = c/sqrt(1+c^2); upstream stores `cs` and `-ca`.
    """
    c = [-0.6, -0.535, -0.33, -0.185, -0.095, -0.041, -0.0142, -0.0037]
    cs = [1.0 / math.sqrt(1.0 + ci * ci) for ci in c]
    ca = [ci / math.sqrt(1.0 + ci * ci) for ci in c]
    return [q(v, 31) for v in cs], [q(-v, 31) for v in ca]


def twid9_q30() -> list[int]:
    """IMDCT-36 post-twiddle: cos/sin of (2k+1)*pi/72, k counting down from 8."""
    angles = [(2 * (8 - i) + 1) * math.pi / 72.0 for i in range(9)]
    return [q(math.cos(a), 30) for a in angles] + [q(math.sin(a), 30) for a in angles]


def twid3_q30() -> list[int]:
    """IMDCT-12 post-twiddle: cos/sin of (2k+1)*pi/24, k counting down from 2."""
    angles = [(2 * (2 - i) + 1) * math.pi / 24.0 for i in range(3)]
    return [q(math.cos(a), 30) for a in angles] + [q(math.sin(a), 30) for a in angles]


def mdct_window_q30() -> list[list[int]]:
    """The two long-block windows, ISO 11172-3 2.4.3.4.10.3.

    Row 0 is the normal window, cos/sin of (2i+1)*pi/72. Row 1 is the
    start/stop window: flat for the first six, then the pi/24 family. Upstream
    keeps both in the same cos-then-sin halves layout as the twiddles.
    """
    normal_angles = [(2 * i + 1) * math.pi / 72.0 for i in range(9)]
    normal = [q(math.cos(a), 30) for a in normal_angles] + [
        q(math.sin(a), 30) for a in normal_angles
    ]

    stop_angles = [0.0] * 6 + [(2 * k + 1) * math.pi / 24.0 for k in range(3)]
    stop = [q(math.cos(a), 30) for a in stop_angles] + [
        q(math.sin(a), 30) for a in stop_angles
    ]
    return [normal, stop]


def sec_q27() -> list[int]:
    """DCT-32 secants, three per butterfly stage: 1/(2*cos(theta))."""
    out: list[int] = []
    for i in range(8):
        out.append(q(1.0 / (2.0 * math.cos((31 - 2 * i) * math.pi / 64.0)), 27))
        out.append(q(1.0 / (2.0 * math.cos((2 * i + 1) * math.pi / 64.0)), 27))
        out.append(q(1.0 / (2.0 * math.cos((2 * i + 1) * math.pi / 32.0)), 27))
    return out


def pan_q30() -> list[int]:
    """MPEG-1 intensity-stereo pan, ISO 11172-3 2.4.3.4.9.3.

    ratio = tan(ipos*pi/12); kl = ratio/(1+ratio), kr = 1/(1+ratio).
    """
    out: list[int] = []
    for ipos in range(7):
        ratio = math.tan(ipos * math.pi / 12.0)
        out.append(q(ratio / (1.0 + ratio), 30))
        out.append(q(1.0 / (1.0 + ratio), 30))
    return out


def deq_l12_tables() -> tuple[list[int], list[int]]:
    """Layer I/II dequantiser steps, ISO 11172-3 tables B.3/B.4.

    Upstream writes these as `9.53674316e-07/x` and two siblings, which is
    `2**(-20 - j/3) / steps[i]` for j in [0, 2]; the decoder then scales by
    `2**(21 - b/3)`. Carried as mantissa+exponent because the raw values are
    around 1e-7 and the runtime shift can move them a long way either side.
    """
    steps = [3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383,
             32767, 65535, 3, 5, 9]
    mant: list[int] = []
    exp: list[int] = []
    for step in steps:
        for j in range(3):
            m, e = normalize(2.0 ** (-20.0 - j / 3.0) / float(step))
            mant.append(m)
            exp.append(e)
    return mant, exp


def scalar_constants() -> list[tuple[str, int, int, str]]:
    """Named scalars used inline by the DCT/IMDCT kernels: (name, value, bits, comment)."""
    return [
        ("MP3D_Q30_SQRT2", q(math.sqrt(2.0), 30), 30,
         "sqrt(2), the mid/side stereo rescale"),
        ("MP3D_Q31_COS_PI_4", q(math.cos(math.pi / 4.0), 31), 31,
         "cos(pi/4), the DCT-32 half-butterfly scale"),
        ("MP3D_Q31_TAN_PI_16", q(math.tan(math.pi / 16.0), 31), 31,
         "tan(pi/16), first and third steps of the pi/8 rotation"),
        ("MP3D_Q31_SIN_PI_8", q(math.sin(math.pi / 8.0), 31), 31,
         "sin(pi/8), middle step of the pi/8 rotation"),
        ("MP3D_Q29_SEC_PI_16", q(1.0 / (2.0 * math.cos(math.pi / 16.0)), 29), 29,
         "1/(2 cos(pi/16))"),
        ("MP3D_Q29_SEC_PI_8", q(1.0 / (2.0 * math.cos(math.pi / 8.0)), 29), 29,
         "1/(2 cos(pi/8))"),
        ("MP3D_Q29_SEC_3PI_16", q(1.0 / (2.0 * math.cos(3.0 * math.pi / 16.0)), 29), 29,
         "1/(2 cos(3pi/16))"),
        ("MP3D_Q29_SEC_5PI_16", q(1.0 / (2.0 * math.cos(5.0 * math.pi / 16.0)), 29), 29,
         "1/(2 cos(5pi/16))"),
        ("MP3D_Q29_SEC_3PI_8", q(1.0 / (2.0 * math.cos(3.0 * math.pi / 8.0)), 29), 29,
         "1/(2 cos(3pi/8))"),
        ("MP3D_Q29_SEC_7PI_16", q(1.0 / (2.0 * math.cos(7.0 * math.pi / 16.0)), 29), 29,
         "1/(2 cos(7pi/16))"),
        ("MP3D_Q31_COS_PI_9", q(math.cos(math.pi / 9.0), 31), 31, "cos(20 deg)"),
        ("MP3D_Q31_COS_2PI_9", q(math.cos(2.0 * math.pi / 9.0), 31), 31, "cos(40 deg)"),
        ("MP3D_Q31_COS_4PI_9", q(math.cos(4.0 * math.pi / 9.0), 31), 31, "cos(80 deg)"),
        ("MP3D_Q31_COS_PI_6", q(math.cos(math.pi / 6.0), 31), 31, "cos(30 deg)"),
        ("MP3D_Q31_COS_PI_18", q(math.cos(math.pi / 18.0), 31), 31, "cos(10 deg)"),
        ("MP3D_Q31_COS_7PI_18", q(math.cos(7.0 * math.pi / 18.0), 31), 31, "cos(70 deg)"),
        ("MP3D_Q31_COS_5PI_18", q(math.cos(5.0 * math.pi / 18.0), 31), 31, "cos(50 deg)"),
        ("MP3D_Q30_POW43_C1", q(4.0 / 3.0, 30), 30,
         "first-order term of the x**(4/3) interpolation"),
        ("MP3D_Q30_POW43_C2", q(2.0 / 9.0, 30), 30,
         "second-order term of the x**(4/3) interpolation"),
    ]


# --------------------------------------------------------------------------
# Emission
# --------------------------------------------------------------------------


def emit_array(ctype: str, name: str, values: list[int], per_line: int,
               comment: str) -> str:
    lines = [f"/* {comment} */",
             f"static const {ctype} {name}[{len(values)}] = {{"]
    for start in range(0, len(values), per_line):
        chunk = values[start:start + per_line]
        lines.append("    " + ",".join(str(v) for v in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def render() -> str:
    pow43_mant, pow43_exp = pow43_tables()
    aa_cs, aa_ca = antialias_q31()
    deq_mant, deq_exp = deq_l12_tables()
    windows = mdct_window_q30()

    parts: list[str] = []
    parts.append("""// SPDX-License-Identifier: CC0-1.0
/*
    Fixed-point constant tables for minimp3's integer DSP path.

    GENERATED FILE -- do not edit by hand. Regenerate with:
        uv run python ci/codec_tables/generate_minimp3_fixed_tables.py --write
    and verify with `--check`, which CI runs.

    Every constant is derived from its ISO 11172-3 / 13818-3 formula in the
    generator and rounded once into the Q format named on the array. Values are
    dedicated to the public domain along with the rest of minimp3.

    NOTE: this file has no include guard and opens no namespace on purpose. It
    is included from inside minimp3.h's already-open fl::MINIMP3_NAMESPACE, so
    that a translation unit holding more than one decoder variant gets its own
    copy of the tables per variant rather than the first one silently winning.
*/
""")

    parts.append(emit_array(
        "int32_t", "g_pow43_mant", pow43_mant, 8,
        "x**(4/3) mantissas, value = mant * 2**(exp - 30); upstream g_pow43 layout"))
    parts.append(emit_array(
        "int8_t", "g_pow43_exp", pow43_exp, 24,
        "matching exponents for g_pow43_mant"))
    parts.append(emit_array(
        "int32_t", "g_expfrac_q30", expfrac_q30(), 4,
        "2**(r/4) for r in [0,3], Q30 -- fractional part of a gain exponent"))
    parts.append(emit_array(
        "int32_t", "g_aa_cs_q31", aa_cs, 8,
        "alias reduction cs = 1/sqrt(1+c^2), Q31"))
    parts.append(emit_array(
        "int32_t", "g_aa_ca_q31", aa_ca, 8,
        "alias reduction -ca = -c/sqrt(1+c^2), Q31"))
    parts.append(emit_array(
        "int32_t", "g_twid9_q30", twid9_q30(), 9,
        "IMDCT-36 twiddles: cos then sin of (2k+1)*pi/72, Q30"))
    parts.append(emit_array(
        "int32_t", "g_twid3_q30", twid3_q30(), 6,
        "IMDCT-12 twiddles: cos then sin of (2k+1)*pi/24, Q30"))
    parts.append(emit_array(
        "int32_t", "g_mdct_window_normal_q30", windows[0], 9,
        "long-block window, cos then sin of (2i+1)*pi/72, Q30"))
    parts.append(emit_array(
        "int32_t", "g_mdct_window_stop_q30", windows[1], 9,
        "start/stop-block window, Q30"))
    parts.append(emit_array(
        "int32_t", "g_sec_q27", sec_q27(), 6,
        "DCT-32 secants 1/(2 cos theta), Q27 (values reach 10.19)"))
    parts.append(emit_array(
        "int32_t", "g_pan_q30", pan_q30(), 6,
        "MPEG-1 intensity stereo pan pairs (kl, kr), Q30"))
    parts.append(emit_array(
        "int32_t", "g_deq_L12_mant", deq_mant, 6,
        "Layer I/II dequantiser steps, value = mant * 2**(exp - 30)"))
    parts.append(emit_array(
        "int8_t", "g_deq_L12_exp", deq_exp, 18,
        "matching exponents for g_deq_L12_mant"))

    scalars = ["/* Inline kernel scalars. */"]
    for name, value, bits, comment in scalar_constants():
        scalars.append(f"#define {name} {value} /* Q{bits}: {comment} */")
    parts.append("\n".join(scalars))

    return "\n\n".join(parts) + "\n"


# --------------------------------------------------------------------------
# Cross-check against the upstream float literals
# --------------------------------------------------------------------------


def cross_check() -> list[str]:
    """Compare generated constants against upstream's CC0 float literals.

    Upstream's numbers are the independent witness that a formula was read
    correctly. The relative tolerance is one part in 2**20 -- far looser than
    the Q formats, far tighter than a transposed digit. It is paired with an
    absolute floor because upstream writes its literals to eight decimal
    places, so a small coefficient like g_aa[1][7] (0.00369997, truly
    0.0036999746) carries barely six significant digits and cannot meet a
    relative bound on its own.
    """
    problems: list[str] = []

    def compare(label: str, generated: float, upstream: float) -> None:
        absolute = abs(generated - upstream)
        if absolute <= 5e-9:
            return
        scale = max(abs(upstream), 1e-9)
        if absolute / scale > 2.0**-20:
            problems.append(
                f"{label}: generated {generated!r} vs upstream {upstream!r}")

    aa_cs, aa_ca = antialias_q31()
    upstream_aa0 = [0.85749293, 0.88174200, 0.94962865, 0.98331459,
                    0.99551782, 0.99916056, 0.99989920, 0.99999316]
    upstream_aa1 = [0.51449576, 0.47173197, 0.31337745, 0.18191320,
                    0.09457419, 0.04096558, 0.01419856, 0.00369997]
    for i, expected in enumerate(upstream_aa0):
        compare(f"g_aa[0][{i}]", aa_cs[i] / 2.0**31, expected)
    for i, expected in enumerate(upstream_aa1):
        compare(f"g_aa[1][{i}]", aa_ca[i] / 2.0**31, expected)

    upstream_twid9 = [0.73727734, 0.79335334, 0.84339145, 0.88701083,
                      0.92387953, 0.95371695, 0.97629601, 0.99144486,
                      0.99904822, 0.67559021, 0.60876143, 0.53729961,
                      0.46174861, 0.38268343, 0.30070580, 0.21643961,
                      0.13052619, 0.04361938]
    generated_twid9 = twid9_q30()
    for i, expected in enumerate(upstream_twid9):
        compare(f"g_twid9[{i}]", generated_twid9[i] / 2.0**30, expected)

    upstream_twid3 = [0.79335334, 0.92387953, 0.99144486,
                      0.60876143, 0.38268343, 0.13052619]
    generated_twid3 = twid3_q30()
    for i, expected in enumerate(upstream_twid3):
        compare(f"g_twid3[{i}]", generated_twid3[i] / 2.0**30, expected)

    upstream_sec = [10.19000816, 0.50060302, 0.50241929, 3.40760851, 0.50547093,
                    0.52249861, 2.05778098, 0.51544732, 0.56694406, 1.48416460,
                    0.53104258, 0.64682180, 1.16943991, 0.55310392, 0.78815460,
                    0.97256821, 0.58293498, 1.06067765, 0.83934963, 0.62250412,
                    1.72244716, 0.74453628, 0.67480832, 5.10114861]
    generated_sec = sec_q27()
    for i, expected in enumerate(upstream_sec):
        compare(f"g_sec[{i}]", generated_sec[i] / 2.0**27, expected)

    upstream_pan = [0, 1, 0.21132487, 0.78867513, 0.36602540, 0.63397460, 0.5,
                    0.5, 0.63397460, 0.36602540, 0.78867513, 0.21132487, 1, 0]
    generated_pan = pan_q30()
    for i, expected in enumerate(upstream_pan):
        compare(f"g_pan[{i}]", generated_pan[i] / 2.0**30, expected)

    upstream_windows = [
        [0.99904822, 0.99144486, 0.97629601, 0.95371695, 0.92387953,
         0.88701083, 0.84339145, 0.79335334, 0.73727734, 0.04361938,
         0.13052619, 0.21643961, 0.30070580, 0.38268343, 0.46174861,
         0.53729961, 0.60876143, 0.67559021],
        [1, 1, 1, 1, 1, 1, 0.99144486, 0.92387953, 0.79335334,
         0, 0, 0, 0, 0, 0, 0.13052619, 0.38268343, 0.60876143],
    ]
    generated_windows = mdct_window_q30()
    for row, expected_row in enumerate(upstream_windows):
        for i, expected in enumerate(expected_row):
            compare(f"g_mdct_window[{row}][{i}]",
                    generated_windows[row][i] / 2.0**30, expected)

    # x**(4/3): the table half that upstream spells out.
    upstream_pow43_head = [0, 1, 2.519842, 4.326749, 6.349604, 8.549880,
                           10.902724, 13.390518, 16.000000]
    mant, exp = pow43_tables()
    for x, expected in enumerate(upstream_pow43_head):
        value = mant[16 + x] * 2.0 ** (exp[16 + x] - 30)
        compare(f"g_pow43[16+{x}]", value, expected)

    # Layer I/II: upstream's first triple is DQ(3).
    deq_mant, deq_exp = deq_l12_tables()
    for j, expected in enumerate([9.53674316e-07 / 3, 7.56931807e-07 / 3,
                                  6.00777173e-07 / 3]):
        value = deq_mant[j] * 2.0 ** (deq_exp[j] - 30)
        compare(f"g_deq_L12[{j}]", value, expected)

    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true",
                        help="write the generated header")
    parser.add_argument("--check", action="store_true",
                        help="verify the committed header matches, and that "
                             "every constant agrees with upstream's floats")
    args = parser.parse_args()
    if not args.write and not args.check:
        parser.error("pass --write or --check")

    problems = cross_check()
    if problems:
        print("Generated constants disagree with upstream float literals:",
              file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1

    rendered = render()
    if args.write:
        OUTPUT.write_text(rendered, encoding="utf-8")
        print(f"wrote {OUTPUT.relative_to(ROOT)}")
    if args.check:
        if not OUTPUT.exists():
            print(f"{OUTPUT.relative_to(ROOT)} is missing; run --write",
                  file=sys.stderr)
            return 1
        current = OUTPUT.read_text(encoding="utf-8")
        if current != rendered:
            print(f"{OUTPUT.relative_to(ROOT)} is stale; run --write",
                  file=sys.stderr)
            return 1
        print("fixed-point tables are current and agree with upstream")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
