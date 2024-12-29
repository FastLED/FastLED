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
#ifndef FsName_h
#define FsName_h
#include <stdint.h>

#include "SysCall.h"
/**
 * \file
 * \brief FsName class.
 */
/**
 * \class FsName
 * \brief Handle UTF-8 file names.
 */
class FsName {
 public:
  /** Beginning of LFN. */
  const char* begin;
  /** Next LFN character of end. */
  const char* next;
  /** Position one beyond last LFN character. */
  const char* end;
#if !USE_UTF8_LONG_NAMES
  /** \return true if at end. */
  bool atEnd() { return next == end; }
  /** Reset to start of LFN. */
  void reset() { next = begin; }
  /** \return next char of LFN. */
  char getch() { return atEnd() ? 0 : *next++; }
  /** \return next UTF-16 unit of LFN. */
  uint16_t get16() { return atEnd() ? 0 : *next++; }
#else   // !USE_UTF8_LONG_NAMES
  uint16_t ls = 0;
  bool atEnd() { return !ls && next == end; }
  void reset() {
    next = begin;
    ls = 0;  // lowSurrogate
  }
  uint16_t get16();
#endif  // !USE_UTF8_LONG_NAMES
};
#endif  // FsName_h
