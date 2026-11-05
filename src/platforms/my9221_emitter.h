#pragma once

// ok no namespace fl
// Private placement dispatcher; all includes precede namespace definitions.
#include "platforms/is_platform.h"
#include "platforms/shared/my9221_typed_emitter.h" // IWYU pragma: keep
#if defined(FL_IS_ESP8266) && !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM)
#include "platforms/esp/8266/my9221_emitter.h" // IWYU pragma: keep
#endif
