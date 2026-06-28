#pragma once

#include "fl/audio/audio.h"
#include "fl/audio/fft/fft.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/flat_map.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {

struct BandEnergy {
    float bass = 0.0f;
    float mid = 0.0f;
    float treb = 0.0f;
};

class Context {
public:
    explicit Context(const Sample& sample) FL_NO_EXCEPT;
    ~Context() FL_NO_EXCEPT;

    // ----- Basic Sample Access -----
    const Sample& getSample() const FL_NO_EXCEPT { return mSample; }
    span<const i16> getPCM() const FL_NO_EXCEPT { return mSample.pcm(); }
    float getRMS() const FL_NO_EXCEPT { return mSample.rms(); }
    float getZCF() const FL_NO_EXCEPT { return mSample.zcf(); }
    u32 getTimestamp() const FL_NO_EXCEPT { return mSample.timestamp(); }

    // ----- Lazy fft::FFT Computation (with shared_ptr caching + recycling) -----
    shared_ptr<const fft::Bins> getFFT(
        int bands = 16,
        float fmin = fft::Args::DefaultMinFrequency(),
        float fmax = fft::Args::DefaultMaxFrequency(),
        fft::Mode mode = fft::Mode::AUTO,
        fft::Window window = fft::Window::BLACKMAN_HARRIS
    ) FL_NO_EXCEPT;
    bool hasFFT() const FL_NO_EXCEPT { return !mFFTCache.empty(); }

    // 3-band energy from 3 linear bins (20-11025 Hz).
    // bass: 20-3688 Hz, mid: 3688-7356 Hz, treb: 7356-11025 Hz.
    BandEnergy getBandEnergy() FL_NO_EXCEPT;

    // Standard 16-bin fft::FFT (90-14080 Hz).
    // Detectors that need 16 bins should use this to share a single cached fft::FFT.
    shared_ptr<const fft::Bins> getFFT16(fft::Mode mode = fft::Mode::LOG_REBIN,
                                       fft::Window window = fft::Window::BLACKMAN_HARRIS) FL_NO_EXCEPT;

    // ----- fft::FFT History (for temporal analysis) -----
    void setFFTHistoryDepth(int depth) FL_NO_EXCEPT;
    const vector<fft::Bins>& getFFTHistory() const FL_NO_EXCEPT { return mFFTHistory; }
    bool hasFFTHistory() const FL_NO_EXCEPT { return mFFTHistoryDepth > 0; }
    const fft::Bins* getHistoricalFFT(int framesBack) const FL_NO_EXCEPT;

    // ----- Sample Rate -----
    void setSampleRate(int sampleRate) FL_NO_EXCEPT { mSampleRate = sampleRate; }
    int getSampleRate() const FL_NO_EXCEPT { return mSampleRate; }

    // ----- Silence Flag -----
    // Populated per-frame by the pipeline owner (Processor / Reactive) from
    // NoiseFloorTracker::isAboveFloor(). Detectors read this to gate their
    // outputs during silence. Default false — detectors that check this flag
    // simply won't gate if the pipeline hasn't enabled noise-floor tracking.
    // Reset to false on each setSample(); callers must re-populate after their
    // NoiseFloorTracker.update() call.
    void setSilent(bool silent) FL_NO_EXCEPT { mIsSilent = silent; }
    bool isSilent() const FL_NO_EXCEPT { return mIsSilent; }

    // ----- Update & Reset -----
    void setSample(const Sample& sample) FL_NO_EXCEPT;
    void clearCache() FL_NO_EXCEPT;

private:
    static constexpr int MAX_FFT_CACHE_ENTRIES = 4;

    struct FFTCacheEntry {
        fft::Args args;
        shared_ptr<fft::Bins> bins;
    };

    // Create cache key hash from fft::Args for O(1) lookup
    static fl::size hashFFTArgs(const fft::Args& args) FL_NO_EXCEPT;

    int mSampleRate = 44100;
    Sample mSample;
    fft::FFT mFFT; // fft::FFT engine (has its own kernel cache)
    vector<FFTCacheEntry> mFFTCache; // Strong cache: co-owned with callers
    flat_map<fl::size, int> mFFTCacheMap; // Maps args hash to index in mFFTCache
    vector<shared_ptr<fft::Bins>> mRecyclePool; // Recycled bins for zero-alloc reuse
    vector<fft::Bins> mFFTHistory;
    int mFFTHistoryDepth = 0;
    int mFFTHistoryIndex = 0;
    bool mIsSilent = false;
};

/// Compute the time delta (in seconds) for an audio buffer.
/// Each buffer of pcmSize samples at sampleRate Hz represents exactly
/// pcmSize/sampleRate seconds of audio, regardless of when the CPU reads it.
inline float computeAudioDt(fl::size pcmSize, int sampleRate) FL_NO_EXCEPT {
    if (sampleRate <= 0 || pcmSize == 0) return 0.0f;
    return static_cast<float>(pcmSize) / static_cast<float>(sampleRate);
}

} // namespace audio
} // namespace fl
