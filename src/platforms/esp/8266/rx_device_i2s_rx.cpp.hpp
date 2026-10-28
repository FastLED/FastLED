// IWYU pragma: private

/// @file platforms/esp/8266/rx_device_i2s_rx.cpp.hpp
/// @brief Implementation of the ESP8266 I2S RX hardware device.

#include "platforms/esp/8266/rx_device_i2s_rx.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP8266)

#include "fl/channels/rx/decode_ws2812.h"
#include "fl/log/log.h"
#include "fl/stl/has_include.h"
#include "fl/stl/noexcept.h"
#include "platforms/esp/8266/i2s_ws2812_codec.h"

#if FL_HAS_INCLUDE(<core_esp8266_i2s.h>)
#define FL_ESP8266_I2S_RX_SUPPORTED

// IWYU pragma: begin_keep
#include <core_esp8266_i2s.h>
// IWYU pragma: end_keep

namespace fl {

namespace {
using fl::esp8266::kI2sBitsPerSample;
using fl::esp8266::kI2sSlotHz;

/// ns per I2S slot (312 ns at 3.2 MHz).
constexpr fl::u32 kSlotNs = 1000000000UL / kI2sSlotHz;

/// Bound a single capture so a stuck line cannot spin forever.
constexpr fl::u32 kMaxSamples = 8192;
}  // namespace

fl::shared_ptr<Esp8266I2sRxDevice> Esp8266I2sRxDevice::create(
    int pin) FL_NO_EXCEPT {
    return fl::make_shared<Esp8266I2sRxDevice>(pin);
}

Esp8266I2sRxDevice::Esp8266I2sRxDevice(int pin) FL_NO_EXCEPT
    : mPin(pin), mArmed(false), mFinished(false), mConfig(), mEdges() {}

bool Esp8266I2sRxDevice::begin(const RxConfig& config) FL_NO_EXCEPT {
    mConfig = config;
    mEdges.clear();
    mFinished = false;
    // RX only, driving the RX clocks; the TX engine being measured drives its
    // own. Enable 16-bit samples at the WS2812 slot rate.
    (void)i2s_rxtxdrive_begin(/*enableRx=*/true, /*enableTx=*/false,
                              /*driveRxClocks=*/true, /*driveTxClocks=*/false);
    i2s_set_rate(kI2sSlotHz);
    i2s_set_bits(16);
    mArmed = true;
    return true;
}

bool Esp8266I2sRxDevice::finished() const FL_NO_EXCEPT { return mFinished; }

RxWaitResult Esp8266I2sRxDevice::wait(u32 timeout_ms) FL_NO_EXCEPT {
    if (!mArmed) {
        return RxWaitResult::TIMEOUT;
    }
    // Drain whatever the peripheral captured: read 16-bit samples until empty.
    // `i2s_read_sample` fills the low channel with the sample we emitted (the
    // core packs (right<<16)|left).
    fl::vector<fl::u16> samples;
    (void)timeout_ms;
    while (samples.size() < kMaxSamples && i2s_rx_available() > 0) {
        fl::i16 left = 0;
        fl::i16 right = 0;
        if (!i2s_read_sample(&left, &right, /*blocking=*/false)) {
            break;
        }
        samples.push_back(static_cast<fl::u16>(left));
    }

    samplesToEdges(samples.data(), samples.size(), mEdges);
    mFinished = true;
    mArmed = false;
    return mEdges.empty() ? RxWaitResult::TIMEOUT : RxWaitResult::SUCCESS;
}

void Esp8266I2sRxDevice::samplesToEdges(const fl::u16* samples,
                                        fl::size sampleCount,
                                        fl::vector<EdgeTime>& out) FL_NO_EXCEPT {
    if (sampleCount == 0 || samples == nullptr) {
        return;
    }
    // Expand the samples to the slot-level bit stream (MSB first per sample)
    // and run-length encode it into high/low edge durations.
    bool haveLevel = false;
    bool level = false;
    fl::u32 runSlots = 0;

    const fl::u32 totalSlots =
        static_cast<fl::u32>(sampleCount) * kI2sBitsPerSample;
    for (fl::u32 slot = 0; slot < totalSlots; ++slot) {
        const fl::size sampleIndex = slot / kI2sBitsPerSample;
        // Top four bits of the 16-bit sample, MSB first.
        const fl::u32 shift = 15u - (slot % kI2sBitsPerSample);
        const bool now = ((samples[sampleIndex] >> shift) & 1u) != 0u;
        if (!haveLevel) {
            level = now;
            haveLevel = true;
            runSlots = 1;
        } else if (now == level) {
            ++runSlots;
        } else {
            out.push_back(EdgeTime(level, runSlots * kSlotNs));
            level = now;
            runSlots = 1;
        }
    }
    if (haveLevel) {
        out.push_back(EdgeTime(level, runSlots * kSlotNs));
    }
}

fl::result<u32, DecodeError> Esp8266I2sRxDevice::decode(
    const ChipsetTiming4Phase& timing, fl::span<u8> out) FL_NO_EXCEPT {
    if (mEdges.empty()) {
        FL_WARN("Esp8266I2sRxDevice::decode: no edges captured on pin " << mPin);
        return fl::result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }
    return fl::channels::rx::decodeWs2812Edges(
        timing, fl::span<const EdgeTime>(mEdges), out);
}

size_t Esp8266I2sRxDevice::getRawEdgeTimes(fl::span<EdgeTime> out,
                                           size_t offset) FL_NO_EXCEPT {
    const size_t total = mEdges.size();
    if (offset >= total) {
        return 0;
    }
    size_t count = total - offset;
    if (count > out.size()) {
        count = out.size();
    }
    for (size_t i = 0; i < count; ++i) {
        out[i] = mEdges[offset + i];
    }
    return count;
}

const char* Esp8266I2sRxDevice::name() const FL_NO_EXCEPT { return "I2S_RX"; }

int Esp8266I2sRxDevice::getPin() const FL_NO_EXCEPT { return mPin; }

bool Esp8266I2sRxDevice::injectEdges(fl::span<const EdgeTime> edges) FL_NO_EXCEPT {
    for (size_t i = 0; i < edges.size(); ++i) {
        mEdges.push_back(edges[i]);
    }
    return true;
}

}  // namespace fl

#endif  // FL_HAS_INCLUDE(<core_esp8266_i2s.h>)
#endif  // FL_IS_ESP8266
