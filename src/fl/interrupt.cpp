/// Generic interrupt control implementation
/// Dispatcher to platform-specific implementations

#include "platforms/interrupt.h"

// Platform-specific implementations are in:
// - platforms/esp/32/interrupt.cpp (ESP32)
// - platforms/avr/interrupt.cpp (AVR)
// - platforms/arm/interrupt.cpp (ARM Cortex-M)
// - platforms/wasm/interrupt.cpp (WebAssembly)
// - platforms/shared/interrupt.cpp (Desktop/Generic)
