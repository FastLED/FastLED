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
#define FREE_STACK_CPP
#include "FreeStack.h"
#if defined(HAS_UNUSED_STACK) && HAS_UNUSED_STACK
//------------------------------------------------------------------------------
inline char* stackBegin() {
#if defined(__AVR__)
  return __brkval ? __brkval : &__bss_end;
#elif defined(__IMXRT1062__)
  return reinterpret_cast<char*>(&_ebss);
#elif defined(__arm__)
  return reinterpret_cast<char*>(sbrk(0));
#else  // defined(__AVR__)
#error "undefined stackBegin"
#endif  // defined(__AVR__)
}
//------------------------------------------------------------------------------
inline char* stackPointer() {
#if defined(__AVR__)
  return reinterpret_cast<char*>(SP);
#elif defined(__arm__)
  register uint32_t sp asm("sp");
  return reinterpret_cast<char*>(sp);
#else  // defined(__AVR__)
#error "undefined stackPointer"
#endif  // defined(__AVR__)
}
//------------------------------------------------------------------------------
/** Stack fill pattern. */
const char FILL = 0x55;
void FillStack() {
  char* p = stackBegin();
  char* top = stackPointer();
  while (p < top) {
    *p++ = FILL;
  }
}
//------------------------------------------------------------------------------
// May fail if malloc or new is used.
int UnusedStack() {
  char* h = stackBegin();
  char* top = stackPointer();
  int n;

  for (n = 0; (h + n) < top; n++) {
    if (h[n] != FILL) {
      if (n >= 16) {
        break;
      }
      // Attempt to skip used heap.
      h += n;
      n = 0;
    }
  }
  return n;
}
#endif  // defined(HAS_UNUSED_STACK) && HAS_UNUSED_STACK
