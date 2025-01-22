/*!
 *  @file       midi_Platform.h
 *  Project     Arduino MIDI Library
 *  @brief      MIDI Library for the Arduino - Platform
 *  @license    MIT - Copyright (c) 2015 Francois Best
 *  @author     lathoub
 *  @date       22/03/20
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "midi_Defs.h"

BEGIN_MIDI_NAMESPACE

#if ARDUINO

// DefaultPlatform is the Arduino Platform
struct DefaultPlatform
{
   static unsigned long now() { return ::millis(); };
};

#else

struct DefaultPlatform
{
   static unsigned long now() { return 0; };
};

#endif

END_MIDI_NAMESPACE
