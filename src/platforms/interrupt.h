// ok no namespace fl
#pragma once

/// Platform-specific interrupt control implementation
/// This file dispatches to the appropriate platform-specific implementation

#if defined(ESP32)
    #include "platforms/esp/32/interrupt.h"
#elif defined(__AVR__)
    #include "platforms/avr/interrupt.h"
#elif defined(FASTLED_ARM)
    #include "platforms/arm/interrupt.h"
#elif defined(__EMSCRIPTEN__)
    #include "platforms/wasm/interrupt.h"
#else
    // Default platform (desktop/generic)
    #include "platforms/shared/interrupt.h"
#endif
