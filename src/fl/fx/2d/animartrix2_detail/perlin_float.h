#pragma once

// Perlin noise implementation for Animartrix2
// Extracted from animartrix_detail.hpp to enable standalone operation
//
// Ken Perlins improved noise - http://mrl.nyu.edu/~perlin/noise/
// C-port: http://www.fundza.com/c4serious/noise/perlin/perlin.html
// by Malcolm Kesson; arduino port by Peter Chiochetti, Sep 2007

#include "fl/stl/stdint.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"

namespace fl {

// Perlin noise permutation table
FL_ALIGNAS(64) static const fl::u8 PERLIN_NOISE[] = {
    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,
    225, 140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190,
    6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117,
    35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136,
    171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158,
    231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,  46,
    245, 40,  244, 102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,  209,
    76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130, 116, 188, 159, 86,
    164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250, 124, 123, 5,
    202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227, 47,  16,
    58,  17,  182, 189, 28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,
    154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,   129, 22,  39,  253,
    19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104, 218, 246, 97,
    228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,  51,
    145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157, 184,
    84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,
    222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156,
    180};

FASTLED_FORCE_INLINE fl::u8 P(fl::u8 x) {
    const fl::u8 idx = x & 255;
    const fl::u8 *ptr = PERLIN_NOISE + idx;
    return *ptr;
}

// Perlin fade curve: 6t^5 - 15t^4 + 10t^3
inline float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

// Linear interpolation
inline float lerp(float t, float a, float b) {
    return a + t * (b - a);
}

// Gradient function
inline float grad(int hash, float x, float y, float z) {
    int h = hash & 15;       /* CONVERT LO 4 BITS OF HASH CODE */
    float u = h < 8 ? x : y, /* INTO 12 GRADIENT DIRECTIONS.   */
        v = h < 4                ? y
            : h == 12 || h == 14 ? x
                                 : z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

// 3D Perlin noise
inline float pnoise(float x, float y, float z) {
    int X = (int)fl::floorf(x) & 255, /* FIND UNIT CUBE THAT */
        Y = (int)fl::floorf(y) & 255, /* CONTAINS POINT.     */
        Z = (int)fl::floorf(z) & 255;
    x -= fl::floorf(x); /* FIND RELATIVE X,Y,Z */
    y -= fl::floorf(y); /* OF POINT IN CUBE.   */
    z -= fl::floorf(z);
    float u = fade(x), /* COMPUTE FADE CURVES */
        v = fade(y),   /* FOR EACH OF X,Y,Z.  */
        w = fade(z);
    int A = P(X) + Y, AA = P(A) + Z,
        AB = P(A + 1) + Z, /* HASH COORDINATES OF */
        B = P(X + 1) + Y, BA = P(B) + Z,
        BB = P(B + 1) + Z; /* THE 8 CUBE CORNERS, */

    return lerp(w,
                lerp(v,
                     lerp(u, grad(P(AA), x, y, z),        /* AND ADD */
                          grad(P(BA), x - 1, y, z)),      /* BLENDED */
                     lerp(u, grad(P(AB), x, y - 1, z),    /* RESULTS */
                          grad(P(BB), x - 1, y - 1, z))), /* FROM  8 */
                lerp(v,
                     lerp(u, grad(P(AA + 1), x, y, z - 1),   /* CORNERS */
                          grad(P(BA + 1), x - 1, y, z - 1)), /* OF CUBE */
                     lerp(u, grad(P(AB + 1), x, y - 1, z - 1),
                          grad(P(BB + 1), x - 1, y - 1, z - 1))));
}

} // namespace fl
