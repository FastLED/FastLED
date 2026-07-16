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
#include "platforms/arm/teensy/sdfat/SpiDriver/SdSpiDriver.h"
#include "fl/stl/compiler_control.h"
namespace fl { namespace platforms { namespace teensy { namespace sdfat {

#if ENABLE_ARDUINO_FEATURES
#if SD_CHIP_SELECT_MODE == 0
//------------------------------------------------------------------------------
void sdCsInit(SdCsPin_t pin) {
  pinMode(pin, OUTPUT);
}
//------------------------------------------------------------------------------
void sdCsWrite(SdCsPin_t pin, bool level) {
  digitalWrite(pin, level);
}
#elif SD_CHIP_SELECT_MODE == 1
//------------------------------------------------------------------------------
FL_LINK_WEAK void sdCsInit(SdCsPin_t pin) {
  pinMode(pin, OUTPUT);
}
//------------------------------------------------------------------------------
FL_LINK_WEAK void sdCsWrite(SdCsPin_t pin, bool level) {
  digitalWrite(pin, level);
}
#endif  // SD_CHIP_SELECT_MODE == 0
#endif  // ENABLE_ARDUINO_FEATURES
} } } }  // namespace fl::platforms::teensy::sdfat
