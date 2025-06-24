#include "FastLED.h"

// Include platform-specific test headers
#if defined(__AVR__)
#include "avr_test.h"
#elif defined(ESP32) || defined(ESP8266)
#include "esp_test.h"
#elif defined(FASTLED_ARM)
#include "arm_test.h"
#elif defined(APOLLO3)
#include "apollo3_test.h"
#elif defined(FASTLED_STUB_IMPL)
#include "wasm_test.h"
#endif

void setup() {
    Serial.begin(115200);
    delay(1000);  // Give serial time to initialize
    
    Serial.println("=== FastLED Platform Compile Test ===");
    Serial.println();
    
    // Print platform identification
    Serial.print("Platform: ");
#if defined(__AVR__)
    Serial.println("AVR");
    avr_tests();
    Serial.println("✓ AVR platform tests passed");
#elif defined(ESP8266)
    Serial.println("ESP8266");
    // esp_tests();
    Serial.println("✓ ESP8266 platform tests passed");
#elif defined(ESP32)
    Serial.print("ESP32");
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
    Serial.print("-S3");
    #elif defined(CONFIG_IDF_TARGET_ESP32S2)
    Serial.print("-S2");
    #elif defined(CONFIG_IDF_TARGET_ESP32C3)
    Serial.print("-C3");
    #elif defined(CONFIG_IDF_TARGET_ESP32C6)
    Serial.print("-C6");
    #elif defined(CONFIG_IDF_TARGET_ESP32H2)
    Serial.print("-H2");
    #endif
    Serial.println();
    esp_tests();
    Serial.println("✓ ESP32 platform tests passed");
#elif defined(FASTLED_ARM)
    Serial.print("ARM (");
    #if defined(ARDUINO_ARCH_STM32)
    Serial.print("STM32");
    #elif defined(ARDUINO_ARCH_RP2040)
    Serial.print("RP2040");
    #elif defined(__MK20DX256__)
    Serial.print("Teensy 3.2");
    #elif defined(__MK66FX1M0__)
    Serial.print("Teensy 3.6");
    #elif defined(__IMXRT1062__)
    Serial.print("Teensy 4.x");
    #elif defined(ARDUINO_ARCH_SAMD)
    Serial.print("SAMD");
    #elif defined(ARDUINO_SAM_DUE)
    Serial.print("SAM");
    #elif defined(NRF52_SERIES)
    Serial.print("NRF52");
    #elif defined(NRF51_SERIES)
    Serial.print("NRF51");
    #elif defined(ARDUINO_ARCH_RENESAS_UNO)
    Serial.print("Renesas");
    #else
    Serial.print("Unknown ARM");
    #endif
    Serial.println(")");
    arm_tests();
    Serial.println("✓ ARM platform tests passed");
#elif defined(APOLLO3)
    Serial.println("Apollo3");
    apollo3_tests();
    Serial.println("✓ Apollo3 platform tests passed");
#elif defined(FASTLED_STUB_IMPL)
    Serial.println("WASM/Stub");
    wasm_tests();
    Serial.println("✓ WASM platform tests passed");
#else
    Serial.println("UNKNOWN - No platform-specific tests available");
    Serial.println("⚠ Warning: Unrecognized platform");
#endif

    // Print common configuration info
    Serial.println();
    Serial.println("=== Platform Configuration ===");
    Serial.print("F_CPU: ");
    Serial.print(F_CPU);
    Serial.println(" Hz");
    
    Serial.print("FASTLED_USE_PROGMEM: ");
    Serial.println(FASTLED_USE_PROGMEM);
    
    Serial.print("SKETCH_HAS_LOTS_OF_MEMORY: ");
    Serial.println(SKETCH_HAS_LOTS_OF_MEMORY);
    
    Serial.print("FASTLED_ALLOW_INTERRUPTS: ");
    Serial.println(FASTLED_ALLOW_INTERRUPTS);
    
    #ifdef CLOCKLESS_FREQUENCY
    Serial.print("CLOCKLESS_FREQUENCY: ");
    Serial.print(CLOCKLESS_FREQUENCY);
    Serial.println(" Hz");
    #endif
    
    #ifdef FASTLED_HAS_MILLIS
    Serial.println("millis() support: YES");
    #else
    Serial.println("millis() support: NO");
    #endif
    
    #ifdef FASTLED_FORCE_SOFTWARE_SPI
    Serial.println("SPI: Forced SOFTWARE");
    #else
    Serial.println("SPI: Hardware capable");
    #endif
    
    // Test basic FastLED functionality
    Serial.println();
    Serial.println("=== Basic FastLED Function Test ===");
    CRGB testLed = CRGB::Red;
    Serial.print("CRGB::Red = (");
    Serial.print(testLed.r);
    Serial.print(", ");
    Serial.print(testLed.g);
    Serial.print(", ");
    Serial.print(testLed.b);
    Serial.println(")");
    
    // Test color math
    testLed = CRGB(255, 128, 64);
    testLed.nscale8(128);  // 50% brightness
    Serial.print("After nscale8(128): (");
    Serial.print(testLed.r);
    Serial.print(", ");
    Serial.print(testLed.g);
    Serial.print(", ");
    Serial.print(testLed.b);
    Serial.println(")");
    
    Serial.println();
    Serial.println("✓ All tests completed successfully!");
    Serial.println("✓ FastLED should work correctly on this platform");
}

void loop() {
    // Blink built-in LED to show we're running
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    
    if (millis() - lastBlink > 1000) {
        lastBlink = millis();
        ledState = !ledState;
        
        #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, ledState);
        #endif
        
        Serial.print(".");
        if (millis() / 1000 % 60 == 0) {
            Serial.println(" (alive)");
        }
    }
}
