/// @file channel_engine_rp_bitbang.cpp
/// @brief Host tests for the RP M0 bit-bang edge schedule (#4635).

#include "test.h"

#include "fl/chipsets/led_timing.h"
#include "fl/stl/vector.h"
#include "platforms/arm/rp/rpcommon/channel_engine_rp_bitbang.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

struct Edge {
    bool high;
    u32 hold;
};

class RecordingPin final : public IRpBitBangPin {
  public:
    void set(bool high, u32 holdCycles) FL_NO_EXCEPT override {
        edges.push_back(Edge{high, holdCycles});
    }
    fl::vector<Edge> edges;
};

constexpr u32 kCpuHz = 125000000;

void checkByte(const RecordingPin& pin, const RpBitBangBitCycles& c, u8 byte,
               size_t firstPair) {
    for (int i = 0; i < 8; ++i) {
        const bool one = ((byte >> (7 - i)) & 1u) != 0;
        const Edge& hi = pin.edges[(firstPair + i) * 2];
        const Edge& lo = pin.edges[(firstPair + i) * 2 + 1];
        FL_CHECK(hi.high);
        FL_CHECK_FALSE(lo.high);
        FL_CHECK_EQ(hi.hold, one ? c.t1t2 : c.t1);
        FL_CHECK_EQ(hi.hold + lo.hold, c.period);
    }
}

}  // namespace

FL_TEST_CASE("RP bit-bang ns to cycles") {
    FL_CHECK_EQ(rpBitBangCycles(1250, 125000000), 156u);
    FL_CHECK_EQ(rpBitBangCycles(0, 125000000), 0u);
}

FL_TEST_CASE("RP bit-bang emits MSB-first WS2812 edge schedule") {
    const ChipsetTimingConfig timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    const RpBitBangBitCycles c = rpBitBangBitCycles(timing, kCpuHz);
    FL_CHECK_EQ(c.t1, rpBitBangCycles(timing.t1_ns, kCpuHz));
    FL_CHECK_EQ(c.t1t2, rpBitBangCycles(timing.t1_ns + timing.t2_ns, kCpuHz));
    FL_CHECK_EQ(c.period, rpBitBangCycles(timing.total_period_ns(), kCpuHz));

    RecordingPin pin;
    const u8 byte = 0xA0;
    rpBitBangEmitBytes(pin, c, &byte, 1, 0);
    FL_REQUIRE_EQ(pin.edges.size(), static_cast<size_t>(16));
    checkByte(pin, c, byte, 0);
}

FL_TEST_CASE("RP bit-bang appends extra zero bits per byte") {
    const ChipsetTimingConfig timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    const RpBitBangBitCycles c = rpBitBangBitCycles(timing, kCpuHz);

    RecordingPin pin;
    const u8 byte = 0xA0;
    rpBitBangEmitBytes(pin, c, &byte, 1, 1);
    FL_REQUIRE_EQ(pin.edges.size(), static_cast<size_t>(18));
    checkByte(pin, c, byte, 0);
    FL_CHECK(pin.edges[16].high);
    FL_CHECK_EQ(pin.edges[16].hold, c.t1);
    FL_CHECK_FALSE(pin.edges[17].high);
    FL_CHECK_EQ(pin.edges[16].hold + pin.edges[17].hold, c.period);
}

}  // FL_TEST_FILE
