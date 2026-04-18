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
// Opt-in: ESP-DSP on ESP32 is typically 3-5× faster than kiss_fftr.
// Expected cost on ESP32-S3: ~15 µs vs ~54 µs for the default 512-point
// real FFT. See issue #2308 for the broader audio performance plan.
//
// Default is OFF — enabling requires additional validation (numeric output
// parity vs kiss_fftr, hardware benchmarks, linker verification across ESP32
// variants). Users opt in via:
//
//   -DFL_FFT_USE_ESP_DSP=1
//
// in build_flags. The math is the standard "real FFT from packed N/2-complex
// FFT" identity; see inline comments on the unpack for derivation.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"
// IWYU pragma: begin_keep
#include "third_party/cq_kernel/kiss_fftr.h"
// IWYU pragma: end_keep

#ifndef FL_FFT_USE_ESP_DSP
#define FL_FFT_USE_ESP_DSP 0
#endif

// FL_FFT_ESP_DSP_AVAILABLE: ESP-DSP backend code is compiled in.
// FL_FFT_ESP_DSP_ACTIVE:    dispatcher routes real FFT calls through it.
//
// Both gate on FL_FFT_USE_ESP_DSP to keep the experimental backend fully
// dormant in default builds. The AudioFftParity example sketch sets
// FL_FFT_USE_ESP_DSP=1 locally to exercise the implementation.
//
// NOTE: the ESP-DSP implementation below is NOT yet bit-for-bit validated
// against kiss_fftr — hardware sanity tests via AudioFftParity currently
// report incorrect output (flat magnitudes for DC / single-tone inputs).
// Root cause is under investigation; do NOT enable this in production.
// See issue #2308.
#if FL_FFT_USE_ESP_DSP && defined(FL_IS_ESP32)
#define FL_FFT_ESP_DSP_AVAILABLE 1
#include "esp_dsp.h"
#include "fl/stl/vector.h"
#include "fl/system/log.h"
#include "fl/math/math.h"  // cosf / sinf wrappers
#else
#define FL_FFT_ESP_DSP_AVAILABLE 0
#endif

#define FL_FFT_ESP_DSP_ACTIVE FL_FFT_ESP_DSP_AVAILABLE

namespace fl {
namespace audio {
namespace fft {

#if FL_FFT_ESP_DSP_AVAILABLE

namespace detail {

/// @brief ESP-DSP real-FFT context (lazily initialized per-size).
///
/// Caches the packed work buffer and per-k twiddle tables needed for the
/// real-FFT unpack. Single-context cache — if multiple sizes are needed in a
/// session (AudioContext can cache multiple FFT sizes), this context reallocs
/// on the fly. Cost: O(N) reallocation only on size transitions.
struct EspDspRealCtx {
    int n = 0;                     // full logical FFT size (N), power of 2
    fl::vector<float> work;        // packed complex buffer: size N (N/2 pairs)
    fl::vector<float> cos_table;   // cos(2πk/N) for k in [1, N/2), size N/2-1
    fl::vector<float> sin_table;   // sin(2πk/N) for k in [1, N/2), size N/2-1
};

inline EspDspRealCtx &espDspRealCtx() FL_NOEXCEPT {
    static EspDspRealCtx ctx;
    return ctx;
}

inline bool espDspGlobalInit() FL_NOEXCEPT {
    static bool initialized = false;
    if (initialized) return true;
    esp_err_t err =
        dsps_fft2r_init_fc32(nullptr, CONFIG_DSP_MAX_FFT_SIZE);
    if (err != ESP_OK) {
        FL_WARN("dsps_fft2r_init_fc32 failed: " << static_cast<int>(err));
        return false;
    }
    initialized = true;
    return true;
}

inline bool espDspEnsureTwiddles(int N) FL_NOEXCEPT {
    EspDspRealCtx &ctx = espDspRealCtx();
    if (ctx.n == N) return true;
    if (!espDspGlobalInit()) return false;
    ctx.work.resize(N);
    ctx.cos_table.resize(N / 2 - 1);
    ctx.sin_table.resize(N / 2 - 1);
    const float twoPi = 6.28318530717958647692f;
    const float invN = 1.0f / static_cast<float>(N);
    for (int k = 1; k < N / 2; ++k) {
        float th = twoPi * static_cast<float>(k) * invN;
        ctx.cos_table[k - 1] = ::cosf(th);
        ctx.sin_table[k - 1] = ::sinf(th);
    }
    ctx.n = N;
    return true;
}

/// Forward real FFT via packed N/2-complex trick + unpack.
///
/// Pack: define y[k] = x[2k] + j·x[2k+1] for k in [0, N/2). Run N/2-point
/// complex FFT to get Y[k]. Then reconstruct the N-point real FFT X[k]
/// using conjugate symmetry (Numerical Recipes ch.12 / Oppenheim-Schafer):
///
///   X[0]    = Y[0].r + Y[0].i                         (DC, imag=0)
///   X[N/2]  = Y[0].r − Y[0].i                         (Nyquist, imag=0)
///
///   For k ∈ [1, N/2):
///     a = Y[k].r − Y[N/2−k].r
///     b = Y[k].i + Y[N/2−k].i
///     c = cos(2πk/N), s = sin(2πk/N)
///     X[k].r = ½·((Y[k].r + Y[N/2−k].r) + c·b − s·a)
///     X[k].i = ½·((Y[k].i − Y[N/2−k].i) − c·a − s·b)
inline void espDspRealForward(int N, const float *in,
                              kiss_fft_cpx *out) FL_NOEXCEPT {
    if (!espDspEnsureTwiddles(N)) return;
    EspDspRealCtx &ctx = espDspRealCtx();

    // Pack N real samples into N/2 complex pairs (in-place buffer).
    // data[2k]   = x[2k]      (re)
    // data[2k+1] = x[2k+1]    (im)
    for (int k = 0; k < N / 2; ++k) {
        ctx.work[2 * k] = in[2 * k];
        ctx.work[2 * k + 1] = in[2 * k + 1];
    }

    // N/2-point complex FFT + bit-reverse.
    dsps_fft2r_fc32(ctx.work.data(), N / 2);
    dsps_bit_rev_fc32_ansi(ctx.work.data(), N / 2);

    // Unpack.
    const float Y0r = ctx.work[0];
    const float Y0i = ctx.work[1];
    out[0].r = Y0r + Y0i;
    out[0].i = 0.0f;
    out[N / 2].r = Y0r - Y0i;
    out[N / 2].i = 0.0f;

    for (int k = 1; k < N / 2; ++k) {
        const int kp = N / 2 - k;
        const float Ykr = ctx.work[2 * k];
        const float Yki = ctx.work[2 * k + 1];
        const float Ykpr = ctx.work[2 * kp];
        const float Ykpi = ctx.work[2 * kp + 1];
        const float a = Ykr - Ykpr;
        const float b = Yki + Ykpi;
        const float c = ctx.cos_table[k - 1];
        const float s = ctx.sin_table[k - 1];
        out[k].r = 0.5f * ((Ykr + Ykpr) + c * b - s * a);
        out[k].i = 0.5f * ((Yki - Ykpi) - c * a - s * b);
    }
}

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
                                kiss_fft_cpx *out) FL_NOEXCEPT {
    // NOTE: even when FL_FFT_ESP_DSP_ACTIVE is 1, the dispatcher currently
    // stays on kiss_fftr. The ESP-DSP backend below is `float`-only but
    // kiss_fft_scalar is typedef'd to `int16_t` when FastLED's default
    // FASTLED_FFT_PRECISION=FASTLED_FFT_FIXED16 is in effect. Auto-routing
    // through ESP-DSP would break the int16 call sites in fft_impl.cpp.hpp.
    //
    // Future work: either (a) add an int16 packed-FFT variant using
    // dsps_fft2r_sc16 to match FIXED16 mode, or (b) auto-switch the library
    // to FASTLED_FFT_FLOAT when FL_FFT_USE_ESP_DSP is enabled. Once wired,
    // replace the below with a conditional call to detail::espDspRealForward.
    //
    // The ESP-DSP code is compiled in (when FL_FFT_USE_ESP_DSP=1 + ESP32)
    // and exposed as fl::audio::fft::detail::espDspRealForward() so the
    // AudioFftParity example can exercise it directly for validation.
    (void)N;
    kiss_fftr(cfg, in, out);
}

} // namespace fft
} // namespace audio
} // namespace fl
