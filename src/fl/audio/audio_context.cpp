#include "fl/audio/audio_context.h"

namespace fl {

AudioContext::AudioContext(const AudioSample& sample)
    : mSample(sample)
    , mFFT(0)
    , mFFTComputed(false)
    , mFFTHistoryDepth(0)
    , mFFTHistoryIndex(0)
{}

AudioContext::~AudioContext() = default;

const FFTBins& AudioContext::getFFT(int bands, float fmin, float fmax) {
    FFT_Args args(mSample.size(), bands, fmin, fmax, 44100);

    // Check if we need to recompute
    if (!mFFTComputed || mFFTArgs != args) {
        if (mFFT.size() != static_cast<size>(bands)) {
            mFFT = FFTBins(bands);
        }
        mSample.fft(&mFFT);
        mFFTComputed = true;
        mFFTArgs = args;
    }

    return mFFT;
}

const vector<FFTBins>& AudioContext::getFFTHistory(int depth) {
    if (mFFTHistoryDepth != depth) {
        mFFTHistory.clear();
        mFFTHistory.reserve(depth);
        mFFTHistoryDepth = depth;
        mFFTHistoryIndex = 0;
    }
    return mFFTHistory;
}

const FFTBins* AudioContext::getHistoricalFFT(int framesBack) const {
    if (framesBack < 0 || framesBack >= static_cast<int>(mFFTHistory.size())) {
        return nullptr;
    }
    int index = (mFFTHistoryIndex - 1 - framesBack + static_cast<int>(mFFTHistory.size())) % static_cast<int>(mFFTHistory.size());
    return &mFFTHistory[index];
}

void AudioContext::setSample(const AudioSample& sample) {
    // Save current FFT to history
    if (mFFTComputed && mFFTHistoryDepth > 0) {
        if (static_cast<int>(mFFTHistory.size()) < mFFTHistoryDepth) {
            mFFTHistory.push_back(mFFT);
            mFFTHistoryIndex = static_cast<int>(mFFTHistory.size());
        } else {
            mFFTHistory[mFFTHistoryIndex] = mFFT;
            mFFTHistoryIndex = (mFFTHistoryIndex + 1) % mFFTHistoryDepth;
        }
    }

    mSample = sample;
    mFFTComputed = false;
}

void AudioContext::clearCache() {
    mFFTComputed = false;
    mFFTHistory.clear();
    mFFTHistoryDepth = 0;
    mFFTHistoryIndex = 0;
}

} // namespace fl
