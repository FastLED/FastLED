#pragma once

#ifndef __INC_BITSWAP_H
#define __INC_BITSWAP_H

#include "FastLED.h"
#include "fl/force_inline.h"
#include "fl/int.h"

/// @file bitswap.h
/// Functions for doing a rotation of bits/bytes used by parallel output

FASTLED_NAMESPACE_BEGIN


#if defined(FASTLED_ARM) || defined(FASTLED_ESP8266) || defined(FASTLED_DOXYGEN)
/// Structure representing 8 bits of access
typedef union {
  fl::u8 raw;   ///< the entire byte
  struct {
  fl::u32 a0:1;  ///< bit 0 (0x01)
  fl::u32 a1:1;  ///< bit 1 (0x02)
  fl::u32 a2:1;  ///< bit 2 (0x04)
  fl::u32 a3:1;  ///< bit 3 (0x08)
  fl::u32 a4:1;  ///< bit 4 (0x10)
  fl::u32 a5:1;  ///< bit 5 (0x20)
  fl::u32 a6:1;  ///< bit 6 (0x40)
  fl::u32 a7:1;  ///< bit 7 (0x80)
  };
} just8bits;

/// Structure representing 32 bits of access
typedef struct {
  fl::u32 a0:1;  ///< byte 'a', bit 0 (0x00000000)
  fl::u32 a1:1;  ///< byte 'a', bit 1 (0x00000002)
  fl::u32 a2:1;  ///< byte 'a', bit 2 (0x00000004)
  fl::u32 a3:1;  ///< byte 'a', bit 3 (0x00000008)
  fl::u32 a4:1;  ///< byte 'a', bit 4 (0x00000010)
  fl::u32 a5:1;  ///< byte 'a', bit 5 (0x00000020)
  fl::u32 a6:1;  ///< byte 'a', bit 6 (0x00000040)
  fl::u32 a7:1;  ///< byte 'a', bit 7 (0x00000080)
  fl::u32 b0:1;  ///< byte 'b', bit 0 (0x00000100)
  fl::u32 b1:1;  ///< byte 'b', bit 1 (0x00000200)
  fl::u32 b2:1;  ///< byte 'b', bit 2 (0x00000400)
  fl::u32 b3:1;  ///< byte 'b', bit 3 (0x00000800)
  fl::u32 b4:1;  ///< byte 'b', bit 4 (0x00001000)
  fl::u32 b5:1;  ///< byte 'b', bit 5 (0x00002000)
  fl::u32 b6:1;  ///< byte 'b', bit 6 (0x00004000)
  fl::u32 b7:1;  ///< byte 'b', bit 7 (0x00008000)
  fl::u32 c0:1;  ///< byte 'c', bit 0 (0x00010000)
  fl::u32 c1:1;  ///< byte 'c', bit 1 (0x00020000)
  fl::u32 c2:1;  ///< byte 'c', bit 2 (0x00040000)
  fl::u32 c3:1;  ///< byte 'c', bit 3 (0x00080000)
  fl::u32 c4:1;  ///< byte 'c', bit 4 (0x00100000)
  fl::u32 c5:1;  ///< byte 'c', bit 5 (0x00200000)
  fl::u32 c6:1;  ///< byte 'c', bit 6 (0x00400000)
  fl::u32 c7:1;  ///< byte 'c', bit 7 (0x00800000)
  fl::u32 d0:1;  ///< byte 'd', bit 0 (0x01000000)
  fl::u32 d1:1;  ///< byte 'd', bit 1 (0x02000000)
  fl::u32 d2:1;  ///< byte 'd', bit 2 (0x04000000)
  fl::u32 d3:1;  ///< byte 'd', bit 3 (0x08000000)
  fl::u32 d4:1;  ///< byte 'd', bit 4 (0x10000000)
  fl::u32 d5:1;  ///< byte 'd', bit 5 (0x20000000)
  fl::u32 d6:1;  ///< byte 'd', bit 6 (0x40000000)
  fl::u32 d7:1;  ///< byte 'd', bit 7 (0x80000000)
} sub4;

/// Union containing a full 8 bytes to swap the bit orientation on
typedef union {
  fl::u32 word[2];  ///< two 32-bit values to load for swapping
  fl::u8 bytes[8];  ///< eight 8-bit values to load for swapping
  struct {
    sub4 a;  ///< 32-bit access struct for bit swapping, upper four bytes (word[0] or bytes[0-3])
    sub4 b;  ///< 32-bit access struct for bit swapping, lower four bytes (word[1] or bytes[4-7])
  };
} bitswap_type;


/// Set `out.X` bits 0, 1, 2, and 3 to bit N
/// of `in.a.a`, `in.a.b`, `in.a.b`, `in.a.c`, and `in.a.d`
/// @param X the sub4 of `out` to set
/// @param N the bit of each byte to retrieve
/// @see bitswap_type
#define SWAPSA(X,N) out.  X ## 0 = in.a.a ## N; \
  out.  X ## 1 = in.a.b ## N; \
  out.  X ## 2 = in.a.c ## N; \
  out.  X ## 3 = in.a.d ## N;

/// Set `out.X` bits 0, 1, 2, and 3 to bit N
/// of `in.b.a`, `in.b.b`, `in.b.b`, `in.b.c`, and `in.b.d`
/// @param X the sub4 of `out` to set
/// @param N the bit of each byte to retrieve
/// @see bitswap_type
#define SWAPSB(X,N) out.  X ## 0 = in.b.a ## N; \
  out.  X ## 1 = in.b.b ## N; \
  out.  X ## 2 = in.b.c ## N; \
  out.  X ## 3 = in.b.d ## N;

/// Set `out.X` bits to bit N of both `in.a` and `in.b`
/// in order
/// @param X the sub4 of `out` to set
/// @param N the bit of each byte to retrieve
/// @see bitswap_type
#define SWAPS(X,N) out.  X ## 0 = in.a.a ## N; \
  out.  X ## 1 = in.a.b ## N; \
  out.  X ## 2 = in.a.c ## N; \
  out.  X ## 3 = in.a.d ## N; \
  out.  X ## 4 = in.b.a ## N; \
  out.  X ## 5 = in.b.b ## N; \
  out.  X ## 6 = in.b.c ## N; \
  out.  X ## 7 = in.b.d ## N;


/// Do an 8-byte by 8-bit rotation
FASTLED_FORCE_INLINE void swapbits8(bitswap_type in, bitswap_type & out) {

  // SWAPS(a.a,7);
  // SWAPS(a.b,6);
  // SWAPS(a.c,5);
  // SWAPS(a.d,4);
  // SWAPS(b.a,3);
  // SWAPS(b.b,2);
  // SWAPS(b.c,1);
  // SWAPS(b.d,0);

  // SWAPSA(a.a,7);
  // SWAPSA(a.b,6);
  // SWAPSA(a.c,5);
  // SWAPSA(a.d,4);
  //
  // SWAPSB(a.a,7);
  // SWAPSB(a.b,6);
  // SWAPSB(a.c,5);
  // SWAPSB(a.d,4);
  //
  // SWAPSA(b.a,3);
  // SWAPSA(b.b,2);
  // SWAPSA(b.c,1);
  // SWAPSA(b.d,0);
  // //
  // SWAPSB(b.a,3);
  // SWAPSB(b.b,2);
  // SWAPSB(b.c,1);
  // SWAPSB(b.d,0);

  for(int i = 0; i < 8; ++i) {
    just8bits work;
    work.a3 = in.word[0] >> 31;
    work.a2 = in.word[0] >> 23;
    work.a1 = in.word[0] >> 15;
    work.a0 = in.word[0] >> 7;
    in.word[0] <<= 1;
    work.a7 = in.word[1] >> 31;
    work.a6 = in.word[1] >> 23;
    work.a5 = in.word[1] >> 15;
    work.a4 = in.word[1] >> 7;
    in.word[1] <<= 1;
    out.bytes[i] = work.raw;
  }
}

/// Slow version of the 8 byte by 8 bit rotation
FASTLED_FORCE_INLINE void slowswap(unsigned char *A, unsigned char *B) {

  for(int row = 0; row < 7; ++row) {
    fl::u8 x = A[row];

    fl::u8 bit = (1<<row);
    unsigned char *p = B;
    for(fl::u32 mask = 1<<7 ; mask ; mask >>= 1) {
      if(x & mask) {
        *p++ |= bit;
      } else {
        *p++ &= ~bit;
      }
    }
    // B[7] |= (x & 0x01) << row; x >>= 1;
    // B[6] |= (x & 0x01) << row; x >>= 1;
    // B[5] |= (x & 0x01) << row; x >>= 1;
    // B[4] |= (x & 0x01) << row; x >>= 1;
    // B[3] |= (x & 0x01) << row; x >>= 1;
    // B[2] |= (x & 0x01) << row; x >>= 1;
    // B[1] |= (x & 0x01) << row; x >>= 1;
    // B[0] |= (x & 0x01) << row; x >>= 1;
  }
}

/// Simplified form of bits rotating function. 
/// This rotates data into LSB for a faster write (the code using this data can happily walk the array backwards).  
/// Based on code found here: https://web.archive.org/web/20190108225554/http://www.hackersdelight.org/hdcodetxt/transpose8.c.txt
void transpose8x1_noinline(unsigned char *A, unsigned char *B);

/// @copydoc transpose8x1_noinline()
FASTLED_FORCE_INLINE void transpose8x1(unsigned char *A, unsigned char *B) {
  fl::u32 x, y, t;

  // Load the array and pack it into x and y.
  y = *(unsigned int*)(A);
  x = *(unsigned int*)(A+4);

  // pre-transform x
  t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
  t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);

  // pre-transform y
  t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
  t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

  // final transform
  t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
  y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
  x = t;

  *((uint32_t*)B) = y;
  *((uint32_t*)(B+4)) = x;
}

/// Simplified form of bits rotating function. 
/// Based on code found here: https://web.archive.org/web/20190108225554/http://www.hackersdelight.org/hdcodetxt/transpose8.c.txt
FASTLED_FORCE_INLINE void transpose8x1_MSB(unsigned char *A, unsigned char *B) {
  fl::u32 x, y, t;

  // Load the array and pack it into x and y.
  y = *(unsigned int*)(A);
  x = *(unsigned int*)(A+4);

  // pre-transform x
  t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
  t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);

  // pre-transform y
  t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
  t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

  // final transform
  t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
  y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
  x = t;

  B[7] = y; y >>= 8;
  B[6] = y; y >>= 8;
  B[5] = y; y >>= 8;
  B[4] = y;

  B[3] = x; x >>= 8;
  B[2] = x; x >>= 8;
  B[1] = x; x >>= 8;
  B[0] = x; /* */
}

/// Templated bit-rotating function. 
/// Based on code found here: https://web.archive.org/web/20190108225554/http://www.hackersdelight.org/hdcodetxt/transpose8.c.txt
template<int m, int n>
FASTLED_FORCE_INLINE void transpose8(unsigned char *A, unsigned char *B) {
  fl::u32 x, y, t;

  // Load the array and pack it into x and y.
  if(m == 1) {
    y = *(unsigned int*)(A);
    x = *(unsigned int*)(A+4);
  } else {
    x = (A[0]<<24)   | (A[m]<<16)   | (A[2*m]<<8) | A[3*m];
    y = (A[4*m]<<24) | (A[5*m]<<16) | (A[6*m]<<8) | A[7*m];
  }

  // pre-transform x
  t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
  t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);

  // pre-transform y
  t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
  t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

  // final transform
  t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
  y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
  x = t;

  B[7*n] = y; y >>= 8;
  B[6*n] = y; y >>= 8;
  B[5*n] = y; y >>= 8;
  B[4*n] = y;

  B[3*n] = x; x >>= 8;
  B[2*n] = x; x >>= 8;
  B[n] = x; x >>= 8;
  B[0] = x;
  // B[0]=x>>24;    B[n]=x>>16;    B[2*n]=x>>8;  B[3*n]=x>>0;
  // B[4*n]=y>>24;  B[5*n]=y>>16;  B[6*n]=y>>8;  B[7*n]=y>>0;
}

#endif

FASTLED_NAMESPACE_END

#endif
