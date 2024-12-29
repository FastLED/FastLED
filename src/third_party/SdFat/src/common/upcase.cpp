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
#include "upcase.h"

#include <stddef.h>
#ifdef __AVR__
#include <avr/pgmspace.h>
#define TABLE_MEM PROGMEM
#define readTable8(sym) pgm_read_byte(&sym)
#define readTable16(sym) pgm_read_word(&sym)
#else  // __AVR__
#define TABLE_MEM
#define readTable8(sym) (sym)
#define readTable16(sym) (sym)
#endif  // __AVR__

struct map16 {
  uint16_t base;
  int8_t off;
  uint8_t count;
};
typedef struct map16 map16_t;

struct pair16 {
  uint16_t key;
  uint16_t val;
};
typedef struct pair16 pair16_t;
//------------------------------------------------------------------------------
static const map16_t mapTable[] TABLE_MEM = {
    {0X0061, -32, 26}, {0X00E0, -32, 23}, {0X00F8, -32, 7},  {0X0100, 1, 48},
    {0X0132, 1, 6},    {0X0139, 1, 16},   {0X014A, 1, 46},   {0X0179, 1, 6},
    {0X0182, 1, 4},    {0X01A0, 1, 6},    {0X01B3, 1, 4},    {0X01CD, 1, 16},
    {0X01DE, 1, 18},   {0X01F8, 1, 40},   {0X0222, 1, 18},   {0X0246, 1, 10},
    {0X03AD, -37, 3},  {0X03B1, -32, 17}, {0X03C3, -32, 9},  {0X03D8, 1, 24},
    {0X0430, -32, 32}, {0X0450, -80, 16}, {0X0460, 1, 34},   {0X048A, 1, 54},
    {0X04C1, 1, 14},   {0X04D0, 1, 68},   {0X0561, -48, 38}, {0X1E00, 1, 150},
    {0X1EA0, 1, 90},   {0X1F00, 8, 8},    {0X1F10, 8, 6},    {0X1F20, 8, 8},
    {0X1F30, 8, 8},    {0X1F40, 8, 6},    {0X1F60, 8, 8},    {0X1F70, 74, 2},
    {0X1F72, 86, 4},   {0X1F76, 100, 2},  {0X1F7A, 112, 2},  {0X1F7C, 126, 2},
    {0X1F80, 8, 8},    {0X1F90, 8, 8},    {0X1FA0, 8, 8},    {0X1FB0, 8, 2},
    {0X1FD0, 8, 2},    {0X1FE0, 8, 2},    {0X2170, -16, 16}, {0X24D0, -26, 26},
    {0X2C30, -48, 47}, {0X2C67, 1, 6},    {0X2C80, 1, 100},  {0X2D00, 0, 38},
    {0XFF41, -32, 26},
};
const size_t MAP_DIM = sizeof(mapTable) / sizeof(map16_t);
//------------------------------------------------------------------------------
static const pair16_t lookupTable[] TABLE_MEM = {
    {0X00FF, 0X0178}, {0X0180, 0X0243}, {0X0188, 0X0187}, {0X018C, 0X018B},
    {0X0192, 0X0191}, {0X0195, 0X01F6}, {0X0199, 0X0198}, {0X019A, 0X023D},
    {0X019E, 0X0220}, {0X01A8, 0X01A7}, {0X01AD, 0X01AC}, {0X01B0, 0X01AF},
    {0X01B9, 0X01B8}, {0X01BD, 0X01BC}, {0X01BF, 0X01F7}, {0X01C6, 0X01C4},
    {0X01C9, 0X01C7}, {0X01CC, 0X01CA}, {0X01DD, 0X018E}, {0X01F3, 0X01F1},
    {0X01F5, 0X01F4}, {0X023A, 0X2C65}, {0X023C, 0X023B}, {0X023E, 0X2C66},
    {0X0242, 0X0241}, {0X0253, 0X0181}, {0X0254, 0X0186}, {0X0256, 0X0189},
    {0X0257, 0X018A}, {0X0259, 0X018F}, {0X025B, 0X0190}, {0X0260, 0X0193},
    {0X0263, 0X0194}, {0X0268, 0X0197}, {0X0269, 0X0196}, {0X026B, 0X2C62},
    {0X026F, 0X019C}, {0X0272, 0X019D}, {0X0275, 0X019F}, {0X027D, 0X2C64},
    {0X0280, 0X01A6}, {0X0283, 0X01A9}, {0X0288, 0X01AE}, {0X0289, 0X0244},
    {0X028A, 0X01B1}, {0X028B, 0X01B2}, {0X028C, 0X0245}, {0X0292, 0X01B7},
    {0X037B, 0X03FD}, {0X037C, 0X03FE}, {0X037D, 0X03FF}, {0X03AC, 0X0386},
    {0X03C2, 0X03A3}, {0X03CC, 0X038C}, {0X03CD, 0X038E}, {0X03CE, 0X038F},
    {0X03F2, 0X03F9}, {0X03F8, 0X03F7}, {0X03FB, 0X03FA}, {0X04CF, 0X04C0},
    {0X1D7D, 0X2C63}, {0X1F51, 0X1F59}, {0X1F53, 0X1F5B}, {0X1F55, 0X1F5D},
    {0X1F57, 0X1F5F}, {0X1F78, 0X1FF8}, {0X1F79, 0X1FF9}, {0X1FB3, 0X1FBC},
    {0X1FCC, 0X1FC3}, {0X1FE5, 0X1FEC}, {0X1FFC, 0X1FF3}, {0X214E, 0X2132},
    {0X2184, 0X2183}, {0X2C61, 0X2C60}, {0X2C76, 0X2C75},
};
const size_t LOOKUP_DIM = sizeof(lookupTable) / sizeof(pair16_t);
//------------------------------------------------------------------------------
static size_t searchPair16(const pair16_t* table, size_t size, uint16_t key) {
  size_t left = 0;
  size_t right = size;
  while (right - left > 1) {
    size_t mid = left + (right - left) / 2;
    if (readTable16(table[mid].key) <= key) {
      left = mid;
    } else {
      right = mid;
    }
  }
  return left;
}
//------------------------------------------------------------------------------
uint16_t toUpcase(uint16_t chr) {
  uint16_t i, first;
  // Optimize for simple ASCII.
  if (chr < 127) {
    return chr - ('a' <= chr && chr <= 'z' ? 'a' - 'A' : 0);
  }
  i = searchPair16(reinterpret_cast<const pair16_t*>(mapTable), MAP_DIM, chr);
  first = readTable16(mapTable[i].base);
  if (first <= chr && (chr - first) < readTable8(mapTable[i].count)) {
    int8_t off = readTable8(mapTable[i].off);
    if (off == 1) {
      return chr - ((chr - first) & 1);
    }
    return chr + (off ? off : -0x1C60);
  }
  i = searchPair16(lookupTable, LOOKUP_DIM, chr);
  if (readTable16(lookupTable[i].key) == chr) {
    return readTable16(lookupTable[i].val);
  }
  return chr;
}
//------------------------------------------------------------------------------
uint32_t upcaseChecksum(uint16_t uc, uint32_t sum) {
  sum = (sum << 31) + (sum >> 1) + (uc & 0XFF);
  sum = (sum << 31) + (sum >> 1) + (uc >> 8);
  return sum;
}
