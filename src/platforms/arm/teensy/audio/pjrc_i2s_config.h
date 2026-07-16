#pragma once

// Private FastLED interface for the Teensy 4.x I2S clock and frame registers.
// IWYU pragma: private

namespace fl {
namespace platforms {
namespace teensy {

void config_i2s1();
void config_i2s2();

} // namespace teensy
} // namespace platforms
} // namespace fl
