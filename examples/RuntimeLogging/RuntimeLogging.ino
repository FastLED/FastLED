/**
 * @file RuntimeLogging.ino
 * @brief Example of using FastLED's runtime category-based logging system
 *
 * This sketch demonstrates:
 * - Enabling/disabling logging for specific subsystems at runtime
 * - Using FL_LOG_* macros for structured diagnostics
 * - Toggling logging via serial commands
 *
 * Build with: -DFASTLED_LOG_SPI_ENABLED (or -DFASTLED_LOG_ALL_ENABLED for all categories)
 */

#include "FastLED.h"

// Configuration
#define NUM_LEDS 60
#define LED_PIN 5

// LED array
CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println("\n\n=== FastLED Runtime Logging Example ===");
    Serial.println("Commands:");
    Serial.println("  's' = Toggle SPI logging");
    Serial.println("  'r' = Toggle RMT logging");
    Serial.println("  'v' = Toggle VIDEO logging");
    Serial.println("  'i' = Toggle I2S logging");
    Serial.println("  'l' = Toggle LCD logging");
    Serial.println("  'u' = Toggle UART logging");
    Serial.println("  't' = Toggle TIMING logging");
    Serial.println("  'a' = Enable ALL logging");
    Serial.println("  'x' = Disable ALL logging");
    Serial.println("  '?' = Show status\n");

    // Initialize FastLED
    // With -DFASTLED_LOG_SPI_ENABLED, you'll see initialization logs
    FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(100);

    // By default, all logging is DISABLED even when compiled in
    // Uncomment to start with some logging enabled:
    // fl::LogState::enable(fl::LogCategory::SPI);
    // fl::LogState::enable(fl::LogCategory::TIMING);

    Serial.println("Setup complete. Send commands via serial.\n");
}

void loop() {
    // Handle serial commands for toggling logging
    if (Serial.available()) {
        char cmd = Serial.read();
        handleLoggingCommand(cmd);
    }

    // Simple animation that will generate log messages
    // if logging is enabled and the driver uses FL_LOG_SPI
    animateLedsSlowly();

    FastLED.show();
    delay(10);
}

/**
 * Handle serial commands to toggle logging on/off
 */
void handleLoggingCommand(char cmd) {
    // Skip whitespace
    if (cmd == '\n' || cmd == '\r' || cmd == ' ') {
        return;
    }

    bool enabled = false;
    fl::LogCategory category;
    const char* name = nullptr;

    // Parse command
    switch (cmd) {
        case 's':
            category = fl::LogCategory::SPI;
            name = "SPI";
            break;
        case 'r':
            category = fl::LogCategory::RMT;
            name = "RMT";
            break;
        case 'v':
            category = fl::LogCategory::VIDEO;
            name = "VIDEO";
            break;
        case 'i':
            category = fl::LogCategory::I2S;
            name = "I2S";
            break;
        case 'l':
            category = fl::LogCategory::LCD;
            name = "LCD";
            break;
        case 'u':
            category = fl::LogCategory::UART;
            name = "UART";
            break;
        case 't':
            category = fl::LogCategory::TIMING;
            name = "TIMING";
            break;
        case 'a':
            fl::LogState::enableAll();
            Serial.println("\n>>> ALL logging ENABLED");
            return;
        case 'x':
            fl::LogState::disableAll();
            Serial.println("\n>>> ALL logging DISABLED");
            return;
        case '?':
            showLoggingStatus();
            return;
        default:
            Serial.print("Unknown command: ");
            Serial.println(cmd);
            return;
    }

    // Toggle the category
    if (fl::LogState::isEnabled(category)) {
        fl::LogState::disable(category);
        enabled = false;
    } else {
        fl::LogState::enable(category);
        enabled = true;
    }

    Serial.print("\n>>> ");
    Serial.print(name);
    Serial.print(" logging ");
    Serial.println(enabled ? "ENABLED" : "DISABLED");
}

/**
 * Show the current logging status
 */
void showLoggingStatus() {
    Serial.println("\n=== Logging Status ===");

    const char* categories[] = {"SPI", "RMT", "VIDEO", "I2S", "LCD", "UART", "TIMING"};
    fl::LogCategory cats[] = {
        fl::LogCategory::SPI,
        fl::LogCategory::RMT,
        fl::LogCategory::VIDEO,
        fl::LogCategory::I2S,
        fl::LogCategory::LCD,
        fl::LogCategory::UART,
        fl::LogCategory::TIMING
    };

    int enabledCount = 0;
    for (int i = 0; i < 7; i++) {
        Serial.print("  ");
        Serial.print(categories[i]);
        Serial.print(": ");

        if (fl::LogState::isEnabled(cats[i])) {
            Serial.println("ON");
            enabledCount++;
        } else {
            Serial.println("OFF");
        }
    }

    Serial.print("\nTotal enabled: ");
    Serial.print(enabledCount);
    Serial.println("/7\n");
}

/**
 * Simple LED animation
 * This generates SPI traffic which will be logged if SPI logging is enabled
 */
void animateLedsSlowly() {
    static uint8_t hue = 0;

    // Update every 50ms
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate < 50) {
        return;
    }
    lastUpdate = millis();

    // Simple hue rotation
    hue += 2;

    // Fill with gradient
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(hue + (i * 255 / NUM_LEDS), 200, 200);
    }

    // This generates log messages if FL_LOG_SPI is enabled and compiled in
    FL_LOG_SPI("Updating " << NUM_LEDS << " LEDs, hue=" << static_cast<int>(hue));
}

/**
 * Examples of using the logging system in your own code:
 *
 * In your custom driver:
 * ---
 * void myCustomDriver::begin(const Config& config) {
 *     FL_LOG_I2S("Initializing I2S driver");
 *     FL_LOG_I2S("  Buffer size: " << config.buffer_size);
 *     FL_LOG_I2S("  Sample rate: " << config.sample_rate);
 *     // ... initialization code ...
 * }
 *
 * void myCustomDriver::transmit(const uint8_t* data, size_t size) {
 *     FL_LOG_I2S("Transmitting " << size << " bytes");
 *     // ... transmission code ...
 * }
 *
 * Usage in sketch:
 * ---
 * void loop() {
 *     if (Serial.available() && Serial.read() == 'i') {
 *         if (fl::LogState::isEnabled(fl::LogCategory::I2S)) {
 *             fl::LogState::disable(fl::LogCategory::I2S);
 *             Serial.println("I2S logging OFF");
 *         } else {
 *             fl::LogState::enable(fl::LogCategory::I2S);
 *             Serial.println("I2S logging ON");
 *         }
 *     }
 * }
 */
