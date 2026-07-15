// RP2040/RP2350 fixed-SPI byte-loopback AutoResearch RPC.

#include "fl/system/sketch_macros.h"
#if !defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && !FL_PLATFORM_HAS_LARGE_MEMORY
#define FASTLED_AUTORESEARCH_LOW_MEMORY 1
#endif
#if !(defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && FASTLED_AUTORESEARCH_LOW_MEMORY)

#include "AutoResearchRemote.h"

#include <Arduino.h>

#include "FastLED.h"
#include "fl/channels/data.h"
#include "fl/chipsets/spi.h"
#include "fl/remote/remote.h"
#include "fl/stl/json.h"
#include "fl/stl/move.h"
#include "fl/stl/vector.h"
#include "platforms/arm/rp/is_rp.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
#include "platforms/arm/rp/rpcommon/rp_spi_bus_traits.h"
#endif

namespace {

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

constexpr fl::u8 kLoopbackPattern[] = {
    0x00, 0xFF, 0x55, 0xAA, 0x12, 0x34, 0x56, 0x78,
    0x87, 0x65, 0x43, 0x21, 0x0F, 0xF0, 0xCC, 0x33,
    0x11, 0xEE, 0x22, 0xDD, 0x44, 0xBB, 0x88, 0x77,
    0x66, 0x99, 0x3C, 0xC3, 0x5A, 0xA5, 0x7E, 0x81,
};

bool parseInt(const fl::json& config, const char* key, int* value) {
    if (!config.contains(key) || !config[key].is_int()) return false;
    *value = static_cast<int>(config[key].as_int().value());
    return true;
}

template <fl::u8 Which>
fl::json runLoopback(int mosi_pin, int miso_pin, int sck_pin, int clock_hz) {
    fl::json response = fl::json::object();
    const int expected_mosi = Which == 0 ? 3 : 11;
    const int expected_miso = Which == 0 ? 0 : 8;
    const int expected_sck = Which == 0 ? 2 : 10;
    if (mosi_pin != expected_mosi || miso_pin != expected_miso ||
        sck_pin != expected_sck) {
        response.set("success", false);
        response.set("error", "UnsupportedPins");
        response.set("message", "Pins do not select the requested RP fixed SPI instance.");
        return response;
    }
    if (clock_hz < 100000 || clock_hz > 62500000) {
        response.set("success", false);
        response.set("error", "InvalidClock");
        response.set("message", "clock_hz must be in [100000, 62500000].");
        return response;
    }

    fl::vector_psram<fl::u8> tx;
    tx.assign(kLoopbackPattern, kLoopbackPattern + sizeof(kLoopbackPattern));
    fl::u8 captured[sizeof(kLoopbackPattern)] = {};
    auto channel = fl::ChannelData::create(
        fl::SpiChipsetConfig(mosi_pin, sck_pin,
                             fl::SpiEncoder::apa102(static_cast<fl::u32>(clock_hz))),
        fl::move(tx));
    auto& driver = fl::BusTraits<fl::Bus::SPI, Which>::instance();
    if (!driver.captureNextRxBytes(captured, sizeof(captured))) {
        response.set("success", false);
        response.set("error", "DriverBusy");
        response.set("message", "RP SPI driver was busy before the loopback transfer.");
        return response;
    }
    driver.enqueue(channel);
    driver.show();

    fl::IChannelDriver::DriverState state = fl::IChannelDriver::DriverState::BUSY;
    const unsigned long start_ms = millis();
    while (millis() - start_ms < 2000) {
        state = driver.poll();
        if (state == fl::IChannelDriver::DriverState::READY ||
            state == fl::IChannelDriver::DriverState::ERROR) {
            break;
        }
        delay(1);
    }

    bool exact_match = state == fl::IChannelDriver::DriverState::READY;
    fl::json expected = fl::json::array();
    fl::json received = fl::json::array();
    for (size_t index = 0; index < sizeof(kLoopbackPattern); ++index) {
        expected.push_back(static_cast<int64_t>(kLoopbackPattern[index]));
        received.push_back(static_cast<int64_t>(captured[index]));
        exact_match = exact_match && captured[index] == kLoopbackPattern[index];
    }

    fl::json data = fl::json::object();
    data.set("driver", Which == 0 ? "SPI0" : "SPI1");
    data.set("mosiPin", static_cast<int64_t>(mosi_pin));
    data.set("misoPin", static_cast<int64_t>(miso_pin));
    data.set("sckPin", static_cast<int64_t>(sck_pin));
    data.set("requestedClockHz", static_cast<int64_t>(clock_hz));
    data.set("actualClockHz", static_cast<int64_t>(driver.lastActualClockHz()));
    data.set("wireIdle", state == fl::IChannelDriver::DriverState::READY);
    data.set("expected", expected);
    data.set("received", received);
    response.set("success", exact_match);
    response.set("message", exact_match ? "RP SPI loopback passed." : "RP SPI loopback failed.");
    response.set("data", data);
    return response;
}

template <bool Sk9822>
fl::json runPublicApiLoopback(int pattern) {
    constexpr fl::u8 kMosiPin = 11;
    constexpr fl::u8 kSckPin = 10;
    constexpr size_t kLedCount = 257;
    constexpr size_t kFrameBytes = 4 + (kLedCount * 4) + 36;
    static CRGB leds[kLedCount];
    static bool initialized = false;
    fl::json response = fl::json::object();

    if (!initialized) {
        if constexpr (Sk9822) {
            FastLED.addLeds<SK9822, kMosiPin, kSckPin, RGB, 8000000,
                            fl::Bus::SPI, 1>(leds, kLedCount);
        } else {
            FastLED.addLeds<APA102, kMosiPin, kSckPin, RGB, 8000000,
                            fl::Bus::SPI, 1>(leds, kLedCount);
        }
        initialized = true;
    }

    for (size_t index = 0; index < kLedCount; ++index) leds[index] = CRGB::Black;
    switch (pattern) {
        case 1: for (size_t index = 0; index < kLedCount; ++index) leds[index] = CRGB::Red; break;
        case 2: for (size_t index = 0; index < kLedCount; ++index) leds[index] = CRGB::Green; break;
        case 3: for (size_t index = 0; index < kLedCount; ++index) leds[index] = CRGB::Blue; break;
        case 4: for (size_t index = 0; index < kLedCount; ++index) leds[index] = CRGB::White; break;
        case 5: leds[16] = CRGB::Red; break;
        default: break;
    }
    if (pattern < 0 || pattern > 5) {
        response.set("success", false);
        response.set("error", "InvalidPattern");
        return response;
    }

    fl::u8 captured[kFrameBytes] = {};
    auto& driver = fl::BusTraits<fl::Bus::SPI, 1>::instance();
    if (!driver.captureNextRxBytes(captured, sizeof(captured))) {
        response.set("success", false);
        response.set("error", "DriverBusy");
        return response;
    }
    FastLED.show();
    FastLED.wait(2000);

    bool exact = driver.lastActualClockHz() > 0;
    for (size_t index = 0; index < 4; ++index) exact = exact && captured[index] == 0x00;
    for (size_t led = 0; led < kLedCount; ++led) {
        const size_t offset = 4 + (led * 4);
        exact = exact && captured[offset] == 0xFF;
        exact = exact && captured[offset + 1] == leds[led].r;
        exact = exact && captured[offset + 2] == leds[led].g;
        exact = exact && captured[offset + 3] == leds[led].b;
    }
    for (size_t index = 4 + (kLedCount * 4); index < kFrameBytes; ++index) {
        exact = exact && captured[index] == 0xFF;
    }

    fl::json data = fl::json::object();
    data.set("chipset", Sk9822 ? "SK9822" : "APA102");
    data.set("pattern", static_cast<int64_t>(pattern));
    data.set("actualClockHz", static_cast<int64_t>(driver.lastActualClockHz()));
    data.set("wireIdle", true);
    data.set("frameBytes", static_cast<int64_t>(kFrameBytes));
    response.set("success", exact);
    response.set("message", exact ? "RP SPI public API loopback passed."
                                  : "RP SPI public API loopback failed.");
    response.set("data", data);
    return response;
}

#endif

}  // namespace

void AutoResearchRemoteControl::bindRpSpiMethods(fl::Remote& remote) {
    remote.bind("rpSpiLoopback", [this](const fl::json& args) -> fl::json {
#if !defined(FL_IS_RP2040) && !defined(FL_IS_RP2350)
        (void)args;
        fl::json response = fl::json::object();
        response.set("success", false);
        response.set("error", "PlatformNotSupported");
        response.set("message", "rpSpiLoopback requires an RP2040 or RP2350 target.");
        return response;
#else
        // AutoResearch's GPIO pre-test owns its RX pin through a PIO receive
        // channel. Release that diagnostic reservation before fixed SPI claims
        // the same MISO pin through the RP resource manager.
        if (mState) mState->rx_channel.reset();
        if (!args.is_object()) {
            fl::json response = fl::json::object();
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message", "Expected an object with spi_index, mosi_pin, miso_pin, sck_pin, and clock_hz.");
            return response;
        }
        const fl::json config = args;
        int spi_index = -1;
        int mosi_pin = -1;
        int miso_pin = -1;
        int sck_pin = -1;
        int clock_hz = 0;
        if (!parseInt(config, "spi_index", &spi_index) ||
            !parseInt(config, "mosi_pin", &mosi_pin) ||
            !parseInt(config, "miso_pin", &miso_pin) ||
            !parseInt(config, "sck_pin", &sck_pin) ||
            !parseInt(config, "clock_hz", &clock_hz)) {
            fl::json response = fl::json::object();
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message", "spi_index, mosi_pin, miso_pin, sck_pin, and clock_hz must be integers.");
            return response;
        }
        if (spi_index == 0) return runLoopback<0>(mosi_pin, miso_pin, sck_pin, clock_hz);
        if (spi_index == 1) return runLoopback<1>(mosi_pin, miso_pin, sck_pin, clock_hz);
        fl::json response = fl::json::object();
        response.set("success", false);
        response.set("error", "InvalidSpiIndex");
        response.set("message", "spi_index must be 0 or 1.");
        return response;
#endif
    });
    remote.bind("rpSpiPublicApiLoopback", [this](const fl::json& args) -> fl::json {
#if !defined(FL_IS_RP2040) && !defined(FL_IS_RP2350)
        (void)args;
        fl::json response = fl::json::object();
        response.set("success", false);
        response.set("error", "PlatformNotSupported");
        return response;
#else
        if (mState) mState->rx_channel.reset();
        if (!args.is_object() || !args.contains("sk9822") ||
            !args["sk9822"].is_bool() || !args.contains("pattern") ||
            !args["pattern"].is_int()) {
            fl::json response = fl::json::object();
            response.set("success", false);
            response.set("error", "InvalidArgs");
            return response;
        }
        const bool sk9822 = args["sk9822"].as_bool().value();
        const int pattern = static_cast<int>(args["pattern"].as_int().value());
        return sk9822 ? runPublicApiLoopback<true>(pattern)
                      : runPublicApiLoopback<false>(pattern);
#endif
    });
}

#endif  // !(defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && FASTLED_AUTORESEARCH_LOW_MEMORY)
