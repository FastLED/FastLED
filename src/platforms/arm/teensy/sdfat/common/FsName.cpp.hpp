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
#include "FsName.h"  // ok include path - upstream SdFat local header
#include "FsUtf.h"  // ok include path - upstream SdFat local header
#include "fl/stl/int.h"
namespace fl { namespace platforms { namespace teensy { namespace sdfat {
#if USE_UTF8_LONG_NAMES
fl::u16 FsName::get16() {
  fl::u16 rtn;
  if (ls) {
    rtn = ls;
    ls = 0;
  } else if (next >= end) {
    rtn = 0;
  } else {
    fl::u32 cp;
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
} } } }  // namespace fl::platforms::teensy::sdfat
