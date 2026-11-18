// AutoResearchPwmDmaClockless.h - LPC845 SCT+DMA channels-API engine
// AutoResearch validation harness (FastLED #3468, #5265).
//
// Device-side RPC handlers that exercise the `ChannelEngineLpcSctDma`
// engine on real LPC845-BRK silicon and measure the wire through a jumper.
//
// Gated on `FL_IS_ARM_LPC_845 && FASTLED_LPC_PWM_DMA`. The whole file
// compiles out on any other build target.
//
// Handlers (bound by `autoresearch::pwm_dma_cl::bind(remote)`):
//
//   `pwmDmaClFrameOnce(led_count, color_rgb, data_pin)`
//     Drive one frame; "ok,started_us,done_us,duration_us". ok is 0 when
//     the frame halted before its last bit.
//
//   `pwmDmaClFrameBurst(led_count, color_rgb, n_frames, data_pin)`
//     Drive `n_frames` back-to-back, each after the WS2812 reset time;
//     "ok,total_us,fps_x100".
//
//   `pwmDmaClCaptureSelf(led_count, color_rgb, data_pin, rx_pin, hold_us,
//                        step_rgb)`
//     LED i is `(color_rgb + i * step_rgb) & 0xFFFFFF`, so a nonzero step
//     gives every chunk different bytes. Drive one frame and capture every
//     bit's high time on `rx_pin` with
//     `LpcCtimerPulseCapture` (CTIMER0 + DMA). The SCT RX capture can't be
//     used: the transmitter owns the chip's only SCT. `hold_us` > 0
//     busy-waits that long after show() without calling poll(), as
//     FastLED.show() does between frames, so chunk hand-over must come from
//     the DMA interrupt. Bits use WS2812 T0H/T1H with a 1450 ns low phase
//     (see captureTiming()). A rise more than 3 us after the previous one
//     (the line idling mid-frame) stops the capture, so such a frame
//     decodes short. Returns
//     "1,<n>,<byte>...,bits=<b>,t0h=<min>-<max>,t1h=<min>-<max>,gap_guard=<ns>"
//     (ns), for a host-side byte compare.
//
// Every handler gives up after a bounded wait instead of spinning on
// poll() forever, so a stalled engine answers the RPC with a failure.
//
// Loopback wiring: one jumper from `data_pin` to `rx_pin`, found by the
// findConnectedPins scan. `data_pin` carries SCT_OUT0 and `rx_pin` T0_CAP0,
// both movable SWM functions, so any non-reserved PIO0 pins work.

#pragma once

#include <FastLED.h>

#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_PWM_DMA)

#include "fl/channels/data.h"
#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/remote/remote.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/singleton.h"
#include "fl/stl/sstream.h"
#include "fl/stl/vector.h"
#include "fl/system/pin_probe.h"
#include "platforms/arm/lpc/drivers/sct_dma/channel_engine_lpc_sct_dma.h" // ok platform headers; ok platform specific - this LPC845-only harness tests the concrete SCT+DMA engine type
// The LPC BusTraits<Bus::BIT_BANG> specialization this file names. It used to
// arrive only via clockless_channel_lpc.h, which is skipped when another
// ClocklessController was already picked, leaving the type incomplete.
#include "platforms/arm/lpc/drivers/sct_dma/bus_traits.h" // ok platform headers; ok platform specific - binds the BIT_BANG bus to the LPC845 SCT+DMA engine under test
#include "platforms/arm/lpc/drivers/sct_dma/lpc_ctimer_pulse_capture.h" // ok platform headers; ok platform specific - the SCT is the transmitter, so the wire is measured with LPC845 CTIMER0 (no core pulse-capture API exists)

namespace autoresearch {
namespace pwm_dma_cl {

// Captures are capped at 20 LEDs (480 bits): five chunks of the default
// 96-bit transmitter chunk, so both descriptor slots are reused on the wire.
constexpr int kMaxCaptureLeds = 20;

struct PwmDmaClocklessState {
    fl::u32 highTicks[kMaxCaptureLeds * 24];
    fl::u8 decoded[kMaxCaptureLeds * 3];
};

inline PwmDmaClocklessState& pwmDmaClocklessState() FL_NO_EXCEPT {
    return fl::SingletonShared<PwmDmaClocklessState>::instance();
}

// Standard WS2812B timing: T0H = 350 ns, T1H = 800 ns, period 1250 ns.
inline fl::ChipsetTimingConfig ws2812b_timing() FL_NO_EXCEPT {
    return fl::ChipsetTimingConfig(/*T1=*/350, /*T2=*/450, /*T3=*/450,
                                   /*reset_us=*/50, "WS2812B");
}

// WS2812B high times with a 1450 ns low phase (period 2250 ns), for the
// CTIMER capture. The capture copies each bit's CR0 by DMA after the bit's
// fall and must finish before the next bit's fall. At 1250 ns that window
// is T3 + T0H = 800 ns (19 ticks at 24 MHz). On the bit whose EV2 makes
// the TX reload its next descriptor, the TX transfer and descriptor fetch
// on DMA0 come first and the capture copy misses the window (FastLED
// #5276). The TX itself is unaffected: on the same bits its writes land
// in time. A longer low phase gives the copy room while the TX runs the
// identical path (masks, chunk refills, slot reuse, halt step).
inline fl::ChipsetTimingConfig captureTiming() FL_NO_EXCEPT {
    return fl::ChipsetTimingConfig(/*T1=*/350, /*T2=*/450, /*T3=*/1450,
                                   /*reset_us=*/50, "WS2812B-long-low");
}

// Build a `ChannelData` of N LEDs; LED i is `(rgb + i * step) & 0xFFFFFF`
// (step 0: one solid color). Colors come from the host as 24-bit RGB and
// are stored GRB (WS2812 wire order).
inline fl::ChannelDataPtr makeChannelData(
        int pin, int num_leds, fl::u32 rgb, fl::u32 step,
        const fl::ChipsetTimingConfig& t = ws2812b_timing()) FL_NO_EXCEPT {
    fl::vector_psram<fl::u8> encoded(static_cast<fl::size>(num_leds * 3));
    for (int i = 0; i < num_leds; ++i) {
        const fl::u32 c = (rgb + static_cast<fl::u32>(i) * step) & 0xFFFFFFu;
        encoded[i * 3 + 0] = static_cast<fl::u8>((c >> 8) & 0xFF);    // G
        encoded[i * 3 + 1] = static_cast<fl::u8>((c >> 16) & 0xFF);   // R
        encoded[i * 3 + 2] = static_cast<fl::u8>(c & 0xFF);           // B
    }
    return fl::ChannelData::create(pin, t, fl::move(encoded));
}

inline fl::ChannelDataPtr makeSolidChannelData(
        int pin, int num_leds, fl::u32 rgb) FL_NO_EXCEPT {
    return makeChannelData(pin, num_leds, rgb, 0u);
}

// READY means the engine is idle, not that the frame went out whole: an
// early halt (a refill missed its deadline) or a DMA error is READY too.
inline bool lastFrameComplete(fl::ChannelEngineLpcSctDma& engine) FL_NO_EXCEPT { // ok platform specific - this file tests only the LPC845 SCT+DMA engine; no other driver reports a cut-short frame, so a core API would have one user
    return engine.transmitter().lastFrameComplete(); // ok platform specific - reads the LPC845 SCT+DMA transmitter's cut-short flag; no core API reports a cut-short frame
}

// Spin poll() until READY, at most `timeout_us`. False on timeout.
inline bool waitReady(fl::IChannelDriver& engine, fl::u32 timeout_us) FL_NO_EXCEPT {
    const fl::u32 t0 = micros();
    while (engine.poll() != fl::IChannelDriver::DriverState::READY) {
        if (micros() - t0 > timeout_us) {
            return false;
        }
    }
    return true;
}

// Frame air time (1.25 us per bit) plus 20 ms of slack.
inline fl::u32 frameTimeoutUs(int led_count) FL_NO_EXCEPT {
    return static_cast<fl::u32>(led_count) * 30u + 20000u;
}

// A data pin the engine may take over: SCT_OUT0 on a reserved pad (SWD,
// ISP, console) would cut the link to the host.
inline bool usablePin(int pin) FL_NO_EXCEPT {
    return pin >= 0 && pin <= 31 && fl::pinProbeSkipReason(pin) == nullptr;
}

// Drive one frame through the channels-API engine. Returns
// "success,started_us,done_us,duration_us" as a CSV string.
inline fl::string frameOnceHandler(int led_count, fl::u32 rgb, int data_pin) FL_NO_EXCEPT {
    if (led_count <= 0 || led_count > 300 || !usablePin(data_pin)) {
        return fl::string("0,0,0,0");
    }
    auto& engine = fl::BusTraits<fl::Bus::BIT_BANG>::instance();
    auto ch = makeSolidChannelData(data_pin, led_count, rgb);
    if (!engine.canHandle(ch)) {
        return fl::string("0,0,0,0");
    }

    const fl::u32 t_start = micros();
    engine.enqueue(ch);
    engine.show();
    const bool ok = waitReady(engine, frameTimeoutUs(led_count)) &&
                    lastFrameComplete(engine);
    const fl::u32 t_end = micros();

    fl::sstream s;
    s << (ok ? 1 : 0) << ',' << t_start << ',' << t_end << ',' << (t_end - t_start);
    return s.str();
}

// Drive `n_frames` back-to-back. Returns "success,total_us,fps_x100" as CSV.
inline fl::string frameBurstHandler(int led_count, fl::u32 rgb,
                                    int n_frames, int data_pin) FL_NO_EXCEPT {
    if (led_count <= 0 || led_count > 300 || n_frames <= 0 ||
        n_frames > 1000 || !usablePin(data_pin)) {
        return fl::string("0,0,0");
    }
    auto& engine = fl::BusTraits<fl::Bus::BIT_BANG>::instance();
    auto ch = makeSolidChannelData(data_pin, led_count, rgb);
    if (!engine.canHandle(ch)) {
        return fl::string("0,0,0");
    }

    // The strip latches only after the line idles low for reset_us; a
    // shorter gap would merge two frames, so each frame (the first too, in
    // case a previous RPC just drove the line) starts that long after the
    // last one ended.
    const fl::u32 reset_us = ws2812b_timing().reset_us;
    // A frame left running by an earlier RPC (one that timed out) must end
    // first; the first frame then waits the full reset time from there.
    if (!waitReady(engine, frameTimeoutUs(kMaxCaptureLeds))) {
        return fl::string("0,0,0");
    }
    const fl::u32 t_start = micros();
    fl::u32 last_end = t_start;
    for (int i = 0; i < n_frames; ++i) {
        while (micros() - last_end < reset_us) {
        }
        engine.enqueue(ch);
        engine.show();
        if (!waitReady(engine, frameTimeoutUs(led_count))) {
            fl::sstream s;
            s << "0,stalled_at_frame=" << i;
            return s.str();
        }
        last_end = micros();
        if (!lastFrameComplete(engine)) {
            fl::sstream s;
            s << "0,short_frame=" << i;
            return s.str();
        }
        // Feed the watchdog every N frames so long bursts don't reset us.
        if ((i & 0x3F) == 0) {
            fl::Watchdog::instance().feed();
        }
    }
    const fl::u32 t_end = micros();
    const fl::u32 total_us = t_end - t_start;
    // fps x 100 to keep two decimals of precision in an integer CSV.
    const fl::u32 fps_x100 = total_us > 0
        ? (static_cast<fl::u32>(n_frames) * 100u * 1000000u / total_us)
        : 0u;

    fl::sstream s;
    s << 1 << ',' << total_us << ',' << fps_x100;
    return s.str();
}

inline fl::u32 ticksToNs(fl::u32 ticks) FL_NO_EXCEPT {
    return ticks * 1000u / static_cast<fl::u32>(F_CPU / 1000000UL);
}

// A rise this long after the previous one ends the capture, so the frame
// comes up short. Capture bits rise every 2250 ns; some WS2812-class LEDs
// latch after only ~6 us low, so any stretch mid-frame is a failure.
constexpr fl::u32 kMaxRiseGapNs = 3000;

// Drive one frame AND measure it on `rx_pin`. Each bit's high time is
// captured by CTIMER0 + DMA; a high time above the midpoint of T0H and T1H
// decodes as '1'. A rise more than kMaxRiseGapNs after the previous one
// stops the capture, so a frame that idles mid-way decodes short. Returns
// "1,<n>,<byte>...,bits=..,t0h=min-max,t1h=min-max,gap_guard=.." (or
// "0,<reason>").
inline fl::string captureFrame(int led_count, fl::u32 rgb, fl::u32 step,
                               int data_pin, int rx_pin, int hold_us,
                               fl::u32 max_gap_ns) FL_NO_EXCEPT {
    const fl::ChipsetTimingConfig t = captureTiming();
    const fl::u32 t0h = fl::lpc_sct_ticks(t.t1_ns);
    const fl::u32 t1h = fl::lpc_sct_ticks(t.t1_ns + t.t2_ns);
    const fl::u32 period = fl::lpc_sct_ticks(t.t1_ns + t.t2_ns + t.t3_ns);

    auto& st = pwmDmaClocklessState();
    const fl::u32 n_bits = static_cast<fl::u32>(led_count) * 24u;
    // Sample halfway between the longest high time and the next bit's
    // rise: CR0 then holds this bit's high time.
    fl::LpcCtimerPulseCapture cap;
    if (!cap.begin(rx_pin, (t1h + period) / 2u, fl::lpc_sct_ticks(max_gap_ns),
                   fl::span<fl::u32>(st.highTicks, n_bits))) {
        return fl::string("0,capture_begin");
    }

    auto& engine = fl::BusTraits<fl::Bus::BIT_BANG>::instance();
    auto ch = makeChannelData(data_pin, led_count, rgb, step, t);
    if (!engine.canHandle(ch)) {
        cap.end();
        return fl::string("0,cannot_handle");
    }
    engine.enqueue(ch);
    engine.show();
    if (hold_us > 0) {
        delayMicroseconds(static_cast<fl::u32>(hold_us));   // no poll()
    }
    const bool sent = waitReady(engine, frameTimeoutUs(led_count));
    delayMicroseconds(20);   // the last bit's MR0 copy lands ~1 us after it
    const fl::u32 got = cap.count();
    cap.end();
    if (!sent) {
        fl::sstream s;
        s << "0,tx_stalled,bits=" << got;
        return s.str();
    }
    if (!lastFrameComplete(engine)) {
        fl::sstream s;
        s << "0,tx_short,bits=" << got;
        return s.str();
    }

    const fl::u32 threshold = (t0h + t1h) / 2u;
    fl::u32 lo0 = 0xFFFFFFFFu, hi0 = 0, lo1 = 0xFFFFFFFFu, hi1 = 0;
    fl::memset(st.decoded, 0, sizeof(st.decoded));
    for (fl::u32 i = 0; i < got; ++i) {
        const fl::u32 h = st.highTicks[i];
        if (h > threshold) {
            st.decoded[i >> 3] |= static_cast<fl::u8>(0x80u >> (i & 7u));
            lo1 = h < lo1 ? h : lo1;
            hi1 = h > hi1 ? h : hi1;
        } else {
            lo0 = h < lo0 ? h : lo0;
            hi0 = h > hi0 ? h : hi0;
        }
    }
    const fl::u32 n_bytes = got / 8u;
    fl::sstream s;
    s << 1 << ',' << n_bytes;
    for (fl::u32 i = 0; i < n_bytes; ++i) {
        s << ',' << static_cast<fl::u32>(st.decoded[i]);
    }
    s << ",bits=" << got;
    s << ",t0h=" << (hi0 ? ticksToNs(lo0) : 0u) << '-' << ticksToNs(hi0);
    s << ",t1h=" << (hi1 ? ticksToNs(lo1) : 0u) << '-' << ticksToNs(hi1);
    s << ",gap_guard=" << max_gap_ns;
    return s.str();
}

inline fl::string captureSelfHandler(int led_count, fl::u32 rgb,
                                     int data_pin, int rx_pin,
                                     int hold_us, fl::u32 step) FL_NO_EXCEPT {
    if (led_count <= 0 || led_count > kMaxCaptureLeds || !usablePin(data_pin) ||
        !usablePin(rx_pin) || rx_pin == data_pin || hold_us < 0 ||
        hold_us > 100000) {
        return fl::string("0,bad_args");
    }
    return captureFrame(led_count, rgb, step, data_pin, rx_pin, hold_us,
                        kMaxRiseGapNs);
}

// Bind the three handlers on the passed-in `Remote`. Call from
// AutoResearch.ino under `#if defined(FASTLED_LPC_PWM_DMA)`.
inline void bind(fl::Remote& remote) FL_NO_EXCEPT {
    remote.bind("pwmDmaClFrameOnce",
        [](int led_count, fl::u32 rgb, int data_pin) -> fl::string {
            return frameOnceHandler(led_count, rgb, data_pin);
        });
    remote.bind("pwmDmaClFrameBurst",
        [](int led_count, fl::u32 rgb, int n_frames, int data_pin) -> fl::string {
            return frameBurstHandler(led_count, rgb, n_frames, data_pin);
        });
    remote.bind("pwmDmaClCaptureSelf",
        [](int led_count, fl::u32 rgb, int data_pin, int rx_pin,
           int hold_us, fl::u32 step_rgb) -> fl::string {
            return captureSelfHandler(led_count, rgb, data_pin, rx_pin,
                                       hold_us, step_rgb);
        });
}

}  // namespace pwm_dma_cl
}  // namespace autoresearch

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_PWM_DMA
