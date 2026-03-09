#pragma once

#include "fl/audio/audio.h"
#include "fl/audio/fft/fft.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/unordered_map.h"

namespace fl {

struct BandEnergy {
    float bass = 0.0f;
    float mid = 0.0f;
    float treb = 0.0f;
};

class AudioContext {
public:
    explicit AudioContext(const AudioSample& sample);
    ~AudioContext();

    // ----- Basic Sample Access -----
    const AudioSample& getSample() const { return mSample; }
    span<const i16> getPCM() const { return mSample.pcm(); }
    float getRMS() const { return mSample.rms(); }
    float getZCF() const { return mSample.zcf(); }
    u32 getTimestamp() const { return mSample.timestamp(); }

    // ----- Lazy FFT Computation (with shared_ptr caching + recycling) -----
    shared_ptr<const FFTBins> getFFT(
        int bands = 16,
        float fmin = FFT_Args::DefaultMinFrequency(),
        float fmax = FFT_Args::DefaultMaxFrequency(),
        FFTMode mode = FFTMode::AUTO
    );
    bool hasFFT() const { return !mFFTCache.empty(); }

    // 3-band energy from 3 linear bins (20-11025 Hz).
    // bass: 20-3688 Hz, mid: 3688-7356 Hz, treb: 7356-11025 Hz.
    BandEnergy getBandEnergy();

    // Standard 16-bin FFT (174.6-4698.3 Hz).
    // Detectors that need 16 bins should use this to share a single cached FFT.
    shared_ptr<const FFTBins> getFFT16(FFTMode mode = FFTMode::LOG_REBIN);

    // ----- FFT History (for temporal analysis) -----
    void setFFTHistoryDepth(int depth);
    const vector<FFTBins>& getFFTHistory() const { return mFFTHistory; }
    bool hasFFTHistory() const { return mFFTHistoryDepth > 0; }
    const FFTBins* getHistoricalFFT(int framesBack) const;

    // ----- Sample Rate -----
    void setSampleRate(int sampleRate) { mSampleRate = sampleRate; }
    int getSampleRate() const { return mSampleRate; }

    // ----- Update & Reset -----
    void setSample(const AudioSample& sample);
    void clearCache();

private:
    static constexpr int MAX_FFT_CACHE_ENTRIES = 4;

    struct FFTCacheEntry {
        FFT_Args args;
        shared_ptr<FFTBins> bins;
    };

    // Create cache key hash from FFT_Args for O(1) lookup
    static fl::size hashFFTArgs(const FFT_Args& args);

    int mSampleRate = 44100;
    AudioSample mSample;
    FFT mFFT; // FFT engine (has its own kernel cache)
    vector<FFTCacheEntry> mFFTCache; // Strong cache: co-owned with callers
    unordered_map<fl::size, int> mFFTCacheMap; // Maps args hash to index in mFFTCache
    vector<shared_ptr<FFTBins>> mRecyclePool; // Recycled bins for zero-alloc reuse
    vector<FFTBins> mFFTHistory;
    int mFFTHistoryDepth = 0;
    int mFFTHistoryIndex = 0;
};

/// Compute the time delta (in seconds) for an audio buffer.
/// Each buffer of pcmSize samples at sampleRate Hz represents exactly
/// pcmSize/sampleRate seconds of audio, regardless of when the CPU reads it.
inline float computeAudioDt(fl::size pcmSize, int sampleRate) {
    if (sampleRate <= 0 || pcmSize == 0) return 0.0f;
    return static_cast<float>(pcmSize) / static_cast<float>(sampleRate);
}

} // namespace fl
