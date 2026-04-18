#pragma once

// fft_backend.h — FFT backend dispatcher for fl::audio::fft
//
// Centralizes the forward real-to-complex FFT call so the implementation can
// select between:
//   - kiss_fftr (third_party, portable, default everywhere)
//   - ESP-DSP dsps_fft2r_fc32 (ESP32*, opt-in via FL_FFT_USE_ESP_DSP=1)
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
//                                 unpack trick. Separate `dsps_fft2r_init_*`
//                                 allocates global twiddle tables (once).
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
// This isolates the FFT layout quirks of each vendor library at a single
// boundary and leaves all of fl::audio::fft downstream code untouched.
//
// ----------------------------------------------------------------------------
// Opt-in: ESP-DSP on ESP32 is typically 3-5× faster than kiss_fftr because
// it uses hardware MAC / radix-2 butterfly instructions. Expected cost on
// ESP32-S3: ~15 µs vs ~54 µs for the default 512-point real FFT. See issue
// #2308 for the broader audio performance plan.
//
// Default is OFF — enabling requires additional validation (numeric output
// parity vs kiss_fftr, hardware benchmarks, linker verification across ESP32
// variants). Users can opt in via:
//
//   -DFL_FFT_USE_ESP_DSP=1
//
// in build_flags. Future work: validate + land the ESP-DSP real-FFT
// implementation (packing/unpacking the conjugate-symmetric spectrum), then
// auto-enable. CMSIS-DSP backend is a parallel future track for STM32 /
// Teensy 4.x / nRF52 / SAMD51 (same Cortex-M4+ class).

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"
#include "third_party/cq_kernel/kiss_fftr.h"

// Opt-in at compile time. Defaults to OFF everywhere — no behavior change.
#ifndef FL_FFT_USE_ESP_DSP
#define FL_FFT_USE_ESP_DSP 0
#endif

// Guard: ESP-DSP only makes sense on ESP32 platforms that have DSP
// acceleration. Anywhere else, fall back to kiss_fftr even if the user
// sets FL_FFT_USE_ESP_DSP=1.
#if FL_FFT_USE_ESP_DSP && defined(FL_IS_ESP32)
#define FL_FFT_ESP_DSP_ACTIVE 1
#else
#define FL_FFT_ESP_DSP_ACTIVE 0
#endif

namespace fl {
namespace audio {
namespace fft {

/// @brief Forward real-to-complex FFT.
/// @param cfg   Kiss FFT real-to-complex plan describing the size N.
///              Used by the kiss backend directly; the ESP-DSP backend reads
///              only the logical size from this cfg and maintains its own
///              twiddle tables internally.
/// @param in    Pointer to N real time-domain samples.
/// @param out   Pointer to N/2 + 1 complex frequency-domain bins.
///
/// Output format matches kiss_fftr exactly (half-spectrum for real input):
/// out[0] is DC (imag = 0), out[N/2] is Nyquist (imag = 0), out[1..N/2-1]
/// are the positive-frequency complex bins. This lets all downstream code
/// (magnitude, binning, windowing logic) stay identical regardless of
/// which backend produced the spectrum.
inline void fl_fft_real_forward(kiss_fftr_cfg cfg,
                                const kiss_fft_scalar *in,
                                kiss_fft_cpx *out) FL_NOEXCEPT {
#if FL_FFT_ESP_DSP_ACTIVE
    // NOTE: ESP-DSP integration scaffold. The actual implementation would:
    //   1. Init `dsps_fft2r_fc32` tables once per (size N) on first call.
    //   2. Pack N real inputs as N/2 complex pairs: data[2i]=in[2i],
    //      data[2i+1]=in[2i+1].
    //   3. Run dsps_fft2r_fc32(data, N/2); dsps_bit_rev_fc32_ansi(data, N/2).
    //   4. Unpack N/2-point complex FFT result to N/2+1 kiss_fft_cpx bins
    //      via the standard real-FFT post-processing formulas.
    //
    // Until validated on hardware (bit-for-bit parity vs kiss_fftr within
    // float rounding tolerance), fall through to kiss_fftr. This keeps the
    // dispatcher's shape stable — no breaking changes at call sites.
#endif
    kiss_fftr(cfg, in, out);
}

} // namespace fft
} // namespace audio
} // namespace fl
