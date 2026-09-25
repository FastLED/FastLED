/// @file tests/fl/channels/spi_legacy_golden.cpp
/// @brief Golden-byte lock-down for legacy `FastLED.addLeds<ESPIChipsets ...>`
///        under FASTLED_SPI_USES_CHANNEL_API (#4585 T1).
///
/// Purpose: the legacy addLeds<ESPIChipsets CHIPSET, DATA_PIN, CLOCK_PIN, ...>
/// overloads, when FASTLED_SPI_USES_CHANNEL_API is enabled (src/FastLED.h,
/// the `#elif FASTLED_SPI_USES_CHANNEL_API` block), build an
/// `fl::SpiChipsetConfig` + `fl::ChannelConfig` exactly the way
/// `fl::Channel::create(cfg)` is built directly, and route the frame through
/// the same `fl::Channel::encodeXXX()` free functions
/// (src/fl/channels/channel.cpp.hpp). This file pins that public-API
/// surface down for a later refactor (T2, which replaces the fl::Channel
/// back end with a slim controller) so the bytes a mock driver receives stay
/// byte-identical before and after.
///
/// Each case drives ONLY the public addLeds<>() + FastLED.show() API and
/// inspects the bytes a capturing mock IChannelDriver receives. Because the
/// legacy addLeds<>() overloads each keep a function-local `static
/// fl::ChannelPtr sChannel` (src/FastLED.h ~1167), every test case below
/// uses a distinct (DATA_PIN, CLOCK_PIN) pair so each addLeds<> call site is
/// instantiated exactly once, and the same instance is reused (and
/// re-registered on the shared, growing draw list) across the whole file.
/// Where a literal byte array is not safely hand-derivable (e.g. any case
/// that depends on the internal brightness/dither scaling pipeline), the
/// case instead builds the reference frame the same way `fl::Channel::create
/// (cfg)` (src/FastLED.h's own Channel-API addLeds<>() implementation) does,
/// and diffs against that -- marked `// differential: literal not
/// hand-derived`. That is not a weaker check: FastLED.h's Channel-API
/// addLeds<>() overloads construct the identical SpiEncoder/SpiChipsetConfig
/// /ChannelConfig and call the identical `fl::Channel::encodeXXX()`, so a
/// diff here still exercises (and pins) the real production code path.

#define FASTLED_SPI_USES_CHANNEL_API 1

#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/channels/manager.h"
#include "fl/chipsets/spi.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "crgb.h"
#include "test.h"

using namespace fl;

FL_TEST_FILE(FL_FILEPATH) {

namespace spi_legacy_golden {

/// Capturing mock driver: registered at a very high priority so it always
/// wins draw-list dispatch. Snapshots each enqueued frame's encoded bytes
/// (copied at enqueue time, since poll()/show() may free/reuse the source).
class SpiLegacyGoldenMock : public IChannelDriver {
  public:
    int enqueueCount = 0;
    int showCount = 0;
    fl::vector<fl::vector<u8>> capturedData;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override {
        return data && data->isSpi();
    }

    void enqueue(ChannelDataPtr data) FL_NO_EXCEPT override {
        if (!data) {
            return;
        }
        ++enqueueCount;
        const auto& src = data->getData();
        fl::vector<u8> copy;
        for (fl::size i = 0; i < src.size(); ++i) {
            copy.push_back(src[i]);
        }
        capturedData.push_back(copy);
        data->setInUse(false);
    }

    void show() FL_NO_EXCEPT override { ++showCount; }

    DriverState poll() FL_NO_EXCEPT override { return DriverState::READY; }

    fl::string getName() const FL_NO_EXCEPT override {
        return fl::string::from_literal("SPI_GOLDEN_MOCK");
    }

    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(false, true);  // SPI only
    }

    void reset() {
        enqueueCount = 0;
        showCount = 0;
        capturedData.clear();
    }
};

inline SpiLegacyGoldenMock& mockDriverInstance() {
    static SpiLegacyGoldenMock driver;
    return driver;
}

/// Registers the shared mock singleton exactly once for the whole file, at
/// a priority high enough to beat any other driver registered by other test
/// files sharing this process's ChannelManager.
inline void ensureMockRegistered() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    ChannelManager::instance().addDriver(100000,
        fl::make_shared_no_tracking(mockDriverInstance()));
}

/// Test fixture: clears capture state and normalizes FastLED global state
/// before each case. Does NOT remove previously-registered addLeds<>()
/// controllers -- they are function-local statics that persist for the
/// life of the process (see file header) -- so every FastLED.show() call
/// re-enqueues every controller/channel added by every earlier case in this
/// file. mockDriverInstance().capturedData.back() is therefore always the
/// most-recently-added channel's frame: the legacy path always appends its
/// static ChannelPtr's channel after all previously-existing ones.
struct SpiLegacyGoldenFixture {
    SpiLegacyGoldenFixture() {
        ensureMockRegistered();
        mockDriverInstance().reset();
        FastLED.setBrightness(255);
        FastLED.setDither(DISABLE_DITHER);
    }
};

/// Builds the "reference" frame the same way FastLED.h's Channel-API
/// addLeds<>() does: SpiEncoder -> SpiChipsetConfig -> ChannelConfig ->
/// FastLED.add(). Returns the captured bytes for that channel (always the
/// last entry captured by the immediately-following FastLED.show()).
template <typename... Unused>
struct Unused_ {};

}  // namespace spi_legacy_golden

using namespace spi_legacy_golden;

// ===========================================================================
// APA102
// ===========================================================================

FL_TEST_CASE("APA102 default overload (RGB order, default speed) golden bytes") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLeds<APA102, 40, 41>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& legacy = mockDriverInstance().capturedData.back();

    // Differential: build the same ChannelConfig directly, the way
    // FastLED.h's Channel-API addLeds<>() does for the default overload
    // (RGB order, default speed).
    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102);
    SpiChipsetConfig spiCfg(42, 43, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy.size(), reference.size());
    FL_CHECK_EQ(legacy, reference);

    // Structural invariants, hand-derived from
    // src/fl/chipsets/encoders/apa102.h: 4-byte zero start frame, then
    // [0xE0|bri5][B][G][R] per LED, then (n/32)+1 dwords of 0xFF.
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));
    FL_CHECK_EQ(legacy[0], 0x00);
    FL_CHECK_EQ(legacy[1], 0x00);
    FL_CHECK_EQ(legacy[2], 0x00);
    FL_CHECK_EQ(legacy[3], 0x00);
    FL_CHECK_EQ(legacy[4] & 0xE0, 0xE0);
    FL_CHECK_EQ(legacy[8] & 0xE0, 0xE0);
    FL_CHECK_EQ(legacy[12], 0xFF);
    FL_CHECK_EQ(legacy[13], 0xFF);
    FL_CHECK_EQ(legacy[14], 0xFF);
    FL_CHECK_EQ(legacy[15], 0xFF);

    FastLED.remove(refChannel);
}

FL_TEST_CASE("APA102 explicit BGR order golden bytes") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLeds<APA102, 44, 45, BGR>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102);
    SpiChipsetConfig spiCfg(46, 47, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), BGR);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));

    FastLED.remove(refChannel);
}

FL_TEST_CASE("APA102 brightness 64 golden bytes") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLeds<APA102, 48, 49>(leds, 2);
    FastLED.setBrightness(64);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived (global brightness scaling
    // pipeline is not safely hand-derivable byte-for-byte).
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102);
    SpiChipsetConfig spiCfg(50, 51, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));
    // FASTLED_USE_GLOBAL_BRIGHTNESS defaults to 0 (fastled_config.h), so
    // writeAPA102() takes the full-brightness path (apa102.h encodeAPA102,
    // hard-coded global_brightness=31): the 5-bit header field is pinned at
    // 0xE0|0x1F regardless of FastLED.setBrightness(); the RGB scaling
    // happens upstream in the pixel controller instead.
    FL_CHECK_EQ(legacy[4] & 0x1F, 0x1F);
    FL_CHECK_EQ(legacy[8] & 0x1F, 0x1F);

    FastLED.setBrightness(255);
    FastLED.remove(refChannel);
}

// ===========================================================================
// SK9822
// ===========================================================================

FL_TEST_CASE("SK9822 default overload golden bytes") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLeds<SK9822, 52, 53>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::SK9822);
    SpiChipsetConfig spiCfg(54, 55, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    // Same 4+4n+end framing as APA102 (encoders/apa102.h / sk9822.h share
    // the [0xE0|bri5][B][G][R] wire layout).
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));
    FL_CHECK_EQ(legacy[0], 0x00);
    FL_CHECK_EQ(legacy[1], 0x00);
    FL_CHECK_EQ(legacy[2], 0x00);
    FL_CHECK_EQ(legacy[3], 0x00);

    FastLED.remove(refChannel);
}

FL_TEST_CASE("SK9822 brightness 64 golden bytes") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLeds<SK9822, 56, 57>(leds, 2);
    FastLED.setBrightness(64);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::SK9822);
    SpiChipsetConfig spiCfg(58, 59, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));

    FastLED.setBrightness(255);
    FastLED.remove(refChannel);
}

// ===========================================================================
// HD107 (turbo APA102)
// ===========================================================================

FL_TEST_CASE("HD107 explicit RGB order golden bytes") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLeds<HD107, 60, 61, RGB>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived. HD107 shares Channel::encodeAPA102
    // (channel.cpp.hpp) with APA102 -- only the SpiEncoder's default clock_hz
    // differs (40 MHz vs 6 MHz), which does not affect the emitted bytes.
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::HD107);
    SpiChipsetConfig spiCfg(62, 63, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));

    FastLED.remove(refChannel);
}

// ===========================================================================
// HD108
// ===========================================================================

FL_TEST_CASE("HD108 default overload defaults to GRB order") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLeds<HD108, 64, 65>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& legacy = mockDriverInstance().capturedData.back();

    // FastLED.h's default-order Channel-API overload special-cases HD108 to
    // GRB (src/FastLED.h: "HD108 defaults to GRB; all other SPI chipsets
    // default to RGB"). Build the differential reference with GRB explicitly
    // to pin that behaviour.
    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::HD108);
    SpiChipsetConfig spiCfg(66, 67, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), GRB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);

    // Structural invariants from src/fl/chipsets/encoders/pixel_iterator.h
    // PixelIterator::writeHD108(): 8-byte zero start frame, 2-byte gain
    // header (0xFF 0xFF per encoder_utils.h hd108BrightnessHeader) + 3x
    // 16-bit big-endian channels per LED, then (n/2 + 4) bytes of 0xFF.
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(8 + 2 * 8 + (2 / 2 + 4)));
    FL_CHECK_EQ(legacy[0], 0x00);
    FL_CHECK_EQ(legacy[7], 0x00);
    FL_CHECK_EQ(legacy[8], 0xFF);
    FL_CHECK_EQ(legacy[9], 0xFF);

    FastLED.remove(refChannel);
}

FL_TEST_CASE("HD108 explicit RGB order golden bytes") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLeds<HD108, 68, 69, RGB>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::HD108);
    SpiChipsetConfig spiCfg(70, 71, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(8 + 2 * 8 + (2 / 2 + 4)));

    FastLED.remove(refChannel);
}

// ===========================================================================
// WS2801
// ===========================================================================

FL_TEST_CASE("WS2801 golden bytes: 3 bytes/LED, no frame overhead") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLeds<WS2801, 72, 73>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::WS2801);
    SpiChipsetConfig spiCfg(74, 75, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    // Structural invariant, hand-derived from
    // src/fl/chipsets/encoders/ws2801.h: exactly 3 RGB-order bytes per LED,
    // no start/end frame.
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(3 * 2));

    FastLED.remove(refChannel);
}

// ===========================================================================
// LPD8806
// ===========================================================================

FL_TEST_CASE("LPD8806 golden bytes: GRB MSB-set encoding + latch") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[3] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80), CRGB(0x00, 0x00, 0x00)};
    FastLED.addLeds<LPD8806, 76, 77>(leds, 3);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::LPD8806);
    SpiChipsetConfig spiCfg(78, 79, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 3), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);

    // Structural invariants from src/fl/chipsets/encoders/lpd8806.h:
    // 3 GRB bytes/LED (each with MSB 0x80 set), then
    // (num_leds*3 + 63) / 64 zero latch bytes.
    const fl::size ledBytes = 3 * 3;
    const fl::size latchBytes = (3 * 3 + 63) / 64;
    FL_REQUIRE_EQ(legacy.size(), ledBytes + latchBytes);
    for (fl::size i = 0; i < ledBytes; ++i) {
        FL_CHECK_EQ(legacy[i] & 0x80, 0x80);
    }
    for (fl::size i = ledBytes; i < legacy.size(); ++i) {
        FL_CHECK_EQ(legacy[i], 0x00);
    }

    FastLED.remove(refChannel);
}

// ===========================================================================
// P9813
// ===========================================================================

FL_TEST_CASE("P9813 golden bytes: flag-byte encoding + boundaries") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLeds<P9813, 80, 81>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::P9813);
    SpiChipsetConfig spiCfg(82, 83, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);

    // Structural invariants from src/fl/chipsets/encoders/p9813.h:
    // 4-byte zero start boundary, [flag][B][G][R] per LED (flag = 0xC0 |
    // checksum), 4-byte zero end boundary.
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));
    FL_CHECK_EQ(legacy[0], 0x00);
    FL_CHECK_EQ(legacy[1], 0x00);
    FL_CHECK_EQ(legacy[2], 0x00);
    FL_CHECK_EQ(legacy[3], 0x00);
    FL_CHECK_EQ(legacy[4] & 0xC0, 0xC0);
    FL_CHECK_EQ(legacy[8] & 0xC0, 0xC0);
    const fl::size endOffset = 4 + 4 * 2;
    FL_CHECK_EQ(legacy[endOffset + 0], 0x00);
    FL_CHECK_EQ(legacy[endOffset + 1], 0x00);
    FL_CHECK_EQ(legacy[endOffset + 2], 0x00);
    FL_CHECK_EQ(legacy[endOffset + 3], 0x00);

    FastLED.remove(refChannel);
}

// ===========================================================================
// Full 5-param form: chipset, pins, order, and explicit data rate
// ===========================================================================

FL_TEST_CASE("APA102 full 5-param form (explicit DATA_RATE_MHZ) golden bytes") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    FastLED.addLeds<APA102, 84, 85, RGB, DATA_RATE_MHZ(12)>(leds, 2);

    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& legacy = mockDriverInstance().capturedData.back();

    // differential: literal not hand-derived. DATA_RATE_MHZ(12) only changes
    // the SpiEncoder's clock_hz, which is not part of the encoded byte
    // stream -- pinned here for completeness of the addLeds<> overload set.
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102, DATA_RATE_MHZ(12));
    SpiChipsetConfig spiCfg(86, 87, encoder);
    ChannelConfig cfg(spiCfg, fl::span<CRGB>(leds, 2), RGB);
    ChannelPtr refChannel = FastLED.add(cfg);
    FL_REQUIRE(refChannel != nullptr);
    mockDriverInstance().reset();
    FastLED.show();
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& reference = mockDriverInstance().capturedData.back();

    FL_CHECK_EQ(legacy, reference);
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));

    FastLED.remove(refChannel);
}

// ===========================================================================
// Bus pinning: host/stub has no BusTraits<Bus::SPI> specialization, so pin
// the host's actual DefaultBus<SpiChipsetConfig>::value (Bus::BIT_BANG on
// FL_IS_STUB / FL_IS_WASM, src/fl/channels/bus.h) instead of Bus::SPI.
// ===========================================================================

FL_TEST_CASE("APA102 with explicit host default bus compiles and routes consistently") {
    SpiLegacyGoldenFixture fixture;
    CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};
    constexpr fl::Bus kHostDefaultSpiBus = fl::DefaultBus<fl::SpiChipsetConfig>::value;
    ::CLEDController& controller =
        FastLED.addLeds<APA102, 88, 89, RGB, DATA_RATE_MHZ(6), kHostDefaultSpiBus>(leds, 2);
    (void)controller;

    // Does not crash, and the frame is still routed to the SPI-accepting
    // mock driver consistently with every other case in this file.
    FastLED.show();
    FL_CHECK_GT(mockDriverInstance().enqueueCount, 0);
    FL_REQUIRE_FALSE(mockDriverInstance().capturedData.empty());
    const auto& legacy = mockDriverInstance().capturedData.back();
    FL_REQUIRE_EQ(legacy.size(), (fl::size)(4 + 4 * 2 + 4));
}

}  // FL_TEST_FILE
