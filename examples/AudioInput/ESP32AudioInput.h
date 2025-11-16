// ESP32 I2S Audio Input Implementation
// This example uses the INMP441 I2S MEMS microphone (popular as of 2025)

#pragma once

#include "fl/audio_input.h"

// ============================================================================
// WIRING FOR INMP441 MICROPHONE → ESP32
// ============================================================================
// INMP441 Pin  → ESP32 Pin
// SCK (BCLK)   → GPIO 4  (I2S Bit Clock)
// WS (LRCLK)   → GPIO 7  (I2S Word Select)
// SD (Data)    → GPIO 8  (I2S Data In)
// L/R Select   → 3.3V (Right channel - recommended)
// VDD          → 3.3V
// GND          → GND
//
// Notes:
//   - Connect L/R to 3.3V so it's recognized as a right channel microphone
//   - Adjust pin assignments below if needed for your board
//
// ============================================================================

// I2S Configuration for INMP441 microphone
#define I2S_WS_PIN 7  // Word Select (LRCLK)
#define I2S_SD_PIN 8  // Serial Data (DIN)
#define I2S_CLK_PIN 4 // Serial Clock (BCLK)
#define I2S_CHANNEL fl::Right

// Platform-specific initialization delay (ESP32 needs longer startup time)
#define PLATFORM_INIT_DELAY_MS 5000

// Platform name for serial output
#define PLATFORM_NAME "ESP32 I2S Audio FastLED Example"

// Create platform-specific AudioConfig
inline fl::AudioConfig createAudioConfig() {
    return fl::AudioConfig::CreateInmp441(I2S_WS_PIN, I2S_SD_PIN, I2S_CLK_PIN, I2S_CHANNEL);
}

// Print platform-specific setup information
inline void printPlatformInfo() {
    Serial.println("ESP32 Configuration:");
    Serial.print("  BCLK Pin: ");
    Serial.println(I2S_CLK_PIN);
    Serial.print("  LRCLK Pin: ");
    Serial.println(I2S_WS_PIN);
    Serial.print("  Data Pin: ");
    Serial.println(I2S_SD_PIN);
    Serial.print("  Channel: ");
    Serial.println(I2S_CHANNEL == fl::Right ? "Right" : "Left");
}
