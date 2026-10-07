#pragma once

// fft_backend.h — FFT backend dispatcher for fl::audio::fft
//
// Centralizes the forward real-to-complex FFT call so the implementation can
// select between:
//   - kiss_fftr (third_party, portable, default everywhere)
//   - ESP-DSP dsps_fft2r_fc32 (ESP32*, auto-detected via FL_HAS_INCLUDE("esp_dsp.h"))
//   - CMSIS-DSP arm_rfft_fast_f32 (ARM Cortex-M4/M7/M33, FUTURE)
//
// ----------------------------------------------------------------------------
// Hardware-FFT API survey (2026, per issue #2308 research)
// ----------------------------------------------------------------------------
// There is NO universal standardized real-FFT API across the embedded
// ecosystem. The two leading candidates have semantically different output
// layouts:
//
//   kiss_fftr (FastLED current):  N real in → N/2+1 `kiss_fft_cpx`. DC in
//                                 bin[0] (imag=0), Nyquist in bin[N/2]
//                                 (imag=0). Total output = (N/2+1)*2 floats.
//
//   ESP-DSP dsps_fft2r_fc32:      In-place N-complex FFT. For real input we
//                                 do the packed N/2-point complex + manual
//                                 unpack trick (this file). Separate
//                                 dsps_fft2r_init_* allocates global twiddle
//                                 tables (once per process).
//                                 Sizes: power-of-2, ≤ CONFIG_DSP_MAX_FFT_SIZE.
//
//   CMSIS-DSP arm_rfft_fast_f32:  ARM's de facto "standard" for Cortex-M4+.
//                                 Packs DC into out[0].r AND Nyquist into
//                                 out[0].i — total output = N floats. Must
//                                 be unpacked to kiss_fftr shape. Sizes
//                                 restricted to {32,64,128,256,512,1024,
//                                 2048,4096}. Typical 4-5× speedup on
//                                 Cortex-M4 with FPU vs kiss_fftr.
//
//   Teensy Audio Library:         Wraps CMSIS-DSP under the hood; exposes
//                                 only magnitude not raw complex.
//
//   ARM Cortex-M0+ (RP2040 etc.): No FPU. CMSIS-DSP offers Q15/Q31 only —
//                                 different output semantics, require
//                                 post-scaling by 1/(N/2). Not drop-in.
//
//   Host (x86_64/aarch64):        FFTW (gold standard), Apple vDSP, pocketfft
//                                 — all vendor-specific layouts.
//
// ----------------------------------------------------------------------------
// Decision: keep kiss_fftr's layout as FastLED's internal abstraction.
// ----------------------------------------------------------------------------
// Rationale: kiss_fft_cpx {float r, i;} and the N/2+1-bin half-spectrum is
// already what every downstream audio consumer assumes (magnitude, binning,
// CQ kernels, windowing, AudioContext cache). Changing it would require
// rewriting all detectors + the ESP-DSP conversion glue ends up nearly as
// complex as just keeping the shape. Each backend's conversion happens
// privately inside `fl_fft_real_forward()`:
//
//   kiss backend    → identity (no conversion)
//   ESP-DSP backend → pack N reals as N/2 complex, complex FFT, unpack
//                     conjugate-symmetric output to N/2+1 kiss_fft_cpx bins
//   CMSIS backend   → call arm_rfft_fast_f32, then split out[0] into
//                     DC and Nyquist positions expected by kiss layout
//
// ----------------------------------------------------------------------------
// Auto-detect: ESP-DSP on ESP32 is typically 3-5× faster than kiss_fftr.
// Expected cost on ESP32-S3: ~15 µs vs ~54 µs for the default 512-point
// real FFT. See issue #2308 for the broader audio performance plan.
//
// Gate has moved from user opt-in (FL_FFT_USE_ESP_DSP=1) to automatic
// `__has_include("esp_dsp.h")` detection — same pattern used for esp_cache.h
// in src/platforms/esp/32/drivers/lcd_spi/. The ESP32 variants that do not
// ship esp_dsp in their toolchain bundle (esp32c2 / esp32s2 / esp32h2 /
// esp32c5) automatically fall through to the kiss_fftr scalar path; no per-
// example `@filter` line is needed. See issue #2629 / PR #2625 for history.
//
// The math is the standard "real FFT from packed N/2-complex FFT" identity;
// see inline comments on the unpack for derivation.

#include "fl/stl/compiler_control.h"
#include "fl/stl/has_include.h"
#include "platforms/is_platform.h"
// IWYU pragma: begin_keep
#include "third_party/cq_kernel/kiss_fftr.h"
// IWYU pragma: end_keep

// FL_FFT_ESP_DSP_AVAILABLE: ESP-DSP backend code is compiled in.
// FL_FFT_ESP_DSP_ACTIVE:    dispatcher routes real FFT calls through it.
//
// Both gate on `defined(FL_IS_ESP32) && FL_HAS_INCLUDE("esp_dsp.h")` so the
// backend is silently elided on toolchains that do not provide esp_dsp.h
// (esp32c2 / esp32s2 / esp32h2 / esp32c5). The AudioFftParity example
// exercises the backend on any ESP32 board where the header is present.
//
// NOTE: the ESP-DSP implementation below is NOT yet bit-for-bit validated
// against kiss_fftr — hardware sanity tests via AudioFftParity currently
// report incorrect output (flat magnitudes for DC / single-tone inputs).
// Root cause is under investigation; the dispatcher (`fl_fft_real_forward`)
// stays on kiss_fftr by default. See issue #2308.
#if defined(FL_IS_ESP32) && FL_HAS_INCLUDE("esp_dsp.h")
#define FL_FFT_ESP_DSP_AVAILABLE 1
#else
#define FL_FFT_ESP_DSP_AVAILABLE 0
#endif

#define FL_FFT_ESP_DSP_ACTIVE FL_FFT_ESP_DSP_AVAILABLE

namespace fl {
namespace audio {
namespace fft {

#if FL_FFT_ESP_DSP_AVAILABLE

namespace detail {

/// @brief Forward real FFT through the ESP-DSP backend.
/// Implementation and vendor headers are isolated in the ESP32 platform TU.
void espDspRealForward(int N, const float *in,
                       kiss_fft_cpx *out) FL_NO_EXCEPT;

} // namespace detail

#endif // FL_FFT_ESP_DSP_AVAILABLE

/// @brief Forward real-to-complex FFT.
/// @param cfg   Kiss FFT real-to-complex plan describing the size N.
///              Used directly by the kiss backend; the ESP-DSP backend
///              ignores cfg and uses its own cached context keyed by N.
/// @param N     Logical FFT size (must match the size cfg was allocated for).
///              Redundant for kiss (embedded in cfg) but required for ESP-DSP
///              because kiss_fftr_cfg is opaque.
/// @param in    Pointer to N real time-domain samples.
/// @param out   Pointer to N/2 + 1 complex frequency-domain bins.
///
/// Output format matches kiss_fftr exactly regardless of backend.
inline void fl_fft_real_forward(kiss_fftr_cfg cfg, int N,
                                const kiss_fft_scalar *in,
                                kiss_fft_cpx *out) FL_NO_EXCEPT {
    // NOTE: even when FL_FFT_ESP_DSP_ACTIVE is 1, the dispatcher currently
    // stays on kiss_fftr. The ESP-DSP backend below is `float`-only but
    // kiss_fft_scalar is typedef'd to `int16_t` when FastLED's default
    // FASTLED_FFT_PRECISION=FASTLED_FFT_FIXED16 is in effect. Auto-routing
    // through ESP-DSP would break the int16 call sites in fft_impl.cpp.hpp.
    //
    // Future work: either (a) add an int16 packed-FFT variant using
    // dsps_fft2r_sc16 to match FIXED16 mode, or (b) auto-switch the library
    // to FASTLED_FFT_FLOAT when FL_FFT_ESP_DSP_AVAILABLE is on. Once wired,
    // replace the below with a conditional call to detail::espDspRealForward.
    //
    // The ESP-DSP code is compiled in whenever the ESP32 toolchain ships
    // esp_dsp.h (auto-detected via FL_HAS_INCLUDE) and exposed as
    // fl::audio::fft::detail::espDspRealForward() so the AudioFftParity
    // example can exercise it directly for validation.
    (void)N;
    kiss_fftr(cfg, in, out);
}

} // namespace fft
} // namespace audio
} // namespace fl
