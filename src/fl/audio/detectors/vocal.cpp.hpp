// VocalDetector - Human voice detection implementation
// Part of FastLED Audio System v2.0 - Phase 3 (Differentiators)

#include "fl/audio/detectors/vocal.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/fft/fft.h"
#include "fl/stl/math.h"
#include "fl/stl/algorithm.h"

namespace fl {

VocalDetector::VocalDetector()
    : mVocalActive(false)
    , mPreviousVocalActive(false)
    , mConfidence(0.0f)
    , mSpectralCentroid(0.0f)
    , mSpectralRolloff(0.0f)
    , mFormantRatio(0.0f)
{}

VocalDetector::~VocalDetector() = default;

void VocalDetector::update(shared_ptr<AudioContext> context) {
    mSampleRate = context->getSampleRate();
    mRetainedFFT = context->getFFT(128);
    const FFTBins& fft = *mRetainedFFT;
    mNumBins = static_cast<int>(fft.raw().size());

    // Calculate spectral features
    mSpectralCentroid = calculateSpectralCentroid(fft);
    mSpectralRolloff = calculateSpectralRolloff(fft);
    mFormantRatio = estimateFormantRatio(fft);
    mSpectralFlatness = calculateSpectralFlatness(fft);
    mHarmonicDensity = calculateHarmonicDensity(fft);
    mVocalPresenceRatio = calculateVocalPresenceRatio(fft);
    mSpectralFlux = calculateSpectralFlux(fft);
    mSpectralVariance = calculateSpectralVariance(fft);

    // Calculate time-domain features from raw PCM
    span<const i16> pcm = context->getPCM();
    const float dt = computeAudioDt(pcm.size(), context->getSampleRate());
    mEnvelopeJitter = mEnvelopeJitterSmoother.update(calculateEnvelopeJitter(pcm), dt);
    mAutocorrelationIrregularity = mAcfIrregularitySmoother.update(
        calculateAutocorrelationIrregularity(pcm), dt);
    mZeroCrossingCV = mZcCVSmoother.update(calculateZeroCrossingCV(pcm), dt);

    // Calculate raw confidence and apply time-aware smoothing
    float rawConfidence = calculateRawConfidence(
        mSpectralCentroid, mSpectralRolloff, mFormantRatio,
        mSpectralFlatness, mHarmonicDensity, mVocalPresenceRatio,
        mSpectralFlux, mSpectralVariance);
    float smoothedConfidence = mConfidenceSmoother.update(rawConfidence, dt);

    // Hysteresis: use separate on/off thresholds to prevent chattering
    bool wantActive;
    if (mVocalActive) {
        wantActive = (smoothedConfidence >= mOffThreshold);
    } else {
        wantActive = (smoothedConfidence >= mOnThreshold);
    }

    // Debounce: require state to persist for MIN_FRAMES_TO_TRANSITION frames
    if (wantActive != mVocalActive) {
        mFramesInState++;
        if (mFramesInState >= MIN_FRAMES_TO_TRANSITION) {
            mVocalActive = wantActive;
            mFramesInState = 0;
        }
    } else {
        mFramesInState = 0;
    }

    // Track state changes for fireCallbacks
    mStateChanged = (mVocalActive != mPreviousVocalActive);
}

void VocalDetector::fireCallbacks() {
    if (mStateChanged) {
        if (onVocal) onVocal(static_cast<u8>(mConfidenceSmoother.value() * 255.0f));
        if (mVocalActive && onVocalStart) onVocalStart();
        if (!mVocalActive && onVocalEnd) onVocalEnd();
        mPreviousVocalActive = mVocalActive;
        mStateChanged = false;
    }
}

void VocalDetector::reset() {
    mVocalActive = false;
    mPreviousVocalActive = false;
    mConfidence = 0.0f;
    mConfidenceSmoother.reset();
    mSpectralCentroid = 0.0f;
    mSpectralRolloff = 0.0f;
    mFormantRatio = 0.0f;
    mSpectralFlatness = 0.0f;
    mHarmonicDensity = 0.0f;
    mVocalPresenceRatio = 0.0f;
    mSpectralFlux = 0.0f;
    mSpectralVariance = 0.0f;
    mEnvelopeJitter = 0.0f;
    mAutocorrelationIrregularity = 0.0f;
    mZeroCrossingCV = 0.0f;
    mEnvelopeJitterSmoother.reset();
    mAcfIrregularitySmoother.reset();
    mZcCVSmoother.reset();
    mPrevBins.clear();
    mSpectralVarianceFilter.reset();
    mFramesInState = 0;
}

float VocalDetector::calculateSpectralCentroid(const FFTBins& fft) {
    float weightedSum = 0.0f;
    float magnitudeSum = 0.0f;

    for (fl::size i = 0; i < fft.raw().size(); i++) {
        float magnitude = fft.raw()[i];
        weightedSum += i * magnitude;
        magnitudeSum += magnitude;
    }

    return (magnitudeSum < 1e-6f) ? 0.0f : weightedSum / magnitudeSum;
}

float VocalDetector::calculateSpectralRolloff(const FFTBins& fft) {
    const float rolloffThreshold = 0.85f;
    float totalEnergy = 0.0f;

    // Calculate total energy
    for (fl::size i = 0; i < fft.raw().size(); i++) {
        float magnitude = fft.raw()[i];
        totalEnergy += magnitude * magnitude;
    }

    float energyThreshold = totalEnergy * rolloffThreshold;
    float cumulativeEnergy = 0.0f;

    // Find rolloff point
    for (fl::size i = 0; i < fft.raw().size(); i++) {
        float magnitude = fft.raw()[i];
        cumulativeEnergy += magnitude * magnitude;
        if (cumulativeEnergy >= energyThreshold) {
            return static_cast<float>(i) / fft.raw().size();
        }
    }

    return 1.0f;
}

float VocalDetector::estimateFormantRatio(const FFTBins& fft) {
    if (fft.raw().size() < 8) return 0.0f;

    // Use CQ log-spaced bin mapping via FFTBins methods
    const int numBins = static_cast<int>(fft.raw().size());

    // F1 range: 250-900 Hz (first vocal formant — widened for /i/ vowel)
    const int f1MinBin = fl::max(0, fft.freqToBin(250.0f));
    const int f1MaxBin = fl::min(numBins - 1, fft.freqToBin(900.0f));

    // F2 range: 1000-3000 Hz (second vocal formant — widened for /i/ vowel)
    const int f2MinBin = fl::max(0, fft.freqToBin(1000.0f));
    const int f2MaxBin = fl::min(numBins - 1, fft.freqToBin(3000.0f));

    // Find peak energy in F1 range
    float f1Energy = 0.0f;
    for (int i = f1MinBin; i <= f1MaxBin && i < numBins; i++) {
        f1Energy = fl::max(f1Energy, fft.raw()[i]);
    }

    // Find peak energy in F2 range
    float f2Energy = 0.0f;
    for (int i = f2MinBin; i <= f2MaxBin && i < numBins; i++) {
        f2Energy = fl::max(f2Energy, fft.raw()[i]);
    }

    // Verify F1 peak is a real formant (not flat noise)
    float f1Avg = 0.0f;
    int f1Count = 0;
    for (int i = f1MinBin; i <= f1MaxBin && i < numBins; ++i) {
        f1Avg += fft.raw()[i];
        ++f1Count;
    }
    f1Avg = (f1Count > 0) ? f1Avg / static_cast<float>(f1Count) : 0.0f;

    float f2Avg = 0.0f;
    int f2Count = 0;
    for (int i = f2MinBin; i <= f2MaxBin && i < numBins; ++i) {
        f2Avg += fft.raw()[i];
        ++f2Count;
    }
    f2Avg = (f2Count > 0) ? f2Avg / static_cast<float>(f2Count) : 0.0f;

    // Peaks must be 1.5x above average — otherwise it's flat noise, not a formant
    if (f1Energy < f1Avg * 1.5f || f2Energy < f2Avg * 1.5f) {
        return 0.0f;
    }

    return (f1Energy < 1e-6f) ? 0.0f : f2Energy / f1Energy;
}

float VocalDetector::calculateSpectralFlatness(const FFTBins& fft) {
    // Spectral flatness via log-domain trick:
    //   geometric_mean = exp(mean(ln(x_i)))
    //   flatness = geometric_mean / arithmetic_mean
    //
    // Since bins_db[i] = 20*log10(bins_raw[i]), we can convert:
    //   ln(bins_raw[i]) = bins_db[i] * ln(10)/20 = bins_db[i] * 0.1151293f
    //
    // This avoids per-bin logf() calls — only one expf() at the end.

    float sumLn = 0.0f;
    float sumRaw = 0.0f;
    int count = 0;

    for (fl::size i = 0; i < fft.raw().size(); ++i) {
        if (fft.raw()[i] <= 1e-6f) continue;
        sumLn += fft.db()[i] * 0.1151293f;  // dB to natural log
        sumRaw += fft.raw()[i];
        ++count;
    }

    if (count < 2) return 0.0f;

    float geometricMean = fl::expf(sumLn / static_cast<float>(count));
    float arithmeticMean = sumRaw / static_cast<float>(count);

    if (arithmeticMean < 1e-6f) return 0.0f;
    return geometricMean / arithmeticMean;
}

float VocalDetector::calculateHarmonicDensity(const FFTBins& fft) {
    float peak = 0.0f;
    for (fl::size i = 0; i < fft.raw().size(); ++i) {
        peak = fl::max(peak, fft.raw()[i]);
    }

    if (peak < 1e-6f) return 0.0f;

    float threshold = peak * 0.1f;
    int count = 0;
    for (fl::size i = 0; i < fft.raw().size(); ++i) {
        if (fft.raw()[i] >= threshold) ++count;
    }

    return static_cast<float>(count);
}

float VocalDetector::calculateSpectralFlux(const FFTBins& fft) {
    // Spectral flux: normalized L2 distance between consecutive frames.
    // Voice has more frame-to-frame spectral variation (phoneme transitions,
    // formant shifts) than sustained guitar notes.
    // Uses normalized spectra (sum-to-1) so flux is amplitude-independent.
    const auto& bins = fft.raw();
    const int n = static_cast<int>(bins.size());
    if (n == 0) return 0.0f;

    // Normalize current frame
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) sum += bins[i];
    if (sum < 1e-6f) {
        mPrevBins.clear();
        return 0.0f;
    }

    fl::vector<float> normBins;
    normBins.resize(n);
    float invSum = 1.0f / sum;
    for (int i = 0; i < n; ++i) normBins[i] = bins[i] * invSum;

    if (mPrevBins.size() != static_cast<fl::size>(n)) {
        mPrevBins = normBins;
        return 0.0f;
    }

    // L2 distance between normalized spectra
    float flux = 0.0f;
    for (int i = 0; i < n; ++i) {
        float diff = normBins[i] - mPrevBins[i];
        flux += diff * diff;
    }
    flux = fl::sqrtf(flux);

    mPrevBins = normBins;
    return flux;
}

float VocalDetector::calculateVocalPresenceRatio(const FFTBins& fft) {
    // Vocal presence ratio using LINEAR bins for better high-frequency resolution.
    // CQ bins compress the presence band (2-4 kHz) into few bins; linear bins
    // give uniform frequency resolution across the spectrum.
    //
    // Ratio = mean energy in 2-4 kHz (F3 formant / sibilance) /
    //         mean energy in 80-400 Hz (guitar fundamentals / bass)
    // Voice adds energy in the 2-4 kHz range; guitar energy decays above 2 kHz.
    auto linearBins = fft.linear();
    if (linearBins.size() < 4) return 0.0f;

    const int numBins = static_cast<int>(linearBins.size());
    const float fmin = fft.linearFmin();
    const float fmax = fft.linearFmax();
    const float binWidth = (fmax - fmin) / static_cast<float>(numBins);

    // Map frequency to linear bin index
    auto freqToLinBin = [&](float freq) -> int {
        if (freq <= fmin) return 0;
        if (freq >= fmax) return numBins - 1;
        return fl::min(numBins - 1, static_cast<int>((freq - fmin) / binWidth));
    };

    const int presMinBin = freqToLinBin(2000.0f);
    const int presMaxBin = freqToLinBin(4000.0f);
    const int bassMinBin = freqToLinBin(200.0f);
    const int bassMaxBin = freqToLinBin(500.0f);

    float presEnergy = 0.0f;
    int presCount = 0;
    for (int i = presMinBin; i <= presMaxBin && i < numBins; ++i) {
        presEnergy += linearBins[i] * linearBins[i];
        ++presCount;
    }
    if (presCount > 0) presEnergy /= static_cast<float>(presCount);

    float bassEnergy = 0.0f;
    int bassCount = 0;
    for (int i = bassMinBin; i <= bassMaxBin && i < numBins; ++i) {
        bassEnergy += linearBins[i] * linearBins[i];
        ++bassCount;
    }
    if (bassCount > 0) bassEnergy /= static_cast<float>(bassCount);

    if (bassEnergy < 1e-12f) return 0.0f;
    return presEnergy / bassEnergy;
}

float VocalDetector::calculateSpectralVariance(const FFTBins& fft) {
    // Delegates to SpectralVariance filter — per-bin EMA with mean relative
    // deviation measurement. Voice has higher variance (~1.01) than guitar
    // (~0.74) due to vocal cord modulation and formant transitions.
    return mSpectralVarianceFilter.update(fft.raw());
}

float VocalDetector::calculateEnvelopeJitter(span<const i16> pcm) {
    const int n = static_cast<int>(pcm.size());
    if (n < 44) return 0.0f;

    const float normFactor = 1.0f / 32768.0f;
    const int halfWin = fl::max(2, mSampleRate / 4000); // ~11 samples at 44100
    const int winSize = 2 * halfWin + 1;
    const float invWinSize = 1.0f / static_cast<float>(winSize);

    // Single pass: envelope deviation (sliding window) + half-cycle shimmer
    // Seed the sliding window sum
    float windowSum = 0.0f;
    for (int j = 0; j < winSize && j < n; ++j) {
        windowSum += fl::abs(static_cast<float>(pcm[j])) * normFactor;
    }

    float sumEnv = 0.0f;
    float sumDev = 0.0f;
    int count = 0;
    float sumPeaks = 0.0f;
    float sumSqPeaks = 0.0f;
    int numCycles = 0;
    float currentPeak = 0.0f;
    bool wasPositive = pcm[halfWin] >= 0;

    for (int i = halfWin; i < n - halfWin; ++i) {
        float absVal = fl::abs(static_cast<float>(pcm[i])) * normFactor;
        float smoothed = windowSum * invWinSize;

        sumEnv += smoothed;
        sumDev += fl::abs(absVal - smoothed);
        ++count;

        // Shimmer: track peaks between zero crossings
        currentPeak = fl::max(currentPeak, absVal);
        bool isPositive = pcm[i] >= 0;
        if (isPositive != wasPositive) {
            if (currentPeak > 0.01f) {
                sumPeaks += currentPeak;
                sumSqPeaks += currentPeak * currentPeak;
                ++numCycles;
            }
            currentPeak = 0.0f;
        }
        wasPositive = isPositive;

        // Slide window
        if (i + 1 < n - halfWin) {
            windowSum -= fl::abs(static_cast<float>(pcm[i - halfWin])) * normFactor;
            windowSum += fl::abs(static_cast<float>(pcm[i + halfWin + 1])) * normFactor;
        }
    }

    if (sumEnv < 1e-6f || count == 0) return 0.0f;
    float envelopeJitter = (sumDev / static_cast<float>(count))
                         / (sumEnv / static_cast<float>(count));

    float shimmer = 0.0f;
    if (numCycles >= 3) {
        float meanPeak = sumPeaks / static_cast<float>(numCycles);
        if (meanPeak > 0.01f) {
            float variance = sumSqPeaks / static_cast<float>(numCycles)
                           - meanPeak * meanPeak;
            if (variance < 0.0f) variance = 0.0f;
            shimmer = fl::sqrtf(variance) / meanPeak;
        }
    }

    return envelopeJitter + shimmer * 0.5f;
}

float VocalDetector::calculateAutocorrelationIrregularity(span<const i16> pcm) {
    const int n = static_cast<int>(pcm.size());

    // Vocal fundamental lag range at mSampleRate
    const int minLag = fl::max(2, mSampleRate / 500);   // 500 Hz
    const int maxLag = fl::min(n / 2, mSampleRate / 172); // 172 Hz

    if (minLag >= maxLag || maxLag >= n) return 0.0f;

    // Subsample by 4: safe since step(4) << minLag(88).
    // normFactor² cancels in the ratio sum/acf0, so work in raw i16 space.
    const int step = 4;

    // ACF[0] = energy (subsampled)
    float acf0 = 0.0f;
    for (int i = 0; i < n; i += step) {
        float s = static_cast<float>(pcm[i]);
        acf0 += s * s;
    }
    if (acf0 < 1.0f) return 0.0f;

    // Probe 16 evenly-spaced lags, each with subsampled inner loop.
    // Total: ~16 × (n/4)/lag ≈ 1700 MACs (vs 34K without subsampling).
    const int lagRange = maxLag - minLag;
    const int maxProbes = 16;
    const int lagStep = fl::max(1, lagRange / maxProbes);
    float bestPeak = 0.0f;
    for (int lag = minLag; lag <= maxLag; lag += lagStep) {
        float sum = 0.0f;
        for (int i = 0; i + lag < n; i += step) {
            sum += static_cast<float>(pcm[i]) * static_cast<float>(pcm[i + lag]);
        }
        float normalized = sum / acf0;
        bestPeak = fl::max(bestPeak, normalized);
    }

    return 1.0f - bestPeak; // 0 = perfectly periodic, 1 = no periodicity
}

float VocalDetector::calculateZeroCrossingCV(span<const i16> pcm) {
    const int n = static_cast<int>(pcm.size());
    if (n < 10) return 0.0f;

    // Single-pass: compute zero-crossing interval statistics
    int prevCrossing = -1;
    int numIntervals = 0;
    float sumIntervals = 0.0f;
    float sumSqIntervals = 0.0f;

    for (int i = 1; i < n; ++i) {
        if ((pcm[i - 1] >= 0 && pcm[i] < 0) ||
            (pcm[i - 1] < 0 && pcm[i] >= 0)) {
            if (prevCrossing >= 0) {
                float interval = static_cast<float>(i - prevCrossing);
                sumIntervals += interval;
                sumSqIntervals += interval * interval;
                ++numIntervals;
            }
            prevCrossing = i;
        }
    }

    if (numIntervals < 2) return 0.0f;

    float mean = sumIntervals / static_cast<float>(numIntervals);
    if (mean < 1e-6f) return 0.0f;

    float variance = sumSqIntervals / static_cast<float>(numIntervals)
                   - mean * mean;
    if (variance < 0.0f) variance = 0.0f;
    float stdev = fl::sqrtf(variance);

    return stdev / mean; // coefficient of variation
}

float VocalDetector::calculateRawConfidence(float centroid, float rolloff, float formantRatio,
                                             float spectralFlatness, float harmonicDensity,
                                             float vocalPresenceRatio, float spectralFlux,
                                             float spectralVariance) {
    // Normalize centroid to 0-1 range using actual bin count
    float normalizedCentroid = centroid / static_cast<float>(mNumBins);

    // Continuous confidence scores for each feature (no hard binary cutoffs)
    // Tuned for both synthetic vowels (~0.5 centroid, ~0.65 rolloff) and
    // real mixed audio (~0.21 centroid, ~0.23 rolloff in CQ space).

    // Centroid score: broad peak centered at 0.35, accepts 0.1-0.6
    float centroidScore = fl::max(0.0f, 1.0f - fl::abs(normalizedCentroid - 0.35f) * 2.0f);

    // Rolloff score: broad peak at 0.40, accepts 0.1-0.8
    // Real audio ~0.23, synthetic ~0.65 — need to cover both
    float rolloffScore = fl::max(0.0f, 1.0f - fl::abs(rolloff - 0.40f) / 0.45f);

    // Formant score: voice has F2/F1 ratio; real audio ratio ~0.14, synthetic ~0.7
    // Ramp zone 0-0.12 catches very low ratios (pure tone, sparse sines).
    // /i/ vowel (form~0.15) and guitar (form~0.17) overlap in formant ratio,
    // so guitar rejection relies on time-domain penalties (jitter, zcCV) instead.
    float formantScore;
    if (formantRatio < 0.12f) {
        formantScore = formantRatio / 0.12f * 0.30f;  // Ramp: very low → 0..0.30
    } else {
        // Peak at 0.5, floor 0.30, width 0.70 (accommodates /i/ vowel)
        float dist = fl::abs(formantRatio - 0.5f);
        formantScore = fl::max(0.30f, 1.0f - dist / 0.70f);
    }

    // Spectral flatness score: wider window to survive mix contamination.
    // Isolated vocal ~0.44, vocal-in-mix ~0.60, backing ~0.57.
    // Peak at 0.43, width ±0.22 — mix vocal (0.60) still scores ~0.22.
    float flatnessScore;
    if (spectralFlatness < 0.20f) {
        // Too tonal (pure tones, sparse sines)
        flatnessScore = spectralFlatness / 0.20f * 0.3f;
    } else if (spectralFlatness <= 0.65f) {
        // Voice range — peak at 0.43, wide window
        float dist = fl::abs(spectralFlatness - 0.43f);
        flatnessScore = fl::max(0.0f, 1.0f - dist / 0.22f);
    } else {
        // Too flat — noise-like (steep decay)
        flatnessScore = fl::max(0.0f, 1.0f - (spectralFlatness - 0.65f) / 0.10f);
    }

    // Harmonic density score: with 128 CQ bins
    // Real audio: ~48-52, synthetic voice: 60-100, pure tone: ~30, noise: ~120
    float densityScore;
    if (harmonicDensity < 20.0f) {
        // Too sparse — pure tone or near-tonal
        densityScore = harmonicDensity / 20.0f * 0.3f;
    } else if (harmonicDensity <= 100.0f) {
        // Voice-like range (includes real audio at ~50)
        densityScore = 0.3f + (harmonicDensity - 20.0f) / 80.0f * 0.7f;
    } else {
        // Too dense — noise-like
        densityScore = fl::max(0.0f, 1.0f - (harmonicDensity - 100.0f) / 28.0f);
    }

    // Spectral flux score: voice has higher frame-to-frame spectral change
    // than sustained instruments (phoneme transitions, formant shifts).
    // spectralFlux is already computed but was previously unused in scoring.
    float spectralFluxScore;
    if (spectralFlux < 0.02f) {
        spectralFluxScore = 0.0f;
    } else if (spectralFlux < 0.30f) {
        spectralFluxScore = (spectralFlux - 0.02f) / 0.28f;
    } else {
        spectralFluxScore = 1.0f;
    }

    // Weighted average — presence removed (actively hurts in 3-way mixes:
    // drums inflate 2-4kHz, making backing score HIGHER than voice).
    // Centroid/rolloff minimized (0-5% separation between voice and guitar).
    // Freed budget redistributed to formant/flatness (best discriminators)
    // and spectral flux (new: voice has higher frame-to-frame change).
    float weightedAvg = 0.02f * centroidScore
                      + 0.02f * rolloffScore
                      + 0.31f * formantScore
                      + 0.33f * flatnessScore
                      + 0.12f * densityScore
                      + 0.10f * spectralFluxScore;

    // Accumulate all multiplicative boosts into totalBoost, then cap.
    // This prevents the theoretical 3.2x runaway when all boosts fire
    // simultaneously. Cap at 1.60x allows two boosts to stack but
    // prevents pathological cases. Synthetic signals trigger zero boosts
    // (jitter <0.15, acfIrreg <0.30), so existing tests are unaffected.
    float totalBoost = 1.0f;

    // Temporal spectral variance boost (not base score).
    // Voice has higher variance (~1.01/~0.84 in 3-way) than guitar (~0.74).
    // Applied only when variance > 0 (real multi-frame audio).
    if (spectralVariance > 0.01f) {
        float varianceBoost = 1.0f + 0.40f * (spectralVariance - 0.74f) / 0.76f;
        varianceBoost = fl::clamp(varianceBoost, 0.90f, 1.25f);
        totalBoost *= varianceBoost;
    }

    // Low-jitter penalty: very periodic signals (guitar jit=0.10) get penalized.
    // Voice jitter is always >0.50, so this only penalizes instruments.
    if (mEnvelopeJitter < 0.15f && mEnvelopeJitter > 0.001f) {
        float periodicPenalty = 1.0f - 0.25f * (0.15f - mEnvelopeJitter) / 0.15f;
        totalBoost *= periodicPenalty;
    }

    // Envelope jitter boost: only for very high jitter (>0.55).
    // Drums alone reach 0.61, so only extreme jitter gets a small boost.
    if (mEnvelopeJitter > 0.55f) {
        float jitterBoost = 1.0f + 0.8f * (mEnvelopeJitter - 0.55f);
        jitterBoost = fl::clamp(jitterBoost, 1.0f, 1.15f);
        totalBoost *= jitterBoost;
    }

    // Autocorrelation irregularity boost: only above 0.75 (above drum baseline).
    if (mAutocorrelationIrregularity > 0.75f) {
        float acfBoost = 1.0f + 1.0f * (mAutocorrelationIrregularity - 0.75f);
        acfBoost = fl::clamp(acfBoost, 1.0f, 1.25f);
        totalBoost *= acfBoost;
    }

    // Low-zcCV penalty: very regular zero-crossings (guitar zcCV=0.05) penalized.
    // Voice zcCV is always >0.30, so this only hits periodic instruments.
    if (mZeroCrossingCV < 0.10f && mZeroCrossingCV > 0.001f) {
        float zcPenalty = 1.0f - 0.20f * (0.10f - mZeroCrossingCV) / 0.10f;
        totalBoost *= zcPenalty;
    }

    // Zero-crossing CV: peaked boost for moderate values, penalty for high.
    if (mZeroCrossingCV > 0.02f) {
        float zcBoost;
        if (mZeroCrossingCV < 0.80f) {
            zcBoost = 1.0f + 0.25f * (mZeroCrossingCV - 0.10f) / 0.70f;
        } else if (mZeroCrossingCV < 1.50f) {
            zcBoost = 1.0f;
        } else {
            zcBoost = 1.0f - 0.15f * (mZeroCrossingCV - 1.50f) / 1.00f;
        }
        zcBoost = fl::clamp(zcBoost, 0.80f, 1.20f);
        totalBoost *= zcBoost;
    }

    // Combined aperiodicity-jitter boost: when BOTH indicate voice.
    if (mAutocorrelationIrregularity > 0.75f && mEnvelopeJitter > 0.55f) {
        float minExcess = fl::min(mAutocorrelationIrregularity - 0.75f,
                                   mEnvelopeJitter - 0.55f);
        float combinedBoost = 1.0f + 1.5f * minExcess;
        combinedBoost = fl::clamp(combinedBoost, 1.0f, 1.20f);
        totalBoost *= combinedBoost;
    }

    // Cap total boost chain to prevent runaway
    totalBoost = fl::clamp(totalBoost, 0.50f, 1.60f);
    weightedAvg *= totalBoost;

    // Formant gate/boost: weak formant → gate down, strong formant → mild boost.
    float formantMultiplier;
    if (formantScore < 0.40f) {
        formantMultiplier = formantScore * 2.5f;  // gate (0.0 to 1.0)
    } else {
        formantMultiplier = 1.0f + 0.40f * (formantScore - 0.40f);  // boost (1.0 to 1.24)
    }

    // Clamp final confidence to [0, 1]
    mConfidence = fl::clamp(weightedAvg * formantMultiplier, 0.0f, 1.0f);
    return mConfidence;
}

} // namespace fl
