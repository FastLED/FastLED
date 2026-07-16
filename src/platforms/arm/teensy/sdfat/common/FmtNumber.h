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
#ifndef FmtNumber_h
#define FmtNumber_h
#include "fl/stl/int.h"

// IWYU pragma: private
namespace fl { namespace platforms { namespace teensy { namespace sdfat {
inline bool isDigit(char c) {
  return '0' <= (c) && (c) <= '9';
}
inline bool isSpace(char c) {
  return (c) == ' ' || (0X9 <= (c) && (c) <= 0XD);
}
char* fmtBase10(char* str, fl::u16 n);
char* fmtBase10(char* str, fl::u32 n);
char* fmtDouble(char* str, double d, fl::u8 prec, bool altFmt);
char* fmtDouble(char* str, double d, fl::u8 prec, bool altFmt, char expChar);
char* fmtHex(char* str, fl::u32 n);
char* fmtSigned(char* str, fl::i32 n, fl::u8 base, bool caps);
char* fmtUnsigned(char* str, fl::u32 n, fl::u8 base, bool caps);
} } } }  // namespace fl::platforms::teensy::sdfat
#endif  // FmtNumber_h
