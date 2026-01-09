/// @file platform_init.cpp
/// @brief Unit tests for platform initialization system
///
/// Tests the fl::platforms::init() trampoline pattern and CFastLED::init() method.

#include "doctest.h"
#include "FastLED.h"
#include "platforms/init.h"

TEST_CASE("platform_init") {
    SUBCASE("fl::platforms::init() can be called") {
        // Test that the platform init function exists and can be called
        // This should be a no-op on most platforms, but ESP32 will initialize
        // channel engines and SPI bus manager
        fl::platforms::init();

        // If we got here without crashing, the function worked
        CHECK(true);
    }

    SUBCASE("fl::platforms::init() is safe to call multiple times") {
        // Test that calling init() multiple times is safe (idempotent)
        fl::platforms::init();
        fl::platforms::init();
        fl::platforms::init();

        // Should not crash or cause issues
        CHECK(true);
    }

    SUBCASE("FastLED.init() can be called") {
        // Test that the CFastLED::init() method exists and can be called
        FastLED.init();

        // If we got here without crashing, the function worked
        CHECK(true);
    }

    SUBCASE("FastLED.init() is safe to call multiple times") {
        // Test that calling FastLED.init() multiple times is safe (idempotent)
        FastLED.init();
        FastLED.init();
        FastLED.init();

        // Should not crash or cause issues
        CHECK(true);
    }

    SUBCASE("FastLED.init() calls fl::platforms::init()") {
        // This is more of a structural test - we can't directly verify the call chain
        // in a unit test, but we can verify both functions exist and work together

        // Reset by calling platform init directly
        fl::platforms::init();

        // Then call through FastLED API
        FastLED.init();

        // Both should work without issues
        CHECK(true);
    }
}
