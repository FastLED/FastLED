// @filter: (platform is esp32)

// examples/Validation/Validation.ino
//
// FastLED LED Timing Validation Sketch for ESP32.
//
// This sketch validates LED output by reading back timing values using the
// RMT peripheral in receive mode. It performs TX→RX loopback testing to verify
// that transmitted LED data matches received data.
//
// DEMONSTRATES:
// 1. Runtime Channel API (FastLED.addChannel) for iterating through all available
//    drivers (RMT, SPI, PARLIO) and testing multiple chipset timings dynamically
//    by creating and destroying controllers for each driver.
// 2. Multi-channel validation support: Pass span<const ChannelConfig> to validate
//    multiple LED strips/channels simultaneously. Each channel is independently
//    validated with its own RX loopback channel.
//
// Use case: When developing a FastLED driver for a new peripheral, it is useful
// to read back the LED's received data to verify that the timing is correct.
//
// MULTI-CHANNEL MODE:
// - Single-channel: Pass one ChannelConfig - uses shared RX channel on PIN_RX
// - Multi-channel: Pass multiple ChannelConfigs - creates dynamic RX channels
//   on each TX pin for independent loopback validation
// - Each channel in the span is validated sequentially with its own RX channel
// - For RMT: Uses internal loopback (no jumper needed)
// - For SPI/PARLIO: Requires physical jumper from each TX pin to itself
//
// Hardware Setup:
//   ⚠️ IMPORTANT: Physical jumper wire required for non-RMT TX → RMT RX loopback
//
//   When non-RMT peripherals are used for TX (e.g., SPI, ParallelIO):
//   - Connect GPIO PIN_DATA to itself with a physical jumper wire
//   - Internal loopback (io_loop_back flag) only works for RMT TX → RMT RX
//   - ESP32 GPIO matrix cannot route other peripheral outputs internally to RMT input
//
//   When RMT is used for TX (lower peripheral priority or disable other peripherals):
//   - No jumper wire needed - io_loop_back works for RMT TX → RMT RX
//
//   Alternative: Connect an LED strip between TX pin and ground, then connect
//   RX pin to LED data line to capture actual LED protocol timing (requires
//   two separate GPIO pins for TX and RX).
//
// Platform Support:
//   - ESP32 (classic)
//   - ESP32-S3 (Xtensa)
//   - ESP32-C3 (RISC-V)
//   - ESP32-C6 (RISC-V)
//
// Expected Output:
//   Serial monitor will show:
//   - List of discovered drivers (RMT, SPI, PARLIO - availability depends on platform)
//   - Test results for each driver with PASS/FAIL status for each chipset timing
//   - Each driver is tested independently by creating/destroying channels
//   - In multi-channel mode: Separate validation results for each channel/pin
//
// MULTI-CHANNEL EXAMPLE:
//   ```cpp
//   // Create multiple LED arrays
//   CRGB leds1[100], leds2[100], leds3[100];
//
//   // Create channel configs for each strip on different pins
//   fl::ChannelConfig configs[] = {
//       fl::ChannelConfig(5, timing, fl::span<CRGB>(leds1, 100), RGB),
//       fl::ChannelConfig(6, timing, fl::span<CRGB>(leds2, 100), RGB),
//       fl::ChannelConfig(7, timing, fl::span<CRGB>(leds3, 100), RGB)
//   };
//
//   // Validate all 3 channels (each gets its own RX loopback)
//   validateChipsetTiming(timing, "WS2812", fl::span(configs, 3), nullptr, buffer);
//   ```
//

#include <FastLED.h>
#include "ValidationTest.h"

#ifndef PIN_DATA
#define PIN_DATA 5
#endif

#define PIN_RX 2

#define NUM_LEDS 255
#define CHIPSET WS2812B
#define COLOR_ORDER RGB  // No reordering needed.

CRGB leds[NUM_LEDS];
uint8_t rx_buffer[8192];  // 255 LEDs × 24 bits/LED = 6120 symbols, use 8192 for headroom

// RMT RX channel (persistent across loop iterations)
fl::shared_ptr<fl::RmtRxChannel> rx_channel;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    FL_WARN("\n=== FastLED RMT RX Validation Sketch ===");
    FL_WARN("Hardware: ESP32 (any variant)");
    FL_WARN("TX Pin (MOSI): GPIO " << PIN_DATA << " (ESP32-S3: use GPIO 11 for IO_MUX)");
    FL_WARN("RX Pin: GPIO " << PIN_RX);
    FL_WARN("");
    FL_WARN("⚠️  HARDWARE SETUP REQUIRED:");
    FL_WARN("   If using non-RMT peripherals for TX (e.g., SPI, ParallelIO):");
    FL_WARN("   → Connect GPIO " << PIN_DATA << " to GPIO " << PIN_RX << " with a physical jumper wire");
    FL_WARN("   → Internal loopback (io_loop_back) only works for RMT TX → RMT RX");
    FL_WARN("   → ESP32 GPIO matrix cannot route other peripheral outputs to RMT input");
    FL_WARN("");
    FL_WARN("   ESP32-S3 IMPORTANT: Use GPIO 11 (MOSI) for best performance");
    FL_WARN("   → GPIO 11 is SPI2 IO_MUX pin (bypasses GPIO matrix for 80MHz speed)");
    FL_WARN("   → Other GPIOs use GPIO matrix routing (limited to 26MHz, may see timing issues)");
    FL_WARN("");

    // Create RMT RX channel FIRST (before FastLED init to avoid pin conflicts)
    // Buffer size: 255 LEDs × 24 bits/LED = 6120 symbols, use 8192 for headroom
    rx_channel = fl::RmtRxChannel::create(PIN_RX, 40000000, 8192);
    if (!rx_channel) {
        FL_WARN("ERROR: Failed to create RX channel - validation tests will fail");
        FL_WARN("       Check that RMT peripheral is available and not in use");
        return;
    }

    // Note: Toggle test removed - validated in Iteration 5 that RMT RX works correctly
    // Skipping toggle test to avoid GPIO ownership conflicts with SPI MOSI

    FL_WARN("\nStarting continuous validation test loop...");
}

void loop() {
    // Run all validation tests continuously until timeout
    runAllValidationTests(PIN_DATA, PIN_RX, fl::span<CRGB>(leds, NUM_LEDS),
                         rx_channel, fl::span<uint8_t>(rx_buffer, 8192), COLOR_ORDER);

    FL_WARN("\n========== TEST ITERATION COMPLETE - RESTARTING ==========\n");
    delay(1000);
}
