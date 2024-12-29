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
#ifndef CompileDateTime_h
#define CompileDateTime_h
#include <stdint.h>
// Note - these functions will compile to a few bytes
//        since they are evaluated at compile time.

/** \return year field of the __DATE__ macro. */
constexpr uint16_t compileYear() {
  return 1000 * (__DATE__[7] - '0') + 100 * (__DATE__[8] - '0') +
         10 * (__DATE__[9] - '0') + (__DATE__[10] - '0');
}
/** \return month field of the __DATE__ macro. */
constexpr uint8_t compileMonth() {
  return __DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n'   ? 1
         : __DATE__[0] == 'F' && __DATE__[1] == 'e' && __DATE__[2] == 'b' ? 2
         : __DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r' ? 3
         : __DATE__[0] == 'A' && __DATE__[1] == 'p' && __DATE__[2] == 'r' ? 4
         : __DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y' ? 5
         : __DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n' ? 6
         : __DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l' ? 7
         : __DATE__[0] == 'A' && __DATE__[1] == 'u' && __DATE__[2] == 'g' ? 8
         : __DATE__[0] == 'S' && __DATE__[1] == 'e' && __DATE__[2] == 'p' ? 9
         : __DATE__[0] == 'O' && __DATE__[1] == 'c' && __DATE__[2] == 't' ? 10
         : __DATE__[0] == 'N' && __DATE__[1] == 'o' && __DATE__[2] == 'v' ? 11
         : __DATE__[0] == 'D' && __DATE__[1] == 'e' && __DATE__[2] == 'c' ? 12
                                                                          : 0;
}
/** \return day field of the __DATE__ macro. */
constexpr uint8_t compileDay() {
  return 10 * ((__DATE__[4] == ' ' ? '0' : __DATE__[4]) - '0') +
         (__DATE__[5] - '0');
}
/** \return hour field of the __TIME__ macro. */
constexpr uint8_t compileHour() {
  return 10 * (__TIME__[0] - '0') + __TIME__[1] - '0';
}
/** \return minute field of the __TIME__ macro. */
constexpr uint8_t compileMinute() {
  return 10 * (__TIME__[3] - '0') + __TIME__[4] - '0';
}
/** \return second field of the __TIME__ macro. */
constexpr uint8_t compileSecond() {
  return 10 * (__TIME__[6] - '0') + __TIME__[7] - '0';
}
#endif  // CompileDateTime_h
