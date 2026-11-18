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
#include "fl/system/pin.h"
#include "fl/system/pin_probe.h"
#include "platforms/arm/lpc/drivers/sct_dma/channel_engine_lpc_sct_dma.h" // ok platform headers; ok platform specific - this LPC845-only harness tests the concrete SCT+DMA engine type
// The LPC BusTraits<Bus::BIT_BANG> specialization this file names. It used to
// arrive only via clockless_channel_lpc.h, which is skipped when another
// ClocklessController was already picked, leaving the type incomplete.
#include "platforms/arm/lpc/drivers/sct_dma/bus_traits.h" // ok platform headers; ok platform specific - binds the BIT_BANG bus to the LPC845 SCT+DMA engine under test
#include "platforms/arm/lpc/drivers/sct_dma/lpc_ctimer_pulse_capture.h" // ok platform headers; ok platform specific - the SCT is the transmitter, so the wire is measured with LPC845 CTIMER0 (no core pulse-capture API exists)
#include "platforms/arm/lpc/lpc_dma_descriptor_table.h" // ok platform headers; ok platform specific - #5278 fault injection and DMA0 stressor address LPC845 DMA descriptors
#include "platforms/arm/lpc/lpc_dma_isr.h" // ok platform headers; ok platform specific - FASTLED_LPC_DMA_ISR selects the expected long-frame behaviour (#5278)

namespace autoresearch {
namespace pwm_dma_cl {

// Captures are capped at 17 LEDs (408 bits): still five chunks of the
// default 96-bit transmitter chunk, so both descriptor slots are reused on
// the wire. Not more: the capture buffer is static, and this core's _sbrk
// lets the heap grow into the stack unchecked. At 20 LEDs the stack and heap
// were 244 bytes apart, and a 20-LED reply HardFaulted every second call
// (#5278).
constexpr int kMaxCaptureLeds = 17;

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

// Min-max of captured high times, printed as "<min>-<max>" in ns ("0-0"
// when empty).
struct HighRange {
    fl::u32 lo = 0xFFFFFFFFu;
    fl::u32 hi = 0;
    void add(fl::u32 ticks) FL_NO_EXCEPT {
        lo = ticks < lo ? ticks : lo;
        hi = ticks > hi ? ticks : hi;
    }
};

inline fl::sstream& operator<<(fl::sstream& s, const HighRange& r) FL_NO_EXCEPT {
    s << (r.hi ? ticksToNs(r.lo) : 0u) << '-' << ticksToNs(r.hi);
    return s;
}

// ---------------------------------------------------------------------------
// #5278 cases: a forced DMA error, and DMA0 contention.
// ---------------------------------------------------------------------------

// The second chunk's descriptor is the one the transmitter's channel links
// to. Pointing its destination at GPIO->SET[0], which DMA0 cannot reach
// (the IOP bus, UM11029 ch. 2), makes the chunk-1 reload raise ERRINT. Must
// run while chunk 0 plays, i.e. right after show(). False if the frame has
// no second chunk.
inline bool corruptSecondChunk() FL_NO_EXCEPT { // ok platform specific - LPC845 DMA0 fault injection for the #5278 error-path case; no core API injects a DMA error
    volatile fl::lpc::DmaChannelDescriptor* table =
        reinterpret_cast<volatile fl::lpc::DmaChannelDescriptor*>(DMA0->SRAMBASE); // ok reinterpret cast - DMA descriptor table base
    const fl::u32 link = table[FASTLED_LPC_PWM_DMA_BASECH].link;
    if (link == 0u) {
        return false;
    }
    volatile fl::lpc::DmaChannelDescriptor* next =
        reinterpret_cast<volatile fl::lpc::DmaChannelDescriptor*>(link); // ok reinterpret cast - linked DMA descriptor address
    next->dst_end = reinterpret_cast<fl::u32>(&GPIO->SET[0]); // ok reinterpret cast - MMIO address the DMA cannot reach
    return true;
}

// The contention stressor: a memory-to-memory DMA0 channel (a self-linked
// 1024-word descriptor between two RAM words) in one of two modes:
//  - unpaced: software-triggered, so it requests all the time. No FastLED
//    driver does this; it is the worst case.
//  - paced: hardware-triggered by SCT DMA request 1 on the transmitter's own
//    T1H event, moving a burst of 2^burst_power words per bit. Every FastLED
//    DMA0 user is peripheral-paced like this, one transfer per request
//    (burst_power 0; UART-DMA requests once per byte, i.e. less often), and
//    colliding on the same event is the worst case for them.
// Channel 7 is clear of the transmitter (0), the capture (1), the USART
// requests (2..5) and SPI (4/6).
constexpr fl::u32 kStressCh = 7u;

inline volatile fl::lpc::DmaChannelDescriptor& stressLinkDesc() FL_NO_EXCEPT {
    static volatile fl::lpc::DmaChannelDescriptor d;
    return d;
}

inline volatile fl::u32* stressWords() FL_NO_EXCEPT {
    static volatile fl::u32 w[2];
    return w;
}

inline void stressStop() FL_NO_EXCEPT { // ok platform specific - LPC845 DMA0 register sequence (UM11029 16.6.15) for the bench stressor
    DMA0->COMMON[0].ENABLECLR = (1u << kStressCh);
    for (fl::u32 i = 0; i < 1000u && (DMA0->COMMON[0].BUSY & (1u << kStressCh)); ++i) {
    }
    DMA0->COMMON[0].ABORT = (1u << kStressCh);
    SCT0->DMAREQ1 = 0u;
}

// Start the stressor at CHPRIORITY `priority` (0 highest, 7 lowest; the
// transmitter and the capture run at 0). Paced mode must start after show(),
// since the transmitter clears SCT DMAREQ1 when it sets up.
inline void stressStart(fl::u32 priority, bool paced,
                        fl::u32 burst_power) FL_NO_EXCEPT { // ok platform specific - LPC845 DMA0 memory-to-memory stressor for the #5278 contention case
    fl::lpc::ensureDmaSramBase();
    // CFGVALID | RELOAD | SWTRIG (unpaced only) | 32-bit, no address
    // increments, 1024 words.
    const fl::u32 xfer = (1u << 0) | (1u << 1) | (paced ? 0u : (1u << 2)) |
                         (2u << 8) | (1023u << 16);
    volatile fl::u32* w = stressWords();
    volatile fl::lpc::DmaChannelDescriptor& link = stressLinkDesc();
    link.reserved = xfer;
    link.src_end = reinterpret_cast<fl::u32>(&w[0]); // ok reinterpret cast - RAM source address
    link.dst_end = reinterpret_cast<fl::u32>(&w[1]); // ok reinterpret cast - RAM destination address
    link.link = reinterpret_cast<fl::u32>(&link); // ok reinterpret cast - self-linked descriptor
    stressStop();
    volatile fl::lpc::DmaChannelDescriptor* table =
        reinterpret_cast<volatile fl::lpc::DmaChannelDescriptor*>(DMA0->SRAMBASE); // ok reinterpret cast - DMA descriptor table base
    table[kStressCh].src_end = link.src_end;
    table[kStressCh].dst_end = link.dst_end;
    table[kStressCh].link = link.link;
    // Paced: SCT_DMA1 trigger (UM11029 Table 288), HWTRIGEN, rising edge,
    // TRIGBURST of 2^burst_power transfers.
    INPUTMUX->DMA_ITRIG_INMUX[kStressCh] = paced ? 0x3u : 0xFu;
    DMA0->CHANNEL[kStressCh].CFG =
        (paced ? ((1u << 1) | (1u << 4) | (1u << 6) | ((burst_power & 15u) << 8))
               : 0u) |
        ((priority & 7u) << 16);
    if (paced) {
        SCT0->DMAREQ1 = SCT0->DMAREQ0;   // the transmitter's T1H event
    }
    DMA0->COMMON[0].SETVALID = (1u << kStressCh);
    DMA0->COMMON[0].ENABLESET = (1u << kStressCh);
    DMA0->CHANNEL[kStressCh].XFERCFG = xfer;   // SWTRIG starts it
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
                               fl::u32 max_gap_ns,
                               const fl::ChipsetTimingConfig& t = captureTiming(),
                               int stress_priority = -1,
                               int stress_paced = 0,
                               bool reload_stats = false) FL_NO_EXCEPT {
    const fl::u32 t0h = fl::lpc_sct_ticks(t.t1_ns);
    const fl::u32 t1h = fl::lpc_sct_ticks(t.t1_ns + t.t2_ns);
    const fl::u32 period = fl::lpc_sct_ticks(t.t1_ns + t.t2_ns + t.t3_ns);

    auto& st = pwmDmaClocklessState();
    const fl::u32 n_bits = static_cast<fl::u32>(led_count) * 24u;
    // Sample halfway between the longest high time and the next bit's
    // rise: CR0 then holds this bit's high time. The capture's DMA copy must
    // land before the next bit's fall, or it records that bit instead; for
    // the stressor's RPC (reload_stats) the sample moves to 100 ns after
    // T1H, so the capture's DMA deadline is within ~150 ns of the
    // transmitter's (#5278), not ~250 ns tighter.
    const fl::u32 sample = reload_stats ? t1h + fl::lpc_sct_ticks(100)
                                        : (t1h + period) / 2u;
    fl::LpcCtimerPulseCapture cap;
    if (!cap.begin(rx_pin, sample, fl::lpc_sct_ticks(max_gap_ns),
                   fl::span<fl::u32>(st.highTicks, n_bits))) {
        return fl::string("0,capture_begin");
    }

    auto& engine = fl::BusTraits<fl::Bus::BIT_BANG>::instance();
    auto ch = makeChannelData(data_pin, led_count, rgb, step, t);
    if (!engine.canHandle(ch)) {
        cap.end();
#if !FASTLED_LPC_DMA_ISR
        // Without the DMA ISR hub the engine refuses frames over two chunks
        // (#5278); the host expects this refusal in such a build.
        if (n_bits > 2u * FASTLED_LPC_PWM_DMA_CHUNK_BITS) {
            return fl::string("0,hub_off_refused");
        }
#endif
        return fl::string("0,cannot_handle");
    }
    if (stress_priority >= 0 && stress_paced == 0) {
        stressStart(static_cast<fl::u32>(stress_priority), false, 0u);
    }
    engine.enqueue(ch);
    engine.show();
    if (stress_priority >= 0 && stress_paced > 0) {
        stressStart(static_cast<fl::u32>(stress_priority), true,
                    static_cast<fl::u32>(stress_paced - 1));
    }
    if (hold_us > 0) {
        delayMicroseconds(static_cast<fl::u32>(hold_us));   // no poll()
    }
    const bool sent = waitReady(engine, frameTimeoutUs(led_count));
    delayMicroseconds(20);   // the last bit's MR0 copy lands ~1 us after it
    if (stress_priority >= 0) {
        stressStop();
    }
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
    // Overall ranges, and separately the bits right after a chunk reload
    // (bit i with i % CHUNK_BITS == 0, i > 0): their clear mask is the first
    // transfer of a freshly loaded descriptor, the tightest DMA deadline.
    HighRange r0, r1, rr0, rr1;
    fl::memset(st.decoded, 0, sizeof(st.decoded));
    for (fl::u32 i = 0; i < got; ++i) {
        const fl::u32 h = st.highTicks[i];
        const bool reload = i > 0u && (i % FASTLED_LPC_PWM_DMA_CHUNK_BITS) == 0u;
        if (h > threshold) {
            st.decoded[i >> 3] |= static_cast<fl::u8>(0x80u >> (i & 7u));
            r1.add(h);
            if (reload) rr1.add(h);
        } else {
            r0.add(h);
            if (reload) rr0.add(h);
        }
    }
    const fl::u32 n_bytes = got / 8u;
    fl::sstream s;
    if (reload_stats) {
        // Compact reply for the stressor's RPC: the bytes are compared here,
        // because a ~250-character reply sent back to back drops replies on
        // the way to the host. LED i is (rgb + i * step), sent G, R, B.
        fl::u32 bad = n_bytes == n_bits / 8u ? 0u : 1u;
        fl::u32 first = 0xFFFFFFFFu, first_exp = 0;
        for (fl::u32 i = 0; i < n_bytes && i < n_bits / 8u; ++i) {
            const fl::u32 c = (rgb + (i / 3u) * step) & 0xFFFFFFu;
            const fl::u32 shift = (i % 3u) == 0u ? 8u : ((i % 3u) == 1u ? 16u : 0u);
            const fl::u8 want = static_cast<fl::u8>(c >> shift);
            if (st.decoded[i] != want) {
                if (bad == 0u) {
                    first = i;
                    first_exp = want;
                }
                ++bad;
            }
        }
        s << 1 << ",bytes=" << n_bytes << ",bad=" << bad;
        if (first != 0xFFFFFFFFu) {   // first bad byte: index:got/expected
            s << ",first=" << first << ':'
              << static_cast<fl::u32>(st.decoded[first]) << '/' << first_exp;
        }
        s << ",bits=" << got
          << ",t0h=" << r0 << ",t1h=" << r1 << ",reload_t0h=" << rr0
          << ",reload_t1h=" << rr1;
        return s.str();
    }
    s << 1 << ',' << n_bytes;
    for (fl::u32 i = 0; i < n_bytes; ++i) {
        s << ',' << static_cast<fl::u32>(st.decoded[i]);
    }
    s << ",bits=" << got;
    s << ",t0h=" << r0 << ",t1h=" << r1;
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

// #5278: `pwmDmaClCaptureStress` is `pwmDmaClCaptureSelf` with a choice of
// timing (`real_timing` 1: WS2812 1250 ns bits; 0: the 1450 ns low phase of
// captureTiming()) and an optional DMA0 stressor running for the whole frame
// at CHPRIORITY `stress_priority` (-1: none). `paced` 0 is the unpaced
// stressor; n >= 1 is paced with 2^(n-1) words per bit (see stressStart()).
inline fl::string captureStressHandler(int led_count, fl::u32 rgb,
                                       int data_pin, int rx_pin, fl::u32 step,
                                       int real_timing, int stress_priority,
                                       int paced) FL_NO_EXCEPT {
    if (led_count <= 0 || led_count > kMaxCaptureLeds || !usablePin(data_pin) ||
        !usablePin(rx_pin) || rx_pin == data_pin || stress_priority < -1 ||
        stress_priority > 7 || paced < 0 || paced > 4) {
        return fl::string("0,bad_args");
    }
    return captureFrame(led_count, rgb, step, data_pin, rx_pin, 0,
                        kMaxRiseGapNs,
                        real_timing ? ws2812b_timing() : captureTiming(),
                        stress_priority, paced, /*reload_stats=*/true);
}

// #5278: force a DMA error at the first chunk reload (corruptSecondChunk())
// and check that the frame ends cleanly. `hold_us` > 0 skips poll() that
// long first, as FastLED.show() does, so only the DMA interrupt can end it.
// The capture runs to its full kMaxCaptureLeds buffer, so stale bits clocked out
// after the error are counted. Returns
// "1,ready=<0|1>,complete=<0|1>,bits=<n>,chunk_bits=<c>,line=<0|1>,hub=<0|1>":
// a clean failure is ready=1, complete=0, line=0, and bits just past
// chunk_bits (the error is taken one bit after the reload). Without the DMA
// ISR hub (hub=0) only poll() stops it, so with hold_us > 0 stale bits run
// on until the hold ends.
inline fl::string dmaErrorHandler(int led_count, fl::u32 rgb, int data_pin,
                                  int rx_pin, int hold_us) FL_NO_EXCEPT {
    const fl::u32 n_bits = static_cast<fl::u32>(led_count) * 24u;
    if (n_bits <= FASTLED_LPC_PWM_DMA_CHUNK_BITS || led_count > kMaxCaptureLeds ||
        !usablePin(data_pin) || !usablePin(rx_pin) || rx_pin == data_pin ||
        hold_us < 0 || hold_us > 100000) {
        return fl::string("0,bad_args");
    }
    const fl::ChipsetTimingConfig t = captureTiming();
    const fl::u32 t1h = fl::lpc_sct_ticks(t.t1_ns + t.t2_ns);
    const fl::u32 period = fl::lpc_sct_ticks(t.t1_ns + t.t2_ns + t.t3_ns);
    auto& engine = fl::BusTraits<fl::Bus::BIT_BANG>::instance();
    if (!waitReady(engine, frameTimeoutUs(kMaxCaptureLeds))) {
        return fl::string("0,busy");
    }
    auto ch = makeChannelData(data_pin, led_count, rgb, 0u, t);
    if (!engine.canHandle(ch)) {
        return fl::string("0,cannot_handle");
    }
    auto& st = pwmDmaClocklessState();
    fl::LpcCtimerPulseCapture cap;
    if (!cap.begin(rx_pin, (t1h + period) / 2u, fl::lpc_sct_ticks(kMaxRiseGapNs),
                   fl::span<fl::u32>(st.highTicks, kMaxCaptureLeds * 24))) {
        return fl::string("0,capture_begin");
    }
    engine.enqueue(ch);
    engine.show();
    // Chunk 0 plays for 96 bits (216 us at this timing); the reload that
    // fails comes after that.
    if (!corruptSecondChunk()) {
        cap.end();
        engine.poll();
        return fl::string("0,no_second_chunk");
    }
    if (hold_us > 0) {
        delayMicroseconds(static_cast<fl::u32>(hold_us));   // no poll()
    }
    const bool ready = waitReady(engine, frameTimeoutUs(led_count));
    delayMicroseconds(20);
    const fl::u32 got = cap.count();
    cap.end();
    fl::pinMode(rx_pin, fl::PinMode::Input);
    const bool line_high = fl::digitalRead(rx_pin) == fl::PinValue::High;
    fl::sstream s;
    s << 1 << ",ready=" << (ready ? 1 : 0)
      << ",complete=" << (lastFrameComplete(engine) ? 1 : 0)
      << ",bits=" << got << ",chunk_bits=" << FASTLED_LPC_PWM_DMA_CHUNK_BITS
      << ",line=" << (line_high ? 1 : 0) << ",hub=" << FASTLED_LPC_DMA_ISR;
    return s.str();
}

// Bind the handlers on the passed-in `Remote`. Call from
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
    remote.bind("pwmDmaClCaptureStress",
        [](int led_count, fl::u32 rgb, int data_pin, int rx_pin,
           fl::u32 step_rgb, int real_timing, int stress_priority,
           int paced) -> fl::string {
            return captureStressHandler(led_count, rgb, data_pin, rx_pin,
                                        step_rgb, real_timing, stress_priority,
                                        paced);
        });
    remote.bind("pwmDmaClDmaError",
        [](int led_count, fl::u32 rgb, int data_pin, int rx_pin,
           int hold_us) -> fl::string {
            return dmaErrorHandler(led_count, rgb, data_pin, rx_pin, hold_us);
        });
}

}  // namespace pwm_dma_cl
}  // namespace autoresearch

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_PWM_DMA
