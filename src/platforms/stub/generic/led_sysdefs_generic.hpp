#ifdef FASTLED_STUB_IMPL  // Only use this if explicitly defined.

#include "platforms/stub/led_sysdefs_stub.h"
#include "fl/unused.h"
#include "fl/compiler_control.h"


// No timing-related includes needed here anymore

void pinMode(uint8_t pin, uint8_t mode) {
    // Empty stub as we don't actually ever write anything
    FASTLED_UNUSED(pin);
    FASTLED_UNUSED(mode);
}

// Timing functions are now provided by time_stub.cpp
// This keeps only pinMode here for simplicity

#endif  // FASTLED_STUB_IMPL
