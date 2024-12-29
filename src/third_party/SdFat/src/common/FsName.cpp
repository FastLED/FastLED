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
#include "FsName.h"

#include "FsUtf.h"
#if USE_UTF8_LONG_NAMES
uint16_t FsName::get16() {
  uint16_t rtn;
  if (ls) {
    rtn = ls;
    ls = 0;
  } else if (next >= end) {
    rtn = 0;
  } else {
    uint32_t cp;
    const char* ptr = FsUtf::mbToCp(next, end, &cp);
    if (!ptr) {
      goto fail;
    }
    next = ptr;
    if (cp <= 0XFFFF) {
      rtn = cp;
    } else {
      ls = FsUtf::lowSurrogate(cp);
      rtn = FsUtf::highSurrogate(cp);
    }
  }
  return rtn;

fail:
  return 0XFFFF;
}
#endif  // USE_UTF8_LONG_NAMES
