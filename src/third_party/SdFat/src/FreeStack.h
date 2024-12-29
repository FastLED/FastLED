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
#ifndef FreeStack_h
#define FreeStack_h
/**
 * \file
 * \brief FreeStack() function.
 */
#include <stdint.h>
#if defined(__AVR__) || defined(DOXYGEN)
#include <avr/io.h>
/** Indicate FillStack() and UnusedStack() are available. */
#define HAS_UNUSED_STACK 1
/** boundary between stack and heap. */
extern char* __brkval;
/** End of bss section.*/
extern char __bss_end;
/** Amount of free stack space.
 * \return The number of free bytes.
 */
inline int FreeStack() {
  char* sp = reinterpret_cast<char*>(SP);
  return __brkval ? sp - __brkval : sp - &__bss_end;
}
#elif defined(ARDUINO_ARCH_APOLLO3)
#define HAS_UNUSED_STACK 0
#elif defined(PLATFORM_ID)  // Particle board
#include "Arduino.h"
inline int FreeStack() { return System.freeMemory(); }
#elif defined(__IMXRT1062__)
#define HAS_UNUSED_STACK 1
extern uint8_t _ebss;
inline int FreeStack() {
  register uint32_t sp asm("sp");
  return reinterpret_cast<char*>(sp) - reinterpret_cast<char*>(&_ebss);
}
#elif defined(__arm__)
#define HAS_UNUSED_STACK 1
extern "C" char* sbrk(int incr);
inline int FreeStack() {
  register uint32_t sp asm("sp");
  return reinterpret_cast<char*>(sp) - reinterpret_cast<char*>(sbrk(0));
}
#else  // defined(__AVR__) || defined(DOXYGEN)
#ifndef FREE_STACK_CPP
#warning FreeStack is not defined for this system.
#endif  // FREE_STACK_CPP
inline int FreeStack() { return 0; }
#endif  // defined(__AVR__) || defined(DOXYGEN)
#if defined(HAS_UNUSED_STACK) || defined(DOXYGEN)
/** Fill stack with 0x55 pattern */
void FillStack();
/**
 * Determine the amount of unused stack.
 *
 * FillStack() must be called to fill the stack with a 0x55 pattern.
 *
 * UnusedStack() may fail if malloc() or new is use.
 *
 * \return number of bytes with 0x55 pattern.
 */
int UnusedStack();
#else  // HAS_UNUSED_STACK
#define HAS_UNUSED_STACK 0
inline void FillStack() {}
inline int UnusedStack() { return 0; }
#endif  // defined(HAS_UNUSED_STACK)
#endif  // FreeStack_h
