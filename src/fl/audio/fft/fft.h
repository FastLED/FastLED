#pragma once

#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/math/math.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {

class Sample;  // Forward declare in fl::audio (correct namespace)

namespace fft {

class Impl;
class FloatVectorPool;

// Time-domain window function applied before FFT.
//   AUTO           — Selects best window based on Mode (see resolveArgs)
//   NONE           — No windowing (rectangular window)
//   HANNING        — Classic cosine window, -31 dB sidelobe rejection
//   BLACKMAN_HARRIS — 4-term window, -92 dB sidelobe rejection
enum class Window {
    AUTO,
    NONE,
    HANNING,
    BLACKMAN_HARRIS,
};

// Controls which algorithm produces the log-spaced (CQ) output bins.
//   AUTO         — LOG_REBIN for <= 32 bins, CQ_NAIVE or CQ_OCTAVE for > 32
//   LOG_REBIN    — Single FFT + geometric bin grouping (~0.15ms on ESP32-S3)
//   CQ_NAIVE     — Single FFT + CQ kernels (no input window)
//   CQ_OCTAVE    — Octave-wise Constant-Q Transform (~1-5ms on ESP32-S3)
//   CQ_HYBRID    — LOG_REBIN for upper freqs + CQ_OCTAVE decimation for bass
enum class Mode {
    AUTO,
    LOG_REBIN,
    CQ_NAIVE,
    CQ_OCTAVE,
    CQ_HYBRID  // If you aren't sure just use this.
};

class Bins {
    friend class Context;

  public:
    Bins(fl::size n) FL_NO_EXCEPT;
    ~Bins() FL_NO_EXCEPT;

    Bins(const Bins &) FL_NO_EXCEPT = default;
    Bins &operator=(const Bins &) = default;
    Bins(Bins &&) FL_NO_EXCEPT = default;
    Bins &operator=(Bins &&) FL_NO_EXCEPT = default;

    void clear() FL_NO_EXCEPT;

    // Configured band count (stable across clear/populate cycles)
    fl::size bands() const FL_NO_EXCEPT;

    // Read-only span accessors
    fl::span<const float> raw() const FL_NO_EXCEPT;

    // dB magnitudes: 20 * log10(raw[i]).
    // Lazily computed from raw() on first access; cached until raw changes.
    fl::span<const float> db() const FL_NO_EXCEPT;

    // Bin-width-normalized raw magnitudes: raw[i] * normFactor[i].
    // Corrects for wider high-frequency bins accumulating more sidelobe energy.
    // Use this for equalization display; use raw() for feature extraction.
    // Returns a span into an internal buffer (no per-call allocation).
    // Lazily recomputed only when raw bins or norm factors change.
    fl::span<const float> rawNormalized() const FL_NO_EXCEPT;

    // Linear-spaced magnitude bins captured directly from raw FFT output.
    // Same count as CQ bins, evenly spaced from linearFmin() to linearFmax().
    fl::span<const float> linear() const FL_NO_EXCEPT;
    float linearFmin() const FL_NO_EXCEPT;
    float linearFmax() const FL_NO_EXCEPT;

    // CQ parameters (set by Impl after populating bins)
    float fmin() const FL_NO_EXCEPT;
    float fmax() const FL_NO_EXCEPT;
    int sampleRate() const FL_NO_EXCEPT;

    // Log-spaced center frequency for CQ bin i
    float binToFreq(int i) const FL_NO_EXCEPT;

    // Find which CQ bin contains a given frequency (inverse of binToFreq)
    int freqToBin(float freq) const FL_NO_EXCEPT;

    // Frequency boundary between adjacent CQ bins i and i+1 (geometric mean)
    float binBoundary(int i) const FL_NO_EXCEPT;

  private:
    static FloatVectorPool& pool() FL_NO_EXCEPT;

    // Mutable accessors for Context (the only writer).
    // raw_mut() invalidates derived caches (db, normalized).
    fl::vector<float>& raw_mut() FL_NO_EXCEPT;
    fl::vector<float>& linear_mut() FL_NO_EXCEPT;

    void setParams(float fmin, float fmax, int sampleRate) FL_NO_EXCEPT;
    void setLinearParams(float linearFmin, float linearFmax) FL_NO_EXCEPT;

    // Copy data into the existing buffer to reuse pooled capacity.
    void setNormFactors(const fl::vector<float>& factors) FL_NO_EXCEPT;

    fl::size mBands;
    fl::vector<float> mBinsRaw;
    fl::vector<float> mBinsLinear;
    fl::vector<float> mNormFactors;
    mutable fl::vector<float> mBinsDb;             // lazily computed from mBinsRaw
    mutable fl::vector<float> mBinsRawNormalized;  // lazily computed from mBinsRaw + mNormFactors
    mutable bool mDbDirty = true;                  // invalidated when raw changes
    mutable bool mNormalizedDirty = true;           // invalidated when raw or normFactors change
    float mFmin = 90.0f;
    float mFmax = 14080.0f;
    int mSampleRate = 44100;
    float mLinearFmin = 0.0f;
    float mLinearFmax = 0.0f;
};

struct Args {
    static int DefaultSamples() FL_NO_EXCEPT { return 512; }
    static int DefaultBands() FL_NO_EXCEPT { return 16; }
    static float DefaultMinFrequency() FL_NO_EXCEPT { return 90.0f; }
    static float DefaultMaxFrequency() FL_NO_EXCEPT { return 14080.0f; }
    static int DefaultSampleRate() FL_NO_EXCEPT { return 44100; }

    int samples = DefaultSamples();
    int bands = DefaultBands();
    float fmin = DefaultMinFrequency();
    float fmax = DefaultMaxFrequency();
    int sample_rate = DefaultSampleRate();
    Mode mode = Mode::AUTO;
    Window window = Window::AUTO;

    Args(int samples = DefaultSamples(), int bands = DefaultBands(),
             float fmin = DefaultMinFrequency(),
             float fmax = DefaultMaxFrequency(),
             int sample_rate = DefaultSampleRate(),
             Mode mode = Mode::AUTO,
             Window window = Window::AUTO)
 FL_NO_EXCEPT : samples(samples), bands(bands), fmin(fmin), fmax(fmax),
          sample_rate(sample_rate), mode(mode), window(window) {}

    // Resolve AUTO values for mode and window in-place.
    // Mode is resolved first; window depends on the resolved mode.
    // Takes bins (band count), samples, fmin, fmax as context.
    static void resolveModeEnums(Mode &mode, Window &window, int bands,
                                 int samples, float fmin, float fmax) FL_NO_EXCEPT;

    bool operator==(const Args &other) const FL_NO_EXCEPT;
    bool operator!=(const Args &other) const FL_NO_EXCEPT { return !(*this == other); }
};

class FFT {
  public:
    FFT() FL_NO_EXCEPT = default;
    ~FFT() FL_NO_EXCEPT = default;

    FFT(FFT &&) FL_NO_EXCEPT = default;
    FFT &operator=(FFT &&) FL_NO_EXCEPT = default;
    FFT(const FFT &) FL_NO_EXCEPT = default;
    FFT &operator=(const FFT &) FL_NO_EXCEPT = default;

    void run(const span<const i16> &sample, Bins *out,
             const Args &args = Args()) FL_NO_EXCEPT;

    void clear() FL_NO_EXCEPT;
    fl::size size() const FL_NO_EXCEPT;

    // FFT kernels are expensive to create, so they are stored in a global
    // LRU cache shared by all AudioContext instances. This sets the max
    // number of cached Impl entries (default 10).
    static void setFFTCacheSize(fl::size size) FL_NO_EXCEPT;

  private:
    struct ImplCache;
    // Global LRU kernel cache — shared across all FFT / AudioContext instances.
    static ImplCache &globalCache() FL_NO_EXCEPT;
};

} // namespace fft

} // namespace audio
}; // namespace fl
