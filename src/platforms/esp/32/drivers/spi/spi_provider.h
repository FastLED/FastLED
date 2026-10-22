#pragma once

// IWYU pragma: private

#include "platforms/shared/spi_hw_1.h"

namespace fl {
namespace platforms {

// The native implementation supplies a weak default factory. An explicitly
// linked application can replace it with an Arduino-backed SpiHw1, selecting
// the complete lifecycle before any native bus acquisition. Keep this
// declaration strong so application definitions do not inherit weak linkage.
// Bus numbers are logical ESP32 hosts 2 and 3. Null makes that host unavailable;
// it does not authorize a native fallback to take an application's bus.
fl::shared_ptr<SpiHw1> fastled_spi_provider(u8 bus) FL_NO_EXCEPT;

} // namespace platforms
} // namespace fl
