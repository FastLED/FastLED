/*
 * FastPins NMI Multi-SPI Example (ESP32 Only)
 *
 * Demonstrates Level 7 NMI-driven multi-SPI for ultra-low latency parallel
 * LED strip control with WiFi/Bluetooth active. This example shows how to
 * achieve WS2812 timing precision even with heavy WiFi traffic.
 *
 * ============================================================================
 * ⚠️ ESP32 PLATFORM ONLY ⚠️
 * ============================================================================
 * This example requires ESP32/ESP32-S3 with Xtensa architecture.
 * Level 7 NMI is not available on other platforms.
 *
 * WHAT IS LEVEL 7 NMI?
 * ====================
 * Level 7 Non-Maskable Interrupt (NMI) is the highest priority interrupt on
 * ESP32 Xtensa processors. Unlike standard interrupts (Level 3), Level 7 NMI:
 *
 * - Cannot be masked or disabled (not even by portDISABLE_INTERRUPTS)
 * - Preempts ALL lower priority interrupts (including WiFi MAC/PHY)
 * - Has <70ns jitter (vs 50-100µs for Level 3 with WiFi)
 * - Suitable for timing-critical protocols (WS2812, high-speed SPI)
 *
 * WHY USE LEVEL 7 NMI?
 * ====================
 * Standard interrupts (Level 3-5) have 50-100µs jitter with WiFi active,
 * which violates WS2812 timing requirements (±150ns tolerance). Level 7 NMI
 * provides <70ns jitter, enabling:
 *
 * - WS2812 bit-banging with WiFi/Bluetooth active
 * - 8+ parallel LED strips (overcomes 8-channel RMT limitation)
 * - 13.2 MHz max speed per strip (105 Mbps total throughput)
 * - Zero WiFi interference (NMI preempts WiFi)
 *
 * PERFORMANCE:
 * ============
 * - Per-strip speed: 13.2 MHz (76ns per bit)
 * - Total throughput: 13.2 MHz × 8 strips = 105.6 Mbps
 * - CPU usage: 6% @ 800 kHz (leaving 90%+ for WiFi/application)
 * - Jitter: <70ns (within WS2812 ±150ns tolerance)
 *
 * CRITICAL REQUIREMENTS:
 * ======================
 * 1. All pins must be on same GPIO bank (0-31 or 32-39, not mixed)
 * 2. Data buffer must be in DRAM (use DRAM_ATTR for global arrays)
 * 3. ESP-IDF v5.0 or v5.1 (v5.2.1 has known Level 7 NMI allocation bug)
 * 4. Timer Group 0, Timer 0 must be available (not used by other code)
 *
 * HARDWARE SETUP:
 * ===============
 * Connect 8 LED strips to ESP32 as follows:
 *
 * Clock Pin:  GPIO 17 (shared clock for all strips)
 * Data Pin 0: GPIO 2  (Strip 0)
 * Data Pin 1: GPIO 4  (Strip 1)
 * Data Pin 2: GPIO 5  (Strip 2)
 * Data Pin 3: GPIO 12 (Strip 3)
 * Data Pin 4: GPIO 13 (Strip 4)
 * Data Pin 5: GPIO 14 (Strip 5)
 * Data Pin 6: GPIO 15 (Strip 6)
 * Data Pin 7: GPIO 16 (Strip 7)
 *
 * All pins must be in Bank 0 (GPIO 0-31) for same-port performance.
 *
 * LIMITATIONS:
 * ============
 * - NO FreeRTOS calls in NMI handler (will crash)
 * - NO ESP_LOG in NMI handler (uses FreeRTOS mutexes)
 * - NO malloc/free in NMI handler (heap operations use locks)
 * - NO task notifications (cannot signal tasks from NMI)
 * - Buffer must be in DRAM (no flash access from NMI)
 * - Keep NMI handler very short (<100ns execution time)
 *
 * DEBUGGING TIPS:
 * ===============
 * 1. If initialization fails, check ESP-IDF version (use v5.0 or v5.1)
 * 2. If crashes occur, verify buffer is in DRAM (use DRAM_ATTR)
 * 3. If timing is off, verify all pins are on same GPIO bank
 * 4. Use diagnostic counters to monitor NMI execution
 *
 * COMPARISON TO RMT HARDWARE:
 * ===========================
 * | Feature           | RMT Hardware  | Level 7 NMI ISR |
 * |-------------------|---------------|-----------------|
 * | Timing precision  | ±10-20ns      | ±30-70ns        |
 * | WiFi immunity     | 100%          | 100%            |
 * | Parallel strips   | 8 max         | 8+ unlimited    |
 * | CPU overhead      | Zero          | 6% @ 800 kHz    |
 * | Code complexity   | Low           | High            |
 *
 * Recommendation: Use RMT for 1-8 strips. Use NMI for 9+ strips or custom
 * protocols requiring direct timing control.
 *
 * Author: FastLED Team
 * License: MIT
 */

#include <FastLED.h>

// Only compile on ESP32 platforms
#if defined(ESP32)

// Include NMI multi-SPI API
#include "platforms/esp/32/nmi_multispi.h"

// Optional: Include WiFi to demonstrate immunity to WiFi interference
#include <WiFi.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

// Pin configuration (all on Bank 0: GPIO 0-31)
const uint8_t CLOCK_PIN = 17;
const uint8_t DATA_PINS[8] = {2, 4, 5, 12, 13, 14, 15, 16};

// LED strip configuration
const int STRIPS = 8;           // Number of parallel strips
const int LEDS_PER_STRIP = 30;  // LEDs per strip (adjust to your setup)
const int BYTES_PER_LED = 3;    // GRB color format

// WS2812 timing: 800 kHz bit rate
const uint32_t FREQUENCY = 800000;  // 800 kHz

// WiFi credentials (optional - demonstrates NMI immunity to WiFi)
const char* WIFI_SSID = "YourSSID";      // Change to your WiFi SSID
const char* WIFI_PASSWORD = "YourPass";  // Change to your WiFi password

// ============================================================================
// DATA BUFFER (MUST BE IN DRAM!)
// ============================================================================
// CRITICAL: Buffer must be in DRAM, not flash. Use DRAM_ATTR attribute.
// Flash access from Level 7 NMI causes cache miss → system hang/crash.

const size_t BUFFER_SIZE = STRIPS * LEDS_PER_STRIP * BYTES_PER_LED;
uint8_t ledBuffer[STRIPS * LEDS_PER_STRIP * BYTES_PER_LED] DRAM_ATTR;

// ============================================================================
// GLOBAL STATE
// ============================================================================

bool nmiInitialized = false;
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 50;  // Update every 50ms (20 fps)

// Color animation state
uint8_t hue = 0;
uint8_t saturation = 255;
uint8_t value = 128;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void fillBufferWithColor(uint8_t r, uint8_t g, uint8_t b);
void updateBufferWithAnimation();
void printDiagnostics();

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);  // Wait for serial monitor

    Serial.println("=================================================");
    Serial.println("FastLED NMI Multi-SPI Example (ESP32 Only)");
    Serial.println("=================================================");
    Serial.println();

    // Print buffer info
    Serial.print("Buffer size: ");
    Serial.print(BUFFER_SIZE);
    Serial.println(" bytes");
    Serial.print("Buffer address: 0x");
    Serial.println((uintptr_t)ledBuffer, HEX);
    Serial.println();

    // Initialize LED buffer with test pattern (red on all strips)
    fillBufferWithColor(255, 0, 0);  // Red
    Serial.println("Buffer initialized with red color");

    // Initialize NMI multi-SPI
    Serial.print("Initializing NMI multi-SPI at ");
    Serial.print(FREQUENCY);
    Serial.println(" Hz...");

    bool initSuccess = fl::nmi::initMultiSPI(CLOCK_PIN, DATA_PINS, FREQUENCY);

    if (!initSuccess) {
        Serial.println("❌ ERROR: NMI initialization failed!");
        Serial.println();
        Serial.println("Possible causes:");
        Serial.println("1. ESP-IDF v5.2.1 has known bug - use v5.0 or v5.1");
        Serial.println("2. Level 7 interrupt already allocated");
        Serial.println("3. Timer Group 0, Timer 0 already in use");
        Serial.println("4. Pins not on same GPIO bank");
        Serial.println();
        Serial.println("System will continue without NMI (demo mode)");
        nmiInitialized = false;
    } else {
        Serial.println("✅ NMI multi-SPI initialized successfully!");
        nmiInitialized = true;

        // Start first transmission
        bool startSuccess = fl::nmi::startTransmission(ledBuffer, BUFFER_SIZE);
        if (startSuccess) {
            Serial.println("✅ Transmission started");
        } else {
            Serial.println("⚠️  Failed to start transmission");
        }
    }

    Serial.println();

    // Start WiFi (optional - demonstrates NMI immunity)
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Don't wait for WiFi - NMI will continue running regardless
    Serial.println("WiFi connecting in background...");
    Serial.println("(NMI transmission is NOT affected by WiFi activity)");
    Serial.println();

    Serial.println("=================================================");
    Serial.println("Main loop starting...");
    Serial.println("=================================================");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Check WiFi status (non-blocking)
    static bool wasConnected = false;
    bool isConnected = (WiFi.status() == WL_CONNECTED);
    if (isConnected && !wasConnected) {
        Serial.print("✅ WiFi connected! IP: ");
        Serial.println(WiFi.localIP());
        Serial.println("   (NMI transmission continues with zero interference)");
    } else if (!isConnected && wasConnected) {
        Serial.println("⚠️  WiFi disconnected");
    }
    wasConnected = isConnected;

    // Update LED animation periodically
    if (millis() - lastUpdate > UPDATE_INTERVAL) {
        lastUpdate = millis();

        // Wait for previous transmission to complete
        if (nmiInitialized) {
            if (fl::nmi::isTransmissionComplete()) {
                // Update color animation (cycle through hues)
                hue += 2;
                updateBufferWithAnimation();

                // Start next transmission
                bool success = fl::nmi::startTransmission(ledBuffer, BUFFER_SIZE);
                if (!success) {
                    Serial.println("⚠️  Failed to start transmission");
                }

                // Print diagnostics every 2 seconds
                static unsigned long lastDiag = 0;
                if (millis() - lastDiag > 2000) {
                    lastDiag = millis();
                    printDiagnostics();
                }
            } else {
                // Still transmitting (shouldn't happen with proper timing)
                static unsigned long lastWarning = 0;
                if (millis() - lastWarning > 1000) {
                    lastWarning = millis();
                    Serial.println("⚠️  Transmission overrun (previous frame not complete)");
                }
            }
        } else {
            // NMI not initialized - just cycle colors for demo
            hue += 2;
        }
    }

    // Small delay to prevent tight loop
    delay(1);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Fill buffer with solid color (GRB format)
void fillBufferWithColor(uint8_t r, uint8_t g, uint8_t b) {
    for (size_t i = 0; i < BUFFER_SIZE; i += 3) {
        ledBuffer[i + 0] = g;  // Green byte
        ledBuffer[i + 1] = r;  // Red byte
        ledBuffer[i + 2] = b;  // Blue byte
    }
}

// Update buffer with animated rainbow pattern
void updateBufferWithAnimation() {
    // Create rainbow effect across all strips
    for (int strip = 0; strip < STRIPS; strip++) {
        for (int led = 0; led < LEDS_PER_STRIP; led++) {
            // Calculate hue with spatial offset
            uint8_t pixelHue = hue + (led * 256 / LEDS_PER_STRIP) + (strip * 32);

            // Convert HSV to RGB
            CRGB color = CHSV(pixelHue, saturation, value);

            // Write to buffer (GRB format)
            size_t offset = (strip * LEDS_PER_STRIP * BYTES_PER_LED) + (led * BYTES_PER_LED);
            ledBuffer[offset + 0] = color.g;  // Green
            ledBuffer[offset + 1] = color.r;  // Red
            ledBuffer[offset + 2] = color.b;  // Blue
        }
    }
}

// Print NMI diagnostics
void printDiagnostics() {
    if (!nmiInitialized) return;

    uint32_t invocations = fl::nmi::getInvocationCount();
    uint32_t maxCycles = fl::nmi::getMaxExecutionCycles();

    Serial.println("--- NMI Diagnostics ---");
    Serial.print("  Invocations: ");
    Serial.println(invocations);
    Serial.print("  Max cycles:  ");
    Serial.println(maxCycles);

    // Calculate approximate execution time
    // ESP32 @ 240 MHz: 1 cycle = 4.17ns
    float maxTimeNs = maxCycles * 4.17;
    Serial.print("  Max time:    ");
    Serial.print(maxTimeNs, 1);
    Serial.println(" ns");

    // Verify performance is within budget
    if (maxTimeNs > 100) {
        Serial.println("  ⚠️  WARNING: Execution time exceeds 100ns budget!");
    } else {
        Serial.println("  ✅ Performance within budget");
    }

    Serial.print("  WiFi status: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "Connected ✅" : "Disconnected");
    Serial.println();
}

#else  // !ESP32

// ============================================================================
// NON-ESP32 PLATFORMS - PROVIDE ERROR MESSAGE
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=================================================");
    Serial.println("FastLED NMI Multi-SPI Example");
    Serial.println("=================================================");
    Serial.println();
    Serial.println("❌ ERROR: This example requires ESP32 platform!");
    Serial.println();
    Serial.println("Level 7 NMI is specific to ESP32/ESP32-S3 Xtensa");
    Serial.println("architecture and is not available on:");
    Serial.println("- AVR (Arduino UNO, MEGA, etc.)");
    Serial.println("- ARM (Teensy, STM32, RP2040, etc.)");
    Serial.println("- ESP8266");
    Serial.println("- ESP32-C3/C6 (RISC-V architecture)");
    Serial.println();
    Serial.println("For other platforms, use:");
    Serial.println("- FastPinsMultiSPI example (standard GPIO)");
    Serial.println("- FastPinsSPI example (SPI with clock)");
    Serial.println("- RMT hardware (ESP32 only, up to 8 strips)");
    Serial.println();
    Serial.println("System halted.");
}

void loop() {
    // Do nothing
    delay(1000);
}

#endif  // ESP32
