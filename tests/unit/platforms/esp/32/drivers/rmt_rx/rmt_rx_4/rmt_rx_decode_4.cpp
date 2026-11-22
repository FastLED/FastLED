#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_4/rmt_rx_decode_4.h"

#include "fl/stl/vector.h"
#include "test.h"

namespace {

fl::ChipsetTiming4Phase rmt4DecodeTiming() {
    fl::ChipsetTiming4Phase timing;
    timing.t0h_min_ns = 150;
    timing.t0h_max_ns = 300;
    timing.t0l_min_ns = 900;
    timing.t0l_max_ns = 1100;
    timing.t1h_min_ns = 500;
    timing.t1h_max_ns = 650;
    timing.t1l_min_ns = 550;
    timing.t1l_max_ns = 750;
    timing.reset_min_us = 50;
    timing.gap_tolerance_ns = 0;
    return timing;
}

// Classic ESP32 RX starts with idle LOW and pairs LOW/HIGH in its hardware
// symbols. This wire capture carries 0xA5, ending after the final HIGH.
fl::vector<fl::EdgeTime> rmt4LowFirstCapture() {
    return {{false, 71125},
            {true, 575}, {false, 625},
            {true, 225}, {false, 975},
            {true, 575}, {false, 625},
            {true, 225}, {false, 975},
            {true, 225}, {false, 975},
            {true, 575}, {false, 625},
            {true, 225}, {false, 975},
            {true, 575}};
}

} // namespace

FL_TEST_CASE("RMT4 RX decodes LOW/HIGH symbol phase and final HIGH") {
    const auto edges = rmt4LowFirstCapture();
    fl::u8 out[1] = {};
    const auto result = fl::detail::decodeRmt4Edges(
        rmt4DecodeTiming(), edges, out);
    FL_REQUIRE(result.ok());
    FL_CHECK(result.value() == 1u);
    FL_CHECK(out[0] == 0xA5);
}

FL_TEST_CASE("RMT4 RX accepts HIGH/LOW phase with a complete final bit") {
    auto edges = rmt4LowFirstCapture();
    edges.erase(edges.begin());
    edges.push_back({false, 625});
    fl::u8 out[1] = {};
    const auto result = fl::detail::decodeRmt4Edges(
        rmt4DecodeTiming(), edges, out);
    FL_REQUIRE(result.ok());
    FL_CHECK(result.value() == 1u);
    FL_CHECK(out[0] == 0xA5);
}

FL_TEST_CASE("RMT4 RX retains last bit before stretched reset LOW") {
    auto edges = rmt4LowFirstCapture();
    edges.push_back({false, 60000});
    edges.push_back({true, 50}); // Data beyond the frame reset is ignored.
    fl::u8 out[1] = {};
    const auto result = fl::detail::decodeRmt4Edges(
        rmt4DecodeTiming(), edges, out);
    FL_REQUIRE(result.ok());
    FL_CHECK(result.value() == 1u);
    FL_CHECK(out[0] == 0xA5);
}

FL_TEST_CASE("RMT4 RX retains final zero bit when LOW is omitted") {
    auto edges = rmt4LowFirstCapture();
    edges.back() = {true, 225};
    fl::u8 out[1] = {};
    const auto result = fl::detail::decodeRmt4Edges(
        rmt4DecodeTiming(), edges, out);
    FL_REQUIRE(result.ok());
    FL_CHECK(result.value() == 1u);
    FL_CHECK(out[0] == 0xA4);
}

FL_TEST_CASE("RMT4 RX tolerates configured LOW gaps across reset threshold") {
    auto edges = rmt4LowFirstCapture();
    edges[2] = {false, 55000};
    auto timing = rmt4DecodeTiming();
    timing.gap_tolerance_ns = 60000;
    fl::u8 out[1] = {};
    const auto result = fl::detail::decodeRmt4Edges(timing, edges, out);
    FL_REQUIRE(result.ok());
    FL_CHECK(result.value() == 1u);
    FL_CHECK(out[0] == 0xA5);
}

FL_TEST_CASE("RMT4 RX rejects malformed HIGH, LOW and adjacent HIGH phases") {
    fl::u8 out[1] = {};
    auto edges = rmt4LowFirstCapture();
    edges[1] = {true, 50};
    auto result = fl::detail::decodeRmt4Edges(rmt4DecodeTiming(), edges, out);
    FL_REQUIRE_FALSE(result.ok());
    FL_CHECK(result.error() == fl::DecodeError::HIGH_ERROR_RATE);

    edges = rmt4LowFirstCapture();
    edges[2] = {false, 100};
    result = fl::detail::decodeRmt4Edges(rmt4DecodeTiming(), edges, out);
    FL_REQUIRE_FALSE(result.ok());
    FL_CHECK(result.error() == fl::DecodeError::HIGH_ERROR_RATE);

    edges = rmt4LowFirstCapture();
    edges[2] = {true, 225};
    result = fl::detail::decodeRmt4Edges(rmt4DecodeTiming(), edges, out);
    FL_REQUIRE_FALSE(result.ok());
    FL_CHECK(result.error() == fl::DecodeError::HIGH_ERROR_RATE);
}

FL_TEST_CASE("RMT4 RX reports output overflow without overwriting the buffer") {
    auto edges = rmt4LowFirstCapture();
    edges.push_back({false, 625});
    const auto second = rmt4LowFirstCapture();
    for (fl::size i = 1; i < second.size(); ++i) {
        edges.push_back(second[i]);
    }
    fl::u8 out[2] = {0, 0xCC};
    const auto result = fl::detail::decodeRmt4Edges(
        rmt4DecodeTiming(), edges, fl::span<fl::u8>(out, 1));
    FL_REQUIRE_FALSE(result.ok());
    FL_CHECK(result.error() == fl::DecodeError::BUFFER_OVERFLOW);
    FL_CHECK(out[0] == 0xA5);
    FL_CHECK(out[1] == 0xCC);

    const auto complete = fl::detail::decodeRmt4Edges(
        rmt4DecodeTiming(), edges, out);
    FL_REQUIRE(complete.ok());
    FL_CHECK(complete.value() == 2u);
    FL_CHECK(out[0] == 0xA5);
    FL_CHECK(out[1] == 0xA5);
}

FL_TEST_CASE("RMT4 RX ignores idle and SDK zero-duration terminators") {
    auto edges = rmt4LowFirstCapture();
    edges.push_back({false, 0});
    fl::u8 out[1] = {};
    const auto result = fl::detail::decodeRmt4Edges(
        rmt4DecodeTiming(), edges, out);
    FL_REQUIRE(result.ok());
    FL_CHECK(result.value() == 1u);
    FL_CHECK(out[0] == 0xA5);
}

FL_TEST_CASE("RMT4 RX rejects empty input or output") {
    const auto edges = rmt4LowFirstCapture();
    fl::u8 out[1] = {};
    const auto noEdges = fl::detail::decodeRmt4Edges(
        rmt4DecodeTiming(), {}, out);
    FL_REQUIRE_FALSE(noEdges.ok());
    FL_CHECK(noEdges.error() == fl::DecodeError::INVALID_ARGUMENT);
    const auto noOutput = fl::detail::decodeRmt4Edges(
        rmt4DecodeTiming(), edges, {});
    FL_REQUIRE_FALSE(noOutput.ok());
    FL_CHECK(noOutput.error() == fl::DecodeError::INVALID_ARGUMENT);
}
