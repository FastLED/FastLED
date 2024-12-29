/**
 * Copyright (c) 2011-2022 Bill Greiman
 * This file is part of the SdFat library for SD memory cards.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "FmtNumber.h"
// always use fmtBase10() - seems fast even on teensy 3.6.
#define USE_FMT_BASE10 1

// Use Stimmer div/mod 10 on avr
#ifdef __AVR__
#include <avr/pgmspace.h>
#define USE_STIMMER
#endif  // __AVR__
//------------------------------------------------------------------------------
// Stimmer div/mod 10 for AVR
// this code fragment works out i/10 and i%10 by calculating
// i*(51/256)*(256/255)/2 == i*51/510 == i/10
// by "j.k" I mean 32.8 fixed point, j is integer part, k is fractional part
// j.k = ((j+1.0)*51.0)/256.0
// (we add 1 because we will be using the floor of the result later)
// divmod10_asm16 and divmod10_asm32 are public domain code by Stimmer.
// http://forum.arduino.cc/index.php?topic=167414.msg1293679#msg1293679
#define divmod10_asm16(in32, mod8, tmp8)   \
  asm volatile(                            \
      " ldi %2,51     \n\t"                \
      " mul %A0,%2    \n\t"                \
      " clr %A0       \n\t"                \
      " add r0,%2     \n\t"                \
      " adc %A0,r1    \n\t"                \
      " mov %1,r0     \n\t"                \
      " mul %B0,%2    \n\t"                \
      " clr %B0       \n\t"                \
      " add %A0,r0    \n\t"                \
      " adc %B0,r1    \n\t"                \
      " clr r1        \n\t"                \
      " add %1,%A0    \n\t"                \
      " adc %A0,%B0   \n\t"                \
      " adc %B0,r1   \n\t"                 \
      " add %1,%B0    \n\t"                \
      " adc %A0,r1   \n\t"                 \
      " adc %B0,r1    \n\t"                \
      " lsr %B0       \n\t"                \
      " ror %A0       \n\t"                \
      " ror %1        \n\t"                \
      " ldi %2,10     \n\t"                \
      " mul %1,%2     \n\t"                \
      " mov %1,r1     \n\t"                \
      " clr r1        \n\t"                \
      : "+r"(in32), "=d"(mod8), "=d"(tmp8) \
      :                                    \
      : "r0")

#define divmod10_asm32(in32, mod8, tmp8)   \
  asm volatile(                            \
      " ldi %2,51     \n\t"                \
      " mul %A0,%2    \n\t"                \
      " clr %A0       \n\t"                \
      " add r0,%2     \n\t"                \
      " adc %A0,r1    \n\t"                \
      " mov %1,r0     \n\t"                \
      " mul %B0,%2    \n\t"                \
      " clr %B0       \n\t"                \
      " add %A0,r0    \n\t"                \
      " adc %B0,r1    \n\t"                \
      " mul %C0,%2    \n\t"                \
      " clr %C0       \n\t"                \
      " add %B0,r0    \n\t"                \
      " adc %C0,r1    \n\t"                \
      " mul %D0,%2    \n\t"                \
      " clr %D0       \n\t"                \
      " add %C0,r0    \n\t"                \
      " adc %D0,r1    \n\t"                \
      " clr r1        \n\t"                \
      " add %1,%A0    \n\t"                \
      " adc %A0,%B0   \n\t"                \
      " adc %B0,%C0   \n\t"                \
      " adc %C0,%D0   \n\t"                \
      " adc %D0,r1    \n\t"                \
      " add %1,%B0    \n\t"                \
      " adc %A0,%C0   \n\t"                \
      " adc %B0,%D0   \n\t"                \
      " adc %C0,r1    \n\t"                \
      " adc %D0,r1    \n\t"                \
      " add %1,%D0    \n\t"                \
      " adc %A0,r1    \n\t"                \
      " adc %B0,r1    \n\t"                \
      " adc %C0,r1    \n\t"                \
      " adc %D0,r1    \n\t"                \
      " lsr %D0       \n\t"                \
      " ror %C0       \n\t"                \
      " ror %B0       \n\t"                \
      " ror %A0       \n\t"                \
      " ror %1        \n\t"                \
      " ldi %2,10     \n\t"                \
      " mul %1,%2     \n\t"                \
      " mov %1,r1     \n\t"                \
      " clr r1        \n\t"                \
      : "+r"(in32), "=d"(mod8), "=d"(tmp8) \
      :                                    \
      : "r0")
//------------------------------------------------------------------------------
/*
// C++ code is based on this version of divmod10 by robtillaart.
// http://forum.arduino.cc/index.php?topic=167414.msg1246851#msg1246851
// from robtillaart post:
// The code is based upon the divu10() code from the book Hackers Delight1.
// My insight was that the error formula in divu10() was in fact modulo 10
// but not always. Sometimes it was 10 more.
void divmod10(uint32_t in, uint32_t &div, uint32_t &mod)
{
  // q = in * 0.8;
  uint32_t q = (in >> 1) + (in >> 2);
  q = q + (q >> 4);
  q = q + (q >> 8);
  q = q + (q >> 16);  // not needed for 16 bit version

  // q = q / 8;  ==> q =  in *0.1;
  q = q >> 3;

  // determine error
  uint32_t r = in - ((q << 3) + (q << 1));   // r = in - q*10;
  div = q + (r > 9);
  if (r > 9) mod = r - 10;
  else mod = r;
}
// See: https://github.com/hcs0/Hackers-Delight
// Code below uses 8/10 = 0.1100 1100 1100 1100 1100 1100 1100 1100.
// 15 ops including the multiply, or 17 elementary ops.
unsigned divu10(unsigned n) {
   unsigned q, r;

   q = (n >> 1) + (n >> 2);
   q = q + (q >> 4);
   q = q + (q >> 8);
   q = q + (q >> 16);
   q = q >> 3;
   r = n - q*10;
   return q + ((r + 6) >> 4);
// return q + (r > 9);
}
*/
//------------------------------------------------------------------------------
// Format 16-bit unsigned
char* fmtBase10(char* str, uint16_t n) {
  while (n > 9) {
#ifdef USE_STIMMER
    uint8_t tmp8, r;
    divmod10_asm16(n, r, tmp8);
#else   // USE_STIMMER
    uint16_t t = n;
    n = (n >> 1) + (n >> 2);
    n = n + (n >> 4);
    n = n + (n >> 8);
    // n = n + (n >> 16);  // no code for 16-bit n
    n = n >> 3;
    uint8_t r = t - (((n << 2) + n) << 1);
    if (r > 9) {
      n++;
      r -= 10;
    }
#endif  // USE_STIMMER
    *--str = r + '0';
  }
  *--str = n + '0';
  return str;
}
//------------------------------------------------------------------------------
// format 32-bit unsigned
char* fmtBase10(char* str, uint32_t n) {
  while (n > 0XFFFF) {
#ifdef USE_STIMMER
    uint8_t tmp8, r;
    divmod10_asm32(n, r, tmp8);
#else   //  USE_STIMMER
    uint32_t t = n;
    n = (n >> 1) + (n >> 2);
    n = n + (n >> 4);
    n = n + (n >> 8);
    n = n + (n >> 16);
    n = n >> 3;
    uint8_t r = t - (((n << 2) + n) << 1);
    if (r > 9) {
      n++;
      r -= 10;
    }
#endif  // USE_STIMMER
    *--str = r + '0';
  }
  return fmtBase10(str, (uint16_t)n);
}
//------------------------------------------------------------------------------
char* fmtHex(char* str, uint32_t n) {
  do {
    uint8_t h = n & 0XF;
    *--str = h + (h < 10 ? '0' : 'A' - 10);
    n >>= 4;
  } while (n);
  return str;
}
//------------------------------------------------------------------------------
char* fmtSigned(char* str, int32_t num, uint8_t base, bool caps) {
  bool neg = base == 10 && num < 0;
  if (neg) {
    num = -num;
  }
  str = fmtUnsigned(str, num, base, caps);
  if (neg) {
    *--str = '-';
  }
  return str;
}
//-----------------------------------------------------------------------------
char* fmtUnsigned(char* str, uint32_t num, uint8_t base, bool caps) {
#if USE_FMT_BASE10
  if (base == 10) return fmtBase10(str, (uint32_t)num);
#endif  // USE_FMT_BASE10
  do {
    int c = num % base;
    *--str = c + (c < 10 ? '0' : caps ? 'A' - 10 : 'a' - 10);
  } while (num /= base);
  return str;
}
//-----------------------------------------------------------------------------

static const double powTen[] = {1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9};
static const double rnd[] = {5e-1, 5e-2, 5e-3, 5e-4, 5e-5,
                             5e-6, 5e-7, 5e-8, 5e-9, 5e-10};
static const size_t MAX_PREC = sizeof(powTen) / sizeof(powTen[0]);

char* fmtDouble(char* str, double num, uint8_t prec, bool altFmt) {
  bool neg = num < 0;
  if (neg) {
    num = -num;
  }
  if (isnan(num)) {
    *--str = 'n';
    *--str = 'a';
    *--str = 'n';
    return str;
  }
  if (isinf(num)) {
    *--str = 'f';
    *--str = 'n';
    *--str = 'i';
    return str;
  }
  // last float < 2^32
  if (num > 4294967040.0) {
    *--str = 'f';
    *--str = 'v';
    *--str = 'o';
    return str;
  }

  if (prec > MAX_PREC) {
    prec = MAX_PREC;
  }
  num += rnd[prec];
  uint32_t ul = num;
  if (prec) {
    char* s = str - prec;
    uint32_t f = (num - ul) * powTen[prec - 1];
    str = fmtBase10(str, f);
    while (str > s) {
      *--str = '0';
    }
  }
  if (prec || altFmt) {
    *--str = '.';
  }
  str = fmtBase10(str, ul);
  if (neg) {
    *--str = '-';
  }
  return str;
}
//------------------------------------------------------------------------------
/** Print a number followed by a field terminator.
 * \param[in] value The number to be printed.
 * \param[in] ptr Pointer to last char in buffer.
 * \param[in] prec Number of digits after decimal point.
 * \param[in] expChar Use exp format if non zero.
 * \return Pointer to first character of result.
 */
char* fmtDouble(char* str, double value, uint8_t prec, bool altFmt,
                char expChar) {
  if (expChar != 'e' && expChar != 'E') {
    expChar = 0;
  }
  bool neg = value < 0;
  if (neg) {
    value = -value;
  }
  // check for nan inf ovf
  if (isnan(value)) {
    *--str = 'n';
    *--str = 'a';
    *--str = 'n';
    return str;
  }
  if (isinf(value)) {
    *--str = 'f';
    *--str = 'n';
    *--str = 'i';
    return str;
  }
  if (!expChar && value > 4294967040.0) {
    *--str = 'f';
    *--str = 'v';
    *--str = 'o';
    return str;
  }
  if (prec > 9) {
    prec = 9;
  }
  if (expChar) {
    int8_t exp = 0;
    bool expNeg = false;
    if (value) {
      if (value > 10.0L) {
        while (value > 1e16L) {
          value *= 1e-16L;
          exp += 16;
        }
        while (value > 1e4L) {
          value *= 1e-4L;
          exp += 4;
        }
        while (value > 10.0L) {
          value *= 0.1L;
          exp++;
        }
      } else if (value < 1.0L) {
        while (value < 1e-16L) {
          value *= 1e16L;
          exp -= 16;
        }
        while (value < 1e-4L) {
          value *= 1e4L;
          exp -= 4;
        }
        while (value < 1.0L) {
          value *= 10.0L;
          exp--;
        }
      }
      value += rnd[prec];
      if (value >= 10.0L) {
        value *= 0.1L;
        exp++;
      }
      expNeg = exp < 0;
      if (expNeg) {
        exp = -exp;
      }
    }
    str = fmtBase10(str, (uint16_t)exp);
    if (exp < 10) {
      *--str = '0';
    }
    *--str = expNeg ? '-' : '+';
    *--str = expChar;
  } else {
    // round value
    value += rnd[prec];
  }

  uint32_t whole = value;
  if (prec) {
    char* tmp = str - prec;
    uint32_t fraction = (value - whole) * powTen[prec - 1];
    str = fmtBase10(str, fraction);
    while (str > tmp) {
      *--str = '0';
    }
  }
  if (prec || altFmt) *--str = '.';
  str = fmtBase10(str, whole);
  if (neg) {
    *--str = '-';
  }
  return str;
}
//==============================================================================
//  functions below not used
//------------------------------------------------------------------------------
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef __AVR__
static const float m[] PROGMEM = {1e-1, 1e-2, 1e-4, 1e-8, 1e-16, 1e-32};
static const float p[] PROGMEM = {1e+1, 1e+2, 1e+4, 1e+8, 1e+16, 1e+32};
#else   // __AVR__
static const float m[] = {1e-1, 1e-2, 1e-4, 1e-8, 1e-16, 1e-32};
static const float p[] = {1e+1, 1e+2, 1e+4, 1e+8, 1e+16, 1e+32};
#endif  // __AVR__
#endif  // DOXYGEN_SHOULD_SKIP_THIS
// scale float v by power of ten. return v*10^n
float scale10(float v, int8_t n) {
  const float* s;
  if (n < 0) {
    n = -n;
    s = m;
  } else {
    s = p;
  }
  n &= 63;
  for (uint8_t i = 0; n; n >>= 1, i++) {
#ifdef __AVR__
    if (n & 1) {
      v *= pgm_read_float(&s[i]);
    }
#else   // __AVR__
    if (n & 1) {
      v *= s[i];
    }
#endif  // __AVR__
  }
  return v;
}
//------------------------------------------------------------------------------
float scanFloat(const char* str, const char** ptr) {
  int16_t const EXP_LIMIT = 100;
  bool digit = false;
  bool dot = false;
  uint32_t fract = 0;
  int fracExp = 0;
  uint8_t nd = 0;
  bool neg;
  int c;
  float v;
  const char* successPtr = str;

  if (ptr) {
    *ptr = str;
  }

  while (isSpace((c = *str++))) {
  }
  neg = c == '-';
  if (c == '-' || c == '+') {
    c = *str++;
  }
  // Skip leading zeros
  while (c == '0') {
    c = *str++;
    digit = true;
  }
  for (;;) {
    if (isDigit(c)) {
      digit = true;
      if (nd < 9) {
        fract = 10 * fract + c - '0';
        nd++;
        if (dot) {
          fracExp--;
        }
      } else {
        if (!dot) {
          fracExp++;
        }
      }
    } else if (c == '.') {
      if (dot) {
        goto fail;
      }
      dot = true;
    } else {
      if (!digit) {
        goto fail;
      }
      break;
    }
    successPtr = str;
    c = *str++;
  }
  if (c == 'e' || c == 'E') {
    int exp = 0;
    c = *str++;
    bool expNeg = c == '-';
    if (c == '-' || c == '+') {
      c = *str++;
    }
    while (isDigit(c)) {
      if (exp > EXP_LIMIT) {
        goto fail;
      }
      exp = 10 * exp + c - '0';
      successPtr = str;
      c = *str++;
    }
    fracExp += expNeg ? -exp : exp;
  }
  if (ptr) {
    *ptr = successPtr;
  }
  v = scale10(static_cast<float>(fract), fracExp);
  return neg ? -v : v;

fail:
  return 0;
}
