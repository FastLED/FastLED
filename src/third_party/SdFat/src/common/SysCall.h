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
/**
 * \file
 * \brief SysCall class
 */
#ifndef SysCall_h
#define SysCall_h
#include <stddef.h>
#include <stdint.h>

#include "../SdFatConfig.h"
#if __cplusplus < 201103
#warning nullptr defined
/** Define nullptr if not C++11 */
#define nullptr NULL
#endif  // __cplusplus < 201103
//------------------------------------------------------------------------------
#if ENABLE_ARDUINO_FEATURES
#if defined(ARDUINO)
/** Use Arduino Print. */
typedef Print print_t;
/** Use Arduino Stream. */
typedef Stream stream_t;
#else  // defined(ARDUINO)
#error "Unknown system"
#endif  // defined(ARDUINO)
//------------------------------------------------------------------------------
#ifndef F
/** Define macro for strings stored in flash. */
#define F(str) (str)
#endif  // F
//------------------------------------------------------------------------------
#else  // ENABLE_ARDUINO_FEATURES
#include "PrintBasic.h"
/** If not Arduino */
typedef PrintBasic print_t;
/** If not Arduino */
typedef PrintBasic stream_t;
#endif  // ENABLE_ARDUINO_FEATURES
#endif  // SysCall_h
