// Unit tests for stub platform LED capture functionality

#include "test.h"

#include "FastLED.h"
#include "cled_controller.h"
#include "platforms/shared/active_strip_data/active_strip_data.h"
#include "platforms/stub/fastspi_stub_generic.h"
#include "platforms/stub/clockless_stub_generic.h"
#include "fl/chipsets/timing_traits.h"
#include "eorder.h"

using namespace fl;


TEST_CASE("ClocklessController - LED Data Capture") {
    // Create LED array
    CRGB leds[3] = {CRGB::Red, CRGB::Green, CRGB::Blue};

    // FastLED.addLeds(&controller, leds, 3);

    FastLED.addLeds<NEOPIXEL, RGB>(leds, 3);

    // Trigger FastLED.show() to capture LED data
    FastLED.show();

    // Verify data was captured
    ActiveStripData& data = ActiveStripData::Instance();
    const auto& stripData = data.getData();

    // Should have at least one strip registered
    CHECK(stripData.size() >= 1);

    FL_WARN("StubSPIOutput LED capture test: " << stripData.size() << " strips captured");
}
