// IWYU pragma: private

#include "fl/audio/fft/fft_backend.h"

#if FL_FFT_ESP_DSP_AVAILABLE

#include <dsps_fft2r.h> // IWYU pragma: keep
#include "fl/stl/vector.h"
#include "fl/log/log.h"
#include "fl/math/math.h"

namespace fl {
namespace audio {
namespace fft {
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

inline EspDspRealCtx &espDspRealCtx() FL_NO_EXCEPT {
    static EspDspRealCtx ctx;
    return ctx;
}

inline bool espDspGlobalInit() FL_NO_EXCEPT {
    static bool initialized = false;
    if (initialized) return true;
    esp_err_t err =
        dsps_fft2r_init_fc32(nullptr, CONFIG_DSP_MAX_FFT_SIZE);
    if (err != ESP_OK) {
        FL_WARN("dsps_fft2r_init_fc32 failed: " << (static_cast<fl::i32>(err)));
        return false;
    }
    initialized = true;
    return true;
}

inline bool espDspEnsureTwiddles(int N) FL_NO_EXCEPT {
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
void espDspRealForward(int N, const float *in,
                              kiss_fft_cpx *out) FL_NO_EXCEPT {
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
} // namespace fft
} // namespace audio
} // namespace fl

#endif // FL_FFT_ESP_DSP_AVAILABLE
