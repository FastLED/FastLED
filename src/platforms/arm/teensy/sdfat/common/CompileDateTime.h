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
#ifndef CompileDateTime_h
#define CompileDateTime_h
// IWYU pragma: private

#include "fl/stl/int.h"
namespace fl { namespace platforms { namespace teensy { namespace sdfat {

// Note - these functions will compile to a few bytes
//        since they are evaluated at compile time.

/** \return year field of the __DATE__ macro. */
constexpr fl::u16 compileYear() {
  return  1000*(__DATE__[7] - '0')
         + 100*(__DATE__[8] - '0')
          + 10*(__DATE__[9] - '0')
             + (__DATE__[10] - '0');
}
/** \return true if str equals the month field of the __DATE__ macro. */
constexpr bool compileMonthIs(const char* str) {
  return __DATE__[0] == str[0]
      && __DATE__[1] == str[1]
      && __DATE__[2] == str[2];
}
/** \return month field of the __DATE__ macro. */
constexpr fl::u8 compileMonth() {
  return compileMonthIs("Jan") ? 1 :
         compileMonthIs("Feb") ? 2 :
         compileMonthIs("Mar") ? 3 :
         compileMonthIs("Apr") ? 4 :
         compileMonthIs("May") ? 5 :
         compileMonthIs("Jun") ? 6 :
         compileMonthIs("Jul") ? 7 :
         compileMonthIs("Aug") ? 8 :
         compileMonthIs("Sep") ? 9 :
         compileMonthIs("Oct") ? 10 :
         compileMonthIs("Nov") ? 11 :
         compileMonthIs("Dec") ? 12 : 0;
}
/** \return day field of the __DATE__ macro. */
constexpr fl::u8 compileDay() {
  return 10*(__DATE__[4] == ' ' ? 0 : __DATE__[4] - '0') + (__DATE__[5] - '0');
}
/** \return hour field of the __TIME__ macro. */
constexpr fl::u8 compileHour() {
  return 10*(__TIME__[0] - '0') + __TIME__[1] - '0';
}
/** \return minute field of the __TIME__ macro. */
constexpr fl::u8 compileMinute() {
  return 10*(__TIME__[3] - '0') + __TIME__[4] - '0';
}
/** \return second field of the __TIME__ macro. */
constexpr fl::u8 compileSecond() {
  return 10*(__TIME__[6] - '0') + __TIME__[7] - '0';
}
} } } }  // namespace fl::platforms::teensy::sdfat
#endif  // CompileDateTime_h
