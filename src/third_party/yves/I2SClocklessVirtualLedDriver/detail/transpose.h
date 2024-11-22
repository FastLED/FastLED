#pragma

#include "env.h"

namespace fl {

static inline __attribute__((always_inline)) void IRAM_ATTR transpose16x1_noinline2(unsigned char *A, uint8_t *B)
{

    uint32_t x, y, t;
#if NBIS2SERIALPINS >= 8
    uint32_t x1, y1;
#endif
#if NBIS2SERIALPINS >= 4
    uint32_t ff;
#endif
    uint32_t aa, cc;
    uint32_t ff2;
    aa = AAA;
    cc = CCC;
#if NBIS2SERIALPINS >= 4
    ff = FFF;
#endif
    ff2 = FFF2;
    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    //******
    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    //*************

    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    //******
    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    //  B+=370;
    // A+=16;
    // }
}

}  // namespace fl