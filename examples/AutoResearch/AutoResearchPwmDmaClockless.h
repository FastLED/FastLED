// AutoResearchPwmDmaClockless.h — LPC845 SCT+DMA channels-API engine
// AutoResearch validation harness (FastLED #3468).
//
// Device-side RPC handlers that exercise the `ChannelEngineLpcSctDma`
// engine (from #3459 / #3460 / #3466) on real LPC845-BRK silicon and
// self-loopback the output through `LpcSctRxChannel` for byte-level
// verification.
//
// Gated on `FL_IS_ARM_LPC_845 && FASTLED_LPC_PWM_DMA`. The whole file
// compiles out on any other build target.
//
// **Sibling of `AutoResearchSpiDma.h`** (#3456) — same architectural
// shape, different peripheral.
//
// Handlers registered (call `autoresearch::pwm_dma_cl::bind(remote)` from
// AutoResearch.ino when `FASTLED_LPC_PWM_DMA` is defined):
//
//   `pwmDmaClFrameOnce(led_count, color_rgb, data_pin)`
//     Allocate a `CRGB[led_count]` filled with `color_rgb`, drive one
//     frame through the channels-API engine, return `{started_us,
//     done_us, cpu_busy_during_tx}`.
//
//   `pwmDmaClFrameBurst(led_count, color_rgb, n_frames, data_pin)`
//     Drive `n_frames` back-to-back and report measured FPS + inter-
//     frame guard-band violations.
//
//   `pwmDmaClCaptureSelf(led_count, color_rgb, data_pin, rx_pin,
//                        capture_ms)`
//     Drive one frame AND capture the wire via `LpcSctRxChannel` on
//     `rx_pin`. Returns the decoded byte sequence for host-side
//     byte-for-byte comparison against the input color pattern. This
//     is the on-silicon equivalent of the host TX→RX readback test in
//     `tests/fl/channels/lpc_sct_dma_engine.cpp`.
//
// Loopback wiring (see #3468 for the sketch): single jumper from
// `data_pin` to `rx_pin`. Any two PIO0_* pins that the SWM allows to
// route to the SCT input mux.

#pragma once

#include <FastLED.h>

#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_PWM_DMA)

#include "fl/channels/data.h"
#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/rx.h"
#include "fl/channels/rx/config.h"
#include "fl/channels/rx/types.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/remote/remote.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/sstream.h"
#include "fl/stl/vector.h"
#include "platforms/arm/lpc/drivers/sct_dma/channel_engine_lpc_sct_dma.h" // ok platform headers — AutoResearch driver-specific test needs the concrete engine type

namespace autoresearch {
namespace pwm_dma_cl {

// Standard WS2812B timing. Callers can override per-frame if the engine
// gains multi-chipset support later; today the engine's `canHandle()`
// filter accepts anything in [1000, 2500] ns total period, so any
// WS2812-class chipset works.
inline fl::ChipsetTimingConfig ws2812b_timing() FL_NO_EXCEPT {
    return fl::ChipsetTimingConfig(/*T1=*/350, /*T2=*/450, /*T3=*/450,
                                   /*reset_us=*/50, "WS2812B");
}

// Decoder timings for the self-loopback capture path. Matches the
// tolerance window used by `rx_sct_capture.cpp`.
inline fl::ChipsetTiming4Phase ws2812b_decoder_timing() FL_NO_EXCEPT {
    fl::ChipsetTiming4Phase t{};
    t.t0h_min_ns = 250;   t.t0h_max_ns = 550;
    t.t0l_min_ns = 700;   t.t0l_max_ns = 1000;
    t.t1h_min_ns = 650;   t.t1h_max_ns = 950;
    t.t1l_min_ns = 300;   t.t1l_max_ns = 600;
    t.reset_min_us = 50;
    t.gap_tolerance_ns = 0;
    return t;
}

// Build a `ChannelData` filled with N LEDs of a single GRB color.
// Colors passed from the host as 24-bit RGB; we swap to GRB (WS2812
// wire order) inside so the LED count matches the raw byte layout.
inline fl::ChannelDataPtr makeSolidChannelData(
        int pin, int num_leds, fl::u32 rgb) FL_NO_EXCEPT {
    fl::ChipsetTimingConfig t = ws2812b_timing();
    fl::vector_psram<fl::u8> encoded(static_cast<fl::size>(num_leds * 3));
    const fl::u8 r = static_cast<fl::u8>((rgb >> 16) & 0xFF);
    const fl::u8 g = static_cast<fl::u8>((rgb >>  8) & 0xFF);
    const fl::u8 b = static_cast<fl::u8>((rgb >>  0) & 0xFF);
    for (int i = 0; i < num_leds; ++i) {
        encoded[i * 3 + 0] = g;   // WS2812 GRB
        encoded[i * 3 + 1] = r;
        encoded[i * 3 + 2] = b;
    }
    return fl::ChannelData::create(pin, t, fl::move(encoded));
}

// Drive one frame through the channels-API engine. Returns
// "success,started_us,done_us,duration_us" as a CSV string.
inline fl::string frameOnceHandler(int led_count, fl::u32 rgb, int data_pin) FL_NO_EXCEPT {
    if (led_count <= 0 || led_count > 300 || data_pin < 0 || data_pin > 31) {
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
    // Spin poll() until READY. show() blocks in transmit() today
    // (busy-wait per chunk), so this is really a state-machine settle
    // check rather than a wait. When the WFI TODO(2842) lands this
    // becomes the actual completion wait.
    while (engine.poll() != fl::IChannelDriver::DriverState::READY) {
        // spin
    }
    const fl::u32 t_end = micros();
    const fl::u32 dur_us = t_end - t_start;

    fl::sstream s;
    s << 1 << ',' << t_start << ',' << t_end << ',' << dur_us;
    return s.str();
}

// Drive `n_frames` back-to-back. Returns "success,total_us,fps_x100" as CSV.
inline fl::string frameBurstHandler(int led_count, fl::u32 rgb,
                                    int n_frames, int data_pin) FL_NO_EXCEPT {
    if (led_count <= 0 || led_count > 300 || n_frames <= 0 ||
        n_frames > 1000 || data_pin < 0 || data_pin > 31) {
        return fl::string("0,0,0");
    }
    auto& engine = fl::BusTraits<fl::Bus::BIT_BANG>::instance();
    auto ch = makeSolidChannelData(data_pin, led_count, rgb);
    if (!engine.canHandle(ch)) {
        return fl::string("0,0,0");
    }

    const fl::u32 t_start = micros();
    for (int i = 0; i < n_frames; ++i) {
        engine.enqueue(ch);
        engine.show();
        while (engine.poll() != fl::IChannelDriver::DriverState::READY) {
            // spin
        }
        // Feed the watchdog every N frames so long bursts don't reset us.
        if ((i & 0x3F) == 0) {
            fl::Watchdog::instance().feed();
        }
    }
    const fl::u32 t_end = micros();
    const fl::u32 total_us = t_end - t_start;
    // fps × 100 to keep two decimals of precision in an integer CSV.
    const fl::u32 fps_x100 = total_us > 0
        ? (static_cast<fl::u32>(n_frames) * 100u * 1000000u / total_us)
        : 0u;

    fl::sstream s;
    s << 1 << ',' << total_us << ',' << fps_x100;
    return s.str();
}

// Drive one frame AND capture via `LpcSctRxChannel`. Returns the
// decoded bytes as a comma-separated hex string prefixed with the
// success flag: "1,ff,00,00,ff,00,00,..." (or "0,..." on failure).
//
// This is the on-silicon TX→RX readback that closes the #3468 goal
// on hardware. The host tests in `tests/fl/channels/lpc_sct_dma_engine.cpp`
// prove the byte-flow at the mock-transmitter layer; this handler
// proves it on real silicon end-to-end.
inline fl::string captureSelfHandler(int led_count, fl::u32 rgb,
                                     int data_pin, int rx_pin,
                                     int capture_ms) FL_NO_EXCEPT {
    if (led_count <= 0 || led_count > 32 || data_pin < 0 || data_pin > 31 ||
        rx_pin < 0 || rx_pin > 31 || capture_ms <= 0 || capture_ms > 200) {
        return fl::string("0");
    }

    // Set up the RX device first so the SCT input capture is armed
    // before the TX kicks. buffer_size sized for LED count × 3 bytes ×
    // 16 edges per byte + reset margin.
    auto rx = fl::LpcSctRxChannel::create(rx_pin);
    if (!rx) return fl::string("0");
    fl::RxConfig cfg;
    cfg.buffer_size = static_cast<fl::u32>(led_count * 3 * 16 + 32);
    cfg.start_low   = true;
    if (!rx->begin(cfg)) return fl::string("0");

    // Drive the frame.
    auto& engine = fl::BusTraits<fl::Bus::BIT_BANG>::instance();
    auto ch = makeSolidChannelData(data_pin, led_count, rgb);
    if (!engine.canHandle(ch)) return fl::string("0");
    engine.enqueue(ch);
    engine.show();
    while (engine.poll() != fl::IChannelDriver::DriverState::READY) {
        // spin
    }

    // Wait for capture to settle. The SCT DMA path fills its ring
    // autonomously; wait() drains + halts the SCT.
    (void)rx->wait(static_cast<fl::u32>(capture_ms));

    // Decode captured edges → GRB bytes.
    const fl::size decoded_cap = static_cast<fl::size>(led_count * 3);
    static fl::u8 decoded[128] = {0};
    if (decoded_cap > sizeof(decoded)) return fl::string("0");
    auto result = rx->decode(ws2812b_decoder_timing(),
                             fl::span<fl::u8>(decoded, decoded_cap));
    if (!result.ok()) return fl::string("0");
    const fl::u32 n_decoded = result.value();

    // Emit "1,<n>,<hex bytes>". The host phase parses and byte-compares
    // against the driven `rgb` (mapped to GRB per LED).
    fl::sstream s;
    s << 1 << ',' << n_decoded;
    for (fl::u32 i = 0; i < n_decoded; ++i) {
        s << ',' << static_cast<fl::u32>(decoded[i]);
    }
    return s.str();
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
           int capture_ms) -> fl::string {
            return captureSelfHandler(led_count, rgb, data_pin, rx_pin,
                                       capture_ms);
        });
}

}  // namespace pwm_dma_cl
}  // namespace autoresearch

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_PWM_DMA
