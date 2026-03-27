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
    Bins(fl::size n);
    ~Bins() FL_NOEXCEPT;

    Bins(const Bins &) FL_NOEXCEPT = default;
    Bins &operator=(const Bins &) = default;
    Bins(Bins &&) FL_NOEXCEPT = default;
    Bins &operator=(Bins &&) FL_NOEXCEPT = default;

    void clear();

    // Configured band count (stable across clear/populate cycles)
    fl::size bands() const;

    // Read-only span accessors
    fl::span<const float> raw() const;

    // dB magnitudes: 20 * log10(raw[i]).
    // Lazily computed from raw() on first access; cached until raw changes.
    fl::span<const float> db() const;

    // Bin-width-normalized raw magnitudes: raw[i] * normFactor[i].
    // Corrects for wider high-frequency bins accumulating more sidelobe energy.
    // Use this for equalization display; use raw() for feature extraction.
    // Returns a span into an internal buffer (no per-call allocation).
    // Lazily recomputed only when raw bins or norm factors change.
    fl::span<const float> rawNormalized() const;

    // Linear-spaced magnitude bins captured directly from raw FFT output.
    // Same count as CQ bins, evenly spaced from linearFmin() to linearFmax().
    fl::span<const float> linear() const;
    float linearFmin() const;
    float linearFmax() const;

    // CQ parameters (set by Impl after populating bins)
    float fmin() const;
    float fmax() const;
    int sampleRate() const;

    // Log-spaced center frequency for CQ bin i
    float binToFreq(int i) const;

    // Find which CQ bin contains a given frequency (inverse of binToFreq)
    int freqToBin(float freq) const;

    // Frequency boundary between adjacent CQ bins i and i+1 (geometric mean)
    float binBoundary(int i) const;

  private:
    static FloatVectorPool& pool();

    // Mutable accessors for Context (the only writer).
    // raw_mut() invalidates derived caches (db, normalized).
    fl::vector<float>& raw_mut();
    fl::vector<float>& linear_mut();

    void setParams(float fmin, float fmax, int sampleRate);
    void setLinearParams(float linearFmin, float linearFmax);

    // Copy data into the existing buffer to reuse pooled capacity.
    void setNormFactors(const fl::vector<float>& factors);

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
    static int DefaultSamples() { return 512; }
    static int DefaultBands() { return 16; }
    static float DefaultMinFrequency() { return 90.0f; }
    static float DefaultMaxFrequency() { return 14080.0f; }
    static int DefaultSampleRate() { return 44100; }

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
        : samples(samples), bands(bands), fmin(fmin), fmax(fmax),
          sample_rate(sample_rate), mode(mode), window(window) {}

    // Resolve AUTO values for mode and window in-place.
    // Mode is resolved first; window depends on the resolved mode.
    // Takes bins (band count), samples, fmin, fmax as context.
    static void resolveModeEnums(Mode &mode, Window &window, int bands,
                                 int samples, float fmin, float fmax);

    bool operator==(const Args &other) const ;
    bool operator!=(const Args &other) const { return !(*this == other); }
};

class FFT {
  public:
    FFT() FL_NOEXCEPT = default;
    ~FFT() FL_NOEXCEPT = default;

    FFT(FFT &&) FL_NOEXCEPT = default;
    FFT &operator=(FFT &&) FL_NOEXCEPT = default;
    FFT(const FFT &) FL_NOEXCEPT = default;
    FFT &operator=(const FFT &) FL_NOEXCEPT = default;

    void run(const span<const i16> &sample, Bins *out,
             const Args &args = Args());

    void clear();
    fl::size size() const;

    // FFT kernels are expensive to create, so they are stored in a global
    // LRU cache shared by all AudioContext instances. This sets the max
    // number of cached Impl entries (default 10).
    static void setFFTCacheSize(fl::size size);

  private:
    struct ImplCache;
    // Global LRU kernel cache — shared across all FFT / AudioContext instances.
    static ImplCache &globalCache();
};

} // namespace fft

} // namespace audio
}; // namespace fl
