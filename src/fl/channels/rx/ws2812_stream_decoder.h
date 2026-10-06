/// @file fl/channels/rx/ws2812_stream_decoder.h
/// @brief WS2812 decoder for paired rise/fall capture timestamps, in batch
///        and streaming form.
///
/// Used by the Teensy 4.x FlexPWM input-capture RX
/// (`platforms/arm/teensy/teensy4_common/rx_flexpwm_channel.cpp.hpp`).
/// The capture hardware delivers one (rise, fall) pair of 16-bit counter
/// timestamps per WS2812 bit. The streaming decoder consumes those pairs
/// in chunks (one DMA ring half at a time, from the DMA ISR) and writes
/// decoded bytes straight into the final output buffer, so a frame of any
/// length needs only the output bytes plus a small fixed ring.
///
/// Both forms share one set of threshold rules (the functions below), and
/// the streaming decoder produces the same bytes as running
/// `decodeFlexPwmEdges()` over the full edge list built from the same
/// captures. The batch decoder picks its bit threshold from a pre-pass over
/// the whole frame (the adaptive midpoint). A stream cannot look ahead, so
/// it calibrates the threshold on the first `kCalibrationBits` bits, and
/// records every bit whose HIGH lies within `kExceptionBandNs` of that
/// threshold. Bits that end at a gap (range-checked against the T0H/T1H
/// windows) are recorded too. `finish()` recomputes the whole-frame midpoint
/// with the decode-time timing and re-classifies the recorded bits. That is
/// exact whenever the two midpoints are within the band, the exception list
/// did not overflow, and no gap bit changes between valid and invalid under
/// the decode-time timing; otherwise `stats().inexact` is set.
///
/// The stream needs a timing while it runs (for the static-midpoint fallback
/// and the gap-bit windows) before the caller's `decode(timing)` is known:
/// the capture config supplies it (`RxConfig::stream_timing`).

#pragma once

#include "fl/channels/rx.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/result.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace channels {
namespace rx {

/// A LOW longer than this ends the bit without a LOW edge (gap or reset).
constexpr u32 kFlexPwmGapNs = 5000;

/// Static threshold between a 0 and a 1 HIGH: midpoint of T0H max / T1H min.
inline u32 flexPwmStaticMidpointNs(const ChipsetTiming4Phase& timing) FL_NO_EXCEPT {
    return (timing.t0h_max_ns + timing.t1h_min_ns) / 2u;
}

/// Whether a HIGH width contributes to the adaptive-midpoint pre-pass
/// (idle and glitch widths are excluded).
inline bool flexPwmIsMidpointSample(u32 high_ns) FL_NO_EXCEPT {
    return high_ns >= 100u && high_ns <= 1500u;
}

/// #3416 adaptive midpoint: (min + max) / 2 of the observed HIGH widths once
/// there are enough samples spread wide enough to trust; else the static one.
inline u32 flexPwmAdaptiveMidpointNs(const ChipsetTiming4Phase& timing,
                                     u32 high_min, u32 high_max,
                                     u32 samples) FL_NO_EXCEPT {
    if (samples >= 16 && high_max > high_min + 200u) {
        return (high_min + high_max) / 2u;
    }
    return flexPwmStaticMidpointNs(timing);
}

/// Bit from a HIGH followed by a normal LOW: HIGH-only midpoint split. Always
/// 0 or 1 so one marginal bit cannot shift the rest of the frame (#3219).
inline int flexPwmBitFromMidpoint(u32 high_ns, u32 midpoint_ns) FL_NO_EXCEPT {
    return (high_ns >= midpoint_ns) ? 1 : 0;
}

/// Bit from a HIGH whose LOW was a gap/reset or was not captured: range check
/// against the T0H / T1H windows. Returns -1 when outside both.
inline int flexPwmBitFromHigh(u32 high_ns, const ChipsetTiming4Phase& timing) FL_NO_EXCEPT {
    if (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns) {
        return 0;
    }
    if (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns) {
        return 1;
    }
    return -1;
}

/// Convert a 16-bit capture-counter delta to ns. Unsigned 16-bit subtraction
/// handles a counter wrap inside the pulse. `ns_per_tick_q16` is
/// (1e9 / counter_hz) in Q16.16.
inline u32 flexPwmTickDeltaNs(u16 t0, u16 t1, u32 ns_per_tick_q16) FL_NO_EXCEPT {
    const u16 delta = static_cast<u16>(t1 - t0);
    return static_cast<u32>((static_cast<u64>(delta) * ns_per_tick_q16) >> 16);
}

/// Q16.16 ns-per-tick for a counter clock.
inline u32 flexPwmNsPerTickQ16(u32 counter_hz) FL_NO_EXCEPT {
    return static_cast<u32>((static_cast<u64>(1000000000ULL) << 16) / counter_hz);
}

/// Batch decoder over an edge list (HIGH, LOW, HIGH, ... with a LOW left
/// out after a gap). Used for `injectEdges()` and as the reference the
/// streaming decoder must match.
fl::result<u32, DecodeError>
decodeFlexPwmEdges(const ChipsetTiming4Phase& timing,
                   fl::span<const EdgeTime> edges,
                   fl::span<u8> bytes_out) FL_NO_EXCEPT;

/// Streaming decoder over (rise, fall) capture pairs. Not thread-safe: the
/// owner feeds it from one context at a time (the DMA ISR during the frame,
/// then the thread after the ISR is masked).
class Ws2812StreamDecoder {
  public:
    /// Bits classified before the threshold is fixed. 64 bits = 8 bytes.
    static constexpr u32 kCalibrationBits = 64;
    /// Bits within this distance of the calibrated threshold are recorded
    /// for re-classification against the whole-frame threshold.
    static constexpr u32 kExceptionBandNs = 150;
    static constexpr u32 kMaxExceptions = 32;

    struct Stats {
        u32 pairs = 0;             ///< capture pairs consumed
        u32 bits = 0;              ///< bits classified (batch `total_bits`)
        u32 errors = 0;            ///< out-of-window bits (batch `error_count`)
        u32 fullBytes = 0;         ///< complete bytes decoded (may exceed capacity)
        u32 calibratedMidpointNs = 0;
        u32 frameMidpointNs = 0;   ///< whole-frame midpoint, set by finish()
        u32 exceptions = 0;        ///< near-threshold bits recorded
        u32 exceptionsDropped = 0; ///< near-threshold bits that did not fit
        bool inexact = false;      ///< finish() could not guarantee batch equality
        bool gapBitShift = false;  ///< a gap bit kept/dropped differs from decode timing
        bool overrun = false;      ///< the owner reported lost captures
    };

    /// Start a frame. `stream_timing` classifies bits while capturing.
    /// `out` receives decoded bytes and must outlive the frame;
    /// `diag_edges` (may be empty) receives the first edges for
    /// `getRawEdgeTimes()` diagnostics.
    void reset(const ChipsetTiming4Phase& stream_timing, u32 ns_per_tick_q16,
               fl::span<u8> out, fl::span<EdgeTime> diag_edges) FL_NO_EXCEPT;

    /// Consume capture pairs laid out as [rise0, fall0, rise1, fall1...].
    void push(fl::span<const u16> captures) FL_NO_EXCEPT;

    /// Captures were lost (ring overwritten before decode). The frame result
    /// becomes `DecodeError::CAPTURE_OVERRUN`.
    void markOverrun() FL_NO_EXCEPT { mStats.overrun = true; }

    /// End of capture: decode the final pair (HIGH only; its LOW is the
    /// reset) and fix the calibration. Call
    /// once, after the last push().
    void flush() FL_NO_EXCEPT;

    /// Re-classify the recorded bits with the decode-time `timing` and
    /// write the partial final byte. Idempotent; call after flush().
    void finish(const ChipsetTiming4Phase& timing) FL_NO_EXCEPT;

    /// Copy the decoded frame into `out` with the batch decoder's result
    /// rules (overflow, >10% error rate, partial final byte). Call after
    /// finish().
    fl::result<u32, DecodeError> copyTo(fl::span<u8> out) const FL_NO_EXCEPT;

    const Stats& stats() const FL_NO_EXCEPT { return mStats; }
    size_t diagEdgeCount() const FL_NO_EXCEPT { return mDiagCount; }

  private:
    enum class Mode : u8 { Midpoint, FromHigh };
    struct Pending {
        u32 high_ns;
        Mode mode;
    };
    struct Exception {
        u32 bit_pos;     // position of the bit (or where it was dropped)
        u32 high_ns;
        Mode mode;
        bool stream_valid;  // FromHigh: classified (not dropped) while streaming
    };

    void emitPair(u16 rise, u16 fall, bool has_low, u32 low_ns) FL_NO_EXCEPT;
    void emitEdge(bool high, u32 ns) FL_NO_EXCEPT;
    void pushBit(u32 high_ns, Mode mode) FL_NO_EXCEPT;
    void classify(u32 high_ns, Mode mode) FL_NO_EXCEPT;
    void calibrate() FL_NO_EXCEPT;
    void appendBit(int bit) FL_NO_EXCEPT;

    void record(u32 high_ns, Mode mode, bool stream_valid) FL_NO_EXCEPT;

    ChipsetTiming4Phase mTiming = {};
    u32 mNsPerTickQ16 = 0;
    fl::span<u8> mOut;
    fl::span<EdgeTime> mDiag;
    size_t mDiagCount = 0;

    bool mHavePrev = false;
    bool mStarted = false;
    u16 mPrevRise = 0;
    u16 mPrevFall = 0;

    u32 mEdgeIndex = 0;  // index in the equivalent batch edge list
    u32 mHighMin = 0xFFFFFFFFu;
    u32 mHighMax = 0;
    u32 mHighSamples = 0;

    bool mCalibrated = false;
    u32 mMidpoint = 0;
    Pending mPending[kCalibrationBits];
    u32 mPendingCount = 0;

    Exception mExceptions[kMaxExceptions];
    u32 mExceptionCount = 0;

    u8 mCurByte = 0;
    u32 mBitCount = 0;
    u32 mBitPos = 0;  // bits appended so far
    bool mFlushed = false;
    bool mPartialWritten = false;

    Stats mStats;
};

}  // namespace rx
}  // namespace channels
}  // namespace fl
