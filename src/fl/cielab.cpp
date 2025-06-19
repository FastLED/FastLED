#include "fl/cielab.h"

namespace fl {

// 1) sRGB → linear-light in Q16.16  
static const uint16_t srgb_to_lin_tab[256] = {
    0, 20, 40, 60, 80, 99, 119, 139,
    159, 179, 199, 219, 241, 264, 288, 313,
    340, 367, 396, 427, 458, 491, 526, 562,
    599, 637, 677, 718, 761, 805, 851, 898,
    947, 997, 1048, 1101, 1156, 1212, 1270, 1330,
    1391, 1453, 1517, 1583, 1651, 1720, 1791, 1863,
    1937, 2013, 2090, 2170, 2250, 2333, 2418, 2504,
    2592, 2681, 2773, 2866, 2961, 3058, 3157, 3258,
    3360, 3464, 3570, 3678, 3788, 3900, 4014, 4129,
    4247, 4366, 4488, 4611, 4736, 4864, 4993, 5124,
    5257, 5392, 5530, 5669, 5810, 5953, 6099, 6246,
    6395, 6547, 6701, 6856, 7014, 7174, 7336, 7500,
    7666, 7834, 8004, 8177, 8352, 8529, 8708, 8889,
    9072, 9258, 9446, 9636, 9828, 10022, 10219, 10418,
    10619, 10822, 11028, 11236, 11446, 11658, 11873, 12090,
    12309, 12531, 12754, 12981, 13209, 13440, 13673, 13909,
    14147, 14387, 14629, 14874, 15122, 15372, 15624, 15878,
    16135, 16394, 16656, 16920, 17187, 17456, 17727, 18001,
    18278, 18556, 18838, 19121, 19408, 19696, 19988, 20281,
    20578, 20876, 21178, 21481, 21788, 22096, 22408, 22722,
    23038, 23357, 23679, 24003, 24329, 24659, 24991, 25325,
    25662, 26002, 26344, 26689, 27036, 27387, 27739, 28095,
    28453, 28813, 29177, 29543, 29911, 30283, 30657, 31033,
    31413, 31795, 32180, 32567, 32957, 33350, 33746, 34144,
    34545, 34949, 35355, 35765, 36177, 36591, 37009, 37429,
    37852, 38278, 38707, 39138, 39572, 40009, 40449, 40892,
    41337, 41786, 42237, 42691, 43147, 43607, 44069, 44534,
    45003, 45474, 45947, 46424, 46904, 47386, 47871, 48360,
    48851, 49345, 49842, 50342, 50844, 51350, 51859, 52370,
    52884, 53402, 53922, 54445, 54972, 55501, 56033, 56568,
    57106, 57647, 58191, 58738, 59288, 59841, 60397, 60956,
    61518, 62083, 62651, 63222, 63796, 64373, 64953, 65535
};

// Fixed-point Q16.16 multiply
static inline int32_t mul16(int32_t a, int32_t b) {
    return int32_t((int64_t(a) * b) >> 16);
}

// Integer cube-root via Newton-Raphson (input & output in Q16.16)
static int32_t cbrt_q16(int32_t x) {
    if (x <= 0) return 0;
    int32_t y = x;
    for (int i = 0; i < 10; ++i) {
        int32_t y2 = int32_t((int64_t(y) * y) >> 16);
        if (y2 == 0) break;
        int32_t x_div_y2 = int32_t((int64_t(x) << 16) / y2);
        y = int32_t(((int64_t)2*y + x_div_y2) / 3);
    }
    return y;
}

// f(t) as in CIE Lab pivot (t in Q16.16 → output in Q16.16)
static int32_t f_q16(int32_t t) {
    // 0.008856 * 65536 ≈ 580
    if (t > 580) {
        return cbrt_q16(t);
    } else {
        // 7.787 ≈ 0xC6BB (50955), 16/116 ≈ 9033
        return int32_t(((int64_t)50955 * t >> 16) + 9033);
    }
}

// Main conversion: sRGB_u8 → CIELAB_u16
static void rgb_to_lab_u16_fixed(
    uint8_t  r, uint8_t  g, uint8_t  b,
    uint16_t &outL, uint16_t &outA, uint16_t &outB
) {
    // 1) γ-decode to linear Q16.16
    int32_t R = srgb_to_lin_tab[r];
    int32_t G = srgb_to_lin_tab[g];
    int32_t B = srgb_to_lin_tab[b];

    // 2) linear RGB → XYZ (D65) in Q16.16
    //   X =  0.4124564 R + 0.3575761 G + 0.1804375 B
    //   Y =  0.2126729 R + 0.7151522 G + 0.0721750 B
    //   Z =  0.0193339 R + 0.1191920 G + 0.9503041 B
    const int32_t MxR = 27010, MxG = 23436, MxB = 11821;
    const int32_t MyR = 13955, MyG = 46802, MyB = 4727;
    const int32_t MzR = 1270,  MzG = 7808,  MzB = 62298;
    int32_t X = int32_t(((int64_t)MxR*R + (int64_t)MxG*G + (int64_t)MxB*B) >> 16);
    int32_t Y = int32_t(((int64_t)MyR*R + (int64_t)MyG*G + (int64_t)MyB*B) >> 16);
    int32_t Z = int32_t(((int64_t)MzR*R + (int64_t)MzG*G + (int64_t)MzB*B) >> 16);

    // 3) Normalize by D65 white (Xn, Yn, Zn) → also Q16.16
    //    Xn=0.95047→62254, Yn=1.0→65536, Zn=1.08883→71307
    X = int32_t((int64_t(X) * 65536) / 62254);
    Y = int32_t((int64_t(Y) * 65536) / 65536);
    Z = int32_t((int64_t(Z) * 65536) / 71307);

    // 4) Apply f(t)
    int32_t fx = f_q16(X);
    int32_t fy = f_q16(Y);
    int32_t fz = f_q16(Z);

    // 5) Compute L*, a*, b* in Q16.16
    //    L* = 116·fy – 16
    //    a* = 500·(fx – fy)
    //    b* = 200·(fy – fz)
    int32_t Lq = 116 * fy - (16 << 16);
    int32_t aq = 500 * (fx - fy);
    int32_t bq = 200 * (fy - fz);

    // 6) Scale/clamp into uint16_t range
    //    Lq ∈ [0…100<<16] → outL [0…65535]
    //    aq, bq ∈ [–128<<16…+127<<16] → shift by +128 and map to [0…65535]
    outL = uint16_t(((int64_t)Lq * 65535) / (100 << 16));
    outA = uint16_t((((int64_t)aq + (128LL << 16)) * 65535) / (255LL << 16));
    outB = uint16_t((((int64_t)bq + (128LL << 16)) * 65535) / (255LL << 16));
}



// Forward declarations of table and helpers from rgb_to_lab_u16_fixed

// Invert linear-light Q16.16 → sRGB_u8 by searching the table
static uint8_t lin_to_srgb_u8(int32_t lin_q16) {
    // clamp input to table range
    if (lin_q16 <= 0)   return 0;
    if (lin_q16 >= 65535<<16 >> 0) return 255;
    // search for nearest entry
    for (int i = 0; i < 255; ++i) {
        int32_t v0 = int32_t(srgb_to_lin_tab[i]);
        int32_t v1 = int32_t(srgb_to_lin_tab[i+1]);
        if (lin_q16 >= v0 && lin_q16 <= v1) {
            // pick closer endpoint
            return ( (lin_q16 - v0) < (v1 - lin_q16) ) ? uint8_t(i) : uint8_t(i+1);
        }
    }
    return 255;
}

// Main inverse: CIELAB_u16 → sRGB_u8
static void lab_to_rgb_u8_fixed(
    uint16_t  inL, uint16_t  inA, uint16_t  inB,
    uint8_t  &outR, uint8_t  &outG, uint8_t  &outB
) {
    // 1) unpack into Q16.16 Lab* values
    //    L* in [0…65535] → Lq in [0…100<<16]
    int32_t Lq = int32_t((int64_t)inL * (100<<16) / 65535);
    //    a*, b* in [–128<<16…+127<<16]
    int32_t aq = int32_t((int64_t)inA * (255<<16) / 65535) - (128<<16);
    int32_t bq = int32_t((int64_t)inB * (255<<16) / 65535) - (128<<16);

    // 2) recover fY, fX, fZ
    //    Lq = 116·fy – 16<<16  →  fy = (Lq + 16<<16) / 116
    int32_t fy = int32_t((Lq + (16<<16)) / 116);
    int32_t fx = fy + aq / 500;    // fx = fy + a*/500
    int32_t fz = fy - bq / 200;    // fz = fy – b*/200

    // 3) invert f → t in Q16.16
    //    constant delta = cbrt(0.008856)*65536 ≈ 13592
    const int32_t delta = 13592;
    auto inv_f = [&](int32_t f) {
        if (f > delta) {
            // t = f^3
            int64_t y2 = (int64_t)f * f >> 16;
            return int32_t((y2 * f) >> 16);
        } else {
            // t = (f – 16/116) / 7.787
            // 16/116 ≈ 9033 (Q16.16), 7.787 ≈ 50955/65536
            return int32_t(((int64_t)(f - 9033) << 16) / 50955);
        }
    };
    int32_t Y = inv_f(fy);
    int32_t X = inv_f(fx);
    int32_t Z = inv_f(fz);

    // 4) denormalize by D65 white point (Q16.16)
    //    Xn=0.95047→62254, Yn=1.0→65536, Zn=1.08883→71307
    X = int32_t((int64_t)X * 62254 >> 16);
    // Y = Y * 65536 >> 16 is no-op
    Z = int32_t((int64_t)Z * 71307 >> 16);

    // 5) XYZ → linear RGB (Q16.16)
    //    inverse matrix of RGB→XYZ
    const int32_t MxX = 212119, MxY = -100147, MxZ =  -32653;
    const int32_t MyX = -63438, MyY =  122915, MyZ =   2722;
    const int32_t MzX =   3645, MzY =  -13365, MzZ =   69287;
    int32_t Rlin = int32_t(( (int64_t)MxX*X + (int64_t)MxY*Y + (int64_t)MxZ*Z ) >> 16);
    int32_t Glin = int32_t(( (int64_t)MyX*X + (int64_t)MyY*Y + (int64_t)MyZ*Z ) >> 16);
    int32_t Blin = int32_t(( (int64_t)MzX*X + (int64_t)MzY*Y + (int64_t)MzZ*Z ) >> 16);

    // 6) γ-encode back to sRGB_u8
    outR = lin_to_srgb_u8(Rlin);
    outG = lin_to_srgb_u8(Glin);
    outB = lin_to_srgb_u8(Blin);
}


CIELAB16::CIELAB16(const CRGB& c) {
    rgb_to_lab_u16_fixed(c.r, c.g, c.b, L, A, B);
}

CRGB CIELAB16::ToRGB() const {
    CRGB c;
    lab_to_rgb_u8_fixed(L, A, B, c.r, c.g, c.b);
    return c;
}

void CIELAB16::Fill(const CRGB* c, CIELAB16* lab, size_t numLeds) {
    for (size_t i = 0; i < numLeds; i++) {
        rgb_to_lab_u16_fixed(c[i].r, c[i].g, c[i].b, lab[i].L, lab[i].A, lab[i].B);
    }
}

void CIELAB16::Fill(const CIELAB16* c, CRGB* lab, size_t numLeds) {
    for (size_t i = 0; i < numLeds; i++) {
        lab_to_rgb_u8_fixed(c[i].L, c[i].A, c[i].B, lab[i].r, lab[i].g, lab[i].b);
    }
}


}  // namespace fl
