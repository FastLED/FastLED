// ok cpp include
// Tests for the shared colorimetric_response math primitives (issue #3255).
//
// These exercise the helpers in isolation of the RGBW solver topology, so a
// future RGB-only colorimetric path (PR 3 of #3255) and the existing RGBW
// path share the same coverage of the underlying primitives. The
// rgbw_colorimetric.cpp test suite covers them indirectly via the RGBW
// solvers; here we hit them directly through the canonical
// `fl::colorimetric_response::` namespace.

#include "test.h"

#include "fl/gfx/colorimetric_response.h"
#include "fl/math/math.h"

using namespace fl;
using namespace fl::colorimetric_response;


FL_TEST_CASE("xyY_to_XYZ: round trip vs. direct math") {
    // Build XYZ from (x=0.5, y=0.3, Y=1.0), then verify the relationships
    //   X = x * Y / y
    //   Z = (1 - x - y) * Y / y
    float xyz[3];
    xyY_to_XYZ(0.5f, 0.3f, 1.0f, xyz);
    FL_CHECK_CLOSE(xyz[0], 0.5f / 0.3f, 1e-5f);
    FL_CHECK_CLOSE(xyz[1], 1.0f, 1e-5f);
    FL_CHECK_CLOSE(xyz[2], (1.0f - 0.5f - 0.3f) / 0.3f, 1e-5f);
}


FL_TEST_CASE("xyY_to_XYZ: degenerate y returns zero") {
    float xyz[3] = {99.0f, 99.0f, 99.0f};
    xyY_to_XYZ(0.5f, 0.0f, 1.0f, xyz);
    FL_CHECK_CLOSE(xyz[0], 0.0f, 1e-9f);
    FL_CHECK_CLOSE(xyz[1], 0.0f, 1e-9f);
    FL_CHECK_CLOSE(xyz[2], 0.0f, 1e-9f);
}


FL_TEST_CASE("invert3x3: identity") {
    const float I[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    float Iinv[3][3];
    FL_CHECK(invert3x3(I, Iinv));
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            FL_CHECK_CLOSE(Iinv[r][c], I[r][c], 1e-6f);
        }
    }
}


FL_TEST_CASE("invert3x3: M * M^-1 == I on a non-trivial matrix") {
    const float M[3][3] = {
        {1.0f, 2.0f, 3.0f},
        {0.0f, 1.0f, 4.0f},
        {5.0f, 6.0f, 0.0f}};
    float Minv[3][3];
    FL_CHECK(invert3x3(M, Minv));

    // P = M * Minv -- should be identity within float tolerance.
    float P[3][3] = {{0}};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            for (int k = 0; k < 3; ++k) {
                P[r][c] += M[r][k] * Minv[k][c];
            }
        }
    }
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            const float expected = (r == c) ? 1.0f : 0.0f;
            FL_CHECK_CLOSE(P[r][c], expected, 1e-4f);
        }
    }
}


FL_TEST_CASE("invert3x3: singular matrix returns false") {
    // Two identical rows -- determinant is zero.
    const float singular[3][3] = {
        {1.0f, 2.0f, 3.0f},
        {1.0f, 2.0f, 3.0f},
        {4.0f, 5.0f, 6.0f}};
    float out[3][3];
    FL_CHECK(!invert3x3(singular, out));
}


FL_TEST_CASE("matvec3: hand-computed product") {
    const float M[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    const float v[3] = {1.0f, 0.0f, -1.0f};
    float out[3];
    matvec3(M, v, out);
    // [1*1 + 2*0 + 3*(-1), 4*1 + 5*0 + 6*(-1), 7*1 + 8*0 + 9*(-1)]
    //  = [-2, -2, -2]
    FL_CHECK_CLOSE(out[0], -2.0f, 1e-6f);
    FL_CHECK_CLOSE(out[1], -2.0f, 1e-6f);
    FL_CHECK_CLOSE(out[2], -2.0f, 1e-6f);
}


FL_TEST_CASE("barycentric_xy: inside/outside classification") {
    const float A[2] = {0.0f, 0.0f};
    const float B[2] = {1.0f, 0.0f};
    const float C[2] = {0.0f, 1.0f};

    const float centroid[2] = {1.0f / 3.0f, 1.0f / 3.0f};
    float bary[3];
    FL_CHECK(barycentric_xy(centroid, A, B, C, bary));
    FL_CHECK(bary[0] >= 0.0f);
    FL_CHECK(bary[1] >= 0.0f);
    FL_CHECK(bary[2] >= 0.0f);
    FL_CHECK_CLOSE(bary[0] + bary[1] + bary[2], 1.0f, 1e-5f);

    const float outside[2] = {2.0f, 2.0f};
    FL_CHECK(barycentric_xy(outside, A, B, C, bary));
    FL_CHECK(bary[0] < 0.0f);
}


FL_TEST_CASE("cct_to_xy: known Planckian-locus points") {
    // Krystek's algorithm returns Planckian (blackbody) chromaticities.
    // Reference values from the Planckian locus tables (loose tolerance
    // for single-precision drift).
    float xy[2];

    cct_to_xy(6500, xy);
    FL_CHECK_CLOSE(xy[0], 0.3135f, 0.005f);
    FL_CHECK_CLOSE(xy[1], 0.3237f, 0.005f);

    cct_to_xy(2700, xy);
    FL_CHECK_CLOSE(xy[0], 0.4600f, 0.005f);
    FL_CHECK_CLOSE(xy[1], 0.4107f, 0.005f);

    cct_to_xy(4000, xy);
    FL_CHECK_CLOSE(xy[0], 0.3805f, 0.005f);
    FL_CHECK_CLOSE(xy[1], 0.3768f, 0.005f);
}


FL_TEST_CASE("cct_to_xy: clamps out-of-range CCTs") {
    float xy_low[2], xy_clamp_low[2];
    cct_to_xy(1500, xy_low);
    cct_to_xy(500, xy_clamp_low);
    FL_CHECK_CLOSE(xy_low[0], xy_clamp_low[0], 1e-5f);
    FL_CHECK_CLOSE(xy_low[1], xy_clamp_low[1], 1e-5f);

    float xy_high[2], xy_clamp_high[2];
    cct_to_xy(15000, xy_high);
    cct_to_xy(25000, xy_clamp_high);
    FL_CHECK_CLOSE(xy_high[0], xy_clamp_high[0], 1e-5f);
    FL_CHECK_CLOSE(xy_high[1], xy_clamp_high[1], 1e-5f);
}


FL_TEST_CASE("quantize_u8: rounding + saturation") {
    FL_CHECK(quantize_u8(-1.0f) == 0);
    FL_CHECK(quantize_u8(0.0f) == 0);
    FL_CHECK(quantize_u8(1.0f) == 255);
    FL_CHECK(quantize_u8(2.0f) == 255);
    FL_CHECK(quantize_u8(0.5f) == 128);
}


FL_TEST_CASE("build_source_matrix: Rec.709 + D65 reproduces published sRGB -> XYZ") {
    // Rec.709 / sRGB primary chromaticities + D65 white.
    const float xy_r[2] = {0.640f, 0.330f};
    const float xy_g[2] = {0.300f, 0.600f};
    const float xy_b[2] = {0.150f, 0.060f};
    const float xy_w[2] = {0.3127f, 0.3290f};
    float M[3][3];
    FL_CHECK(build_source_matrix(xy_r, xy_g, xy_b, xy_w, M));

    // M * [1, 1, 1] must land at D65 in XYZ-at-Y=1.
    const float one[3] = {1.0f, 1.0f, 1.0f};
    float white_xyz[3];
    matvec3(M, one, white_xyz);

    float d65_xyz[3];
    xyY_to_XYZ(xy_w[0], xy_w[1], 1.0f, d65_xyz);
    FL_CHECK_CLOSE(white_xyz[0], d65_xyz[0], 1e-4f);
    FL_CHECK_CLOSE(white_xyz[1], d65_xyz[1], 1e-4f);
    FL_CHECK_CLOSE(white_xyz[2], d65_xyz[2], 1e-4f);

    // Compare against the published linear sRGB -> XYZ D65 matrix (Bruce
    // Lindbloom). Loose tolerance because Lindbloom's published constants
    // use rounded primaries; the construction is exact from the inputs.
    FL_CHECK_CLOSE(M[0][0], 0.4124f, 5e-3f);
    FL_CHECK_CLOSE(M[1][0], 0.2126f, 5e-3f);
    FL_CHECK_CLOSE(M[2][0], 0.0193f, 5e-3f);
}


FL_TEST_CASE("build_source_matrix: singular primaries return false") {
    // Three identical primary chromaticities -- all columns of the primary
    // matrix collapse to one vector, det is exactly zero in float
    // arithmetic, so invert3x3() bails and build_source_matrix() reports
    // failure. Near-collinear but distinct primaries can still pass float
    // precision; only the exact-collapse case is a hard guarantee.
    const float xy_same[2] = {0.30f, 0.30f};
    const float xy_w[2] = {0.30f, 0.30f};
    float M[3][3];
    FL_CHECK(!build_source_matrix(xy_same, xy_same, xy_same, xy_w, M));
}


FL_TEST_CASE("nnls3: identity system trivial recovery") {
    // M = I, b = (1, 2, 3) -> t ~= (1, 2, 3), residual ~= 0.
    // Tolerance reflects the documented solver shape -- 500 iterations at
    // step 0.01 has geometric convergence 0.99^500 ~= 0.66 %, so the gap
    // for target 3 is ~0.02. Use a comfortable 3 % bound.
    const float M[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const float b[3] = {1.0f, 2.0f, 3.0f};
    float t[3];
    float residual = -1.0f;
    nnls3(M, b, t, &residual);
    FL_CHECK_CLOSE(t[0], 1.0f, 0.03f);
    FL_CHECK_CLOSE(t[1], 2.0f, 0.06f);
    FL_CHECK_CLOSE(t[2], 3.0f, 0.10f);
    FL_CHECK(residual < 0.10f);
}


FL_TEST_CASE("nnls3: non-negativity is enforced") {
    // M = I, b = (-1, 0, 0). The unconstrained least-squares solution is
    // (-1, 0, 0) but the non-negativity constraint should clamp t[0] to 0.
    const float M[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const float b[3] = {-1.0f, 0.0f, 0.0f};
    float t[3];
    nnls3(M, b, t, nullptr);
    FL_CHECK(t[0] >= 0.0f);
    FL_CHECK(t[1] >= 0.0f);
    FL_CHECK(t[2] >= 0.0f);
    FL_CHECK_CLOSE(t[0], 0.0f, 1e-3f);
}


FL_TEST_CASE("hermite_basis: node values at t = 0, 0.5, 1") {
    // h00(0)=1, h01(0)=0, h10(0)=0, h11(0)=0
    // h00(1)=0, h01(1)=1, h10(1)=0, h11(1)=0
    // h00(0.5)=0.5, h01(0.5)=0.5, h10(0.5)=0.125, h11(0.5)=-0.125
    float b[4];

    hermite_basis(0.0f, b);
    FL_CHECK_CLOSE(b[0], 1.0f, 1e-6f);
    FL_CHECK_CLOSE(b[1], 0.0f, 1e-6f);
    FL_CHECK_CLOSE(b[2], 0.0f, 1e-6f);
    FL_CHECK_CLOSE(b[3], 0.0f, 1e-6f);

    hermite_basis(1.0f, b);
    FL_CHECK_CLOSE(b[0], 0.0f, 1e-6f);
    FL_CHECK_CLOSE(b[1], 1.0f, 1e-6f);
    FL_CHECK_CLOSE(b[2], 0.0f, 1e-6f);
    FL_CHECK_CLOSE(b[3], 0.0f, 1e-6f);

    hermite_basis(0.5f, b);
    FL_CHECK_CLOSE(b[0], 0.5f, 1e-6f);
    FL_CHECK_CLOSE(b[1], 0.5f, 1e-6f);
    FL_CHECK_CLOSE(b[2], 0.125f, 1e-6f);
    FL_CHECK_CLOSE(b[3], -0.125f, 1e-6f);
}


FL_TEST_CASE("hermite_basis: partition-of-unity at every node") {
    // h00(t) + h01(t) must equal 1 for ALL t -- they're the value basis.
    for (int i = 0; i <= 10; ++i) {
        const float t = static_cast<float>(i) / 10.0f;
        float b[4];
        hermite_basis(t, b);
        FL_CHECK_CLOSE(b[0] + b[1], 1.0f, 1e-6f);
    }
}


FL_TEST_CASE("quantize_lut_cell: round-trip via kLutQ") {
    // value v -> i16(v * kLutQ + 0.5), then dequantize via i16 * (1/kLutQ).
    // Round-trip error <= 0.5 / kLutQ ~= 1.2e-4.
    const float v_in = 0.7531f;
    const i16 q = quantize_lut_cell(v_in);
    const float v_out = static_cast<float>(q) / static_cast<float>(kLutQ);
    FL_CHECK_CLOSE(v_out, v_in, 0.5f / static_cast<float>(kLutQ));
}


FL_TEST_CASE("quantize_lut_cell: saturates at i16 bounds") {
    // Very large positive value clamps to i16 max (32767).
    FL_CHECK(quantize_lut_cell(100.0f) == 32767);
    // Very large negative value clamps to i16 min (-32768).
    FL_CHECK(quantize_lut_cell(-100.0f) == -32768);
}


FL_TEST_CASE("LUT stride constants match documented per-cell storage") {
    FL_CHECK(kLutStrideBilinear == 4);   // 4 channel values per cell
    FL_CHECK(kLutStrideHermite == 12);   // 4 values + 8 slopes per cell
    FL_CHECK(LutInterp::Bilinear != LutInterp::Hermite);
}


FL_TEST_CASE("EmitterProfile is trivially constructible and zero-initialized") {
    // PR 1 only declares the struct; the solver lands in PR 3. Verify the
    // struct is value-init-compatible (no constructors, no virtual, POD-like)
    // so it can ship in `kRgbDefaultProfile` as a constexpr / extern const.
    EmitterProfile p{};
    FL_CHECK_CLOSE(p.xy_r[0], 0.0f, 1e-9f);
    FL_CHECK_CLOSE(p.lum_g, 0.0f, 1e-9f);
    FL_CHECK_CLOSE(p.input_xy_w[1], 0.0f, 1e-9f);
}
