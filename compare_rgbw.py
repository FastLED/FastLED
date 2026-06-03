"""Compare FastLED's C++ RGBW solver (mirrored in Python) against the Python
reference implementation from
https://gist.github.com/JChalka/cebc5a018066cae05d98cf9088c3b3b9

The C++-side math is ported verbatim from src/fl/gfx/rgbw_colorimetric.{h,cpp.hpp}
so any divergence here is a real divergence in the algorithm, not a translation
artifact. Both sides are configured with the SAME LED primaries (from the
reference's PRIMARIES_XY) so any output differences come from the algorithm,
not the emitter calibration.
"""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

import numpy as np


# ─── Load reference model ──────────────────────────────────────────────────

REF_PATH = Path(__file__).parent / ".ref_xy_model.py"
spec = importlib.util.spec_from_file_location("xy_target_rgbw_model", REF_PATH)
ref = importlib.util.module_from_spec(spec)
sys.modules["xy_target_rgbw_model"] = ref
spec.loader.exec_module(ref)


# ─── FastLED-side port (faithful translation of rgbw_colorimetric.cpp.hpp) ──


def xyY_to_XYZ(x: float, y: float, Y: float) -> np.ndarray:
    """Mirror src/fl/gfx/rgbw_colorimetric.h:xyY_to_XYZ."""
    if y < 1e-12:
        return np.zeros(3)
    return np.array([x * Y / y, Y, (1.0 - x - y) * Y / y])


def invert3x3(m: np.ndarray) -> tuple[np.ndarray, bool]:
    """Mirror invert3x3 — returns (inv, ok)."""
    det = np.linalg.det(m)
    if abs(det) < 1e-20:
        return np.zeros((3, 3)), False
    return np.linalg.inv(m), True


def build_source_matrix(xy_r, xy_g, xy_b, xy_w) -> tuple[np.ndarray, bool]:
    """Mirror src/fl/gfx/rgbw_colorimetric.h:build_source_matrix."""
    xyz_R = xyY_to_XYZ(xy_r[0], xy_r[1], 1.0)
    xyz_G = xyY_to_XYZ(xy_g[0], xy_g[1], 1.0)
    xyz_B = xyY_to_XYZ(xy_b[0], xy_b[1], 1.0)
    xyz_W = xyY_to_XYZ(xy_w[0], xy_w[1], 1.0)
    P = np.column_stack([xyz_R, xyz_G, xyz_B])
    P_inv, ok = invert3x3(P)
    if not ok:
        return np.zeros((3, 3)), False
    k = P_inv @ xyz_W
    M = np.column_stack([k[0] * xyz_R, k[1] * xyz_G, k[2] * xyz_B])
    return M, True


def barycentric_xy(t, A, B, C) -> tuple[np.ndarray, bool]:
    """Mirror barycentric_xy. Returns (bary, ok)."""
    v0 = np.array(B) - np.array(A)
    v1 = np.array(C) - np.array(A)
    v2 = np.array(t) - np.array(A)
    d00 = v0 @ v0
    d01 = v0 @ v1
    d11 = v1 @ v1
    d20 = v2 @ v0
    d21 = v2 @ v1
    den = d00 * d11 - d01 * d01
    if abs(den) < 1e-20:
        return np.zeros(3), False
    u = (d11 * d20 - d01 * d21) / den
    v = (d00 * d21 - d01 * d20) / den
    return np.array([1.0 - u - v, u, v]), True


class ProfileCache:
    """Mirror ProfileCache layout from rgbw_colorimetric.h."""

    def __init__(self, profile: dict):
        self.profile = profile
        self.P_R = xyY_to_XYZ(profile["xy_r"][0], profile["xy_r"][1], profile["lum_r"])
        self.P_G = xyY_to_XYZ(profile["xy_g"][0], profile["xy_g"][1], profile["lum_g"])
        self.P_B = xyY_to_XYZ(profile["xy_b"][0], profile["xy_b"][1], profile["lum_b"])
        self.xy_w = list(profile["xy_w"])
        self.P_W = xyY_to_XYZ(self.xy_w[0], self.xy_w[1], profile["lum_w"])
        P_RGB = np.column_stack([self.P_R, self.P_G, self.P_B])
        P_RGW = np.column_stack([self.P_R, self.P_G, self.P_W])
        P_RBW = np.column_stack([self.P_R, self.P_B, self.P_W])
        P_BGW = np.column_stack([self.P_B, self.P_G, self.P_W])
        self.P_RGB_inv, _ = invert3x3(P_RGB)
        self.P_RGW_inv, _ = invert3x3(P_RGW)
        self.P_RBW_inv, _ = invert3x3(P_RBW)
        self.P_BGW_inv, _ = invert3x3(P_BGW)
        self.d_W = self.P_RGB_inv @ self.P_W
        # Source space (#2705)
        self.M_src, self.has_source_space = build_source_matrix(
            profile["input_xy_r"],
            profile["input_xy_g"],
            profile["input_xy_b"],
            profile["input_xy_w"],
        )


def solve_strict_subgamut(
    cache: ProfileCache, s_r: float, s_g: float, s_b: float
) -> np.ndarray:
    """Mirror solve_strict_subgamut."""
    if cache.has_source_space:
        X_t = cache.M_src @ np.array([s_r, s_g, s_b])
    else:
        X_t = cache.P_R * s_r + cache.P_G * s_g + cache.P_B * s_b
    if X_t.sum() < 1e-9:
        return np.zeros(4)
    xy_t = X_t[:2] / X_t.sum()
    p = cache.profile
    triangles = [
        (p["xy_r"], p["xy_g"], cache.xy_w, cache.P_RGW_inv, (0, 1, 3)),
        (p["xy_r"], p["xy_b"], cache.xy_w, cache.P_RBW_inv, (0, 2, 3)),
        (p["xy_b"], p["xy_g"], cache.xy_w, cache.P_BGW_inv, (2, 1, 3)),
    ]
    eps = 1e-4
    out = np.zeros(4)
    for xy_a, xy_b, xy_c, Pinv, idx in triangles:
        bary, ok = barycentric_xy(xy_t, xy_a, xy_b, xy_c)
        if not ok or any(b < -eps for b in bary):
            continue
        t = Pinv @ X_t
        if any(ti < -eps for ti in t):
            continue
        t = np.maximum(t, 0.0)
        m = float(np.max(t))
        if m > 1.0:
            t /= m
        out[idx[0]] = t[0]
        out[idx[1]] = t[1]
        out[idx[2]] = t[2]
        return out
    return out


def solve_wx_overdrive(cache: ProfileCache, s_r, s_g, s_b, rho=0.5) -> np.ndarray:
    """Mirror solve_wx_overdrive."""
    if cache.has_source_space:
        X_t = cache.M_src @ np.array([s_r, s_g, s_b])
    else:
        X_t = cache.P_R * s_r + cache.P_G * s_g + cache.P_B * s_b
    a = cache.P_RGB_inv @ X_t
    d = cache.d_W
    w_strict = 1.0
    for i in range(3):
        if d[i] > 1e-9 and a[i] >= 0.0:
            lim = a[i] / d[i]
            if lim < w_strict:
                w_strict = lim
    w_strict = max(0.0, min(1.0, w_strict))
    rho = max(0.0, min(1.0, rho))
    w = max(0.0, min(1.0, w_strict + rho * (1.0 - w_strict)))
    t = np.maximum(a - w * d, 0.0)
    out = np.array([t[0], t[1], t[2], w])
    m = float(np.max(out))
    if m > 1.0:
        out /= m
    return out


def quantize_u8(v: float) -> int:
    scaled = v * 255.0 + 0.5
    if scaled <= 0.0:
        return 0
    if scaled >= 255.0:
        return 255
    return int(scaled)


def fastled_rgb_to_rgbw(
    rgb_u8, profile: dict, mode: str = "strict"
) -> tuple[int, int, int, int]:
    """Mirror the dispatch in src/fl/gfx/rgbw.cpp.hpp."""
    r, g, b = rgb_u8
    if r == 0 and g == 0 and b == 0:
        return 0, 0, 0, 0
    s_r = r / 255.0
    s_g = g / 255.0
    s_b = b / 255.0
    cache = ProfileCache(profile)
    if mode == "strict":
        rgbw = solve_strict_subgamut(cache, s_r, s_g, s_b)
    elif mode == "boosted":
        rgbw = solve_wx_overdrive(cache, s_r, s_g, s_b, rho=0.5)
    else:
        raise ValueError(mode)
    return tuple(quantize_u8(v) for v in rgbw)


# ─── Test profile: match reference's emitter values + Rec709/D65 source ────

REF_PROFILE = {
    "xy_r": list(ref.PRIMARIES_XY["R"]),
    "xy_g": list(ref.PRIMARIES_XY["G"]),
    "xy_b": list(ref.PRIMARIES_XY["B"]),
    "xy_w": list(ref.PRIMARIES_XY["W"]),
    "lum_r": ref.MAX_Y["R"] / ref.MAX_Y["W"],
    "lum_g": ref.MAX_Y["G"] / ref.MAX_Y["W"],
    "lum_b": ref.MAX_Y["B"] / ref.MAX_Y["W"],
    "lum_w": 1.0,
    "input_xy_r": [0.6400, 0.3300],
    "input_xy_g": [0.3000, 0.6000],
    "input_xy_b": [0.1500, 0.0600],
    "input_xy_w": [0.31272, 0.32903],
}


def ref_rgb_to_rgbw(
    rgb_u8, method="sub_gamut", gamut="rec709", input_transfer="linear"
) -> tuple[int, int, int, int]:
    """Wrap the reference's solve_rgb16_for_lut at 8-bit, linear input."""
    r16 = rgb_u8[0] * 257
    g16 = rgb_u8[1] * 257
    b16 = rgb_u8[2] * 257
    result_16 = ref.solve_rgb16_for_lut(
        r16,
        g16,
        b16,
        method=method,
        gamut=gamut,
        input_transfer=input_transfer,
        output_bit_depth=8,
    )
    return tuple(int(v) for v in result_16)


# ─── Comparison battery ───────────────────────────────────────────────────


TESTS = [
    # (label, r, g, b)
    ("D65 white", 255, 255, 255),
    ("mid grey", 128, 128, 128),
    ("dim grey", 64, 64, 64),
    ("pure red", 255, 0, 0),
    ("pure green", 0, 255, 0),
    ("pure blue", 0, 0, 255),
    ("cyan", 0, 255, 255),
    ("magenta", 255, 0, 255),
    ("yellow", 255, 255, 0),
    ("warm beige", 220, 200, 150),
    ("cool blue", 100, 150, 220),
    ("near-D65 90%", 230, 230, 230),
    ("orange", 255, 128, 0),
    ("teal", 0, 200, 180),
]


def fmt(rgbw):
    return "(%3d,%3d,%3d,%3d)" % rgbw


def delta(a, b):
    return tuple(int(b[i]) - int(a[i]) for i in range(4))


def main():
    print(
        "Profile: %d×%d×%d × %d-LED primaries from reference; source = Rec709/D65 (linear)"
        % (1, 1, 1, 4)
    )
    print()
    print(
        "Test case          | %-18s | %-18s | %-22s"
        % ("FastLED strict", "Reference sub_gamut", "FastLED − Ref (∆R,G,B,W)")
    )
    print("-" * 100)
    total_strict = 0
    max_strict = 0
    for label, r, g, b in TESTS:
        fl = fastled_rgb_to_rgbw((r, g, b), REF_PROFILE, mode="strict")
        rf = ref_rgb_to_rgbw(
            (r, g, b), method="sub_gamut", gamut="rec709", input_transfer="linear"
        )
        d = delta(fl, rf)
        norm = sum(abs(x) for x in d)
        max_strict = max(max_strict, max(abs(x) for x in d))
        total_strict += norm
        print("%-18s | %s | %s | %s" % (label, fmt(fl), fmt(rf), fmt(d)))
    print()
    print(
        "Strict mode: L1 sum = %d, max |Δ| = %d (across %d cases × 4 channels)"
        % (total_strict, max_strict, len(TESTS))
    )

    print()
    print(
        "Test case          | %-18s | %-18s | %-22s"
        % ("FastLED boosted", "Reference wx_lp_legacy", "FastLED − Ref (∆R,G,B,W)")
    )
    print("-" * 100)
    total_boost = 0
    max_boost = 0
    for label, r, g, b in TESTS:
        fl = fastled_rgb_to_rgbw((r, g, b), REF_PROFILE, mode="boosted")
        rf = ref_rgb_to_rgbw(
            (r, g, b), method="wx", gamut="rec709", input_transfer="linear"
        )
        d = delta(fl, rf)
        norm = sum(abs(x) for x in d)
        max_boost = max(max_boost, max(abs(x) for x in d))
        total_boost += norm
        print("%-18s | %s | %s | %s" % (label, fmt(fl), fmt(rf), fmt(d)))
    print()
    print("Boosted mode (FastLED ρ=0.5 vs reference wx_lp_legacy):")
    print("  L1 sum = %d, max |Δ| = %d" % (total_boost, max_boost))
    print()
    print("Note: FastLED's boosted (wx_overdrive ρ=0.5) is mathematically *different*")
    print(
        "from the reference's wx_lp_legacy (which is the max-W vertex, equal to strict)."
    )
    print("This comparison shows the divergence is real, not a translation bug.")

    # ─── Diagnostic: native gamut (LED primaries == source) ───────────────
    print()
    print("=" * 100)
    print("DIAGNOSTIC: same inputs reinterpreted as 'native' gamut (input primaries")
    print("== LED primaries, no chromatic adaptation, no out-of-hull cases). This")
    print("isolates algorithmic differences from the gamut-boundary projection")
    print("step the reference performs in `_strict_project_target_xyz_to_led_hull`.")
    print("=" * 100)
    native_profile = dict(REF_PROFILE)
    native_profile["input_xy_r"] = list(ref.PRIMARIES_XY["R"])
    native_profile["input_xy_g"] = list(ref.PRIMARIES_XY["G"])
    native_profile["input_xy_b"] = list(ref.PRIMARIES_XY["B"])
    native_profile["input_xy_w"] = [0.3127, 0.3290]
    print()
    print(
        "Test case          | %-18s | %-18s | %-22s"
        % ("FastLED strict", "Reference sub_gamut", "FastLED − Ref (∆R,G,B,W)")
    )
    print("-" * 100)
    total_native = 0
    max_native = 0
    for label, r, g, b in TESTS:
        fl = fastled_rgb_to_rgbw((r, g, b), native_profile, mode="strict")
        rf = ref_rgb_to_rgbw(
            (r, g, b), method="sub_gamut", gamut="native", input_transfer="linear"
        )
        d = delta(fl, rf)
        norm = sum(abs(x) for x in d)
        max_native = max(max_native, max(abs(x) for x in d))
        total_native += norm
        print("%-18s | %s | %s | %s" % (label, fmt(fl), fmt(rf), fmt(d)))
    print()
    print("Native-gamut strict: L1 sum = %d, max |Δ| = %d" % (total_native, max_native))


if __name__ == "__main__":
    main()
