// ok no namespace fl
#pragma once

// ARM fallback placement new operator - in global namespace
// For ARM platforms without <new> header

#include "fl/stl/stdint.h"
#include "fl/int.h"

inline void *operator new(fl::size, void *ptr) noexcept { return ptr; }
