#pragma once

// fastPinLoopback RPC (#5067): drives the selected FastPin<TX> route over the
// discovered TX->RX jumper. Build with --define FASTLED_FORCE_SOFTWARE_PINS to
// check the software route; without it, the same RPC checks native GPIO.
// Pin levels, clock LOW after init, MSB-first data bits at two software-SPI
// rates (their bit periods must differ by the requested 6000 ns) and clock edge counts
// are read back on RX. ESP32 only: the spare pins are the default SPI pins.

#include "fl/channels/rx/channel.h"
#include "fl/channels/rx/config.h"
#include "fl/stl/json.h"
#include "fl/stl/type_traits.h"
#include "fl/system/fastpin.h"
#include "fl/system/fastpin_no_pinmap.h"
#include "fastspi.h"  // SoftwareSPIOutput: ESP32 takes the rate in Hz
#include <Arduino.h>

namespace autoresearch_fastpin {

constexpr int kBytes = 8;
// Single-bit runs first (period estimate), one 8-bit run, ends on a one.
constexpr fl::u8 kPattern[kBytes] = {0xA5, 0x3C, 0x81, 0x5A, 0xF0, 0x0F, 0x96, 0x69};
constexpr int kEdgeCap = 256;

inline bool rxHigh(int rx) { return (fl::digitalRead)(rx) == fl::PinValue::High; }

struct Runs {
    fl::EdgeTime edges[kEdgeCap];
    size_t count = 0;
    bool ok = false;
};

inline bool armRx(const fl::shared_ptr<fl::RxChannel>& ch) {
    fl::RxChannelConfig cfg(ch->getPin());
    cfg.edge_capacity = kEdgeCap;
    cfg.signal_range_max_ns = 100000;  // a 100 us idle ends the capture
    cfg.backend = ch->backend();
    return ch->begin(cfg);
}

inline void collect(const fl::shared_ptr<fl::RxChannel>& ch, Runs& runs) {
    runs.ok = ch->wait(200) == fl::RxWaitResult::SUCCESS;
    runs.count = ch->getRawEdgeTimes(fl::span<fl::EdgeTime>(runs.edges, kEdgeCap), 0);
}

// Rebuilds the MSB-first bit stream from data-line runs. Leading LOW runs are
// before the first one bit; the last run was cut by the idle timeout, so it
// and any missing final run (the level read after capture) fill the rest.
inline int decodeBadBits(const Runs& runs, bool finalHigh, fl::json& r) {
    size_t first = 0;
    while (first < runs.count && !runs.edges[first].high) ++first;
    if (first + 2 > runs.count) return -1;
    fl::u32 t = 0xFFFFFFFFu;
    for (size_t i = first; i + 1 < runs.count; ++i) t = fl::min(t, runs.edges[i].ns);
    if (t == 0) return -1;
    // Refine the period over every complete run.
    fl::u32 sum = 0, bits = 0;
    for (size_t i = first; i + 1 < runs.count; ++i) {
        sum += runs.edges[i].ns;
        bits += (runs.edges[i].ns + t / 2) / t;
    }
    t = bits ? sum / bits : t;
    r.set("bitPeriodNs", static_cast<int>(t));
    bool stream[kBytes * 8];
    int n = 0;
    int repeated = 0;
    for (size_t i = first; i < runs.count && n < kBytes * 8; ++i) {
        // Runs alternate; a repeated level is an RX artifact, not wire time.
        if (n > 0 && runs.edges[i].high == stream[n - 1]) { ++repeated; continue; }
        // Every run is at least one bit; a run cut by the end of capture can be short.
        int len = fl::max(1, static_cast<int>((runs.edges[i].ns + t / 2) / t));
        const bool last = i + 1 == runs.count && runs.edges[i].high == finalHigh;
        if (last || n + len > kBytes * 8) len = kBytes * 8 - n;
        for (int k = 0; k < len; ++k) stream[n++] = runs.edges[i].high;
    }
    // The final level may never end inside the capture, so it may have no run.
    while (n < kBytes * 8) stream[n++] = finalHigh;
    int bad = 0;
    fl::json decoded = fl::json::array();
    for (int b = 0; b < n; ++b) {
        if (stream[b] != ((kPattern[b / 8] & (0x80 >> (b % 8))) != 0)) ++bad;
    }
    for (int byte = 0; byte * 8 + 7 < n; ++byte) {
        int v = 0;
        for (int k = 0; k < 8; ++k) v = (v << 1) | (stream[byte * 8 + k] ? 1 : 0);
        decoded.push_back(v);
    }
    r.set("dataDecoded", decoded);
    r.set("dataRepeatedRuns", repeated);
    return bad;
}

// Writes kPattern with TX as SoftwareSPIOutput data at RATE_HZ and decodes it on RX.
// Sets rateHz, bitPeriodNs and the data* fields; returns the bad bit count, or
// -1 if nothing was decoded.
template <int TX, int SPARE_CLOCK, fl::u32 RATE_HZ>
int measureData(const fl::shared_ptr<fl::RxChannel>& ch, int rx, fl::json& r) {
    typedef fl::FastPin<TX> Pin;
    r.set("rateHz", static_cast<int>(RATE_HZ));
    // Data on TX, starting LOW so the first one bit is the first RX edge.
    SoftwareSPIOutput<TX, SPARE_CLOCK, RATE_HZ> dataSpi;
    dataSpi.init();
    Pin::lo();
    // A zero byte keeps TX LOW but loads the writer into cache, so the first
    // measured bit is not stretched by flash fetches (seen as 1.6 bit periods).
    dataSpi.writeByte(0x00);
    delayMicroseconds(20);
    Runs data;
    const bool armed = armRx(ch);
    if (armed) {
        dataSpi.select();
        dataSpi.writeBytes(const_cast<fl::u8*>(kPattern), kBytes);
        dataSpi.release();
        collect(ch, data);
    }
    delayMicroseconds(20);
    const bool dataFinalHigh = rxHigh(rx);
    Pin::lo();
    r.set("dataArmed", armed);
    r.set("dataCaptured", data.ok);
    r.set("dataRuns", static_cast<int>(data.count));
    fl::json runsNs = fl::json::array();
    for (size_t i = 0; i < data.count && i < 80; ++i) {
        runsNs.push_back(static_cast<int>(data.edges[i].high ? data.edges[i].ns : -static_cast<int>(data.edges[i].ns)));
    }
    r.set("dataRunsNs", runsNs);  // negative = LOW
    r.set("dataLastRunNs", data.count ? static_cast<int>(data.edges[data.count - 1].ns) : -1);
    r.set("dataFinalHigh", dataFinalHigh);
    const int badBits = armed && data.ok ? decodeBadBits(data, dataFinalHigh, r) : -1;
    r.set("dataBadBits", badBits);
    return badBits;
}

// Nominal periods at 250 kHz and 100 kHz are 4000 ns and 10000 ns. Neither may
// be more than 2% short (RX timing resolution; the writer only adds time), and
// their difference must be 6000 ns within 5%: the per-bit GPIO overhead cancels,
// so this checks the Hz-to-divider scale itself. A rate passed through as a
// cycle count would be thousands of times too slow.
inline bool checkRates(fl::json& fast, const fl::json& slow) {
    const int fastNs = static_cast<int>(fast["bitPeriodNs"].as_int().value_or(0));
    const int slowNs = static_cast<int>(slow["bitPeriodNs"].as_int().value_or(0));
    const int slopeNs = slowNs - fastNs;
    const bool ok = fastNs >= 3920 && slowNs >= 9800 && slopeNs >= 5700 && slopeNs <= 6300;
    fast.set("rateOverheadNs", fastNs - 4000);
    fast.set("rateSlopeNs", slopeNs);
    fast.set("rateOk", ok);
    return ok;
}

template <int TX, int SPARE_CLOCK, int SPARE_DATA>
fl::json runAt(const fl::shared_ptr<fl::RxChannel>& ch, int rx, fl::true_type) {
    typedef fl::FastPin<TX> Pin;
    fl::json r = fl::json::object();
#ifdef FASTLED_FORCE_SOFTWARE_PINS
    r.set("forceSoftwarePins", true);
#else
    r.set("forceSoftwarePins", false);
#endif
#ifdef HAS_HARDWARE_PIN_SUPPORT
    r.set("hardwarePinSupport", true);
#else
    r.set("hardwarePinSupport", false);
#endif
    r.set("softwareRoute", fl::isNoPinMapPin(static_cast<const Pin*>(nullptr)));

    // Levels and ordering on the selected FastPin route.
    (fl::pinMode)(rx, fl::PinMode::Input);
    Pin::setOutput();
    Pin::lo();
    delayMicroseconds(20);
    const bool lowAfterLo = !rxHigh(rx);
    static const char kOps[] = "HLHTTLTT";  // ends LOW
    bool expected = false;
    int levelMismatches = 0;
    for (const char* op = kOps; *op; ++op) {
        if (*op == 'H') { Pin::hi(); expected = true; }
        else if (*op == 'L') { Pin::lo(); expected = false; }
        else { Pin::toggle(); expected = !expected; }
        delayMicroseconds(20);
        if (rxHigh(rx) != expected) ++levelMismatches;
    }
    r.set("lowAfterLo", lowAfterLo);
    r.set("levelMismatches", levelMismatches);

    // Data on TX at two rates. The writer's GPIO work adds a fixed number of
    // cycles per bit on top of the requested period, so each period may run
    // long by that overhead, but the difference between the rates must equal
    // the difference between the requested periods (#5051).
    const int badBits = measureData<TX, SPARE_CLOCK, DATA_RATE_KHZ(250)>(ch, rx, r);
    fl::json slow = fl::json::object();
    const int slowBadBits = measureData<TX, SPARE_CLOCK, DATA_RATE_KHZ(100)>(ch, rx, slow);
    const bool rateOk = checkRates(r, slow);
    r.set("rate100k", slow);

    // Clock on TX: init() drives it LOW, then one rising edge per bit and idle LOW.
    Pin::hi();
    delayMicroseconds(20);
    const bool clockHighBeforeInit = rxHigh(rx);
    SoftwareSPIOutput<SPARE_DATA, TX, DATA_RATE_KHZ(250)> clockSpi;
    clockSpi.init();
    delayMicroseconds(20);
    const bool clockLowAfterInit = !rxHigh(rx);
    Runs clock;
    const bool armed = armRx(ch);
    int rising = 0;
    if (armed) {
        clockSpi.select();
        clockSpi.writeBytes(const_cast<fl::u8*>(kPattern), kBytes);
        clockSpi.release();
        collect(ch, clock);
        for (size_t i = 0; i < clock.count; ++i) rising += clock.edges[i].high ? 1 : 0;
    }
    delayMicroseconds(20);
    const bool clockIdleLow = !rxHigh(rx);
    r.set("clockHighBeforeInit", clockHighBeforeInit);
    r.set("clockLowAfterInit", clockLowAfterInit);
    r.set("clockArmed", armed);
    r.set("clockCaptured", clock.ok);
    r.set("clockRisingEdges", rising);
    r.set("clockIdleLow", clockIdleLow);
    (fl::pinMode)(TX, fl::PinMode::Input);
    (fl::pinMode)(SPARE_CLOCK, fl::PinMode::Input);
    (fl::pinMode)(SPARE_DATA, fl::PinMode::Input);

    r.set("success", lowAfterLo && levelMismatches == 0 && badBits == 0 &&
                         slowBadBits == 0 && rateOk &&
                         clockHighBeforeInit && clockLowAfterInit && clock.ok &&
                         rising == kBytes * 8 && clockIdleLow);
    return r;
}

template <int TX, int SPARE_CLOCK, int SPARE_DATA>
fl::json runAt(const fl::shared_ptr<fl::RxChannel>&, int, fl::false_type) {
    fl::json r = fl::json::object();
    r.set("success", false);
    r.set("error", "TxPinNotValidForFastPin");
    return r;
}

template <int SPARE_CLOCK, int SPARE_DATA, int TX = 0>
fl::json dispatch(int tx, const fl::shared_ptr<fl::RxChannel>& ch, int rx) {
    if (tx == TX) {
        return runAt<TX, SPARE_CLOCK, SPARE_DATA>(
            ch, rx, fl::integral_constant<bool, fl::FastPin<TX>::validpin()>());
    }
    if (TX >= 23) {
        return runAt<0, SPARE_CLOCK, SPARE_DATA>(ch, rx, fl::false_type());
    }
    return dispatch<SPARE_CLOCK, SPARE_DATA, (TX < 23 ? TX + 1 : 23)>(tx, ch, rx);
}

}  // namespace autoresearch_fastpin
