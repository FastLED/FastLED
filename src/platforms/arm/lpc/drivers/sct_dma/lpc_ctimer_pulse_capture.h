/// @file lpc_ctimer_pulse_capture.h
/// @brief LPC845 CTIMER0 + DMA high-time capture, the wire verifier for the
///        SCT clockless transmitter (FastLED #5265).
///
/// The SCT input-capture RX (`rx_sct_capture.h`) can't watch the SCT
/// transmitter: the LPC845 has one SCT, and the transmitter owns it. This
/// capture uses CTIMER0 and one DMA channel instead:
///   - every rising edge on the pin clears the timer (CTCR ENCC, CAP0 rise),
///   - the falling edge latches the timer into CR0, i.e. the pulse's high
///     time in timer ticks,
///   - MR0, `sample_ticks` after the rise, raises T0_DMAREQ_M0, which makes
///     the DMA copy CR0 into the next slot of the caller's buffer,
///   - MR1, `max_gap_ticks` after the rise, stops the timer (MCR MR1S). A
///     rise that comes later than that, i.e. the line idling low mid-frame,
///     ends the capture, so the frame comes up short.
/// So each bit leaves one word: its high time. Choose `sample_ticks` after
/// the longest high time and before the next bit's falling edge.
///
/// The pin reaches the timer through the SWM `T0_CAP0` function. Only that
/// SWM byte is touched, and DMA0 and SWM are never reset (#4676).

#pragma once

// IWYU pragma: private

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

// Built only with the opt-in SCT+DMA engine it verifies.
#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_PWM_DMA)

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"

#ifndef FL_LPC_CTIMER_CAPTURE_DMA_CH
#define FL_LPC_CTIMER_CAPTURE_DMA_CH 1u
#endif

namespace fl {

class LpcCtimerPulseCapture {
public:
    /// @brief Arm the capture on `pin`. `out` must stay valid until end().
    ///        Needs `sample_ticks` < `max_gap_ticks`. Returns false for an
    ///        unusable pin, bad ticks, or a bad buffer size (1..1023 words:
    ///        one DMA descriptor, and 1024 would make "armed, empty" look
    ///        like "full").
    bool begin(int pin, u32 sample_ticks, u32 max_gap_ticks,
               fl::span<u32> out) FL_NO_EXCEPT;

    /// @brief High times captured so far.
    u32 count() const FL_NO_EXCEPT;

    /// @brief Stop the timer and DMA channel and release the pin.
    void end() FL_NO_EXCEPT;

private:
    u32 mCapacity = 0;
};

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_PWM_DMA
