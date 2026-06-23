// Vocal - Human voice detection implementation
// Part of FastLED Audio System v2.0 - Phase 3 (Differentiators)

#include "fl/audio/detector/vocal.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/fft/fft.h"
#include "fl/math/math.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

Vocal::Vocal()
    : mVocalActive(false)
    , mPreviousVocalActive(false)
    , mConfidence(0.0f)
    , mSpectralCentroid(0.0f)
    , mSpectralRolloff(0.0f)
    , mFormantRatio(0.0f)
{}

Vocal::~Vocal() FL_NOEXCEPT = default;

void Vocal::update(shared_ptr<Context> context) {
    mSampleRate = context->getSampleRate();
    // Dual FFT: high-resolution formant analysis + broad spectral features.
    // Formant FFT: 64 CQ bins in 200-3500 Hz — concentrates resolution on
    // F1/F2/F3 formants (94% bin utilization vs 73% with full-range).
    // Broad FFT: 16 LOG_REBIN bins in 174.6-4698.3 Hz — full spectral
    // coverage for flatness, density, flux, and vocal presence ratio.
    mRetainedFormantFFT = context->getFFT(64, 200.0f, 3500.0f);
    mRetainedBroadFFT = context->getFFT(16, 174.6f, 4698.3f);
    const fft::Bins& formantFft = *mRetainedFormantFFT;
    const fft::Bins& broadFft = *mRetainedBroadFFT;
    mFormantNumBins = static_cast<int>(formantFft.raw().size());
    mBroadNumBins = static_cast<int>(broadFft.raw().size());

    // Formant ratio from high-res narrow FFT
    computeFormantRatio(formantFft);
    // Broad spectral features (flatness, density, flux) from wide FFT
    computeBroadSpectralFeatures(broadFft);

    // Vocal presence ratio from broad FFT linear bins (needs 200-4000 Hz coverage)
    mVocalPresenceRatio = calculateVocalPresenceRatio(broadFft);
    mSpectralVariance = mSpectralVarianceFilter.update(broadFft.raw());

    // Calculate time-domain features from raw PCM
    span<const i16> pcm = context->getPCM();
    const float dt = computeAudioDt(pcm.size(), context->getSampleRate());
    // Fused pass: envelope jitter + zero-crossing CV in one PCM traversal
    computePCMTimeDomainFeatures(pcm);
    mEnvelopeJitter = mEnvelopeJitterSmoother.update(mEnvelopeJitter, dt);
    mAutocorrelationIrregularity = mAcfIrregularitySmoother.update(
        calculateAutocorrelationIrregularity(pcm), dt);
    mZeroCrossingCV = mZcCVSmoother.update(mZeroCrossingCV, dt);

    // Calculate raw confidence and apply time-aware smoothing
    float rawConfidence = calculateRawConfidence(
        mFormantRatio, mSpectralFlatness, mHarmonicDensity,
        mVocalPresenceRatio, mSpectralFlux, mSpectralVariance);
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

void Vocal::fireCallbacks() {
    if (mStateChanged) {
        if (onVocal) onVocal(static_cast<u8>(mConfidenceSmoother.value() * 255.0f));
        if (mVocalActive && onVocalStart) onVocalStart();
        if (!mVocalActive && onVocalEnd) onVocalEnd();
        mPreviousVocalActive = mVocalActive;
        mStateChanged = false;
    }
}

void Vocal::reset() {
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
    mFluxNormBins.clear();
    mSpectralVarianceFilter.reset();
    mFramesInState = 0;
    mFormantCachedBinCount = -1;
}

void Vocal::computeFormantRatio(const fft::Bins& formantFft) {
    // Formant ratio from high-resolution narrow FFT (64 bins, 200-3500 Hz).
    // All bins concentrated in the formant region — ~94% utilization.
    const auto& bins = formantFft.raw();
    const int n = static_cast<int>(bins.size());
    if (n < 8) {
        mFormantRatio = 0.0f;
        return;
    }

    // Ensure formant bin cache is current (freqToBin uses logf internally)
    if (mFormantCachedBinCount != n) {
        mFormantF1MinBin = fl::max(0, formantFft.freqToBin(250.0f));
        mFormantF1MaxBin = fl::min(n - 1, formantFft.freqToBin(900.0f));
        mFormantF2MinBin = fl::max(0, formantFft.freqToBin(1000.0f));
        mFormantF2MaxBin = fl::min(n - 1, formantFft.freqToBin(3000.0f));
        mFormantCachedBinCount = n;
    }

    float f1Peak = 0.0f, f1Sum = 0.0f;
    int f1Count = 0;
    float f2Peak = 0.0f, f2Sum = 0.0f;
    int f2Count = 0;

    for (int i = 0; i < n; ++i) {
        const float mag = bins[i];
        if (i >= mFormantF1MinBin && i <= mFormantF1MaxBin) {
            f1Peak = fl::max(f1Peak, mag);
            f1Sum += mag;
            ++f1Count;
        }
        if (i >= mFormantF2MinBin && i <= mFormantF2MaxBin) {
            f2Peak = fl::max(f2Peak, mag);
            f2Sum += mag;
            ++f2Count;
        }
    }

    if (f1Count > 0 && f2Count > 0) {
        float f1Avg = f1Sum / static_cast<float>(f1Count);
        float f2Avg = f2Sum / static_cast<float>(f2Count);
        if (f1Peak >= f1Avg * 1.5f && f2Peak >= f2Avg * 1.5f && f1Peak >= 1e-6f) {
            mFormantRatio = f2Peak / f1Peak;
        } else {
            mFormantRatio = 0.0f;
        }
    } else {
        mFormantRatio = 0.0f;
    }
}

void Vocal::computeBroadSpectralFeatures(const fft::Bins& broadFft) {
    // Broad spectral features from low-res wide FFT (16 bins, 174.6-4698.3 Hz).
    // Computes flatness, harmonic density, and spectral flux.
    const auto& bins = broadFft.raw();
    const int n = static_cast<int>(bins.size());
    if (n == 0) {
        mSpectralFlatness = 0.0f;
        mHarmonicDensity = 0.0f;
        mSpectralFlux = 0.0f;
        return;
    }

    // === PASS 1: Flatness + density peak + magnitudeSum ===
    float magnitudeSum = 0.0f;
    float sumLn = 0.0f;
    float sumRaw = 0.0f;
    int flatnessCount = 0;
    float peak = 0.0f;

    for (int i = 0; i < n; ++i) {
        const float mag = bins[i];
        magnitudeSum += mag;
        if (mag > 1e-6f) {
            sumLn += fast_logf_approx(mag);
            sumRaw += mag;
            ++flatnessCount;
        }
        if (mag > peak) peak = mag;
    }

    // --- Flatness result ---
    if (flatnessCount >= 2) {
        float geometricMean = fl::expf(sumLn / static_cast<float>(flatnessCount));
        float arithmeticMean = sumRaw / static_cast<float>(flatnessCount);
        mSpectralFlatness = (arithmeticMean < 1e-6f) ? 0.0f : geometricMean / arithmeticMean;
    } else {
        mSpectralFlatness = 0.0f;
    }

    // === PASS 2: Density count + spectral flux ===
    mHarmonicDensity = 0.0f;
    mSpectralFlux = 0.0f;

    if (magnitudeSum >= 1e-6f) {
        const float densityThreshold = peak * 0.1f;
        const float invSum = 1.0f / magnitudeSum;
        const bool hasPrev = (mPrevBins.size() == static_cast<fl::size>(n));
        int densityCount = 0;
        float flux = 0.0f;

        mFluxNormBins.resize(n);
        for (int i = 0; i < n; ++i) {
            const float mag = bins[i];
            if (mag >= densityThreshold) ++densityCount;
            float norm = mag * invSum;
            mFluxNormBins[i] = norm;
            if (hasPrev) {
                float diff = norm - mPrevBins[i];
                flux += diff * diff;
            }
        }

        mHarmonicDensity = static_cast<float>(densityCount);
        mSpectralFlux = hasPrev ? fl::sqrtf(flux) : 0.0f;
        fl::swap(mPrevBins, mFluxNormBins);
    } else {
        mPrevBins.clear();
    }
}


float Vocal::calculateVocalPresenceRatio(const fft::Bins& fft) {
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


void Vocal::computePCMTimeDomainFeatures(span<const i16> pcm) {
    // Fused single-pass computation of envelope jitter + shimmer AND
    // zero-crossing CV. Previously two separate PCM traversals; now one.
    // Saves ~2-3 us by eliminating redundant PCM reads and cache misses.
    const int n = static_cast<int>(pcm.size());
    if (n < 44) {
        mEnvelopeJitter = 0.0f;
        mZeroCrossingCV = 0.0f;
        return;
    }

    const float normFactor = 1.0f / 32768.0f;
    const int halfWin = fl::max(2, mSampleRate / 4000); // ~11 samples at 44100
    const int winSize = 2 * halfWin + 1;
    const float invWinSize = 1.0f / static_cast<float>(winSize);

    // Seed the sliding window sum
    float windowSum = 0.0f;
    for (int j = 0; j < winSize && j < n; ++j) {
        windowSum += fl::abs(static_cast<float>(pcm[j])) * normFactor;
    }

    // Envelope jitter accumulators
    float sumEnv = 0.0f;
    float sumDev = 0.0f;
    int count = 0;
    float sumPeaks = 0.0f;
    float sumSqPeaks = 0.0f;
    int numCycles = 0;
    float currentPeak = 0.0f;
    bool wasPositive = pcm[halfWin] >= 0;

    // Zero-crossing CV accumulators (fused into same loop)
    int prevCrossing = -1;
    int numIntervals = 0;
    float sumIntervals = 0.0f;
    float sumSqIntervals = 0.0f;

    for (int i = halfWin; i < n - halfWin; ++i) {
        float absVal = fl::abs(static_cast<float>(pcm[i])) * normFactor;
        float smoothed = windowSum * invWinSize;

        sumEnv += smoothed;
        sumDev += fl::abs(absVal - smoothed);
        ++count;

        // Shimmer + zero-crossing detection (shared)
        currentPeak = fl::max(currentPeak, absVal);
        bool isPositive = pcm[i] >= 0;
        if (isPositive != wasPositive) {
            // Shimmer: track peaks between zero crossings
            if (currentPeak > 0.01f) {
                sumPeaks += currentPeak;
                sumSqPeaks += currentPeak * currentPeak;
                ++numCycles;
            }
            currentPeak = 0.0f;

            // ZC CV: track interval statistics
            if (prevCrossing >= 0) {
                float interval = static_cast<float>(i - prevCrossing);
                sumIntervals += interval;
                sumSqIntervals += interval * interval;
                ++numIntervals;
            }
            prevCrossing = i;
        }
        wasPositive = isPositive;

        // Slide window
        if (i + 1 < n - halfWin) {
            windowSum -= fl::abs(static_cast<float>(pcm[i - halfWin])) * normFactor;
            windowSum += fl::abs(static_cast<float>(pcm[i + halfWin + 1])) * normFactor;
        }
    }

    // --- Envelope jitter result ---
    if (sumEnv < 1e-6f || count == 0) {
        mEnvelopeJitter = 0.0f;
    } else {
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
        mEnvelopeJitter = envelopeJitter + shimmer * 0.5f;
    }

    // --- Zero-crossing CV result ---
    if (numIntervals < 2) {
        mZeroCrossingCV = 0.0f;
    } else {
        float mean = sumIntervals / static_cast<float>(numIntervals);
        if (mean < 1e-6f) {
            mZeroCrossingCV = 0.0f;
        } else {
            float variance = sumSqIntervals / static_cast<float>(numIntervals)
                           - mean * mean;
            if (variance < 0.0f) variance = 0.0f;
            mZeroCrossingCV = fl::sqrtf(variance) / mean;
        }
    }
}

float Vocal::calculateAutocorrelationIrregularity(span<const i16> pcm) {
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


float Vocal::calculateRawConfidence(float formantRatio,
                                             float spectralFlatness, float harmonicDensity,
                                             float vocalPresenceRatio, float spectralFlux,
                                             float spectralVariance) {
    // Centroid and rolloff removed from scoring (combined 0.04 weight — negligible
    // impact on accuracy). Centroid still computed for diagnostics.

    // Continuous confidence scores for each feature (no hard binary cutoffs)

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

    // Harmonic density score: with 16 broad bins (scaled from 64-bin calibration).
    // Real audio: ~6-7, synthetic voice: 8-12, pure tone: ~4, noise: ~15
    float densityScore;
    if (harmonicDensity < 2.5f) {
        // Too sparse — pure tone or near-tonal
        densityScore = harmonicDensity / 2.5f * 0.3f;
    } else if (harmonicDensity <= 12.5f) {
        // Voice-like range
        densityScore = 0.3f + (harmonicDensity - 2.5f) / 10.0f * 0.7f;
    } else {
        // Too dense — noise-like
        densityScore = fl::max(0.0f, 1.0f - (harmonicDensity - 12.5f) / 3.5f);
    }

    // Spectral flux score: voice has higher frame-to-frame spectral change
    // than sustained instruments (phoneme transitions, formant shifts).
    // Thresholds scaled by sqrt(16/64) = 0.5 for 16-bin broad FFT.
    float spectralFluxScore;
    if (spectralFlux < 0.01f) {
        spectralFluxScore = 0.0f;
    } else if (spectralFlux < 0.15f) {
        spectralFluxScore = (spectralFlux - 0.01f) / 0.14f;
    } else {
        spectralFluxScore = 1.0f;
    }

    // Weighted average — centroid/rolloff removed (combined 0.04 weight,
    // 0-5% separation between voice and guitar — not discriminative).
    // Budget redistributed to formant/flatness (best discriminators).
    float weightedAvg = 0.33f * formantScore
                      + 0.35f * flatnessScore
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

} // namespace detector
} // namespace audio
} // namespace fl
