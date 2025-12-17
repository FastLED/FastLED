// ok no namespace fl
#pragma once

// Shared/fallback placement new operator - in global namespace
// For platforms without <new> header or __has_include support

#include "fl/stl/stdint.h"
#include "fl/int.h"

inline void *operator new(fl::size, void *ptr) noexcept { return ptr; }
