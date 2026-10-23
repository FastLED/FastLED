#pragma once

// IWYU pragma: private

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace esp32 {

constexpr u32 kPinRoutingUnknown = 0xFFFFFFFFu;

/// @brief Snapshot of how @p pin is routed: output signal, output enable and
/// IOMUX function, or kPinRoutingUnknown when the IDF cannot report it
///
/// A driver that keeps an idle peripheral bound to a pin can lose that pin to
/// another driver or to pinMode() (FastLED#4905, #4907). Comparing the
/// snapshot taken when the driver claimed the pin with a fresh one detects
/// that without knowing which signal the driver routes.
///
/// Uses gpio_get_io_config(), available from IDF 5.5 (Arduino-ESP32 3.3).
u32 pinRoutingSnapshot(int pin) FL_NO_EXCEPT;

} // namespace esp32
} // namespace fl
