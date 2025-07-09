
#include <math.h>

#include "fl/lut.h"
#include "fl/math_macros.h"
#include "fl/transform.h"
#include "lib8tion/intmap.h"
#include "lib8tion/trig8.h"
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(float-equal)

namespace fl {

vec2f TransformFloatImpl::transform(const vec2f &xy) const {
    if (is_identity()) {
        return xy;
    }
    float x = xy.x;
    float y = xy.y;
    if (scale_x != 1.0f) {
        x *= scale_x;
    }
    if (scale_y != 1.0f) {
        y *= scale_y;
    }
    // Assume that adding floats is fast when offset_x == 0.0f
    x += offset_x;
    y += offset_y;

    const bool has_rotation = (rotation != 0.0f);

    if (has_rotation) {
        float radians = rotation * 2 * PI;
        float cos_theta = cosf(radians);
        float sin_theta = sinf(radians);
        float x_rotated = x * cos_theta - y * sin_theta;
        float y_rotated = x * sin_theta + y * cos_theta;
        return vec2f(x_rotated, y_rotated);
    }
    return vec2f(x, y);
}

Transform16 Transform16::ToBounds(alpha16 max_value) {
    Transform16 tx;
    // Compute a Q16 “scale” so that:
    //    (alpha16 * scale) >> 16  == max_value  when alpha16==0xFFFF
    alpha16 scale16 = 0;
    if (max_value) {
        // numerator = max_value * 2^16
        u32 numer = static_cast<u32>(max_value) << 16;
        // denom = 0xFFFF; use ceil so 0xFFFF→max_value exactly:
        u32 scale32 = numer / 0xFFFF;
        scale16 = static_cast<alpha16>(scale32);
    }
    tx.scale_x = scale16;
    tx.scale_y = scale16;
    tx.offset_x = 0;
    tx.offset_y = 0;
    tx.rotation = 0;
    return tx;
}

Transform16 Transform16::ToBounds(const vec2<alpha16> &min,
                                  const vec2<alpha16> &max, alpha16 rotation) {
    Transform16 tx;
    // Compute a Q16 “scale” so that:
    //    (alpha16 * scale) >> 16  == max_value  when alpha16==0xFFFF
    alpha16 scale16 = 0;
    if (max.x > min.x) {
        // numerator = max_value * 2^16
        u32 numer = static_cast<u32>(max.x - min.x) << 16;
        // denom = 0xFFFF; use ceil so 0xFFFF→max_value exactly:
        u32 scale32 = numer / 0xFFFF;
        scale16 = static_cast<alpha16>(scale32);
    }
    tx.scale_x = scale16;
    if (max.y > min.y) {
        // numerator = max_value * 2^16
        u32 numer = static_cast<u32>(max.y - min.y) << 16;
        // denom = 0xFFFF; use ceil so 0xFFFF→max_value exactly:
        u32 scale32 = numer / 0xFFFF;
        scale16 = static_cast<alpha16>(scale32);
    }
    tx.scale_y = scale16;
    tx.offset_x = min.x;
    tx.offset_y = min.y;
    tx.rotation = rotation;
    return tx;
}

vec2<alpha16> Transform16::transform(const vec2<alpha16> &xy) const {
    vec2<alpha16> out = xy;

    // 1) Rotate around the 16‑bit center first
    if (rotation != 0) {
        constexpr i32 MID = 0x7FFF; // center of 0…0xFFFF interval

        // bring into signed centered coords
        i32 x = i32(out.x) - MID;
        i32 y = i32(out.y) - MID;

        // Q15 cosine & sine
        i32 c = cos16(rotation); // [-32768..+32767]
        i32 s = sin16(rotation);

        // rotate & truncate
        i32 xr = (x * c - y * s) >> 15;
        i32 yr = (x * s + y * c) >> 15;

        // shift back into [0…0xFFFF]
        out.x = alpha16(xr + MID);
        out.y = alpha16(yr + MID);
    }

    // 2) Then scale in X/Y (Q16 → map32_to_16)
    if (scale_x != 0xFFFF) {
        u32 tx = u32(out.x) * scale_x;
        out.x = map32_to_16(tx);
    }
    if (scale_y != 0xFFFF) {
        u32 ty = u32(out.y) * scale_y;
        out.y = map32_to_16(ty);
    }

    // 3) Finally translate
    if (offset_x)
        out.x = alpha16(out.x + offset_x);
    if (offset_y)
        out.y = alpha16(out.y + offset_y);

    return out;
}

float TransformFloatImpl::scale() const { return MIN(scale_x, scale_y); }

void TransformFloatImpl::set_scale(float scale) {
    scale_x = scale;
    scale_y = scale;
}

bool TransformFloatImpl::is_identity() const {
    return (scale_x == 1.0f && scale_y == 1.0f && offset_x == 0.0f &&
            offset_y == 0.0f && rotation == 0.0f);
}

Matrix3x3f TransformFloat::compile() const {
    Matrix3x3f out;
    out.m[0][0] = scale_x() * cosf(rotation() * 2.0f * PI);
    out.m[0][1] = -scale_y() * sinf(rotation() * 2.0f * PI);
    out.m[0][2] = offset_x();
    out.m[1][0] = scale_x() * sinf(rotation() * 2.0f * PI);
    out.m[1][1] = scale_y() * cosf(rotation() * 2.0f * PI);
    out.m[1][2] = offset_y();
    out.m[2][2] = 1.0f;
    return out;
}

} // namespace fl

FL_DISABLE_WARNING_POP
