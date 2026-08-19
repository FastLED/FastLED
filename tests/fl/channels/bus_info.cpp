/// @file bus_info.cpp
/// @brief Tests for portable bus introspection.

#include "fl/channels/bus.h"
#include "fl/channels/bus_info.h"
#include "fl/channels/bus_info.json.h"
#include "fl/stl/string.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_STATIC_ASSERT(fl::BusInstanceCount<fl::Bus::BIT_BANG>::value == 1,
                 "BIT_BANG has one portable instance");

#if defined(FL_IS_TEENSY_4X)
FL_STATIC_ASSERT(fl::BusInstanceCount<fl::Bus::FLEX_IO>::value == 2,
                 "Teensy 4.x exposes ObjectFLED and FlexIO through FLEX_IO");
FL_STATIC_ASSERT(fl::BusInstanceCount<fl::Bus::SPI>::value == 4,
                 "Teensy 4.x exposes four SPI entries in the portable table");
FL_STATIC_ASSERT(fl::BusInstanceCount<fl::Bus::UART>::value == 1,
                 "Teensy 4.x exposes one UART entry");
#elif defined(FL_IS_ESP_32S3)
FL_STATIC_ASSERT(fl::BusInstanceCount<fl::Bus::FLEX_IO>::value == 2,
                 "ESP32-S3 exposes two FLEX_IO entries");
FL_STATIC_ASSERT(fl::BusInstanceCount<fl::Bus::SPI>::value == 2,
                 "ESP32-S3 exposes two SPI entries");
FL_STATIC_ASSERT(fl::BusInstanceCount<fl::Bus::UART>::value == 3,
                 "ESP32-S3 exposes three UART entries");
#elif defined(FL_IS_ESP_32C6)
FL_STATIC_ASSERT(fl::BusInstanceCount<fl::Bus::FLEX_IO>::value == 2,
                 "ESP32-C6 exposes two FLEX_IO entries");
FL_STATIC_ASSERT(fl::BusInstanceCount<fl::Bus::SPI>::value == 1,
                 "ESP32-C6 exposes one SPI entry");
FL_STATIC_ASSERT(fl::BusInstanceCount<fl::Bus::UART>::value == 2,
                 "ESP32-C6 exposes two UART entries");
#elif defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
FL_STATIC_ASSERT(fl::BusInstanceCount<fl::Bus::SPI>::value == 2,
                 "RP2xxx exposes two SPI entries");
FL_STATIC_ASSERT(fl::BusInstanceCount<fl::Bus::UART>::value == 2,
                 "RP2xxx exposes two UART entries");
#endif

FL_TEST_CASE("deviceInfo reports BIT_BANG") {
    auto info = fl::deviceInfo<fl::Bus::BIT_BANG>();
    FL_CHECK(info.bus == fl::Bus::BIT_BANG);
    FL_CHECK_EQ(info.which, 0);
    FL_CHECK_EQ(fl::string(info.bus_name), fl::string("BIT_BANG"));
    FL_CHECK_FALSE(info.is_noop);
    FL_CHECK(info.runtime.available);
}

FL_TEST_CASE("deviceInfo reports UART Which0") {
    auto info = fl::deviceInfo<fl::Bus::UART, 0>();
    FL_CHECK(info.bus == fl::Bus::UART);
    FL_CHECK_EQ(info.which, 0);
    FL_CHECK_EQ(fl::string(info.bus_name), fl::string("UART"));
}

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
FL_TEST_CASE("RP UART affinity names and device metadata preserve Which") {
    FL_CHECK_EQ(fl::string(fl::busDriverName(fl::Bus::UART, 0)),
                fl::string("UART0"));
    FL_CHECK_EQ(fl::string(fl::busDriverName(fl::Bus::UART, 1)),
                fl::string("UART1"));

    auto uart0 = fl::deviceInfo<fl::Bus::UART, 0>();
    auto uart1 = fl::deviceInfo<fl::Bus::UART, 1>();
    FL_CHECK_EQ(uart0.which, 0);
    FL_CHECK_EQ(uart1.which, 1);
    FL_CHECK_FALSE(uart0.is_noop);
    FL_CHECK_FALSE(uart1.is_noop);
    FL_CHECK(uart0.runtime.available);
    FL_CHECK(uart1.runtime.available);
    FL_CHECK_EQ(fl::string(uart0.vendor_name), fl::string("PL011"));
    FL_CHECK_EQ(fl::string(uart1.vendor_name), fl::string("PL011"));
    FL_CHECK_EQ(fl::string(uart0.device_name), fl::string("RP UART0"));
    FL_CHECK_EQ(fl::string(uart1.device_name), fl::string("RP UART1"));
}
#endif

FL_TEST_CASE("deviceInfo reports SPI entries and out-of-range no-op") {
    auto spi0 = fl::deviceInfo<fl::Bus::SPI, 0>();
    FL_CHECK(spi0.bus == fl::Bus::SPI);
    FL_CHECK_EQ(spi0.which, 0);

    auto spi99 = fl::deviceInfo<fl::Bus::SPI, 99>();
    FL_CHECK(spi99.bus == fl::Bus::SPI);
    FL_CHECK_EQ(spi99.which, 99);
    FL_CHECK(spi99.is_noop);
    FL_CHECK_FALSE(spi99.runtime.available);
}

FL_TEST_CASE("deviceInfo reports FLEX_IO and RMT") {
    auto flex0 = fl::deviceInfo<fl::Bus::FLEX_IO, 0>();
    FL_CHECK(flex0.bus == fl::Bus::FLEX_IO);
    FL_CHECK_EQ(flex0.which, 0);
    FL_CHECK_EQ(fl::string(flex0.bus_name), fl::string("FLEX_IO"));

    auto rmt0 = fl::deviceInfo<fl::Bus::RMT, 0>();
    FL_CHECK(rmt0.bus == fl::Bus::RMT);
    FL_CHECK_EQ(rmt0.which, 0);
    FL_CHECK_EQ(fl::string(rmt0.bus_name), fl::string("RMT"));
}

FL_TEST_CASE("deviceJson includes vendor_name, which, and runtime") {
    fl::json json = fl::deviceJson<fl::Bus::BIT_BANG>();
    FL_CHECK_EQ(json["vendor_name"] | fl::string(""), fl::string("BIT_BANG"));
    FL_CHECK_EQ(json["which"] | -1, 0);
    FL_CHECK(json["runtime"]["available"] | false);
}

}  // FL_TEST_FILE
