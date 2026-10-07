/// @file fl/channels/rx/ws2812_stream_decoder.cpp.hpp
/// @brief WS2812 capture-pair decoder, batch and streaming (definition).
///
/// See `ws2812_stream_decoder.h` for the contract.

// IWYU pragma: private

#include "fl/channels/rx/ws2812_stream_decoder.h"
#include "fl/stl/compiler_control.h"

namespace fl {
namespace channels {
namespace rx {

fl::result<u32, DecodeError>
decodeFlexPwmEdges(const ChipsetTiming4Phase& timing,
                   fl::span<const EdgeTime> edges,
                   fl::span<u8> bytes_out) FL_NO_EXCEPT {
    if (edges.size() == 0 || bytes_out.size() == 0) {
        return fl::result<u32, DecodeError>::success(0);
    }

    // Pre-pass: the adaptive midpoint from the observed HIGH widths.
    u32 high_min = 0xFFFFFFFFu;
    u32 high_max = 0u;
    u32 high_samples = 0;
    for (size_t k = 0; k + 1 < edges.size(); k += 2) {
        if (!edges[k].high) continue;
        const u32 h = edges[k].ns;
        if (!flexPwmIsMidpointSample(h)) continue;
        if (h < high_min) high_min = h;
        if (h > high_max) high_max = h;
        ++high_samples;
    }
    const u32 midpoint =
        flexPwmAdaptiveMidpointNs(timing, high_min, high_max, high_samples);

    u32 byte_index = 0;
    u8 current_byte = 0;
    u32 bit_count = 0;
    u32 error_count = 0;
    u32 total_bits = 0;

    size_t i = 0;
    while (i < edges.size()) {
        if (!edges[i].high) {
            ++i;  // polarity error: resync on the next HIGH
            continue;
        }
        const u32 high_ns = edges[i].ns;
        int bit = -1;
        if (i + 1 == edges.size()) {
            // The frame's last HIGH: its LOW is the reset, so classify by
            // HIGH width alone, like a mid-frame bit. (Stopping at the last
            // edge dropped the frame's final bit: LSB 1 decoded as 0.)
            bit = flexPwmBitFromMidpoint(high_ns, midpoint);
            i += 1;
        } else if (!edges[i + 1].high) {
            bit = flexPwmBitFromMidpoint(high_ns, midpoint);
            i += 2;
        } else {
            bit = flexPwmBitFromHigh(high_ns, timing);
            i += 1;
        }
        ++total_bits;
        if (bit < 0) {
            ++error_count;
            continue;
        }
        current_byte = static_cast<u8>((current_byte << 1) | static_cast<u8>(bit));
        if (++bit_count == 8) {
            if (byte_index >= bytes_out.size()) {
                return fl::result<u32, DecodeError>::failure(
                    DecodeError::BUFFER_OVERFLOW);
            }
            bytes_out[byte_index++] = current_byte;
            current_byte = 0;
            bit_count = 0;
        }
    }

    // Partial final byte, left-aligned.
    if (bit_count > 0 && byte_index < bytes_out.size()) {
        bytes_out[byte_index++] = static_cast<u8>(current_byte << (8 - bit_count));
    }
    if (total_bits > 0 && (error_count * 10) > total_bits) {
        return fl::result<u32, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
    }
    return fl::result<u32, DecodeError>::success(byte_index);
}

// ---------------------------------------------------------------------------
// Ws2812StreamDecoder
// ---------------------------------------------------------------------------

void Ws2812StreamDecoder::reset(const ChipsetTiming4Phase& stream_timing,
                                u32 ns_per_tick_q16, fl::span<u8> out,
                                fl::span<EdgeTime> diag_edges) FL_NO_EXCEPT {
    mTiming = stream_timing;
    mNsPerTickQ16 = ns_per_tick_q16;
    mOut = out;
    mDiag = diag_edges;
    mDiagCount = 0;
    mHavePrev = false;
    mStarted = false;
    mPrevRise = 0;
    mPrevFall = 0;
    mEdgeIndex = 0;
    mHighMin = 0xFFFFFFFFu;
    mHighMax = 0;
    mHighSamples = 0;
    mCalibrated = false;
    mMidpoint = 0;
    mPendingCount = 0;
    mExceptionCount = 0;
    mCurByte = 0;
    mBitCount = 0;
    mBitPos = 0;
    mFlushed = false;
    mPartialWritten = false;
    mStats = Stats();
}

void Ws2812StreamDecoder::record(u32 high_ns, Mode mode, bool stream_valid) FL_NO_EXCEPT {
    if (mExceptionCount < kMaxExceptions) {
        Exception& e = mExceptions[mExceptionCount++];
        e.bit_pos = mBitPos;
        e.high_ns = high_ns;
        e.mode = mode;
        e.stream_valid = stream_valid;
        ++mStats.exceptions;
    } else {
        ++mStats.exceptionsDropped;
    }
}

FASTLED_FORCE_INLINE void Ws2812StreamDecoder::appendBit(int b) FL_NO_EXCEPT {
    mCurByte = static_cast<u8>((mCurByte << 1) | static_cast<u8>(b));
    ++mBitPos;
    if (++mBitCount == 8) {
        if (mStats.fullBytes < mOut.size()) {
            mOut[mStats.fullBytes] = mCurByte;
        }
        ++mStats.fullBytes;
        mCurByte = 0;
        mBitCount = 0;
    }
}

FASTLED_FORCE_INLINE void Ws2812StreamDecoder::classify(u32 high_ns, Mode mode) FL_NO_EXCEPT {
    ++mStats.bits;
    if (mode == Mode::FromHigh) {
        const int b = flexPwmBitFromHigh(high_ns, mTiming);
        record(high_ns, mode, b >= 0);
        if (b < 0) {
            ++mStats.errors;
            return;
        }
        appendBit(b);
        return;
    }
    const u32 dist = (high_ns > mMidpoint) ? high_ns - mMidpoint : mMidpoint - high_ns;
    if (dist <= kExceptionBandNs) {
        record(high_ns, mode, true);
    }
    appendBit(flexPwmBitFromMidpoint(high_ns, mMidpoint));
}

void Ws2812StreamDecoder::calibrate() FL_NO_EXCEPT {
    mMidpoint = flexPwmAdaptiveMidpointNs(mTiming, mHighMin, mHighMax, mHighSamples);
    mStats.calibratedMidpointNs = mMidpoint;
    mCalibrated = true;
    for (u32 k = 0; k < mPendingCount; ++k) {
        classify(mPending[k].high_ns, mPending[k].mode);
    }
    mPendingCount = 0;
}

FASTLED_FORCE_INLINE void Ws2812StreamDecoder::pushBit(u32 high_ns, Mode mode) FL_NO_EXCEPT {
    if (mCalibrated) {
        classify(high_ns, mode);
        return;
    }
    mPending[mPendingCount].high_ns = high_ns;
    mPending[mPendingCount].mode = mode;
    if (++mPendingCount == kCalibrationBits) {
        calibrate();
    }
}

FASTLED_FORCE_INLINE void Ws2812StreamDecoder::emitEdge(bool high, u32 ns) FL_NO_EXCEPT {
    if (mDiagCount < mDiag.size()) {
        mDiag[mDiagCount++] = EdgeTime(high, ns);
    }
    ++mEdgeIndex;
}

FASTLED_FORCE_INLINE void Ws2812StreamDecoder::emitPair(u16 rise, u16 fall, bool has_low,
                                   u32 low_ns) FL_NO_EXCEPT {
    const u32 high_ns = flexPwmTickDeltaNs(rise, fall, mNsPerTickQ16);
    // The batch pre-pass samples HIGHs at even edge indices that are not
    // the last edge (the final pair, emitted by flush(), never is).
    if ((mEdgeIndex & 1u) == 0 && flexPwmIsMidpointSample(high_ns)) {
        if (high_ns < mHighMin) mHighMin = high_ns;
        if (high_ns > mHighMax) mHighMax = high_ns;
        ++mHighSamples;
    }
    emitEdge(true, high_ns);
    if (has_low) {
        emitEdge(false, low_ns);
        pushBit(high_ns, Mode::Midpoint);
    } else {
        pushBit(high_ns, Mode::FromHigh);
    }
}

// Hot path: runs in the capture DMA ISR, once per ring half.
FL_OPTIMIZE_FUNCTION void Ws2812StreamDecoder::push(fl::span<const u16> captures) FL_NO_EXCEPT {
    const size_t pair_count = captures.size() / 2u;
    for (size_t p = 0; p < pair_count; ++p) {
        const u16 rise = captures[2 * p];
        const u16 fall = captures[2 * p + 1];
        ++mStats.pairs;
        if (mHavePrev) {
            const u32 low_ns = flexPwmTickDeltaNs(mPrevFall, rise, mNsPerTickQ16);
            const bool has_low = low_ns <= kFlexPwmGapNs;
            // Leading phantom pairs (#3219, #3409): the arm sequence can
            // latch a stray pair followed by an idle gap. Drop any leading
            // pair whose LOW is a gap.
            if (mStarted || has_low) {
                mStarted = true;
                emitPair(mPrevRise, mPrevFall, has_low, low_ns);
            }
        }
        mPrevRise = rise;
        mPrevFall = fall;
        mHavePrev = true;
    }
}

void Ws2812StreamDecoder::flush() FL_NO_EXCEPT {
    if (mFlushed) {
        return;
    }
    mFlushed = true;
    if (mHavePrev) {
        // The final pair's HIGH is the last edge: not a pre-pass sample, and
        // classified by the midpoint like the batch decoder's last edge.
        const u32 high_ns = flexPwmTickDeltaNs(mPrevRise, mPrevFall, mNsPerTickQ16);
        emitEdge(true, high_ns);
        pushBit(high_ns, Mode::Midpoint);
        mHavePrev = false;
    }
    if (!mCalibrated) {
        calibrate();
    }
}

void Ws2812StreamDecoder::finish(const ChipsetTiming4Phase& timing) FL_NO_EXCEPT {
    flush();
    mPartialWritten = false;
    mStats.gapBitShift = false;
    if (mBitCount > 0 && mStats.fullBytes < mOut.size()) {
        mOut[mStats.fullBytes] = static_cast<u8>(mCurByte << (8 - mBitCount));
        mPartialWritten = true;
    }

    const u32 frame_mid =
        flexPwmAdaptiveMidpointNs(timing, mHighMin, mHighMax, mHighSamples);
    mStats.frameMidpointNs = frame_mid;
    const u32 drift = (frame_mid > mMidpoint) ? frame_mid - mMidpoint : mMidpoint - frame_mid;
    bool inexact = drift > kExceptionBandNs || mStats.exceptionsDropped > 0;
    for (u32 k = 0; k < mExceptionCount; ++k) {
        const Exception& e = mExceptions[k];
        int b;
        if (e.mode == Mode::FromHigh) {
            b = flexPwmBitFromHigh(e.high_ns, timing);
            if ((b >= 0) != e.stream_valid) {
                inexact = true;  // the bit would be dropped / kept instead
                mStats.gapBitShift = true;
                continue;
            }
            if (b < 0) {
                continue;
            }
        } else {
            b = flexPwmBitFromMidpoint(e.high_ns, frame_mid);
        }
        const u32 byte = e.bit_pos / 8u;
        if (byte >= mOut.size()) {
            continue;
        }
        const u8 mask = static_cast<u8>(0x80u >> (e.bit_pos % 8u));
        mOut[byte] = b ? static_cast<u8>(mOut[byte] | mask)
                       : static_cast<u8>(mOut[byte] & ~mask);
    }
    mStats.inexact = inexact;
}

fl::result<u32, DecodeError> Ws2812StreamDecoder::copyTo(fl::span<u8> out) const FL_NO_EXCEPT {
    if (mStats.overrun) {
        return fl::result<u32, DecodeError>::failure(DecodeError::CAPTURE_OVERRUN);
    }
    if (mStats.gapBitShift) {
        // The capture-time timing kept or dropped a gap bit that the decode
        // timing would not: every later bit is shifted. Pass the decode
        // timing as RxConfig::stream_timing.
        return fl::result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }
    if (mStats.pairs == 0 || out.size() == 0) {
        return fl::result<u32, DecodeError>::success(0);
    }
    const u32 full = mStats.fullBytes;
    if (full > out.size() || full > mOut.size()) {
        return fl::result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
    }
    u32 n = full;
    if (mPartialWritten && n < out.size()) {
        ++n;
    }
    for (u32 k = 0; k < n; ++k) {
        out[k] = mOut[k];
    }
    if (mStats.bits > 0 && (mStats.errors * 10) > mStats.bits) {
        return fl::result<u32, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
    }
    return fl::result<u32, DecodeError>::success(n);
}

}  // namespace rx
}  // namespace channels
}  // namespace fl
