/// @file spi_force_software.cpp
/// @brief Test that SPIBusManager respects FASTLED_FORCE_SOFTWARE_SPI
///
/// This test verifies device registration works with the bus manager and that
/// the proper bus type is assigned. When FASTLED_FORCE_SOFTWARE_SPI is defined,
/// the bus manager forces SOFT_SPI mode while preserving the proxy architecture.

#include "test.h"
#include "platforms/shared/spi_manager.h"

using namespace fl;

namespace {

class TestSpiHw2 : public SpiHw2 {
  public:
    explicit TestSpiHw2(int busId) : mBusId(busId) {}
    bool begin(const Config& config) FL_NO_EXCEPT override {
        mInitialized = true;
        mConfiguredBus = config.bus_num;
        return true;
    }
    void end() FL_NO_EXCEPT override { mInitialized = false; }
    DMABuffer acquireDMABuffer(size_t size) FL_NO_EXCEPT override { return DMABuffer(size); }
    bool transmit(TransmitMode) FL_NO_EXCEPT override { return true; }
    bool waitComplete(u32) FL_NO_EXCEPT override { return true; }
    bool isBusy() const FL_NO_EXCEPT override { return false; }
    bool isInitialized() const FL_NO_EXCEPT override { return mInitialized; }
    int getBusId() const FL_NO_EXCEPT override { return mBusId; }
    const char* getName() const FL_NO_EXCEPT override { return "test-spi"; }
    int configuredBus() const { return mConfiguredBus; }

  private:
    int mBusId;
    int mConfiguredBus = -1;
    bool mInitialized = false;
};

} // namespace

FL_TEST_CASE("SPIBusManager - device registration and bus type") {
    SPIBusManager& manager = getSPIBusManager();
    manager.reset();

    // Register a device
    SPIBusHandle handle = manager.registerDevice(
        18,  // clock_pin
        23,  // data_pin
        1000000,  // speed
        (void*)0x1234  // dummy controller
    );

    FL_CHECK(handle.is_valid);

    // Initialize
    manager.initialize();

    // Verify bus info is available
    const SPIBusInfo* bus = manager.getBusInfo(handle.bus_id);
    FL_CHECK(bus != nullptr);

    // Verify bus type based on FASTLED_FORCE_SOFTWARE_SPI define
    // When the define is set, bus_type should be SOFT_SPI
    // When the define is NOT set, bus_type should be SINGLE_SPI for one device
#if defined(FASTLED_FORCE_SOFTWARE_SPI)
    FL_CHECK(bus->bus_type == SPIBusType::SOFT_SPI);
#else
    FL_CHECK(bus->bus_type == SPIBusType::SINGLE_SPI);
#endif

    manager.reset();
}

FL_TEST_CASE("SPIBusManager - explicit ESP32 hosts are independent") {
    SPIBusManager& manager = getSPIBusManager();
    manager.reset();

    SPIBusHandle spi2 = manager.registerDevice(18, 23, 1000000,
                                                (void*)0x1234,
                                                Esp32SpiBus::HOST2);
    SPIBusHandle spi3 = manager.registerDevice(18, 19, 1000000,
                                                (void*)0x5678,
                                                Esp32SpiBus::HOST3);

    FL_REQUIRE_TRUE(spi2.is_valid);
    FL_REQUIRE_TRUE(spi3.is_valid);
    FL_CHECK_NE(spi2.bus_id, spi3.bus_id);
    FL_CHECK_EQ(manager.getBusInfo(spi2.bus_id)->requested_bus, Esp32SpiBus::HOST2);
    FL_CHECK_EQ(manager.getBusInfo(spi3.bus_id)->requested_bus, Esp32SpiBus::HOST3);
    manager.reset();
}

FL_TEST_CASE("SPIBusManager - AUTO preserves clock-pin grouping") {
    SPIBusManager& manager = getSPIBusManager();
    manager.reset();

    SPIBusHandle first = manager.registerDevice(18, 23, 1000000, (void*)0x1234);
    SPIBusHandle second = manager.registerDevice(18, 19, 1000000, (void*)0x5678);

    FL_REQUIRE_TRUE(first.is_valid);
    FL_REQUIRE_TRUE(second.is_valid);
    FL_CHECK_EQ(first.bus_id, second.bus_id);
    FL_CHECK_EQ(manager.getBusInfo(first.bus_id)->requested_bus, Esp32SpiBus::AUTO);
    manager.reset();
}

FL_TEST_CASE("SPIBusManager - explicit host controls multi-lane allocation") {
    (void)SpiHw2::getAll();
    SpiHw2::clearInstances();
    auto spi2 = fl::make_shared<TestSpiHw2>(2);
    auto spi3 = fl::make_shared<TestSpiHw2>(3);
    SpiHw2::registerInstance(spi2);
    SpiHw2::registerInstance(spi3);

    SPIBusManager& manager = getSPIBusManager();
    manager.reset();
    manager.registerDevice(18, 23, 1000000, (void*)0x1234, Esp32SpiBus::HOST3);
    SPIBusHandle handle = manager.registerDevice(
        18, 19, 1000000, (void*)0x5678, Esp32SpiBus::HOST3);

    FL_REQUIRE_TRUE(manager.initialize());
    FL_CHECK_FALSE(spi2->isInitialized());
    FL_CHECK_TRUE(spi3->isInitialized());
    FL_CHECK_EQ(spi3->configuredBus(), 3);
    FL_CHECK_EQ(manager.getBusInfo(handle.bus_id)->spi_bus_num, 3);

    manager.reset();
    SpiHw2::clearInstances();
}

FL_TEST_CASE("SPIBusManager - AUTO keeps first-available allocation") {
    (void)SpiHw2::getAll();
    SpiHw2::clearInstances();
    auto first = fl::make_shared<TestSpiHw2>(7);
    auto second = fl::make_shared<TestSpiHw2>(8);
    SpiHw2::registerInstance(first);
    SpiHw2::registerInstance(second);

    SPIBusManager& manager = getSPIBusManager();
    manager.reset();
    manager.registerDevice(18, 23, 1000000, (void*)0x1234);
    SPIBusHandle handle = manager.registerDevice(18, 19, 1000000, (void*)0x5678);

    FL_REQUIRE_TRUE(manager.initialize());
    FL_CHECK_TRUE(first->isInitialized());
    FL_CHECK_FALSE(second->isInitialized());
    FL_CHECK_EQ(manager.getBusInfo(handle.bus_id)->spi_bus_num, 7);

    manager.reset();
    SpiHw2::clearInstances();
}
