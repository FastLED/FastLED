// Isolated variant: linked only against the matching production library.
#include "test.h"

#include "FastLED.h"
#include "fl/channels/config.h"
#include "fl/channels/data.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_clockless.h"
#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_mock.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::detail;

namespace {
class UnsupportedDriver : public IChannelDriver {
  public:
    void enqueue(ChannelDataPtr) FL_NO_EXCEPT override {}
    void show() FL_NO_EXCEPT override {}
    DriverState poll() FL_NO_EXCEPT override { return DriverState::READY; }
    Capabilities getCapabilities() const FL_NO_EXCEPT override { return {}; }
    bool canHandle(const ChannelDataPtr&) const FL_NO_EXCEPT override { return false; }
};

void transmit(ChannelDriverLcdClockless& driver) {
    fl::vector_psram<u8> bytes(3, 0);
    driver.enqueue(ChannelData::create(5, makeTimingConfig<TIMING_WS2812_800KHZ>(), fl::move(bytes)));
    driver.show();
}
}

FL_TEST_CASE("Worker defaults reach the real LCD driver") {
    FL_REQUIRE(WorkerConfig{}.isDefault());
    CRGB leds[1] = {};
    ChannelConfig channel("defaults", ClocklessChipset(), leds, RGB);
    FL_REQUIRE(channel.options.mWorker.isDefault());
    UnsupportedDriver unsupported;
    FL_REQUIRE(unsupported.configureWorker(WorkerConfig{}));
    WorkerConfig explicitConfig;
    explicitConfig.priority = Priority::Low;
    FL_REQUIRE(!unsupported.configureWorker(explicitConfig));

    auto& mock = LcdSpiPeripheralMock::instance();
    auto peripheral = fl::make_shared_no_tracking<ILcdSpiPeripheral>(mock);
    // No explicit configuration: defaults must reach the actual driver.
    {
        mock.reset();
        ChannelDriverLcdClockless driver(peripheral);
        FL_REQUIRE(driver.configureWorker(WorkerConfig{}));
        FL_REQUIRE(driver.workerDiagnostics().core == -2);
        transmit(driver);
        FL_REQUIRE(driver.workerDiagnostics().core == 1);
        FL_REQUIRE(driver.workerDiagnostics().priority == 12);
        WorkerConfig matching;
        matching.core = Core::Core1;
        matching.priority = Priority::Medium;
        FL_REQUIRE(driver.configureWorker(matching));
        matching.core = Core::Core0;
        FL_REQUIRE(!driver.configureWorker(matching));
        FL_REQUIRE(driver.configureWorker(WorkerConfig{}));
        mock.simulateTransmitComplete();
        driver.poll();
    }
    // First explicit request overrides defaults before creation. Auto fields
    // inherit defaults, then the first explicit policy locks shared settings.
    for (int scenario = 0; scenario < 3; ++scenario) {
        mock.reset();
        ChannelDriverLcdClockless driver(peripheral);
        WorkerConfig config;
        if (scenario != 1) { config.core = Core::Core0; }
        if (scenario != 0) { config.priority = Priority::Low; }
        FL_REQUIRE(driver.configureWorker(WorkerConfig{}));
        FL_REQUIRE(driver.configureWorker(config));
        WorkerConfig conflicting;
        conflicting.core = scenario == 1 ? Core::Core0 : Core::Core1;
        FL_REQUIRE(!driver.configureWorker(conflicting));
        transmit(driver);
        FL_REQUIRE(driver.workerDiagnostics().core == (scenario == 1 ? 1 : 0));
        FL_REQUIRE(driver.workerDiagnostics().priority == (scenario == 0 ? 12 : 1));
        FL_REQUIRE(driver.configureWorker(config));
        config.priority = Priority::High;
        FL_REQUIRE(!driver.configureWorker(config));
        mock.simulateTransmitComplete();
        driver.poll();
    }
}

} // FL_TEST_FILE
