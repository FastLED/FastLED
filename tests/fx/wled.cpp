// Unit tests for WLEDClient

#include "mock_fastled.h"
#include "fl/stl/cstddef.h"
#include "crgb.h"
#include "test.h"
#include "fl/fx/wled/client.h"
#include "fl/rgb8.h"
#include "fl/slice.h"
#include "fl/stl/shared_ptr.h"

using namespace fl;

FL_TEST_CASE("WLEDClient construction") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    FL_SUBCASE("Initial state is off with max brightness") {
        FL_CHECK(client.getOn() == false);
        FL_CHECK(client.getBrightness() == 255);
    }

    FL_SUBCASE("LED count is accessible") {
        FL_CHECK(client.getNumLEDs() == 50);
    }
}

FL_TEST_CASE("WLEDClient brightness control") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    FL_SUBCASE("Setting brightness when off does not affect controller") {
        client.setBrightness(128);
        FL_CHECK(client.getBrightness() == 128);
        // Controller should still have default brightness (not changed)
        FL_CHECK(mock->getBrightness() == 255);
    }

    FL_SUBCASE("Setting brightness when on applies to controller") {
        client.setOn(true);
        client.setBrightness(128);
        FL_CHECK(client.getBrightness() == 128);
        FL_CHECK(mock->getBrightness() == 128);
    }

    FL_SUBCASE("Brightness is preserved when turning off and on") {
        client.setBrightness(100);
        client.setOn(true);
        FL_CHECK(mock->getBrightness() == 100);

        client.setOn(false);
        FL_CHECK(client.getBrightness() == 100);  // Internal brightness preserved
        FL_CHECK(mock->getBrightness() == 0);      // Controller brightness is 0

        client.setOn(true);
        FL_CHECK(mock->getBrightness() == 100);    // Restored to internal brightness
    }
}

FL_TEST_CASE("WLEDClient on/off control") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    FL_SUBCASE("Turning on applies current brightness") {
        client.setBrightness(150);
        client.setOn(true);
        FL_CHECK(client.getOn() == true);
        FL_CHECK(mock->getBrightness() == 150);
    }

    FL_SUBCASE("Turning off sets controller brightness to 0") {
        client.setBrightness(200);
        client.setOn(true);
        client.setOn(false);
        FL_CHECK(client.getOn() == false);
        FL_CHECK(mock->getBrightness() == 0);
    }

    FL_SUBCASE("Multiple on/off cycles") {
        client.setBrightness(80);

        for (int i = 0; i < 3; i++) {
            client.setOn(true);
            FL_CHECK(mock->getBrightness() == 80);

            client.setOn(false);
            FL_CHECK(mock->getBrightness() == 0);
        }
    }
}

FL_TEST_CASE("WLEDClient clear operation") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    FL_SUBCASE("Clear without write increments clear count but not show count") {
        client.clear(false);
        FL_CHECK(mock->getClearCallCount() == 1);
        FL_CHECK(mock->getShowCallCount() == 0);
    }

    FL_SUBCASE("Clear with write increments both clear and show count") {
        client.clear(true);
        FL_CHECK(mock->getClearCallCount() == 1);
        FL_CHECK(mock->getShowCallCount() == 1);
    }

    FL_SUBCASE("Clear sets all LEDs to black") {
        // Set some LEDs to colors first
        auto leds = client.getLEDs();
        for (size_t i = 0; i < leds.size(); i++) {
            leds[i] = CRGB::Red;
        }

        // Clear them
        client.clear(false);

        // Verify all black
        for (size_t i = 0; i < leds.size(); i++) {
            FL_CHECK(leds[i] == CRGB::Black);
        }
    }
}

FL_TEST_CASE("WLEDClient update operation") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    FL_SUBCASE("Update calls show on controller") {
        client.update();
        FL_CHECK(mock->getShowCallCount() == 1);

        client.update();
        FL_CHECK(mock->getShowCallCount() == 2);
    }

    FL_SUBCASE("LED changes are visible after update") {
        auto leds = client.getLEDs();
        leds[0] = CRGB::Red;
        leds[1] = CRGB::Green;
        leds[2] = CRGB::Blue;

        client.update();

        FL_CHECK(mock->getShowCallCount() == 1);
        FL_CHECK(leds[0] == CRGB::Red);
        FL_CHECK(leds[1] == CRGB::Green);
        FL_CHECK(leds[2] == CRGB::Blue);
    }
}

FL_TEST_CASE("WLEDClient LED array access") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    FL_SUBCASE("Can read and write LEDs directly") {
        auto leds = client.getLEDs();
        FL_CHECK(leds.size() == 50);

        // Write colors
        leds[0] = CRGB(255, 0, 0);
        leds[10] = CRGB(0, 255, 0);
        leds[20] = CRGB(0, 0, 255);

        // Read them back
        FL_CHECK(leds[0].r == 255);
        FL_CHECK(leds[0].g == 0);
        FL_CHECK(leds[0].b == 0);

        FL_CHECK(leds[10].r == 0);
        FL_CHECK(leds[10].g == 255);
        FL_CHECK(leds[10].b == 0);

        FL_CHECK(leds[20].r == 0);
        FL_CHECK(leds[20].g == 0);
        FL_CHECK(leds[20].b == 255);
    }
}

FL_TEST_CASE("WLEDClient complete workflow") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    FL_SUBCASE("Typical usage pattern") {
        // Set brightness and turn on
        client.setBrightness(128);
        client.setOn(true);
        FL_CHECK(mock->getBrightness() == 128);

        // Set some LED colors
        auto leds = client.getLEDs();
        for (size_t i = 0; i < 10; i++) {
            leds[i] = CRGB::Red;
        }

        // Update the strip
        client.update();
        FL_CHECK(mock->getShowCallCount() == 1);

        // Change brightness
        client.setBrightness(200);
        FL_CHECK(mock->getBrightness() == 200);

        // Clear and show
        client.clear(true);
        FL_CHECK(mock->getClearCallCount() == 1);
        FL_CHECK(mock->getShowCallCount() == 2);

        // Turn off
        client.setOn(false);
        FL_CHECK(mock->getBrightness() == 0);
        FL_CHECK(client.getBrightness() == 200);  // Internal brightness preserved
    }
}

FL_TEST_CASE("WLEDClient null controller handling") {
    WLEDClient client(nullptr);

    FL_SUBCASE("Operations with null controller don't crash") {
        FL_CHECK(client.getNumLEDs() == 0);
        FL_CHECK(client.getLEDs().size() == 0);

        // These should not crash
        client.setBrightness(128);
        client.setOn(true);
        client.clear(false);
        client.update();
    }
}

FL_TEST_CASE("WLEDClient segment operations") {
    auto mock = fl::make_shared<MockFastLED>(100);
    WLEDClient client(mock);

    FL_SUBCASE("Set segment restricts LED access to range") {
        // Set segment to LEDs 10-20
        client.setSegment(10, 20);

        // getLEDs should return only segment range
        auto leds = client.getLEDs();
        FL_CHECK(leds.size() == 10);
        FL_CHECK(client.getNumLEDs() == 10);

        // Write colors to segment
        for (size_t i = 0; i < leds.size(); i++) {
            leds[i] = CRGB::Red;
        }

        // Clear segment and verify the full array
        client.clearSegment();
        auto fullArray = client.getLEDs();
        for (size_t i = 10; i < 20; i++) {
            FL_CHECK(fullArray[i] == CRGB::Red);
        }
    }

    FL_SUBCASE("Clear segment restores full array access") {
        // Set a segment first
        client.setSegment(20, 30);
        FL_CHECK(client.getNumLEDs() == 10);

        // Clear the segment
        client.clearSegment();

        // Should have access to full array now
        auto leds = client.getLEDs();
        FL_CHECK(leds.size() == 100);
        FL_CHECK(client.getNumLEDs() == 100);
    }

    FL_SUBCASE("Multiple segment operations") {
        // First segment
        client.setSegment(0, 25);
        FL_CHECK(client.getNumLEDs() == 25);

        auto leds = client.getLEDs();
        for (size_t i = 0; i < leds.size(); i++) {
            leds[i] = CRGB::Red;
        }
        client.update();

        // Second segment
        client.setSegment(25, 50);
        FL_CHECK(client.getNumLEDs() == 25);

        leds = client.getLEDs();
        for (size_t i = 0; i < leds.size(); i++) {
            leds[i] = CRGB::Green;
        }
        client.update();

        // Third segment
        client.setSegment(50, 75);
        FL_CHECK(client.getNumLEDs() == 25);

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
            FL_CHECK(fullArray[i] == CRGB::Red);
        }
        for (size_t i = 25; i < 50; i++) {
            FL_CHECK(fullArray[i] == CRGB::Green);
        }
        for (size_t i = 50; i < 75; i++) {
            FL_CHECK(fullArray[i] == CRGB::Blue);
        }

        FL_CHECK(mock->getShowCallCount() == 3);
    }

    FL_SUBCASE("Segment with clear operation") {
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
            FL_CHECK(fullArray[i] == CRGB::White);  // Not in segment, unchanged
        }
        for (size_t i = 40; i < 60; i++) {
            FL_CHECK(fullArray[i] == CRGB::Black);  // In segment, cleared
        }
        for (size_t i = 60; i < 100; i++) {
            FL_CHECK(fullArray[i] == CRGB::White);  // Not in segment, unchanged
        }
    }
}

FL_TEST_CASE("WLEDClient color correction and temperature") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    FL_SUBCASE("Set color correction") {
        CRGB correction(255, 200, 150);
        client.setCorrection(correction);

        // Verify mock recorded the correction
        FL_CHECK(mock->getLastCorrection() == correction);
    }

    FL_SUBCASE("Set color temperature") {
        CRGB temperature(255, 220, 180);
        client.setTemperature(temperature);

        // Verify mock recorded the temperature
        FL_CHECK(mock->getLastTemperature() == temperature);
    }

    FL_SUBCASE("Apply both correction and temperature") {
        CRGB correction(250, 180, 200);
        CRGB temperature(255, 230, 190);

        client.setCorrection(correction);
        client.setTemperature(temperature);

        FL_CHECK(mock->getLastCorrection() == correction);
        FL_CHECK(mock->getLastTemperature() == temperature);
    }

    FL_SUBCASE("Typical white balance workflow") {
        // Set warm white temperature
        client.setTemperature(CRGB(255, 230, 180));

        // Set some LEDs
        auto leds = client.getLEDs();
        for (size_t i = 0; i < 10; i++) {
            leds[i] = CRGB::White;
        }

        // Update
        client.update();

        FL_CHECK(mock->getShowCallCount() == 1);
        FL_CHECK(mock->getLastTemperature() == CRGB(255, 230, 180));
    }
}

FL_TEST_CASE("WLEDClient max refresh rate") {
    auto mock = fl::make_shared<MockFastLED>(50);
    WLEDClient client(mock);

    FL_SUBCASE("Set and get max refresh rate") {
        client.setMaxRefreshRate(60);
        FL_CHECK(client.getMaxRefreshRate() == 60);
        FL_CHECK(mock->getMaxRefreshRate() == 60);
    }

    FL_SUBCASE("Change max refresh rate multiple times") {
        client.setMaxRefreshRate(30);
        FL_CHECK(client.getMaxRefreshRate() == 30);

        client.setMaxRefreshRate(120);
        FL_CHECK(client.getMaxRefreshRate() == 120);

        client.setMaxRefreshRate(0);  // No limit
        FL_CHECK(client.getMaxRefreshRate() == 0);
    }

    FL_SUBCASE("Max refresh rate with rapid updates") {
        client.setMaxRefreshRate(60);

        // Rapidly update
        for (int i = 0; i < 10; i++) {
            client.update();
        }

        // All updates should have been called (mock doesn't rate limit)
        FL_CHECK(mock->getShowCallCount() == 10);
    }
}

FL_TEST_CASE("WLEDClient advanced integration workflow") {
    auto mock = fl::make_shared<MockFastLED>(100);
    WLEDClient client(mock);

    FL_SUBCASE("Complete advanced feature workflow") {
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
        FL_CHECK(client.getNumLEDs() == 100);

        // Verify state
        FL_CHECK(mock->getShowCallCount() == 3);
        FL_CHECK(mock->getBrightness() == 200);
        FL_CHECK(mock->getLastCorrection() == CRGB(255, 200, 150));
        FL_CHECK(mock->getLastTemperature() == CRGB(255, 230, 180));
        FL_CHECK(mock->getMaxRefreshRate() == 60);

        // Final clear
        client.clear(true);
        FL_CHECK(mock->getShowCallCount() == 4);
        FL_CHECK(mock->getClearCallCount() == 1);
    }
}
