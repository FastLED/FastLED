// ok no header - implementation for fl/gfx/oklab_q16.h

#include "fl/gfx/oklab_q16.h"

#include "fl/math/fixed_point/icbrt.h"

namespace fl {

namespace {

// Ottosson's OKLab matrices, quantized to s16.16 round-to-nearest. The
// float values are in the comments so a reader can check the quantization
// without leaving the file; the inverses are the float inverses quantized,
// not the quantized matrices inverted.
//
// Quantizing the coefficients costs nothing measurable: scoring the mapper
// end-to-end with these integers rather than float64 coefficients moves the
// worst error from 0.152 to 0.153 dE2000, against a budget of 0.5.

/// XYZ -> LMS.
constexpr i32 kLmsFromXyz[3][3] = {
    {  53675,   23718,   -8446},  // +0.8190224432, +0.3619062563, -0.1288737826
    {   2162,   60902,    2369},  // +0.0329836672, +0.9292868469, +0.0361446682
    {   3157,   17317,   41520},  // +0.0481771996, +0.2642395249, +0.6335478258
};

/// Cube-rooted LMS -> OKLab.
constexpr i32 kOklabFromLmsRoot[3][3] = {
    {  13792,   52011,    -267},  // +0.2104542553, +0.7936177850, -0.0040720468
    { 129630, -159160,   29530},  // +1.9779984951, -2.4285922050, +0.4505937099
    {   1698,   51300,  -52997},  // +0.0259040371, +0.7827717662, -0.8086757660
};

/// OKLab -> cube-rooted LMS.
constexpr i32 kLmsRootFromOklab[3][3] = {
    {  65536,   25974,   14143},  // +0.9999999985, +0.3963377922, +0.2158037581
    {  65536,   -6918,   -4185},  // +1.0000000089, -0.1055613423, -0.0638541748
    {  65536,   -5864,  -84639},  // +1.0000000547, -0.0894841821, -1.2914855379
};

/// LMS -> XYZ.
constexpr i32 kXyzFromLms[3][3] = {
    {  80405,  -36557,   18441},  // +1.2268798734, -0.5578149966, +0.2813910502
    {  -2659,   72895,   -4700},  // -0.0405757626, +1.1122868294, -0.0717110667
    {  -5005,  -27623,  104001},  // -0.0763729497, -0.4214933240, +1.5869240244
};

/// Clamp into the domain `kOklabQ16MaxMagnitude` documents.
///
/// Named for this file because .cpp.hpp files share a translation unit under
/// the unity build, so an anonymous namespace does not isolate it from a
/// same-named helper elsewhere in fl/gfx.
i32 clampOklabQ16(i32 value) FL_NO_EXCEPT {
    if (value > kOklabQ16MaxMagnitude) {
        return kOklabQ16MaxMagnitude;
    }
    if (value < -kOklabQ16MaxMagnitude) {
        return -kOklabQ16MaxMagnitude;
    }
    return value;
}

/// One matrix row against a clamped s16.16 vector.
///
/// The accumulator is i64 because each Q16xQ16 product is Q32. Rounding is
/// to nearest rather than truncating: truncation biases every component of
/// every pixel toward zero, and the mapper runs this transform twice per
/// bisection step, so the bias would compound.
i32 dotOklabRowQ16(const i32 (&row)[3], const i32 (&v)[3]) FL_NO_EXCEPT {
    const i64 acc = static_cast<i64>(row[0]) * static_cast<i64>(v[0])
                  + static_cast<i64>(row[1]) * static_cast<i64>(v[1])
                  + static_cast<i64>(row[2]) * static_cast<i64>(v[2]);
    return static_cast<i32>((acc + 32768) >> 16);
}

void dotOklabMatrixQ16(const i32 (&matrix)[3][3], const i32 (&v)[3],
                       i32 (&out)[3]) FL_NO_EXCEPT {
    out[0] = dotOklabRowQ16(matrix[0], v);
    out[1] = dotOklabRowQ16(matrix[1], v);
    out[2] = dotOklabRowQ16(matrix[2], v);
}

/// Signed cube root of an s16.16 raw value.
///
/// For a raw `r` standing for r/2^16, the root `y` satisfies
/// (y/2^16)^3 = r/2^16, so y^3 = r * 2^32 -- the integer cube root of the
/// raw shifted left by 32. The magnitude is taken through i64 because
/// negating INT32_MIN in 32 bits is undefined.
i32 signedCbrtQ16(i32 raw) FL_NO_EXCEPT {
    const i64 magnitude = raw < 0 ? -static_cast<i64>(raw) : static_cast<i64>(raw);
    const i32 root = static_cast<i32>(
        fl::icbrt64(static_cast<u64>(magnitude) << 32));
    return raw < 0 ? -root : root;
}

/// Signed cube of an s16.16 raw value, rounding to nearest at each step.
///
/// Inputs are clamped first, so the largest magnitude squared is 2^36 and
/// the product with the third factor is 2^38 -- comfortably inside i64.
i32 signedCubeQ16(i32 raw) FL_NO_EXCEPT {
    const i32 clamped = clampOklabQ16(raw);
    const i64 magnitude = clamped < 0 ? -static_cast<i64>(clamped)
                                      : static_cast<i64>(clamped);
    const i64 squared = (magnitude * magnitude + 32768) >> 16;
    const i64 cubed = (squared * magnitude + 32768) >> 16;
    return clamped < 0 ? -static_cast<i32>(cubed) : static_cast<i32>(cubed);
}

}  // namespace

void xyzToOklabQ16(const i32 (&xyz)[3], i32 (&out_lab)[3]) FL_NO_EXCEPT {
    const i32 clamped[3] = {
        clampOklabQ16(xyz[0]), clampOklabQ16(xyz[1]), clampOklabQ16(xyz[2])};
    i32 lms[3];
    dotOklabMatrixQ16(kLmsFromXyz, clamped, lms);
    const i32 root[3] = {
        signedCbrtQ16(lms[0]), signedCbrtQ16(lms[1]), signedCbrtQ16(lms[2])};
    dotOklabMatrixQ16(kOklabFromLmsRoot, root, out_lab);
}

void oklabToXyzQ16(const i32 (&lab)[3], i32 (&out_xyz)[3]) FL_NO_EXCEPT {
    const i32 clamped[3] = {
        clampOklabQ16(lab[0]), clampOklabQ16(lab[1]), clampOklabQ16(lab[2])};
    i32 root[3];
    dotOklabMatrixQ16(kLmsRootFromOklab, clamped, root);
    const i32 lms[3] = {
        signedCubeQ16(root[0]), signedCubeQ16(root[1]), signedCubeQ16(root[2])};
    dotOklabMatrixQ16(kXyzFromLms, lms, out_xyz);
}

}  // namespace fl
