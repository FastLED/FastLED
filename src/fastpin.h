/// @file fastpin.h
/// Backward compatibility header - use fl/fastpin.h directly
///
/// Legacy header. Prefer to use fl/fastpin.h instead.

#pragma once

#ifndef __INC_FASTPIN_H
#define __INC_FASTPIN_H

#include "fl/fastpin.h"

// Backward compatibility: bring fl:: fastpin classes into global namespace
using fl::Selectable;
using fl::Pin;
using fl::OutputPin;
using fl::InputPin;
// FastPin and FastPinBB are template classes, so we use template aliases
template<fl::u8 PIN> using FastPin = fl::FastPin<PIN>;
template<fl::u8 PIN> using FastPinBB = fl::FastPinBB<PIN>;
using fl::__FL_PORT_INFO;
using fl::reg32_t;
using fl::ptr_reg32_t;

#endif // __INC_FASTPIN_H
