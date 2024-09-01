#define FASTLED_INTERNAL
#include "FastLED.h"

// This file implements simplex noise, which is an improved Perlin noise. This
// implementation is a fixed-point version that avoids all uses of floating
// point while still being compatible with the floating point version.

// Original author: Stefan Gustavson, converted to Go by Lars Pensjö, converted
// to fixed-point Go and then to C++ by Ayke van Laethem.
// https://github.com/larspensjo/Go-simplex-noise/blob/master/simplexnoise/simplexnoise.go
// https://github.com/aykevl/ledsgo/blob/master/noise.go
//
// The code in this file has been placed in the public domain. You can do
// whatever you want with it. Attribution is appreciated but not required.

// Notation:
// Every fixed-point calculation has a line comment saying how many bits in the
// given integer are used for the fractional part. For example:
//
//     uint32_t n = a + b; // .12
//
// means the result of this operation has the floating point 12 bits from the
// right. Specifically, there are 20 integer bits and 12 fractional bits. It
// can be converted to a floating point using:
//
//     double nf = (double)n / (1 << 12);

FASTLED_NAMESPACE_BEGIN

#define P(x) FL_PGM_READ_BYTE_NEAR(p + (x))

// Permutation table. This is just a random jumble of all numbers.
// This needs to be exactly the same for all instances on all platforms,
// so it's easiest to just keep it as static explicit data.
FL_PROGMEM static uint8_t const p[] = {
    151, 160, 137, 91, 90, 15,
    131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23,
    190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
    88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
    77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
    102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
    135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
    5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
    223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
    129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
    251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
    49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
    138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180,
};

// A lookup table to traverse the simplex around a given point in 4D.
// Details can be found where this table is used, in the 4D noise method.
// TODO: This should not be required, backport it from Bill's GLSL code!
static uint8_t const simplex[64][4] = {
    {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 0, 0, 0}, {0, 2, 3, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {1, 2, 3, 0},
    {0, 2, 1, 3}, {0, 0, 0, 0}, {0, 3, 1, 2}, {0, 3, 2, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {1, 3, 2, 0},
    {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
    {1, 2, 0, 3}, {0, 0, 0, 0}, {1, 3, 0, 2}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {2, 3, 0, 1}, {2, 3, 1, 0},
    {1, 0, 2, 3}, {1, 0, 3, 2}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {2, 0, 3, 1}, {0, 0, 0, 0}, {2, 1, 3, 0},
    {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
    {2, 0, 1, 3}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {3, 0, 1, 2}, {3, 0, 2, 1}, {0, 0, 0, 0}, {3, 1, 2, 0},
    {2, 1, 0, 3}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {3, 1, 0, 2}, {0, 0, 0, 0}, {3, 2, 0, 1}, {3, 2, 1, 0},
};

// hash is 0..0xff, x is 0.12 fixed point
// returns *.12 fixed-point value
static int32_t grad(uint8_t hash, int32_t x) {
    uint8_t h = hash & 15;
    int32_t grad = 1 + (h&7); // Gradient value 1.0, 2.0, ..., 8.0
    if ((h&8) != 0) {
        grad = -grad; // Set a random sign for the gradient
    }
    return grad * x; // Multiply the gradient with the distance (integer * 0.12 = *.12)
}

static int32_t grad(uint8_t hash, int32_t x, int32_t y) {
    uint8_t h = hash & 7;      // Convert low 3 bits of hash code
    int32_t u = h < 4 ? x : y; // into 8 simple gradient directions,
    int32_t v = h < 4 ? y : x; // and compute the dot product with (x,y).
    return ((h&1) != 0 ? -u : u) + ((h&2) != 0 ? -2*v : 2*v);
}

static int32_t grad(uint8_t hash, int32_t x, int32_t y, int32_t z) {
    int32_t h = hash & 15;                                // Convert low 4 bits of hash code into 12 simple
    int32_t u = h < 8 ? x : y;                            // gradient directions, and compute dot product.
    int32_t v = h < 4 ? y : (h == 12 || h == 14 ? x : z); // Fix repeats at h = 12 to 15
    return ((h&1) != 0 ? -u : u) + ((h&2) != 0 ? -v : v);
}

static int32_t grad(uint8_t hash, int32_t x, int32_t y, int32_t z, int32_t t) {
    uint8_t h = hash & 31;      // Convert low 5 bits of hash code into 32 simple
    int32_t u = h < 24 ? x : y; // gradient directions, and compute dot product.
    int32_t v = h < 16 ? y : z;
    int32_t w = h <  8 ? z : t;
    return ((h&1) != 0 ? -u : u) + ((h&2) != 0 ? -v : v) + ((h&4) != 0 ? -w : w);
}

// 1D simplex noise.
uint16_t snoise16(uint32_t x) {
    uint32_t i0 = x >> 12;
    uint32_t i1 = i0 + 1;
    int32_t x0 = x & 0xfff;   // .12
    int32_t x1 = x0 - 0x1000; // .12

    int32_t t0 = 0x8000 - ((x0*x0)>>9);             // .15
    t0 = (t0 * t0) >> 15;                           // .15
    t0 = (t0 * t0) >> 15;                           // .15
    int32_t n0 = (t0 * grad(P(i0&0xff), x0)) >> 12; // .15 * .12 = .15

    int32_t t1 = 0x8000 - ((x1*x1)>>9);             // .15
    t1 = (t1 * t1) >> 15;                           // .15
    t1 = (t1 * t1) >> 15;                           // .15
    int32_t n1 = (t1 * grad(P(i1&0xff), x1)) >> 12; // .15 * .12 = .15

    int32_t n = n0 + n1;   // .15
    n += 2503;             // .15: fix offset, adjust to +0.03
    n = (n * 26694) >> 16; // .15: fix scale to fit in [-1,1]
    return uint16_t(n) + 0x8000;
}

// 2D simplex noise.
uint16_t snoise16(uint32_t x, uint32_t y) {
    const uint64_t F2 = 1572067135; // .32: F2 = 0.5*(sqrt(3.0)-1.0)
    const uint64_t G2 = 907633384;  // .32: G2 = (3.0-Math.sqrt(3.0))/6.0

    // Skew the input space to determine which simplex cell we're in
    uint32_t s = (((uint64_t)x + (uint64_t)y) * F2) >> 32; // (.12 + .12) * .32 = .12: Hairy factor for 2D
    uint32_t i = ((x>>1) + (s>>1)) >> 11;                  // .0
    uint32_t j = ((y>>1) + (s>>1)) >> 11;                  // .0

    uint64_t t = ((uint64_t)i + (uint64_t)j) * G2; // .32
    uint64_t X0 = ((uint64_t)i<<32) - t;           // .32: Unskew the cell origin back to (x,y) space
    uint64_t Y0 = ((uint64_t)j<<32) - t;           // .32
    int32_t x0 = ((uint64_t)x<<2) - (X0>>18);      // .14: The x,y distances from the cell origin
    int32_t y0 = ((uint64_t)y<<2) - (Y0>>18);      // .14

    // For the 2D case, the simplex shape is an equilateral triangle.
    // Determine which simplex we are in.
    uint32_t i1, j1; // Offsets for second (middle) corner of simplex in (i,j) coords
    if (x0 > y0) {
        i1 = 1;
        j1 = 0; // lower triangle, XY order: (0,0)->(1,0)->(1,1)
    } else {
        i1 = 0;
        j1 = 1;
    } // upper triangle, YX order: (0,0)->(0,1)->(1,1)

    // A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
    // a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where
    // c = (3-sqrt(3))/6

    int32_t x1 = x0 - ((int32_t)i1<<14) + (int32_t)(G2>>18); // .14: Offsets for middle corner in (x,y) unskewed coords
    int32_t y1 = y0 - ((int32_t)j1<<14) + (int32_t)(G2>>18); // .14
    int32_t x2 = x0 - (1 << 14) + ((int32_t)(2*G2)>>18);     // .14: Offsets for last corner in (x,y) unskewed coords
    int32_t y2 = y0 - (1 << 14) + ((int32_t)(2*G2)>>18);     // .14

    int32_t n0 = 0, n1 = 0, n2 = 0; // Noise contributions from the three corners

    // Calculate the contribution from the three corners
    int32_t t0 = (((int32_t)1 << 27) - x0*x0 - y0*y0) >> 12; // .16
    if (t0 > 0) {
        t0 = (t0 * t0) >> 16;                                      // .16
        t0 = (t0 * t0) >> 16;                                      // .16
        n0 = t0 * grad(P((i+(uint32_t)(P(j&0xff)))&0xff), x0, y0); // .16 * .14 = .30
    }

    int32_t t1 = (((int32_t)1 << 27) - x1*x1 - y1*y1) >> 12; // .16
    if (t1 > 0) {
        t1 = (t1 * t1) >> 16;                                              // .16
        t1 = (t1 * t1) >> 16;                                              // .16
        n1 = t1 * grad(P((i+i1+(uint32_t)(P((j+j1)&0xff)))&0xff), x1, y1); // .16 * .14 = .30
    }

    int32_t t2 = (((int32_t)1 << 27) - x2*x2 - y2*y2) >> 12; // .16
    if (t2 > 0) {
        t2 = (t2 * t2) >> 16;                                            // .16
        t2 = (t2 * t2) >> 16;                                            // .16
        n2 = t2 * grad(P((i+1+(uint32_t)(P((j+1)&0xff)))&0xff), x2, y2); // .16 * .14 = .30
    }

    // Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the interval [-1,1].
    int32_t n = n0 + n1 + n2;    // .30
    n = ((n >> 8) * 23163) >> 16; // fix scale to fit exactly in an int16
    return (uint16_t)n + 0x8000;
}

// 3D simplex noise.
uint16_t snoise16(uint32_t x, uint32_t y, uint32_t z) {
    // Simple skewing factors for the 3D case
    const uint64_t F3 = 1431655764; // .32: 0.333333333
    const uint64_t G3 = 715827884;  // .32: 0.166666667

    // Skew the input space to determine which simplex cell we're in
    uint32_t s = (((uint64_t)x + (uint64_t)y + (uint64_t)z) * F3) >> 32; // .12 + .32 = .12: Very nice and simple skew factor for 3D
    uint32_t i = ((x>>1) + (s>>1)) >> 11;                                // .0
    uint32_t j = ((y>>1) + (s>>1)) >> 11;                                // .0
    uint32_t k = ((z>>1) + (s>>1)) >> 11;                                // .0

    uint64_t t = ((uint64_t)i + (uint64_t)j + (uint64_t)k) * G3; // .32
    uint64_t X0 = ((uint64_t)i<<32) - t;                         // .32: Unskew the cell origin back to (x,y) space
    uint64_t Y0 = ((uint64_t)j<<32) - t;                         // .32
    uint64_t Z0 = ((uint64_t)k<<32) - t;                         // .32
    int32_t x0 = ((uint64_t)x<<2) - (X0>>18);                    // .14: The x,y distances from the cell origin
    int32_t y0 = ((uint64_t)y<<2) - (Y0>>18);                    // .14
    int32_t z0 = ((uint64_t)z<<2) - (Z0>>18);                    // .14

    // For the 3D case, the simplex shape is a slightly irregular tetrahedron.
    // Determine which simplex we are in.
    uint32_t i1, j1, k1; // Offsets for second corner of simplex in (i,j,k) coords
    uint32_t i2, j2, k2; // Offsets for third corner of simplex in (i,j,k) coords

    // This code would benefit from a backport from the GLSL version!
    if (x0 >= y0) {
        if (y0 >= z0) {
            i1 = 1;
            j1 = 0;
            k1 = 0;
            i2 = 1;
            j2 = 1;
            k2 = 0; // X Y Z order
        } else if (x0 >= z0) {
            i1 = 1;
            j1 = 0;
            k1 = 0;
            i2 = 1;
            j2 = 0;
            k2 = 1; // X Z Y order
        } else {
            i1 = 0;
            j1 = 0;
            k1 = 1;
            i2 = 1;
            j2 = 0;
            k2 = 1; // Z X Y order
        }
    } else { // x0<y0
        if (y0 < z0) {
            i1 = 0;
            j1 = 0;
            k1 = 1;
            i2 = 0;
            j2 = 1;
            k2 = 1; // Z Y X order
        } else if (x0 < z0) {
            i1 = 0;
            j1 = 1;
            k1 = 0;
            i2 = 0;
            j2 = 1;
            k2 = 1; // Y Z X order
        } else {
            i1 = 0;
            j1 = 1;
            k1 = 0;
            i2 = 1;
            j2 = 1;
            k2 = 0; // Y X Z order
        }
    }

    // A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
    // a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
    // a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where
    // c = 1/6.

    int32_t x1 = x0 - ((int32_t)i1<<14) + ((int32_t)(G3>>18));   // .14: Offsets for second corner in (x,y,z) coords
    int32_t y1 = y0 - ((int32_t)j1<<14) + ((int32_t)(G3>>18));   // .14
    int32_t z1 = z0 - ((int32_t)k1<<14) + ((int32_t)(G3>>18));   // .14
    int32_t x2 = x0 - ((int32_t)i2<<14) + ((int32_t)(2*G3)>>18); // .14: Offsets for third corner in (x,y,z) coords
    int32_t y2 = y0 - ((int32_t)j2<<14) + ((int32_t)(2*G3)>>18); // .14
    int32_t z2 = z0 - ((int32_t)k2<<14) + ((int32_t)(2*G3)>>18); // .14
    int32_t x3 = x0 - (1 << 14) + (int32_t)((3*G3)>>18);         // .14: Offsets for last corner in (x,y,z) coords
    int32_t y3 = y0 - (1 << 14) + (int32_t)((3*G3)>>18);         // .14
    int32_t z3 = z0 - (1 << 14) + (int32_t)((3*G3)>>18);         // .14

    // Calculate the contribution from the four corners
    int32_t n0 = 0, n1 = 0, n2 = 0, n3 = 0; // .30
    const int32_t fix0_6 = 161061274;       // .28: 0.6

    int32_t t0 = (fix0_6 - x0*x0 - y0*y0 - z0*z0) >> 12; // .16
    if (t0 > 0) {
        t0 = (t0 * t0) >> 16; // .16
        t0 = (t0 * t0) >> 16; // .16
        // .16 * .14 = .30
        n0 = t0 * grad(P((i+(uint32_t)P((j+(uint32_t)P(k&0xff))&0xff))&0xff), x0, y0, z0);
    }

    int32_t t1 = (fix0_6 - x1*x1 - y1*y1 - z1*z1) >> 12; // .16
    if (t1 > 0) {
        t1 = (t1 * t1) >> 16; // .16
        t1 = (t1 * t1) >> 16; // .16
        // .16 * .14 = .30
        n1 = t1 * grad(P((i+i1+(uint32_t)P((j+j1+(uint32_t)P((k+k1)&0xff))&0xff))&0xff), x1, y1, z1);
    }

    int32_t t2 = (fix0_6 - x2*x2 - y2*y2 - z2*z2) >> 12; // .16
    if (t2 > 0) {
        t2 = (t2 * t2) >> 16; // .16
        t2 = (t2 * t2) >> 16; // .16
        // .16 * .14 = .30
        n2 = t2 * grad(P((i+i2+(uint32_t)P((j+j2+(uint32_t)P((k+k2)&0xff))&0xff))&0xff), x2, y2, z2);
    }

    int32_t t3 = (fix0_6 - x3*x3 - y3*y3 - z3*z3) >> 12; // .16
    if (t3 > 0) {
        t3 = (t3 * t3) >> 16; // .16
        t3 = (t3 * t3) >> 16; // .16
        // .16 * .14 = .30
        n3 = t3 * grad(P((i+1+(uint32_t)P((j+1+(uint32_t)P((k+1)&0xff))&0xff))&0xff), x3, y3, z3);
    }

    // Add contributions from each corner to get the final noise value.
    // The result is scaled to stay just inside [-1,1]
    int32_t n = n0 + n1 + n2 + n3; // .30
    n = ((n >> 8) * 16748) >> 16 ; // fix scale to fit exactly in an int16
    return (uint16_t)n + 0x8000;
}

// 4D simplex noise.
uint16_t snoise16(uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
    // The skewing and unskewing factors are hairy again for the 4D case
    const uint64_t F4 = 331804471; // .30: (Math.sqrt(5.0)-1.0)/4.0 = 0.30901699437494745
    const uint64_t G4 = 593549882; // .32: (5.0-Math.sqrt(5.0))/20.0 = 0.1381966011250105

    // Skew the (x,y,z,w) space to determine which cell of 24 simplices we're
    // in.
    uint32_t s = (((uint64_t)x + (uint64_t)y + (uint64_t)z + (uint64_t)w) * F4) >> 32; // .12 + .30 = .10: Factor for 4D skewing.
    uint32_t i = ((x>>2) + s) >> 10;                                                   // .0
    uint32_t j = ((y>>2) + s) >> 10;                                                   // .0
    uint32_t k = ((z>>2) + s) >> 10;                                                   // .0
    uint32_t l = ((w>>2) + s) >> 10;                                                   // .0

    uint64_t t = (((uint64_t)i + (uint64_t)j + (uint64_t)k + (uint64_t)l) * G4) >> 18; // .14
    uint64_t X0 = ((uint64_t)i<<14) - t;                                               // .14: Unskew the cell origin back to (x,y,z,w) space
    uint64_t Y0 = ((uint64_t)j<<14) - t;                                               // .14
    uint64_t Z0 = ((uint64_t)k<<14) - t;                                               // .14
    uint64_t W0 = ((uint64_t)l<<14) - t;                                               // .14
    int32_t x0 = ((uint64_t)x<<2) - X0;                                                // .14: The x,y,z,w distances from the cell origin
    int32_t y0 = ((uint64_t)y<<2) - Y0;                                                // .14
    int32_t z0 = ((uint64_t)z<<2) - Z0;                                                // .14
    int32_t w0 = ((uint64_t)w<<2) - W0;                                                // .14

    // For the 4D case, the simplex is a 4D shape I won't even try to describe.
    // To find out which of the 24 possible simplices we're in, we need to
    // determine the magnitude ordering of x0, y0, z0 and w0.
    // The method below is a good way of finding the ordering of x,y,z,w and
    // then find the correct traversal order for the simplex we’re in.
    // First, six pair-wise comparisons are performed between each possible pair
    // of the four coordinates, and the results are used to add up binary bits
    // for an integer index.
    int c = 0;
    if (x0 > y0) {
        c += 32;
    }
    if (x0 > z0) {
        c += 16;
    }
    if (y0 > z0) {
        c += 8;
    }
    if (x0 > w0) {
        c += 4;
    }
    if (y0 > w0) {
        c += 2;
    }
    if (z0 > w0) {
        c += 1;
    }

    // simplex[c] is a 4-vector with the numbers 0, 1, 2 and 3 in some order.
    // Many values of c will never occur, since e.g. x>y>z>w makes x<z, y<w and x<w
    // impossible. Only the 24 indices which have non-zero entries make any sense.
    // We use a thresholding to set the coordinates in turn from the largest magnitude.
    // The number 3 in the "simplex" array is at the position of the largest coordinate.
    // The integer offsets for the second simplex corner
    uint32_t i1 = simplex[c][0] >= 3 ? 1 : 0;
    uint32_t j1 = simplex[c][1] >= 3 ? 1 : 0;
    uint32_t k1 = simplex[c][2] >= 3 ? 1 : 0;
    uint32_t l1 = simplex[c][3] >= 3 ? 1 : 0;
    // The number 2 in the "simplex" array is at the second largest coordinate.
    // The integer offsets for the third simplex corner
    uint32_t i2 = simplex[c][0] >= 2 ? 1 : 0;
    uint32_t j2 = simplex[c][1] >= 2 ? 1 : 0;
    uint32_t k2 = simplex[c][2] >= 2 ? 1 : 0;
    uint32_t l2 = simplex[c][3] >= 2 ? 1 : 0;
    // The number 1 in the "simplex" array is at the second smallest coordinate.
    // The integer offsets for the fourth simplex corner
    uint32_t i3 = simplex[c][0] >= 1 ? 1 : 0;
    uint32_t j3 = simplex[c][1] >= 1 ? 1 : 0;
    uint32_t k3 = simplex[c][2] >= 1 ? 1 : 0;
    uint32_t l3 = simplex[c][3] >= 1 ? 1 : 0;
    // The fifth corner has all coordinate offsets = 1, so no need to look that up.

    int32_t x1 = x0 - ((int32_t)i1<<14) + (int32_t)(G4>>18); // .14: Offsets for second corner in (x,y,z,w) coords
    int32_t y1 = y0 - ((int32_t)j1<<14) + (int32_t)(G4>>18);
    int32_t z1 = z0 - ((int32_t)k1<<14) + (int32_t)(G4>>18);
    int32_t w1 = w0 - ((int32_t)l1<<14) + (int32_t)(G4>>18);
    int32_t x2 = x0 - ((int32_t)i2<<14) + (int32_t)(2*G4>>18); // .14: Offsets for third corner in (x,y,z,w) coords
    int32_t y2 = y0 - ((int32_t)j2<<14) + (int32_t)(2*G4>>18);
    int32_t z2 = z0 - ((int32_t)k2<<14) + (int32_t)(2*G4>>18);
    int32_t w2 = w0 - ((int32_t)l2<<14) + (int32_t)(2*G4>>18);
    int32_t x3 = x0 - ((int32_t)i3<<14) + (int32_t)(3*G4>>18); // .14: Offsets for fourth corner in (x,y,z,w) coords
    int32_t y3 = y0 - ((int32_t)j3<<14) + (int32_t)(3*G4>>18);
    int32_t z3 = z0 - ((int32_t)k3<<14) + (int32_t)(3*G4>>18);
    int32_t w3 = w0 - ((int32_t)l3<<14) + (int32_t)(3*G4>>18);
    int32_t x4 = x0 - (1 << 14) + (int32_t)(4*G4>>18); // .14: Offsets for last corner in (x,y,z,w) coords
    int32_t y4 = y0 - (1 << 14) + (int32_t)(4*G4>>18);
    int32_t z4 = z0 - (1 << 14) + (int32_t)(4*G4>>18);
    int32_t w4 = w0 - (1 << 14) + (int32_t)(4*G4>>18);

    int32_t n0 = 0, n1 = 0, n2 = 0, n3 = 0, n4 = 0; // Noise contributions from the five corners
    const int32_t fix0_6 = 161061274;               // .28: 0.6

    // Calculate the contribution from the five corners
    int32_t t0 = (fix0_6 - x0*x0 - y0*y0 - z0*z0 - w0*w0) >> 12; // .16
    if (t0 > 0) {
        t0 = (t0 * t0) >> 16;
        t0 = (t0 * t0) >> 16;
        // .16 * .14 = .30
        n0 = t0 * grad(P((i+(uint32_t)(P((j+(uint32_t)(P((k+(uint32_t)(P(l&0xff)))&0xff)))&0xff)))&0xff), x0, y0, z0, w0);
    }

    int32_t t1 = (fix0_6 - x1*x1 - y1*y1 - z1*z1 - w1*w1) >> 12; // .16
    if (t1 > 0) {
        t1 = (t1 * t1) >> 16;
        t1 = (t1 * t1) >> 16;
        // .16 * .14 = .30
        n1 = t1 * grad(P((i+i1+(uint32_t)(P((j+j1+(uint32_t)(P((k+k1+(uint32_t)(P((l+l1)&0xff)))&0xff)))&0xff)))&0xff), x1, y1, z1, w1);
    }

    int32_t t2 = (fix0_6 - x2*x2 - y2*y2 - z2*z2 - w2*w2) >> 12; // .16
    if (t2 > 0) {
        t2 = (t2 * t2) >> 16;
        t2 = (t2 * t2) >> 16;
        // .16 * .14 = .30
        n2 = t2 * grad(P((i+i2+(uint32_t)(P((j+j2+(uint32_t)(P((k+k2+(uint32_t)(P((l+l2)&0xff)))&0xff)))&0xff)))&0xff), x2, y2, z2, w2);
    }

    int32_t t3 = (fix0_6 - x3*x3 - y3*y3 - z3*z3 - w3*w3) >> 12; // .16
    if (t3 > 0) {
        t3 = (t3 * t3) >> 16;
        t3 = (t3 * t3) >> 16;
        // .16 * .14 = .30
        n3 = t3 * grad(P((i+i3+(uint32_t)(P((j+j3+(uint32_t)(P((k+k3+(uint32_t)(P((l+l3)&0xff)))&0xff)))&0xff)))&0xff), x3, y3, z3, w3);
    }

    int32_t t4 = (fix0_6 - x4*x4 - y4*y4 - z4*z4 - w4*w4) >> 12; // .16
    if (t4 > 0) {
        t4 = (t4 * t4) >> 16;
        t4 = (t4 * t4) >> 16;
        // .16 * .14 = .30
        n4 = t4 * grad(P((i+1+(uint32_t)(P((j+1+(uint32_t)(P((k+1+(uint32_t)(P((l+1)&0xff)))&0xff)))&0xff)))&0xff), x4, y4, z4, w4);
    }

    int32_t n = n0 + n1 + n2 + n3 + n4;  // .30
	n = ((n >> 8) * 13832) >> 16; // fix scale
	return uint16_t(n) + 0x8000;
}

FASTLED_NAMESPACE_END
