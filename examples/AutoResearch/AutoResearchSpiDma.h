// AutoResearchSpiDma.h — LPC845 SPI + DMA async driver AutoResearch
// validation harness (FastLED #3456, Phase 1 of #3453 bench bring-up).
//
// Device-side RPC handlers that exercise the LPC845 SPI-with-DMA0
// async driver (`ARMHardwareSPIOutputDMA<>` from
// `platforms/arm/lpc/spi_arm_lpc_dma.h`, PR #3454) on real silicon.
//
// Gated on `FL_IS_ARM_LPC_845 && FASTLED_LPC_SPI_DMA`. The whole file
// compiles out on any other build target.
//
// **Sibling of `AutoResearchPwmDmaClockless.h`** (#3468) — same
// architectural shape, different peripheral. The two must not be
// compiled into the same build (both claim DMA0 channels and the
// LowMemory flash budget doesn't fit both). Enforced host-side by
// `bash autoresearch` refusing `--dma-spi --pwm-dma-cl` together.
//
// Handlers registered (call `autoresearch::dma_spi::bind(remote)` from
// AutoResearchLowMemory.h when `FASTLED_LPC_SPI_DMA` is defined):
//
//   `dmaSpiTransferOnce(byte_count, byte_pattern)`
//     Fill an `byte_count`-byte buffer with `byte_pattern`, call
//     `kickDmaStream()`, wait for `done()`, return CSV
//     `"success,started_us,done_us,cpu_busy_bit"`. `cpu_busy_bit` is
//     always 0 in this handler (we wait on the CPU); the async proof
//     is via `dmaSpiTransferOverlap` below.
//
//   `dmaSpiTransferOverlap(byte_count, byte_pattern)`
//     Kick the same stream but run a tight beacon-toggle counter
//     between `kickDmaStream()` and `done() == true`. Reports CSV
//     `"success,total_us,toggle_count"`. Non-zero `toggle_count`
//     under DMA load is the affirmative async claim — the CPU was
//     free to run application code while the DMA0 channel drove
//     SPI TX.
//
//   `dmaSpiMeasureSck(divider)`
//     Emits a short burst at the passed divider; returns
//     `"success,total_us,byte_count"` so the host can compute the
//     effective SCK rate via `SCK_hz = byte_count * 8 / total_s`.
//     No SCT input capture on the SCK pin (that's issue #3015 Phase 3
//     territory — the SCT capture backend is opt-in and doesn't ship
//     the mux configuration for arbitrary SPI pins yet); the host
//     compares the measured effective rate against the requested
//     divider band as a first-order sanity check.
//
// Buffer sizing: the harness reuses the driver's static encode buffer
// (`FASTLED_LPC_SPI_DMA_MAX_BYTES`, default 2048). Requests larger
// than that are clamped by the driver.

#pragma once

#include <FastLED.h>

#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_SPI_DMA)

#include "fl/remote/remote.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/sstream.h"
#include "platforms/arm/lpc/spi_arm_lpc_dma.h" // ok platform headers — AutoResearch driver-specific test needs the concrete SPI DMA driver

namespace autoresearch {
namespace dma_spi {

// Default pin assignment matches the LPC845-BRK SPI0 typical routing.
// FastPin::setOutput() runs against these, but the actual MOSI/SCK
// carrier is routed by SWM independently — fbuild's SystemInit
// configures SPI0 to sensible defaults.
//
// If the user needs to move pins, override at compile time via
// `-DFASTLED_LPC_SPI_DMA_HARNESS_DATA_PIN=<N>` /
// `-DFASTLED_LPC_SPI_DMA_HARNESS_CLOCK_PIN=<N>` /
// `-DFASTLED_LPC_SPI_DMA_HARNESS_DIVIDER=<N>`.
#ifndef FASTLED_LPC_SPI_DMA_HARNESS_DATA_PIN
#define FASTLED_LPC_SPI_DMA_HARNESS_DATA_PIN 17
#endif
#ifndef FASTLED_LPC_SPI_DMA_HARNESS_CLOCK_PIN
#define FASTLED_LPC_SPI_DMA_HARNESS_CLOCK_PIN 13
#endif
#ifndef FASTLED_LPC_SPI_DMA_HARNESS_DIVIDER
// 24 MHz core / 6 → 4 MHz SCK. Fast enough that a 256-byte transfer
// completes in ~0.5 ms — well inside the RPC response window.
#define FASTLED_LPC_SPI_DMA_HARNESS_DIVIDER 6
#endif

using HarnessDriver = ARMHardwareSPIOutputDMA<
    static_cast<fl::u8>(FASTLED_LPC_SPI_DMA_HARNESS_DATA_PIN),
    static_cast<fl::u8>(FASTLED_LPC_SPI_DMA_HARNESS_CLOCK_PIN),
    static_cast<fl::u32>(FASTLED_LPC_SPI_DMA_HARNESS_DIVIDER)>;

// Bench buffer: sized to the driver's encode-buffer capacity so callers
// can hit the widest DMA descriptor the driver will accept.
inline fl::u8* harnessBuffer() FL_NO_EXCEPT {
    static fl::u8 buf[FASTLED_LPC_SPI_DMA_MAX_BYTES] = {0};
    return buf;
}

// Lazy driver instantiation — first RPC call configures SPI+DMA once.
inline HarnessDriver& harnessDriver() FL_NO_EXCEPT {
    static HarnessDriver drv;
    static bool inited = false;
    if (!inited) {
        drv.init();
        inited = true;
    }
    return drv;
}

// Bounds guard for host-supplied lengths — matches
// FASTLED_LPC_SPI_DMA_MAX_BYTES so we can't overflow the static
// encode buffer even if the host misbehaves.
inline int clampByteCount(int n) FL_NO_EXCEPT {
    if (n <= 0) return 0;
    const int cap = static_cast<int>(FASTLED_LPC_SPI_DMA_MAX_BYTES);
    return (n > cap) ? cap : n;
}

// One-shot DMA transfer with CPU wait. CSV: "success,started_us,done_us,cpu_busy".
inline fl::string transferOnceHandler(int byte_count, int byte_pattern) FL_NO_EXCEPT {
    const int len = clampByteCount(byte_count);
    if (len == 0) return fl::string("0,0,0,0");
    fl::u8* buf = harnessBuffer();
    const fl::u8 pat = static_cast<fl::u8>(byte_pattern & 0xFF);
    for (int i = 0; i < len; ++i) buf[i] = pat;

    HarnessDriver& drv = harnessDriver();
    // Ensure prior stream has drained before we time this one.
    drv.waitFully();

    const fl::u32 t_start = micros();
    HarnessDriver::kickDmaStream(buf, static_cast<fl::u32>(len));
    // Poll for done() without WFI so the elapsed measurement matches
    // the CPU-busy-wait cost path. (Callers wanting the WFI sleep path
    // use `waitDma()` — that's not what we're measuring here.)
    while (!HarnessDriver::done()) {
        // spin
    }
    const fl::u32 t_end = micros();

    fl::sstream s;
    s << 1 << ',' << t_start << ',' << t_end << ',' << 0;
    return s.str();
}

// Async proof — kick and beacon-toggle a counter while DMA runs.
// CSV: "success,total_us,toggle_count". Non-zero toggle_count is the
// affirmative async claim.
inline fl::string transferOverlapHandler(int byte_count, int byte_pattern) FL_NO_EXCEPT {
    const int len = clampByteCount(byte_count);
    if (len == 0) return fl::string("0,0,0");
    fl::u8* buf = harnessBuffer();
    const fl::u8 pat = static_cast<fl::u8>(byte_pattern & 0xFF);
    for (int i = 0; i < len; ++i) buf[i] = pat;

    HarnessDriver& drv = harnessDriver();
    drv.waitFully();

    const fl::u32 t_start = micros();
    HarnessDriver::kickDmaStream(buf, static_cast<fl::u32>(len));

    // Beacon-toggle loop. Volatile counter forces the compiler to
    // emit real memory ops each iteration — otherwise the loop
    // collapses and the "async proof" becomes an over-optimized nop.
    volatile fl::u32 toggle_count = 0;
    while (!HarnessDriver::done()) {
        ++toggle_count;
    }
    const fl::u32 t_end = micros();
    const fl::u32 total_us = t_end - t_start;

    fl::sstream s;
    s << 1 << ',' << total_us << ',' << toggle_count;
    return s.str();
}

// SCK measurement via wall-clock timing. CSV: "success,total_us,byte_count".
// Host computes effective SCK from `byte_count * 8 / total_us`. The
// `divider_hint` parameter is echoed back for the log — the compile-time
// driver divider is what actually runs (`FASTLED_LPC_SPI_DMA_HARNESS_DIVIDER`).
inline fl::string measureSckHandler(int divider_hint) FL_NO_EXCEPT {
    (void)divider_hint;
    // 512 bytes @ 4 MHz SCK ≈ 1.024 ms — small enough for RPC latency,
    // big enough that the timer resolution isn't the dominant term.
    constexpr int kBenchBytes = 512;
    fl::u8* buf = harnessBuffer();
    for (int i = 0; i < kBenchBytes; ++i) buf[i] = 0xA5;

    HarnessDriver& drv = harnessDriver();
    drv.waitFully();

    const fl::u32 t_start = micros();
    HarnessDriver::kickDmaStream(buf, static_cast<fl::u32>(kBenchBytes));
    while (!HarnessDriver::done()) {
        // spin
    }
    // Include the SPI master shift-register drain so the wire time
    // covers full byte delivery (kickDmaStream returns when DMA is
    // drained, but the SPI master can still be clocking out the
    // final byte in the shift register).
    drv.waitFully();
    const fl::u32 t_end = micros();
    const fl::u32 total_us = t_end - t_start;

    fl::sstream s;
    s << 1 << ',' << total_us << ',' << kBenchBytes;
    return s.str();
}

// Bind the three handlers on the passed-in `Remote`. Call from
// AutoResearchLowMemory.h under `#if defined(FASTLED_LPC_SPI_DMA)`.
inline void bind(fl::Remote& remote) FL_NO_EXCEPT {
    remote.bind("dmaSpiTransferOnce",
        [](int byte_count, int byte_pattern) -> fl::string {
            return transferOnceHandler(byte_count, byte_pattern);
        });
    remote.bind("dmaSpiTransferOverlap",
        [](int byte_count, int byte_pattern) -> fl::string {
            return transferOverlapHandler(byte_count, byte_pattern);
        });
    remote.bind("dmaSpiMeasureSck",
        [](int divider_hint) -> fl::string {
            return measureSckHandler(divider_hint);
        });
}

}  // namespace dma_spi
}  // namespace autoresearch

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_SPI_DMA
