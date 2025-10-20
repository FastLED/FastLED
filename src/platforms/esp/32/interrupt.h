#pragma once

/// ESP32-specific interrupt control interface
/// Minimal bindings to FreeRTOS portDISABLE_INTERRUPTS/portENABLE_INTERRUPTS

namespace fl {

/// Disable interrupts on ESP32
void noInterrupts();

/// Enable interrupts on ESP32
void interrupts();

}  // namespace fl
