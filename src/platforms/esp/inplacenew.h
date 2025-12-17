// ok no namespace fl
#pragma once

// ESP32/ESP8266 fallback placement new operator - in global namespace
// For platforms without <new> header

#include "fl/stl/stdint.h"
#include "fl/int.h"

inline void *operator new(fl::size, void *ptr) noexcept { return ptr; }
