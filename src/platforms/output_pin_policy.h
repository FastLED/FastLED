#pragma once

// ok no namespace fl
// Provider-independent output legality dispatcher, not a GPIO implementation.
#include "platforms/is_platform.h"
#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM)
#include "platforms/esp/32/core/output_pin_policy.h" // IWYU pragma: keep
#endif
