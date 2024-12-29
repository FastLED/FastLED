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
#ifndef DebugMacros_h
#define DebugMacros_h
#include "SysCall.h"

// 0 - disable, 1 - fail, halt 2 - fail, halt, warn
#define USE_DBG_MACROS 0

#if USE_DBG_MACROS
#include "Arduino.h"
#ifndef DBG_FILE
#error DBG_FILE not defined
#endif  // DBG_FILE

__attribute__((unused)) static void dbgFail(uint16_t line) {
  Serial.print(F("DBG_FAIL: "));
  Serial.print(F(DBG_FILE));
  Serial.write('.');
  Serial.println(line);
}
__attribute__((unused)) static void dbgHalt(uint16_t line) {
  Serial.print(F("DBG_HALT: "));
  Serial.print(F(DBG_FILE));
  Serial.write('.');
  Serial.println(line);
  while (true) {
  }
}
#define DBG_FAIL_MACRO dbgFail(__LINE__)
#define DBG_HALT_MACRO dbgHalt(__LINE__)
#define DBG_HALT_IF(b) \
  if (b) {             \
    dbgHalt(__LINE__); \
  }

#else  // USE_DBG_MACROS
#define DBG_FAIL_MACRO
#define DBG_HALT_MACRO
#define DBG_HALT_IF(b)
#endif  // USE_DBG_MACROS

#if USE_DBG_MACROS > 1
__attribute__((unused)) static void dbgWarn(uint16_t line) {
  Serial.print(F("DBG_WARN: "));
  Serial.print(F(DBG_FILE));
  Serial.write('.');
  Serial.println(line);
}
#define DBG_WARN_MACRO dbgWarn(__LINE__)
#define DBG_WARN_IF(b) \
  if (b) {             \
    dbgWarn(__LINE__); \
  }
#else  // USE_DBG_MACROS > 1
#define DBG_WARN_MACRO
#define DBG_WARN_IF(b)
#endif  // USE_DBG_MACROS > 1
#endif  // DebugMacros_h
