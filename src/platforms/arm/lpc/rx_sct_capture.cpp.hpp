/// @file platforms/arm/lpc/rx_sct_capture.cpp.hpp
/// @brief LPC8xx SCT input-capture RX implementation (scaffold + decoder).
///
/// See `rx_sct_capture.h` for the status note. This file ships:
///   * The full edge-pair → byte decoder (kept in-TU per the FlexPWM /
///     FlexIO convention; the follow-up `src/fl/channels/rx/decode_ws2812.h`
///     refactor will consolidate all three).
///   * `injectEdges()` + `getRawEdgeTimes()` against an in-RAM vector.
///   * A documented `// TODO` block in `begin()` mapping out the SCT +
///     DMA register sequence per UM11029 — register references inline
///     so a follow-up bench session can fill it in without re-reading
///     the manual.

// IWYU pragma: private

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/arm/lpc/rx_sct_capture.h"

#if defined(FL_IS_ARM_LPC)

#include "fl/log/log.h"

namespace fl {

// ============================================================================
// Factory
// ============================================================================

fl::shared_ptr<LpcSctRxChannel> LpcSctRxChannel::create(int pin) FL_NOEXCEPT {
    return fl::make_shared<LpcSctRxChannel>(pin);
}

// ============================================================================
// Constructor
// ============================================================================

LpcSctRxChannel::LpcSctRxChannel(int pin) FL_NOEXCEPT
    : mPin(pin)
    , mFinished(false) {
}

// ============================================================================
// RxDevice interface
// ============================================================================

bool LpcSctRxChannel::begin(const RxConfig& config) FL_NOEXCEPT {
    (void)config;
    mEdges.clear();
    mFinished = false;

    // ------------------------------------------------------------------------
    // TODO (#3015 follow-up — hardware bench session required):
    //
    // Wire SCT input capture + DMA to populate `mEdges` on real silicon.
    // The skeleton below is the sequence the follow-up should implement.
    // Register references are UM11029 (LPC84x User Manual, Rev 1.6).
    //
    // 1. Enable SCT in SYSCON:
    //      SYSCON->SYSAHBCLKCTRL0 |= (1 << 8);   // SCT clock
    //      SYSCON->PRESETCTRL0    &= ~(1 << 8);  // de-assert SCT reset
    //      SYSCON->PRESETCTRL0    |=  (1 << 8);  // pulse reset
    //
    // 2. Route the user's pin -> CTIN_0 via the Switch Matrix (SWM).
    //    LPC845 SWM lets any GPIO map to any SCT capture input. The
    //    register is `SWM->PINASSIGN5` (PINASSIGN5[7:0] = CTIN_0). The
    //    value is the PIO0_n number; e.g. for P0_11 the byte is 0x0B.
    //
    // 3. Configure SCT for 32-bit unified up-counter:
    //      SCT->CONFIG = (1 << 0) /*UNIFY*/ | (0 << 1) /*sys clk*/;
    //
    // 4. Set up four capture registers (one per WS2812 edge type — rising,
    //    falling — alternating into CAPTURE[0..3] so the DMA can stream
    //    the result without re-arming):
    //      SCT->REGMODE_L |= 0xF;  // CAPTURE0..3 are CAPTURE (not MATCH)
    //
    // 5. Set up events EV_0..EV_3 each gated on CTIN_0 with alternating
    //    edge polarity:
    //      SCT->EV[0].CTRL = (1 << 10) /*HEVENT*/ | (0 << 12) /*rising*/
    //                      | (1 << 13) /*IOSEL = CTIN_0*/;
    //      SCT->EV[1].CTRL = ... falling ...
    //      ...
    //    Each event triggers its corresponding CAPTURE[n] register
    //    snapshot of the unified counter.
    //
    // 6. Hook a DMA channel to one of the SCT_DMA0/DMA1 request lines
    //    (LPC845 DMA mux: DMATRIGCFG[n]). Source = SCT capture
    //    register address, destination = `mEdges.data()`. Configure as
    //    32-bit width, no-increment src, post-increment dst, transfer
    //    count = `mEdges.capacity()`.
    //
    // 7. Start the SCT: SCT->CTRL_U &= ~(1 << 2);   // clear HALT_L
    //
    // 8. The ISR fires when DMA hits its transfer-complete bit. Set
    //    `mFinished = true;` in the ISR (or here in wait() if polling).
    //
    // None of this is exercised on the host build (FL_IS_STUB) — the
    // edge buffer stays empty until `injectEdges()` populates it from
    // the test harness or the follow-up firmware-side capture path.
    // ------------------------------------------------------------------------

    return true;
}

bool LpcSctRxChannel::finished() const FL_NOEXCEPT {
    return mFinished;
}

RxWaitResult LpcSctRxChannel::wait(u32 timeout_ms) FL_NOEXCEPT {
    (void)timeout_ms;
    mFinished = true;
    if (mEdges.empty()) {
        // No edges captured. Either the SCT/DMA setup is still a TODO
        // (the common case in this PR) or no signal reached the pin.
        return RxWaitResult::TIMEOUT;
    }
    return RxWaitResult::SUCCESS;
}

// ============================================================================
// Decode — edge-pair → bytes
// ============================================================================
//
// Same 4-phase decoder as `rx_device_native.cpp.hpp::decode` (and the
// per-driver copies in FlexPWM / FlexIO). Each WS2812 bit produces two
// edge entries:
//
//   EdgeTime(high=true,  ns=T0H or T1H)  — HIGH duration
//   EdgeTime(high=false, ns=T0L or T1L)  — LOW duration
//
// Bit 0: T0H ∈ [t0h_min, t0h_max], T0L ∈ [t0l_min, t0l_max]
// Bit 1: T1H ∈ [t1h_min, t1h_max], T1L ∈ [t1l_min, t1l_max]
//
// Kept in-TU per the per-driver convention. Consolidation target:
// `src/fl/channels/rx/decode_ws2812.h` (follow-up refactor).
fl::result<u32, DecodeError> LpcSctRxChannel::decode(const ChipsetTiming4Phase& timing,
                                                     fl::span<u8> out) FL_NOEXCEPT {
    size_t edge_count = mEdges.size();
    if (edge_count == 0) {
        FL_WARN("LpcSctRxChannel::decode: No edges recorded for pin " << mPin);
        return fl::result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    size_t bytes_written = 0;
    size_t bit_index = 0;
    u8 current_byte = 0;
    u32 error_count = 0;

    size_t i = 0;
    while (i + 1 < edge_count) {
        EdgeTime high_edge = mEdges[i];
        EdgeTime low_edge  = mEdges[i + 1];

        // Edges must alternate HIGH then LOW.
        if (!high_edge.high || low_edge.high) {
            error_count++;
            i++;
            continue;
        }

        u32 high_ns = high_edge.ns;
        u32 low_ns  = low_edge.ns;

        bool is_bit1 = (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns &&
                        low_ns  >= timing.t1l_min_ns && low_ns  <= timing.t1l_max_ns);
        bool is_bit0 = (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns &&
                        low_ns  >= timing.t0l_min_ns && low_ns  <= timing.t0l_max_ns);

        if (!is_bit0 && !is_bit1) {
            // Reset pulse?
            if (low_ns >= (u32)(timing.reset_min_us * 1000u)) {
                if (bit_index != 0) {
                    FL_WARN("LpcSctRxChannel::decode: Partial byte at reset (bit_index="
                            << bit_index << ")");
                }
                break;
            }
            // Tolerable inter-frame gap (PARLIO-style)?
            if (timing.gap_tolerance_ns > 0 && low_ns <= timing.gap_tolerance_ns) {
                if (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns) {
                    is_bit1 = true;
                } else if (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns) {
                    is_bit0 = true;
                }
            }

            if (!is_bit0 && !is_bit1) {
                error_count++;
                i += 2;
                continue;
            }
        }

        int bit = is_bit1 ? 1 : 0;
        current_byte = (u8)((current_byte << 1) | (u8)bit);
        bit_index++;

        if (bit_index == 8) {
            if (bytes_written >= out.size()) {
                return fl::result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
            }
            out[bytes_written++] = current_byte;
            current_byte = 0;
            bit_index = 0;
        }

        i += 2;
    }

    // > 10% error rate → reject the decode
    if (bytes_written > 0 && error_count * 10 > bytes_written * 8) {
        return fl::result<u32, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
    }

    return fl::result<u32, DecodeError>::success(static_cast<u32>(bytes_written));
}

size_t LpcSctRxChannel::getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset) FL_NOEXCEPT {
    size_t total = mEdges.size();
    if (offset >= total) return 0;

    size_t count = total - offset;
    if (count > out.size()) count = out.size();

    for (size_t i = 0; i < count; i++) {
        out[i] = mEdges[offset + i];
    }
    return count;
}

const char* LpcSctRxChannel::name() const FL_NOEXCEPT {
    return "LPC_SCT_CAPTURE";
}

int LpcSctRxChannel::getPin() const FL_NOEXCEPT {
    return mPin;
}

bool LpcSctRxChannel::injectEdges(fl::span<const EdgeTime> edges) FL_NOEXCEPT {
    for (size_t i = 0; i < edges.size(); i++) {
        mEdges.push_back(edges[i]);
    }
    return true;
}

} // namespace fl

#endif // FL_IS_ARM_LPC
