#include "fl/fx/audio/detectors/frequency_bands.h"
#include "fl/audio/audio_context.h"
#include "fl/fft.h"
#include "fl/stl/math.h"

namespace fl {

namespace {
// Number of FFT bins — higher count gives better frequency resolution
// and cleaner band separation. With 64 bins, the CQ kernel provides
// good frequency isolation for all three bands (bass, mid, treble).
const int kNumBands = 64;

// FFT frequency range covering bass through treble.
// Range [100, 10000] covers the bass (20-250), mid (250-4000), and treble
// (4000-20000) ranges with good CQ kernel resolution. The ratio of 100x
// keeps the highest-frequency kernel window large enough for the 512-sample
// FFT (N_window = 512 / (10000/100) = 5 samples minimum).
const float kFFTMinFreq = 100.0f;
const float kFFTMaxFreq = 10000.0f;

// Compute the log-spaced center frequency for a CQ kernel bin.
// The CQ kernel uses: freq[i] = fmin * exp(log(fmax/fmin) * i / (bands-1))
float cqCenterFreq(int i, int bands, float fmin, float fmax) {
    float m = fl::logf(fmax / fmin);
    return fmin * fl::expf(m * static_cast<float>(i) / static_cast<float>(bands - 1));
}

// Compute the frequency boundary between adjacent CQ bins.
// Uses the geometric mean of adjacent center frequencies.
float cqBinBoundary(int i, int bands, float fmin, float fmax) {
    float f_i = cqCenterFreq(i, bands, fmin, fmax);
    float f_next = cqCenterFreq(i + 1, bands, fmin, fmax);
    return fl::sqrtf(f_i * f_next);
}

} // namespace

FrequencyBands::FrequencyBands()
    : mBass(0.0f)
    , mMid(0.0f)
    , mTreble(0.0f)
    , mBassMin(20.0f)
    , mBassMax(250.0f)
    , mMidMin(250.0f)
    , mMidMax(4000.0f)
    , mTrebleMin(4000.0f)
    , mTrebleMax(20000.0f)
    , mSmoothing(0.7f)
    , mFFTBins(kNumBands)
{}

FrequencyBands::~FrequencyBands() = default;

void FrequencyBands::update(shared_ptr<AudioContext> context) {
    // Use sample rate from context if available
    mSampleRate = context->getSampleRate();

    // Compute FFT directly with higher bin count and wider frequency range
    // for better frequency resolution and band coverage.
    const float fftMin = kFFTMinFreq;
    const float fftMax = kFFTMaxFreq;
    span<const i16> pcm = context->getPCM();
    FFT_Args args(pcm.size(), kNumBands, fftMin, fftMax, mSampleRate);
    mFFTBins.clear();
    mFFT.run(pcm, &mFFTBins, args);

    // Calculate energy for each band using fractional bin overlap
    float bassEnergy = calculateBandEnergy(mFFTBins, mBassMin, mBassMax, fftMin, fftMax);
    float midEnergy = calculateBandEnergy(mFFTBins, mMidMin, mMidMax, fftMin, fftMax);
    float trebleEnergy = calculateBandEnergy(mFFTBins, mTrebleMin, mTrebleMax, fftMin, fftMax);

    // Apply smoothing
    mBass = mSmoothing * mBass + (1.0f - mSmoothing) * bassEnergy;
    mMid = mSmoothing * mMid + (1.0f - mSmoothing) * midEnergy;
    mTreble = mSmoothing * mTreble + (1.0f - mSmoothing) * trebleEnergy;
}

void FrequencyBands::fireCallbacks() {
    if (onLevelsUpdate) {
        onLevelsUpdate(mBass, mMid, mTreble);
    }
    if (onBassLevel) {
        onBassLevel(mBass);
    }
    if (onMidLevel) {
        onMidLevel(mMid);
    }
    if (onTrebleLevel) {
        onTrebleLevel(mTreble);
    }
}

void FrequencyBands::reset() {
    mBass = 0.0f;
    mMid = 0.0f;
    mTreble = 0.0f;
}

float FrequencyBands::calculateBandEnergy(const FFTBins& fft, float minFreq, float maxFreq,
                                           float fftMinFreq, float fftMaxFreq) {
    const int numBins = static_cast<int>(fft.bins_raw.size());
    if (numBins <= 1) {
        return 0.0f;
    }

    float totalEnergy = 0.0f;
    float totalWeight = 0.0f;

    for (int i = 0; i < numBins; i++) {
        // Compute the frequency range for this CQ bin using log-spaced
        // boundaries that match the CQ kernel's actual frequency layout.
        float binLow;
        float binHigh;
        if (i == 0) {
            binLow = fftMinFreq;
        } else {
            binLow = cqBinBoundary(i - 1, numBins, fftMinFreq, fftMaxFreq);
        }
        if (i == numBins - 1) {
            binHigh = fftMaxFreq;
        } else {
            binHigh = cqBinBoundary(i, numBins, fftMinFreq, fftMaxFreq);
        }

        // Calculate fractional overlap between this bin and the target band
        float overlapMin = fl::max(binLow, minFreq);
        float overlapMax = fl::min(binHigh, maxFreq);

        if (overlapMax <= overlapMin) {
            continue; // No overlap
        }

        float binWidth = binHigh - binLow;
        float overlapFraction = (overlapMax - overlapMin) / binWidth;

        totalEnergy += fft.bins_raw[i] * overlapFraction;
        totalWeight += overlapFraction;
    }

    // Normalize by total fractional weight to make bands comparable
    return (totalWeight > 0.0f) ? totalEnergy / totalWeight : 0.0f;
}

} // namespace fl
