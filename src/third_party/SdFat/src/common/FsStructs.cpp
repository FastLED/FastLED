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
#include "FsStructs.h"
// bgnLba = relSector;
// endLba = relSector + partSize - 1;
void lbaToMbrChs(uint8_t* chs, uint32_t capacityMB, uint32_t lba) {
  uint32_t c;
  uint8_t h;
  uint8_t s;

  uint8_t numberOfHeads;
  uint8_t sectorsPerTrack = capacityMB <= 256 ? 32 : 63;
  if (capacityMB <= 16) {
    numberOfHeads = 2;
  } else if (capacityMB <= 32) {
    numberOfHeads = 4;
  } else if (capacityMB <= 128) {
    numberOfHeads = 8;
  } else if (capacityMB <= 504) {
    numberOfHeads = 16;
  } else if (capacityMB <= 1008) {
    numberOfHeads = 32;
  } else if (capacityMB <= 2016) {
    numberOfHeads = 64;
  } else if (capacityMB <= 4032) {
    numberOfHeads = 128;
  } else {
    numberOfHeads = 255;
  }
  c = lba / (numberOfHeads * sectorsPerTrack);
  if (c <= 1023) {
    h = (lba % (numberOfHeads * sectorsPerTrack)) / sectorsPerTrack;
    s = (lba % sectorsPerTrack) + 1;
  } else {
    c = 1023;
    h = 254;
    s = 63;
  }
  chs[0] = h;
  chs[1] = ((c >> 2) & 0XC0) | s;
  chs[2] = c;
}
