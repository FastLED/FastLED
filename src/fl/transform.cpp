
#include <math.h>

#include "lib8tion/trig8.h"
#include "lib8tion/intmap.h"
#include "fl/transform.h"
#include "fl/math_macros.h"
#include "fl/lut.h"

namespace fl {

point_xy_float TransformFloat::transform(const point_xy_float &xy) const {
    float x = xy.x * scale_x + x_offset;
    float y = xy.y * scale_y + y_offset;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    const bool has_rotation = (rotation != 0.0f);
#pragma GCC diagnostic pop
    if (has_rotation) {
        float cos_theta = cosf(rotation);
        float sin_theta = sinf(rotation);
        float x_rotated = x * cos_theta - y * sin_theta;
        float y_rotated = x * sin_theta + y * cos_theta;
        return point_xy_float(x_rotated, y_rotated);
    }
    return point_xy_float(x, y);
}

Transform16 Transform16::ToBounds(alpha16 max_value) {
    Transform16 tx;
    // Compute a Q16 “scale” so that:
    //    (alpha16 * scale) >> 16  == max_value  when alpha16==0xFFFF
    alpha16 scale16 = 0;
    if (max_value) {
        // numerator = max_value * 2^16
        uint32_t numer = static_cast<uint32_t>(max_value) << 16;
        // denom = 0xFFFF; use ceil so 0xFFFF→max_value exactly:
        uint32_t scale32 = numer / 0xFFFF;
        scale16 = static_cast<alpha16>(scale32);
    }
    tx.scale_x = scale16;
    tx.scale_y = scale16;
    tx.x_offset = 0;
    tx.y_offset = 0;
    tx.rotation = 0;
    return tx;
}


Transform16 Transform16::ToBounds(const point_xy<alpha16> &min,
                                  const point_xy<alpha16> &max,
                                  alpha16 rotation) {
    Transform16 tx;
    // Compute a Q16 “scale” so that:
    //    (alpha16 * scale) >> 16  == max_value  when alpha16==0xFFFF
    alpha16 scale16 = 0;
    if (max.x > min.x) {
        // numerator = max_value * 2^16
        uint32_t numer = static_cast<uint32_t>(max.x - min.x) << 16;
        // denom = 0xFFFF; use ceil so 0xFFFF→max_value exactly:
        uint32_t scale32 = numer / 0xFFFF;
        scale16 = static_cast<alpha16>(scale32);
    }
    tx.scale_x = scale16;
    if (max.y > min.y) {
        // numerator = max_value * 2^16
        uint32_t numer = static_cast<uint32_t>(max.y - min.y) << 16;
        // denom = 0xFFFF; use ceil so 0xFFFF→max_value exactly:
        uint32_t scale32 = numer / 0xFFFF;
        scale16 = static_cast<alpha16>(scale32);
    }
    tx.scale_y = scale16;
    tx.x_offset = min.x;
    tx.y_offset = min.y;
    tx.rotation = rotation;
    return tx;
}

point_xy<alpha16> Transform16::transform(const point_xy<alpha16> &xy) const {
    point_xy<alpha16> out = xy;

    // 1) Rotate around the 16‑bit center first
    if (rotation != 0) {
        constexpr int32_t MID = 0x7FFF; // center of 0…0xFFFF interval

        // bring into signed centered coords
        int32_t x = int32_t(out.x) - MID;
        int32_t y = int32_t(out.y) - MID;

        // Q15 cosine & sine
        int32_t c = cos16(rotation); // [-32768..+32767]
        int32_t s = sin16(rotation);

        // rotate & truncate
        int32_t xr = (x * c - y * s) >> 15;
        int32_t yr = (x * s + y * c) >> 15;

        // shift back into [0…0xFFFF]
        out.x = alpha16(xr + MID);
        out.y = alpha16(yr + MID);
    }

    // 2) Then scale in X/Y (Q16 → map32_to_16)
    if (scale_x != 0xFFFF) {
        uint32_t tx = uint32_t(out.x) * scale_x;
        out.x = map32_to_16(tx);
    }
    if (scale_y != 0xFFFF) {
        uint32_t ty = uint32_t(out.y) * scale_y;
        out.y = map32_to_16(ty);
    }

    // 3) Finally translate
    if (x_offset)
        out.x = alpha16(out.x + x_offset);
    if (y_offset)
        out.y = alpha16(out.y + y_offset);

    return out;
}



float TransformFloat::scale() const {
    return MIN(scale_x, scale_y);
}



void TransformFloat::set_scale(float scale) {
    scale_x = scale;
    scale_y = scale;
}

} // namespace fl