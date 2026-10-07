/// @file ws2812_stream_decoder.cpp
/// @brief Host tests for the streaming FlexPWM WS2812 decoder.
///
/// The Teensy 4.x FlexPWM RX decodes (rise, fall) capture pairs one DMA ring
/// half at a time inside its ISR. These tests feed synthetic capture pairs
/// in chunks and check the result against the batch decoder run over the
/// edge list the pre-streaming driver built from the same captures.

#include "fl/channels/rx/ws2812_stream_decoder.h"
#include "fl/channels/rx.h"
#include "fl/stl/vector.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using fl::channels::rx::Ws2812StreamDecoder;
using fl::channels::rx::decodeFlexPwmEdges;
using fl::channels::rx::flexPwmNsPerTickQ16;
using fl::channels::rx::flexPwmTickDeltaNs;

namespace {

constexpr u32 kBusHz = 150000000;  // Teensy 4.x F_BUS_ACTUAL: 6.67 ns/tick

ChipsetTiming4Phase timing() {
    const ChipsetTiming ws2812b{250, 625, 375, 280, "WS2812B"};
    return make4PhaseTiming(ws2812b, 150);  // T0H [100,400], T1H [725,1025]
}

u16 ticks(u32 ns) { return static_cast<u16>((ns * 3u + 10u) / 20u); }

/// Capture pairs for `bytes`, MSB first. HIGH widths per bit value; every
/// bit is 1250 ns. `start` sets the first rise so tests can force a wrap.
struct Wave {
    fl::vector<u16> caps;
    u16 t;
    explicit Wave(u16 start) : t(start) {}
    void pair(u32 high_ns, u32 period_ns) {
        caps.push_back(t);
        caps.push_back(static_cast<u16>(t + ticks(high_ns)));
        t = static_cast<u16>(t + ticks(period_ns));
    }
    void bytes(const fl::vector<u8>& data, u32 t0h, u32 t1h) {
        for (u8 b : data) {
            for (int i = 7; i >= 0; --i) {
                pair(((b >> i) & 1u) ? t1h : t0h, 1250);
            }
        }
    }
    size_t pairs() const { return caps.size() / 2; }
};

/// The edge list the pre-streaming driver built (buildEdgeTimesFromCaptures).
fl::vector<EdgeTime> referenceEdges(const fl::vector<u16>& c, u32 q16) {
    fl::vector<EdgeTime> edges;
    const size_t n = c.size();
    if (n < 2) return edges;
    size_t start = 0;
    while (start + 3 < n && flexPwmTickDeltaNs(c[start + 1], c[start + 2], q16) > 5000) {
        start += 2;
    }
    for (size_t i = start; i + 3 < n; i += 2) {
        edges.push_back(EdgeTime(true, flexPwmTickDeltaNs(c[i], c[i + 1], q16)));
        const u32 low = flexPwmTickDeltaNs(c[i + 1], c[i + 2], q16);
        if (low > 5000) continue;
        edges.push_back(EdgeTime(false, low));
    }
    edges.push_back(EdgeTime(true, flexPwmTickDeltaNs(c[n - 2], c[n - 1], q16)));
    return edges;
}

struct Decoded {
    bool ok = false;
    DecodeError err = DecodeError::OK;
    fl::vector<u8> bytes;
    Ws2812StreamDecoder::Stats stats;
};

/// Stream `caps` through the decoder in `chunk`-pair pushes.
Decoded streamDecode(const fl::vector<u16>& caps, size_t chunk, size_t out_cap) {
    const u32 q16 = flexPwmNsPerTickQ16(kBusHz);
    const ChipsetTiming4Phase t = timing();
    fl::vector<u8> store(out_cap + 1);
    Ws2812StreamDecoder dec;
    dec.reset(t, q16, fl::span<u8>(store.data(), store.size()), fl::span<EdgeTime>());  // ok span from pointer
    const size_t pairs = caps.size() / 2;
    for (size_t p = 0; p < pairs; p += chunk) {
        const size_t n = (pairs - p < chunk) ? pairs - p : chunk;
        dec.push(fl::span<const u16>(caps).subspan(2 * p, 2 * n));
    }
    dec.flush();
    dec.finish(t);
    Decoded d;
    d.bytes.resize(out_cap);
    auto r = dec.copyTo(fl::span<u8>(d.bytes.data(), d.bytes.size()));  // ok span from pointer
    d.ok = r.ok();
    if (r.ok()) {
        d.bytes.resize(r.value());
    } else {
        d.err = r.error();
    }
    d.stats = dec.stats();
    return d;
}

Decoded batchDecode(const fl::vector<u16>& caps, size_t out_cap) {
    const fl::vector<EdgeTime> edges = referenceEdges(caps, flexPwmNsPerTickQ16(kBusHz));
    Decoded d;
    d.bytes.resize(out_cap);
    auto r = decodeFlexPwmEdges(timing(),
                                fl::span<const EdgeTime>(edges.data(), edges.size()),  // ok span from pointer
                                fl::span<u8>(d.bytes.data(), d.bytes.size()));  // ok span from pointer
    d.ok = r.ok();
    if (r.ok()) {
        d.bytes.resize(r.value());
    } else {
        d.err = r.error();
    }
    return d;
}

fl::vector<u8> pattern(size_t n) {
    fl::vector<u8> v;
    for (size_t i = 0; i < n; ++i) {
        v.push_back(static_cast<u8>(i * 37u + 0x55u));
    }
    return v;
}

void checkSame(const Decoded& a, const Decoded& b) {
    FL_CHECK(a.ok == b.ok);
    FL_CHECK(a.err == b.err);
    FL_REQUIRE(a.bytes.size() == b.bytes.size());
    for (size_t i = 0; i < a.bytes.size(); ++i) {
        FL_CHECK(a.bytes[i] == b.bytes[i]);
    }
}

}  // namespace

FL_TEST_CASE("stream decoder: chunk boundaries splitting bits match the batch decoder") {
    // Last byte 0x00 so the final bit (decoded from HIGH alone) is a valid 0.
    fl::vector<u8> data = pattern(30);
    data.push_back(0x00);
    Wave w(1000);
    w.bytes(data, 300, 800);
    const Decoded ref = batchDecode(w.caps, 64);
    FL_REQUIRE(ref.ok);
    FL_REQUIRE(ref.bytes.size() == data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        FL_CHECK(ref.bytes[i] == data[i]);
    }
    const size_t chunks[] = {1, 2, 3, 7, 64, 128, 1000};
    for (size_t chunk : chunks) {
        checkSame(streamDecode(w.caps, chunk, 64), ref);
    }
}

FL_TEST_CASE("stream decoder: counter wrap in the middle of a pulse") {
    // Start 3 ticks before the 16-bit wrap: the first HIGH straddles it,
    // and further wraps land in LOWs and HIGHs every 437 us of frame.
    fl::vector<u8> data = pattern(400);
    data.push_back(0x00);
    Wave w(65533);
    w.bytes(data, 300, 800);
    const Decoded got = streamDecode(w.caps, 128, 512);
    FL_REQUIRE(got.ok);
    FL_REQUIRE(got.bytes.size() == data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        FL_CHECK(got.bytes[i] == data[i]);
    }
    FL_CHECK(got.stats.errors == 0);
}

FL_TEST_CASE("stream decoder: overrun is an explicit error") {
    fl::vector<u8> data = pattern(8);
    Wave w(0);
    w.bytes(data, 300, 800);
    const u32 q16 = flexPwmNsPerTickQ16(kBusHz);
    const ChipsetTiming4Phase t = timing();
    u8 store[16] = {};
    Ws2812StreamDecoder dec;
    dec.reset(t, q16, fl::span<u8>(store, sizeof(store)), fl::span<EdgeTime>());  // ok span from pointer
    const fl::span<const u16> all(w.caps);
    const size_t half = w.pairs() / 2 * 2;
    dec.push(all.first(half));
    dec.markOverrun();  // the owner saw the DMA lap the ring
    dec.push(all.subspan(half));
    dec.flush();
    dec.finish(t);
    u8 out[16] = {};
    auto r = dec.copyTo(fl::span<u8>(out, sizeof(out)));  // ok span from pointer
    FL_REQUIRE(!r.ok());
    FL_CHECK(r.error() == DecodeError::CAPTURE_OVERRUN);
    FL_CHECK(dec.stats().overrun);
}

FL_TEST_CASE("stream decoder: leading phantom pair before an idle gap is dropped") {
    fl::vector<u8> data = {0xF0, 0x0F, 0xAA, 0x00};
    Wave w(500);
    w.pair(230, 20000);  // stray pair, then 20 us idle
    w.bytes(data, 300, 800);
    const Decoded ref = batchDecode(w.caps, 8);
    FL_REQUIRE(ref.ok);
    FL_REQUIRE(ref.bytes.size() == data.size());
    FL_CHECK(ref.bytes[0] == 0xF0);
    checkSame(streamDecode(w.caps, 5, 8), ref);
}

FL_TEST_CASE("stream decoder: near-threshold bits follow the whole-frame midpoint") {
    // First 64 bits put the calibrated midpoint at (300 + 800) / 2 = 550.
    // A later 900 ns HIGH moves the whole-frame midpoint to 600, so a
    // 560 ns HIGH is a 1 while streaming but a 0 for the batch decoder.
    Wave w(0);
    w.bytes(pattern(8), 300, 800);
    w.pair(560, 1250);
    for (int i = 0; i < 7; ++i) w.pair(300, 1250);
    w.pair(900, 1250);
    for (int i = 0; i < 8; ++i) w.pair(300, 1250);
    const Decoded ref = batchDecode(w.caps, 16);
    const Decoded got = streamDecode(w.caps, 16, 16);
    FL_REQUIRE(ref.ok);
    FL_CHECK(ref.bytes[8] == 0x00);
    checkSame(got, ref);
    FL_CHECK(got.stats.calibratedMidpointNs != got.stats.frameMidpointNs);
    FL_CHECK(got.stats.exceptions >= 1);
    FL_CHECK(!got.stats.inexact);
}

FL_TEST_CASE("stream decoder: 1000-LED frame through a 128-pair ring half") {
    fl::vector<u8> data = pattern(3000);
    data.push_back(0x00);
    Wave w(12345);
    w.bytes(data, 230, 590);
    const Decoded ref = batchDecode(w.caps, 3001);
    const Decoded got = streamDecode(w.caps, 128, 3001);
    FL_REQUIRE(ref.ok);
    FL_REQUIRE(got.bytes.size() == data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        FL_CHECK(got.bytes[i] == data[i]);
    }
    checkSame(got, ref);
    FL_CHECK(got.stats.pairs == w.pairs());
}

FL_TEST_CASE("stream decoder: output buffer overflow matches the batch decoder") {
    Wave w(0);
    w.bytes(pattern(10), 300, 800);
    const Decoded ref = batchDecode(w.caps, 4);
    const Decoded got = streamDecode(w.caps, 32, 4);
    FL_CHECK(!ref.ok);
    FL_CHECK(ref.err == DecodeError::BUFFER_OVERFLOW);
    FL_CHECK(!got.ok);
    FL_CHECK(got.err == DecodeError::BUFFER_OVERFLOW);
}

}  // FL_TEST_FILE
