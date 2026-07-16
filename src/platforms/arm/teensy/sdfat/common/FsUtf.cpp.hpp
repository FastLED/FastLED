// IWYU pragma: private
/**
 * Copyright (c) 2011-2021 Bill Greiman
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
// Private implementation unit, included by sdfat/_build.cpp.hpp.
#include "platforms/arm/teensy/sdfat/common/FsUtf.h"
namespace fl { namespace platforms { namespace teensy { namespace sdfat {
namespace FsUtf {
  //----------------------------------------------------------------------------
  char* cpToMb(fl::u32 cp, char* str, char* end) {
    size_t n = end - str;
    if (cp < 0X80) {
      if (n < 1) goto fail;
      *(str++) = static_cast<fl::u8>(cp);
    } else if (cp < 0X800) {
      if (n < 2) goto fail;
      *(str++) = static_cast<fl::u8>((cp >> 6)          | 0XC0);
      *(str++) = static_cast<fl::u8>((cp & 0X3F)        | 0X80);
    } else if (cp < 0X10000) {
      if (n < 3) goto fail;
      *(str++) = static_cast<fl::u8>((cp >> 12)         | 0XE0);
      *(str++) = static_cast<fl::u8>(((cp >> 6) & 0X3F) | 0X80);
      *(str++) = static_cast<fl::u8>((cp & 0X3F)        | 0X80);
    } else {
       if (n < 4) goto fail;
      *(str++) = static_cast<fl::u8>((cp >> 18)         | 0XF0);
      *(str++) = static_cast<fl::u8>(((cp >> 12) & 0X3F)| 0X80);
      *(str++) = static_cast<fl::u8>(((cp >> 6) & 0X3F) | 0X80);
      *(str++) = static_cast<fl::u8>((cp & 0X3F)        | 0X80);
    }
    return str;

   fail:
    return nullptr;
  }
  //----------------------------------------------------------------------------
  // to do?  improve error check
  const char* mbToCp(const char* str, const char* end, fl::u32* rtn) {
    size_t n;
    fl::u32 cp;
    if (str >= end) {
      return nullptr;
    }
    fl::u8 ch = str[0];
    if ((ch & 0X80) == 0) {
      *rtn = ch;
      return str + 1;
    }
    if ((ch & 0XE0) == 0XC0) {
      cp = ch & 0X1F;
      n = 2;
    } else if ((ch & 0XF0) == 0XE0) {
      cp = ch & 0X0F;
      n = 3;
    } else if ((ch & 0XF8) == 0XF0) {
      cp = ch & 0X07;
      n = 4;
    } else {
      return nullptr;
    }
    if ((str + n) > end) {
      return nullptr;
    }
    for (size_t i = 1; i < n; i++) {
      ch = str[i];
      if ((ch & 0XC0) != 0X80) {
        return nullptr;
      }
      cp <<= 6;
      cp |= ch & 0X3F;
    }
    // Don't allow over long as ASCII.
    if (cp < 0X80 || !isValidCp(cp)) {
      return nullptr;
    }
    *rtn = cp;
    return str + n;
  }
  //----------------------------------------------------------------------------
  const char* mbToU16(const char* str,
                      const char* end, fl::u16* hs, fl::u16* ls) {
    fl::u32 cp;
    const char* ptr = mbToCp(str, end, &cp);
    if (!ptr) {
      return nullptr;
    }
    if (cp <= 0XFFFF) {
      *hs = cp;
      *ls = 0;
    } else {
      *hs = highSurrogate(cp);
      *ls = lowSurrogate(cp);
    }
    return ptr;
  }
}  // namespace FsUtf
} } } }  // namespace fl::platforms::teensy::sdfat
