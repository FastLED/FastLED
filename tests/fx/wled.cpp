// Unit tests for WLEDClient

#include "mock_fastled.h"
#include "fl/stl/cstddef.h"
#include "crgb.h"
#include "doctest.h"
#include "fl/fx/wled/client.h"
#include "fl/rgb8.h"
#include "fl/slice.h"
#include "fl/stl/shared_ptr.h"

using namespace fl;

TEST_CASE("WLEDClient construction") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    SUBCASE("Initial state is off with max brightness") {
        CHECK(client.getOn() == false);
        CHECK(client.getBrightness() == 255);
    }

    SUBCASE("LED count is accessible") {
        CHECK(client.getNumLEDs() == 50);
    }
}

TEST_CASE("WLEDClient brightness control") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    SUBCASE("Setting brightness when off does not affect controller") {
        client.setBrightness(128);
        CHECK(client.getBrightness() == 128);
        // Controller should still have default brightness (not changed)
        CHECK(mock->getBrightness() == 255);
    }

    SUBCASE("Setting brightness when on applies to controller") {
        client.setOn(true);
        client.setBrightness(128);
        CHECK(client.getBrightness() == 128);
        CHECK(mock->getBrightness() == 128);
    }

    SUBCASE("Brightness is preserved when turning off and on") {
        client.setBrightness(100);
        client.setOn(true);
        CHECK(mock->getBrightness() == 100);

        client.setOn(false);
        CHECK(client.getBrightness() == 100);  // Internal brightness preserved
        CHECK(mock->getBrightness() == 0);      // Controller brightness is 0

        client.setOn(true);
        CHECK(mock->getBrightness() == 100);    // Restored to internal brightness
    }
}

TEST_CASE("WLEDClient on/off control") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    SUBCASE("Turning on applies current brightness") {
        client.setBrightness(150);
        client.setOn(true);
        CHECK(client.getOn() == true);
        CHECK(mock->getBrightness() == 150);
    }

    SUBCASE("Turning off sets controller brightness to 0") {
        client.setBrightness(200);
        client.setOn(true);
        client.setOn(false);
        CHECK(client.getOn() == false);
        CHECK(mock->getBrightness() == 0);
    }

    SUBCASE("Multiple on/off cycles") {
        client.setBrightness(80);

        for (int i = 0; i < 3; i++) {
            client.setOn(true);
            CHECK(mock->getBrightness() == 80);

            client.setOn(false);
            CHECK(mock->getBrightness() == 0);
        }
    }
}

TEST_CASE("WLEDClient clear operation") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    SUBCASE("Clear without write increments clear count but not show count") {
        client.clear(false);
        CHECK(mock->getClearCallCount() == 1);
        CHECK(mock->getShowCallCount() == 0);
    }

    SUBCASE("Clear with write increments both clear and show count") {
        client.clear(true);
        CHECK(mock->getClearCallCount() == 1);
        CHECK(mock->getShowCallCount() == 1);
    }

    SUBCASE("Clear sets all LEDs to black") {
        // Set some LEDs to colors first
        auto leds = client.getLEDs();
        for (size_t i = 0; i < leds.size(); i++) {
            leds[i] = CRGB::Red;
        }

        // Clear them
        client.clear(false);

        // Verify all black
        for (size_t i = 0; i < leds.size(); i++) {
            CHECK(leds[i] == CRGB::Black);
        }
    }
}

TEST_CASE("WLEDClient update operation") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    SUBCASE("Update calls show on controller") {
        client.update();
        CHECK(mock->getShowCallCount() == 1);

        client.update();
        CHECK(mock->getShowCallCount() == 2);
    }

    SUBCASE("LED changes are visible after update") {
        auto leds = client.getLEDs();
        leds[0] = CRGB::Red;
        leds[1] = CRGB::Green;
        leds[2] = CRGB::Blue;

        client.update();

        CHECK(mock->getShowCallCount() == 1);
        CHECK(leds[0] == CRGB::Red);
        CHECK(leds[1] == CRGB::Green);
        CHECK(leds[2] == CRGB::Blue);
    }
}

TEST_CASE("WLEDClient LED array access") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    SUBCASE("Can read and write LEDs directly") {
        auto leds = client.getLEDs();
        CHECK(leds.size() == 50);

        // Write colors
        leds[0] = CRGB(255, 0, 0);
        leds[10] = CRGB(0, 255, 0);
        leds[20] = CRGB(0, 0, 255);

        // Read them back
        CHECK(leds[0].r == 255);
        CHECK(leds[0].g == 0);
        CHECK(leds[0].b == 0);

        CHECK(leds[10].r == 0);
        CHECK(leds[10].g == 255);
        CHECK(leds[10].b == 0);

        CHECK(leds[20].r == 0);
        CHECK(leds[20].g == 0);
        CHECK(leds[20].b == 255);
    }
}

TEST_CASE("WLEDClient complete workflow") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    SUBCASE("Typical usage pattern") {
        // Set brightness and turn on
        client.setBrightness(128);
        client.setOn(true);
        CHECK(mock->getBrightness() == 128);

        // Set some LED colors
        auto leds = client.getLEDs();
        for (size_t i = 0; i < 10; i++) {
            leds[i] = CRGB::Red;
        }

        // Update the strip
        client.update();
        CHECK(mock->getShowCallCount() == 1);

        // Change brightness
        client.setBrightness(200);
        CHECK(mock->getBrightness() == 200);

        // Clear and show
        client.clear(true);
        CHECK(mock->getClearCallCount() == 1);
        CHECK(mock->getShowCallCount() == 2);

        // Turn off
        client.setOn(false);
        CHECK(mock->getBrightness() == 0);
        CHECK(client.getBrightness() == 200);  // Internal brightness preserved
    }
}

TEST_CASE("WLEDClient null controller handling") {
    WLEDClient client(nullptr);

    SUBCASE("Operations with null controller don't crash") {
        CHECK(client.getNumLEDs() == 0);
        CHECK(client.getLEDs().size() == 0);

        // These should not crash
        client.setBrightness(128);
        client.setOn(true);
        client.clear(false);
        client.update();
    }
}

TEST_CASE("WLEDClient segment operations") {
    auto mock = fl::make_shared<MockFastLED>(100);
    WLEDClient client(mock);

    SUBCASE("Set segment restricts LED access to range") {
        // Set segment to LEDs 10-20
        client.setSegment(10, 20);

        // getLEDs should return only segment range
        auto leds = client.getLEDs();
        CHECK(leds.size() == 10);
        CHECK(client.getNumLEDs() == 10);

        // Write colors to segment
        for (size_t i = 0; i < leds.size(); i++) {
            leds[i] = CRGB::Red;
        }

        // Clear segment and verify the full array
        client.clearSegment();
        auto fullArray = client.getLEDs();
        for (size_t i = 10; i < 20; i++) {
            CHECK(fullArray[i] == CRGB::Red);
        }
    }

    SUBCASE("Clear segment restores full array access") {
        // Set a segment first
        client.setSegment(20, 30);
        CHECK(client.getNumLEDs() == 10);

        // Clear the segment
        client.clearSegment();

        // Should have access to full array now
        auto leds = client.getLEDs();
        CHECK(leds.size() == 100);
        CHECK(client.getNumLEDs() == 100);
    }

    SUBCASE("Multiple segment operations") {
        // First segment
        client.setSegment(0, 25);
        CHECK(client.getNumLEDs() == 25);

        auto leds = client.getLEDs();
        for (size_t i = 0; i < leds.size(); i++) {
            leds[i] = CRGB::Red;
        }
        client.update();

        // Second segment
        client.setSegment(25, 50);
        CHECK(client.getNumLEDs() == 25);

        leds = client.getLEDs();
        for (size_t i = 0; i < leds.size(); i++) {
            leds[i] = CRGB::Green;
        }
        client.update();

        // Third segment
        client.setSegment(50, 75);
        CHECK(client.getNumLEDs() == 25);

        leds = client.getLEDs();
        for (size_t i = 0; i < leds.size(); i++) {
            leds[i] = CRGB::Blue;
        }
        client.update();

        // Clear segment to access full array for verification
        client.clearSegment();
        auto fullArray = client.getLEDs();

        // Verify all three segments
        for (size_t i = 0; i < 25; i++) {
            CHECK(fullArray[i] == CRGB::Red);
        }
        for (size_t i = 25; i < 50; i++) {
            CHECK(fullArray[i] == CRGB::Green);
        }
        for (size_t i = 50; i < 75; i++) {
            CHECK(fullArray[i] == CRGB::Blue);
        }

        CHECK(mock->getShowCallCount() == 3);
    }

    SUBCASE("Segment with clear operation") {
        // Fill full array first (no segment set yet)
        auto leds = client.getLEDs();
        for (size_t i = 0; i < leds.size(); i++) {
            leds[i] = CRGB::White;
        }

        // Set segment and clear only that segment
        client.setSegment(40, 60);
        client.clear(false);

        // Clear segment to access full array for verification
        client.clearSegment();
        auto fullArray = client.getLEDs();

        for (size_t i = 0; i < 40; i++) {
            CHECK(fullArray[i] == CRGB::White);  // Not in segment, unchanged
        }
        for (size_t i = 40; i < 60; i++) {
            CHECK(fullArray[i] == CRGB::Black);  // In segment, cleared
        }
        for (size_t i = 60; i < 100; i++) {
            CHECK(fullArray[i] == CRGB::White);  // Not in segment, unchanged
        }
    }
}

TEST_CASE("WLEDClient color correction and temperature") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    SUBCASE("Set color correction") {
        CRGB correction(255, 200, 150);
        client.setCorrection(correction);

        // Verify mock recorded the correction
        CHECK(mock->getLastCorrection() == correction);
    }

    SUBCASE("Set color temperature") {
        CRGB temperature(255, 220, 180);
        client.setTemperature(temperature);

        // Verify mock recorded the temperature
        CHECK(mock->getLastTemperature() == temperature);
    }

    SUBCASE("Apply both correction and temperature") {
        CRGB correction(250, 180, 200);
        CRGB temperature(255, 230, 190);

        client.setCorrection(correction);
        client.setTemperature(temperature);

        CHECK(mock->getLastCorrection() == correction);
        CHECK(mock->getLastTemperature() == temperature);
    }

    SUBCASE("Typical white balance workflow") {
        // Set warm white temperature
        client.setTemperature(CRGB(255, 230, 180));

        // Set some LEDs
        auto leds = client.getLEDs();
        for (size_t i = 0; i < 10; i++) {
            leds[i] = CRGB::White;
        }

        // Update
        client.update();

        CHECK(mock->getShowCallCount() == 1);
        CHECK(mock->getLastTemperature() == CRGB(255, 230, 180));
    }
}

TEST_CASE("WLEDClient max refresh rate") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    SUBCASE("Set and get max refresh rate") {
        client.setMaxRefreshRate(60);
        CHECK(client.getMaxRefreshRate() == 60);
        CHECK(mock->getMaxRefreshRate() == 60);
    }

    SUBCASE("Change max refresh rate multiple times") {
        client.setMaxRefreshRate(30);
        CHECK(client.getMaxRefreshRate() == 30);

        client.setMaxRefreshRate(120);
        CHECK(client.getMaxRefreshRate() == 120);

        client.setMaxRefreshRate(0);  // No limit
        CHECK(client.getMaxRefreshRate() == 0);
    }

    SUBCASE("Max refresh rate with rapid updates") {
        client.setMaxRefreshRate(60);

        // Rapidly update
        for (int i = 0; i < 10; i++) {
            client.update();
        }

        // All updates should have been called (mock doesn't rate limit)
        CHECK(mock->getShowCallCount() == 10);
    }
}

TEST_CASE("WLEDClient advanced integration workflow") {
    auto mock = fl::make_shared<MockFastLED>(100);
    WLEDClient client(mock);

    SUBCASE("Complete advanced feature workflow") {
        // Setup color correction and temperature
        client.setCorrection(CRGB(255, 200, 150));
        client.setTemperature(CRGB(255, 230, 180));

        // Set max refresh rate
        client.setMaxRefreshRate(60);

        // Turn on and set brightness
        client.setBrightness(200);
        client.setOn(true);

        // Work with first segment
        client.setSegment(0, 33);
        auto leds = client.getLEDs();
        for (size_t i = 0; i < leds.size(); i++) {
            leds[i] = CRGB::Red;
        }
        client.update();

        // Work with second segment
        client.setSegment(33, 66);
        leds = client.getLEDs();
        for (size_t i = 0; i < leds.size(); i++) {
            leds[i] = CRGB::Green;
        }
        client.update();

        // Work with third segment
        client.setSegment(66, 99);
        leds = client.getLEDs();
        for (size_t i = 0; i < leds.size(); i++) {
            leds[i] = CRGB::Blue;
        }
        client.update();

        // Clear segment and work with full array
        client.clearSegment();
        CHECK(client.getNumLEDs() == 100);

        // Verify state
        CHECK(mock->getShowCallCount() == 3);
        CHECK(mock->getBrightness() == 200);
        CHECK(mock->getLastCorrection() == CRGB(255, 200, 150));
        CHECK(mock->getLastTemperature() == CRGB(255, 230, 180));
        CHECK(mock->getMaxRefreshRate() == 60);

        // Final clear
        client.clear(true);
        CHECK(mock->getShowCallCount() == 4);
        CHECK(mock->getClearCallCount() == 1);
    }
}
