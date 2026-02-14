// ok no namespace fl
#pragma once

// IWYU pragma: private

// AVR placement new operator - in global namespace
// AVR doesn't have <new> header, needs manual definition

#include "fl/stl/stdint.h"
#include "fl/int.h"

inline void *operator new(fl::size, void *ptr) noexcept { return ptr; }
