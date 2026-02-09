// Unit tests for stub platform LED capture functionality
// Tests that LED data is properly captured via ActiveStripTracker

#include "test.h"
#include "FastLED.h"
#include "platforms/shared/active_strip_data/active_strip_data.h"
#include "platforms/shared/active_strip_tracker/active_strip_tracker.h"

using namespace fl;

FL_TEST_CASE("ClocklessController - LED Data Capture") {
    FL_SUBCASE("Basic LED capture with single strip") {
        const int NUM_LEDS = 10;
        CRGB leds[NUM_LEDS];

        // Reset tracker state for clean test
        ActiveStripTracker::resetForTesting();

        // Initialize FastLED
        FastLED.addLeds<WS2812, 1>(leds, NUM_LEDS);
        FastLED.setBrightness(255); // Ensure full brightness for accurate capture

        // Set some LED colors
        leds[0] = CRGB::Red;
        leds[1] = CRGB::Green;
        leds[2] = CRGB::Blue;
        for (int i = 3; i < NUM_LEDS; i++) {
            leds[i] = CRGB::Black;
        }

        // Show LEDs - this should capture data
        FastLED.show();

        // Verify data was captured
        auto& strip_data = ActiveStripData::Instance().getData();
        FL_CHECK(!strip_data.empty());

        if (!strip_data.empty()) {
            // Get first strip data (ID should be 0)
            SliceUint8 data;
            bool found = strip_data.get(0, &data);
            FL_REQUIRE(found);

            // Check captured data size (3 bytes per LED)
            FL_CHECK_EQ(data.size(), NUM_LEDS * 3);

            if (data.size() >= NUM_LEDS * 3) {
                // Red LED at position 0 - check red is dominant
                FL_CHECK_GT(data[0], 100);  // R should be high
                FL_CHECK_LT(data[1], 50);   // G should be low
                FL_CHECK_LT(data[2], 50);   // B should be low

                // Green LED at position 1 - check green is dominant
                FL_CHECK_LT(data[3], 50);   // R should be low
                FL_CHECK_GT(data[4], 100);  // G should be high
                FL_CHECK_LT(data[5], 50);   // B should be low

                // Blue LED at position 2 - check blue is dominant
                FL_CHECK_LT(data[6], 50);   // R should be low
                FL_CHECK_LT(data[7], 50);   // G should be low
                FL_CHECK_GT(data[8], 100);  // B should be high
            }
        }
    }

    FL_SUBCASE("LED capture updates on each show()") {
        const int NUM_LEDS = 5;
        CRGB leds[NUM_LEDS];

        ActiveStripTracker::resetForTesting();
        FastLED.addLeds<WS2812, 2>(leds, NUM_LEDS);
        FastLED.setBrightness(255); // Ensure full brightness for accurate capture

        // First show - all red
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB::Red;
        }
        FastLED.show();

        auto& strip_data = ActiveStripData::Instance().getData();
        SliceUint8 data;
        FL_REQUIRE(strip_data.get(0, &data));
        FL_CHECK_GT(data[0], 100); // Red should be high
        FL_CHECK_LT(data[1], 50);  // Green should be low
        FL_CHECK_LT(data[2], 50);  // Blue should be low

        // Second show - all green
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB::Green;
        }
        FastLED.show();

        FL_REQUIRE(strip_data.get(0, &data));
        FL_CHECK_LT(data[0], 50);  // Red should be low
        FL_CHECK_GT(data[1], 100); // Green should be high
        FL_CHECK_LT(data[2], 50);  // Blue should be low
    }

    FL_SUBCASE("Multiple strips captured independently") {
        const int NUM_LEDS = 3;
        CRGB leds1[NUM_LEDS];
        CRGB leds2[NUM_LEDS];

        ActiveStripTracker::resetForTesting();
        FastLED.addLeds<WS2812, 3>(leds1, NUM_LEDS);
        FastLED.addLeds<WS2812, 4>(leds2, NUM_LEDS);
        FastLED.setBrightness(255); // Ensure full brightness for accurate capture

        // Set different colors for each strip
        for (int i = 0; i < NUM_LEDS; i++) {
            leds1[i] = CRGB::Red;
            leds2[i] = CRGB::Blue;
        }

        FastLED.show();

        auto& strip_data = ActiveStripData::Instance().getData();
        FL_CHECK_EQ(strip_data.size(), 2);

        if (strip_data.size() >= 2) {
            SliceUint8 data1, data2;

            // First strip (ID 0) should be red
            FL_REQUIRE(strip_data.get(0, &data1));
            FL_CHECK_GT(data1[0], 100); // R should be high
            FL_CHECK_LT(data1[1], 50);  // G should be low
            FL_CHECK_LT(data1[2], 50);  // B should be low

            // Second strip (ID 1) should be blue
            FL_REQUIRE(strip_data.get(1, &data2));
            FL_CHECK_LT(data2[0], 50);  // R should be low
            FL_CHECK_LT(data2[1], 50);  // G should be low
            FL_CHECK_GT(data2[2], 100); // B should be high
        }
    }
}
