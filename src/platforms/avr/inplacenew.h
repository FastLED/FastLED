// ok no namespace fl
#pragma once

// AVR-specific placement new operator - in global namespace
// AVR doesn't have <new> header, so we define it manually

#include "fl/stl/stdint.h"
#include "fl/int.h"

inline void *operator new(fl::size, void *ptr) noexcept { return ptr; }
