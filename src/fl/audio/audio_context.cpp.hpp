#include "fl/audio/audio_context.h"

namespace fl {

fl::size AudioContext::hashFFTArgs(const FFT_Args& args) {
    // Create a hash from FFT_Args for O(1) cache lookup
    // Use simple hash combining of the integer fields
    fl::size hash = 0;
    hash = (hash * 31) ^ static_cast<fl::size>(args.samples);
    hash = (hash * 31) ^ static_cast<fl::size>(args.bands);
    hash = (hash * 31) ^ static_cast<fl::size>(args.sample_rate);

    // For floats, use bit representation via memcpy (safer than reinterpret_cast)
    u32 fmin_bits, fmax_bits;
    fl::memcpy(&fmin_bits, &args.fmin, sizeof(fmin_bits));
    fl::memcpy(&fmax_bits, &args.fmax, sizeof(fmax_bits));
    hash = (hash * 31) ^ static_cast<fl::size>(fmin_bits);
    hash = (hash * 31) ^ static_cast<fl::size>(fmax_bits);
    hash = (hash * 31) ^ static_cast<fl::size>(args.mode);
    return hash;
}

AudioContext::AudioContext(const AudioSample& sample)
    : mSample(sample)
    , mFFTHistoryDepth(0)
    , mFFTHistoryIndex(0)
{
    mFFTCache.reserve(MAX_FFT_CACHE_ENTRIES);
}

AudioContext::~AudioContext() = default;

shared_ptr<const FFTBins> AudioContext::getFFT(int bands, float fmin, float fmax, FFTMode mode) {
    FFT_Args args(mSample.size(), bands, fmin, fmax, mSampleRate, mode);

    // O(1) cache lookup using hash map
    fl::size argsHash = hashFFTArgs(args);
    auto it = mFFTCacheMap.find(argsHash);
    if (it != mFFTCacheMap.end()) {
        int idx = it->second;
        if (idx >= 0 && idx < static_cast<int>(mFFTCache.size())) {
            // Double-check args match in case of hash collision
            if (mFFTCache[idx].args == args) {
                return mFFTCache[idx].bins;
            }
        }
    }

    // Not cached — try to recycle a previously-used FFTBins with matching band count
    shared_ptr<FFTBins> bins;
    for (size i = 0; i < mRecyclePool.size(); i++) {
        if (static_cast<int>(mRecyclePool[i]->bands()) == bands) {
            bins = fl::move(mRecyclePool[i]);
            mRecyclePool.erase(mRecyclePool.begin() + i);
            bins->clear(); // Vectors keep capacity — zero allocs
            break;
        }
    }
    if (!bins) {
        bins = fl::make_shared<FFTBins>(bands);
    }

    fl::span<const fl::i16> sample = mSample.pcm();
    mFFT.run(sample, bins.get(), args);

    // Evict oldest if at capacity
    if (static_cast<int>(mFFTCache.size()) >= MAX_FFT_CACHE_ENTRIES) {
        // Remove the oldest entry's hash from hash map
        fl::size oldHash = hashFFTArgs(mFFTCache[0].args);
        mFFTCacheMap.erase(oldHash);

        // Shift all remaining entries and update map indices
        for (size i = 1; i < mFFTCache.size(); i++) {
            fl::size key = hashFFTArgs(mFFTCache[i].args);
            mFFTCacheMap[key] = static_cast<int>(i - 1);
        }
        mFFTCache.erase(mFFTCache.begin());
    }

    FFTCacheEntry entry;
    entry.args = args;
    entry.bins = bins;
    int newIndex = static_cast<int>(mFFTCache.size());
    mFFTCache.push_back(fl::move(entry));

    // Add to hash map for O(1) future lookups
    mFFTCacheMap[argsHash] = newIndex;

    return bins;
}

BandEnergy AudioContext::getBandEnergy() {
    auto fft = getFFT(3, 20.0f, 11025.0f);
    BandEnergy out;
    span<const float> lin = fft->linear();
    if (lin.size() >= 3) {
        out.bass = lin[0];
        out.mid = lin[1];
        out.treb = lin[2];
    }
    return out;
}

shared_ptr<const FFTBins> AudioContext::getFFT16(FFTMode mode) {
    return getFFT(16, FFT_Args::DefaultMinFrequency(),
                  FFT_Args::DefaultMaxFrequency(), mode);
}

void AudioContext::setFFTHistoryDepth(int depth) {
    if (mFFTHistoryDepth != depth) {
        mFFTHistory.clear();
        mFFTHistory.reserve(depth);
        mFFTHistoryDepth = depth;
        mFFTHistoryIndex = 0;
    }
}

const FFTBins* AudioContext::getHistoricalFFT(int framesBack) const {
    if (framesBack < 0 || framesBack >= static_cast<int>(mFFTHistory.size())) {
        return nullptr;
    }
    // Ring buffer lookup: walk backwards from the most-recently-written slot.
    // Adding history.size() before the modulo avoids negative values from
    // the subtraction, since C++ modulo of negative ints is implementation-defined.
    const int n = static_cast<int>(mFFTHistory.size());
    int index = (mFFTHistoryIndex - 1 - framesBack + n) % n;
    return &mFFTHistory[index];
}

void AudioContext::setSample(const AudioSample& sample) {
    // Save current FFT to history (use first cached entry if available)
    if (!mFFTCache.empty() && mFFTHistoryDepth > 0) {
        const shared_ptr<FFTBins>& first = mFFTCache[0].bins;
        if (first) {
            if (static_cast<int>(mFFTHistory.size()) < mFFTHistoryDepth) {
                mFFTHistory.push_back(*first);
                // When the history fills up, wrap index to 0 for ring buffer mode
                mFFTHistoryIndex = static_cast<int>(mFFTHistory.size()) % mFFTHistoryDepth;
            } else {
                mFFTHistory[mFFTHistoryIndex] = *first;
                mFFTHistoryIndex = (mFFTHistoryIndex + 1) % mFFTHistoryDepth;
            }
        }
    }

    // Recycle bins that only the cache holds (use_count == 1).
    // These can be reused next frame without allocation.
    mRecyclePool.clear();
    for (size i = 0; i < mFFTCache.size(); i++) {
        if (mFFTCache[i].bins.use_count() == 1) {
            mRecyclePool.push_back(fl::move(mFFTCache[i].bins));
        }
    }

    mSample = sample;
    // Clear per-frame FFT cache (new sample = new data)
    mFFTCache.clear();
}

void AudioContext::clearCache() {
    mFFTCache.clear();
    mFFTCacheMap.clear();
    mRecyclePool.clear();
    mFFTHistory.clear();
    mFFTHistoryDepth = 0;
    mFFTHistoryIndex = 0;
}

} // namespace fl
