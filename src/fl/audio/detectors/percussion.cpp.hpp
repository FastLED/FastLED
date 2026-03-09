// PercussionDetector - Multi-feature spectral classification
// Part of FastLED Audio System v2.0 - Phase 3 (Differentiators)

#include "fl/audio/detectors/percussion.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"

namespace fl {

PercussionDetector::PercussionDetector()
    : mKickDetected(false)
    , mSnareDetected(false)
    , mHiHatDetected(false)
    , mTomDetected(false)
    , mKickConfidence(0.0f)
    , mSnareConfidence(0.0f)
    , mHiHatConfidence(0.0f)
    , mTomConfidence(0.0f)
    , mBassToTotal(0.0f)
    , mTrebleToTotal(0.0f)
    , mClickRatio(0.0f)
    , mTrebleFlatness(0.0f)
    , mMidToTreble(0.0f)
    , mOnsetSharpness(0.0f)
    , mSubBassProxy(0.0f)
    , mZeroCrossingFactor(0.0f)
    , mKickThreshold(0.45f)
    , mSnareThreshold(0.40f)
    , mHiHatThreshold(0.35f)
    , mTomThreshold(0.40f)
    , mLastKickTime(0)
    , mLastSnareTime(0)
    , mLastHiHatTime(0)
    , mLastTomTime(0)
{}

PercussionDetector::~PercussionDetector() = default;

void PercussionDetector::update(shared_ptr<AudioContext> context) {
    mRetainedFFT = context->getFFT16(FFTMode::CQ_OCTAVE);
    const FFTBins& fft = *mRetainedFFT;
    u32 timestamp = context->getTimestamp();

    // Step 0: Get zero-crossing factor from raw audio sample
    mZeroCrossingFactor = context->getZCF();

    // Step 1: Compute spectral features from 16-bin FFT
    computeFeatures(fft);

    // Step 2: Compute total energy and onset sharpness via envelope
    float totalEnergy = 0.0f;
    for (fl::size i = 0; i < fft.raw().size(); ++i) {
        totalEnergy += fft.raw()[i];
    }
    const float dt = computeAudioDt(context->getPCM().size(), context->getSampleRate());
    float envValue = mTotalEnvelope.update(totalEnergy, dt);
    float flux = fl::max(0.0f, totalEnergy - envValue);
    mOnsetSharpness = (totalEnergy > 1e-6f) ? flux / totalEnergy : 0.0f;

    // Step 3: Gate on onset — no onset means no percussion
    static constexpr float kOnsetGate = 0.05f;
    if (mOnsetSharpness < kOnsetGate) {
        mKickDetected = false;
        mSnareDetected = false;
        mHiHatDetected = false;
        mTomDetected = false;
        mKickConfidence = 0.0f;
        mSnareConfidence = 0.0f;
        mHiHatConfidence = 0.0f;
        mTomConfidence = 0.0f;
        return;
    }

    // Step 4: Compute per-type confidence scores
    computeConfidences();

    // Step 5: Apply cross-band rejection
    applyCrossBandRejection();

    // Step 6: Threshold + cooldown → boolean state
    mKickDetected = (mKickConfidence >= mKickThreshold) &&
                    (timestamp - mLastKickTime >= KICK_COOLDOWN_MS);
    mSnareDetected = (mSnareConfidence >= mSnareThreshold) &&
                     (timestamp - mLastSnareTime >= SNARE_COOLDOWN_MS);
    mHiHatDetected = (mHiHatConfidence >= mHiHatThreshold) &&
                     (timestamp - mLastHiHatTime >= HIHAT_COOLDOWN_MS);
    mTomDetected = (mTomConfidence >= mTomThreshold) &&
                   (timestamp - mLastTomTime >= TOM_COOLDOWN_MS);

    // Update timestamps
    if (mKickDetected) mLastKickTime = timestamp;
    if (mSnareDetected) mLastSnareTime = timestamp;
    if (mHiHatDetected) mLastHiHatTime = timestamp;
    if (mTomDetected) mLastTomTime = timestamp;
}

void PercussionDetector::fireCallbacks() {
    if (mKickDetected) {
        if (onKick) onKick();
        if (onPercussionHit) onPercussionHit(PercussionType::Kick);
    }

    if (mSnareDetected) {
        if (onSnare) onSnare();
        if (onPercussionHit) onPercussionHit(PercussionType::Snare);
    }

    if (mHiHatDetected) {
        if (onHiHat) onHiHat();
        if (onPercussionHit) onPercussionHit(PercussionType::HiHat);
    }

    if (mTomDetected) {
        if (onTom) onTom();
        if (onPercussionHit) onPercussionHit(PercussionType::Tom);
    }
}

void PercussionDetector::reset() {
    mKickDetected = false;
    mSnareDetected = false;
    mHiHatDetected = false;
    mTomDetected = false;
    mKickConfidence = 0.0f;
    mSnareConfidence = 0.0f;
    mHiHatConfidence = 0.0f;
    mTomConfidence = 0.0f;
    mBassToTotal = 0.0f;
    mTrebleToTotal = 0.0f;
    mClickRatio = 0.0f;
    mTrebleFlatness = 0.0f;
    mMidToTreble = 0.0f;
    mOnsetSharpness = 0.0f;
    mSubBassProxy = 0.0f;
    mZeroCrossingFactor = 0.0f;
    mTotalEnvelope.reset();
    mLastKickTime = 0;
    mLastSnareTime = 0;
    mLastHiHatTime = 0;
    mLastTomTime = 0;
}

void PercussionDetector::computeFeatures(const FFTBins& fft) {
    auto bins = fft.raw();
    int n = static_cast<int>(bins.size());
    if (n < 16) {
        mBassToTotal = 0.0f;
        mTrebleToTotal = 0.0f;
        mClickRatio = 0.0f;
        mTrebleFlatness = 0.0f;
        mMidToTreble = 0.0f;
        mSubBassProxy = 0.0f;
        return;
    }

    // Band sums for 16-bin CQ FFT
    // Bass: bins 0-3, Mid: bins 4-9, Treble: bins 10-15
    float bassSum = 0.0f;
    for (int i = 0; i < 4; ++i) bassSum += bins[i];

    float midSum = 0.0f;
    for (int i = 4; i < 10; ++i) midSum += bins[i];

    float trebleSum = 0.0f;
    for (int i = 10; i < 16; ++i) trebleSum += bins[i];

    // Click band: bins 10-12 (upper treble, where kick click appears)
    float clickBandSum = 0.0f;
    for (int i = 10; i < 13; ++i) clickBandSum += bins[i];

    // Upper-mid region: bins 4-9
    float upperMidSum = midSum;

    float total = bassSum + midSum + trebleSum;

    // Bass/Total ratio
    mBassToTotal = (total > 1e-6f) ? bassSum / total : 0.0f;

    // Treble/Total ratio
    mTrebleToTotal = (total > 1e-6f) ? trebleSum / total : 0.0f;

    // Click ratio: treble click band vs upper-mid notch region
    mClickRatio = (upperMidSum > 1e-6f) ? clickBandSum / upperMidSum : 0.0f;

    // Mid/Treble ratio
    mMidToTreble = (trebleSum > 1e-6f) ? midSum / trebleSum : 0.0f;

    // Sub-bass proxy: bin[0] / bin[4]
    mSubBassProxy = (bins[4] > 1e-6f) ? bins[0] / bins[4] : 0.0f;

    // Treble flatness: geometric_mean / arithmetic_mean of bins 10-15
    float trebleArithMean = trebleSum / 6.0f;
    if (trebleArithMean > 1e-6f) {
        float sumLog = 0.0f;
        int validCount = 0;
        for (int i = 10; i < 16; ++i) {
            if (bins[i] > 1e-6f) {
                sumLog += fl::logf(bins[i]);
                ++validCount;
            }
        }
        if (validCount > 0) {
            float geoMean = fl::expf(sumLog / static_cast<float>(validCount));
            mTrebleFlatness = geoMean / trebleArithMean;
        } else {
            mTrebleFlatness = 0.0f;
        }
    } else {
        mTrebleFlatness = 0.0f;
    }
}

void PercussionDetector::computeConfidences() {
    // Feature distributions from synthetic generators through 16-bin CQ FFT:
    //
    // Type     | bassToTotal | trebleToTotal | clickRatio | trebleFlatness | midToTreble | subBassProxy
    // ---------|-------------|---------------|------------|----------------|-------------|-------------
    // Kick     |   0.50-0.85 |    0.05-0.20  |  0.3-1.5   |   0.7-0.9      |   1.5-4.0   |   3-15
    // Snare    |   0.20-0.45 |    0.25-0.55  |  0.3-1.0   |   0.5-0.8      |   0.3-1.5   |   0.2-2
    // HiHat   |   0.03-0.10 |    0.65-0.85  |  0.8-1.2   |   0.6-0.8      |   0.1-0.4   |   0.2-1
    // Tom      |   0.85-0.97 |    0.01-0.05  |  0.05-0.15 |   0.8-0.95     |   4-8       |   15-35

    // Kick: bass-dominant with click transient energy
    // Key discriminants from tom: click ratio (kick > tom), lower subBassProxy
    {
        // Bass score: peaks at 0.65 (kick range 0.50-0.85)
        float bassScore = fl::max(0.0f, 1.0f - fl::abs(mBassToTotal - 0.65f) / 0.35f);
        // Click presence: click ratio > 0.3 indicates kick click transient
        float clickScore = fl::min(1.0f, mClickRatio / 1.0f);
        // Sub-bass proxy: moderate (3-15) — not as extreme as tom (15-35)
        float subBassScore = 0.0f;
        if (mSubBassProxy >= 2.0f && mSubBassProxy <= 20.0f) {
            subBassScore = 1.0f - fl::abs(mSubBassProxy - 8.0f) / 12.0f;
            subBassScore = fl::max(0.0f, subBassScore);
        }
        // ZCF: kick has low-moderate zero crossings (~0.1-0.4)
        float zcfScore = fl::max(0.0f, 1.0f - fl::abs(mZeroCrossingFactor - 0.25f) / 0.25f);

        mKickConfidence = 0.30f * bassScore + 0.30f * clickScore +
                          0.20f * subBassScore + 0.20f * zcfScore;
    }

    // Snare: moderate bass + significant treble from noise rattles
    // Key features: bassToTotal ~0.35-0.50, trebleToTotal ~0.35-0.55,
    //               midToTreble ~0.25-0.50, trebleFlatness ~0.75-0.90
    {
        // Bass score: moderate bass (0.25-0.55) — not too high (kick), not too low (hihat)
        float bassScore = fl::max(0.0f, 1.0f - fl::abs(mBassToTotal - 0.40f) / 0.20f);
        // Treble presence: snare has significant treble (0.30-0.55)
        float trebleScore = fl::max(0.0f, 1.0f - fl::abs(mTrebleToTotal - 0.45f) / 0.20f);
        // Mid/Treble ratio: snare ~0.25-0.50 (moderate)
        float midTrebleScore = fl::max(0.0f, 1.0f - fl::abs(mMidToTreble - 0.35f) / 0.25f);
        // Treble flatness: high (snare noise has high flatness ~0.80-0.90)
        float flatnessScore = fl::max(0.0f, (mTrebleFlatness - 0.70f) / 0.25f);
        flatnessScore = fl::min(1.0f, flatnessScore);
        // Click ratio: snare has moderate click ratio (~0.7-1.1)
        float clickScore = fl::max(0.0f, 1.0f - fl::abs(mClickRatio - 0.9f) / 0.6f);

        mSnareConfidence = 0.25f * bassScore + 0.25f * trebleScore +
                           0.20f * midTrebleScore + 0.15f * flatnessScore +
                           0.15f * clickScore;
    }

    // HiHat: treble-dominant with high flatness and high ZCF
    {
        // Treble dominance: hi-hat is mostly treble (0.65-0.85)
        float trebleScore = fl::max(0.0f, (mTrebleToTotal - 0.50f) / 0.40f);
        trebleScore = fl::min(1.0f, trebleScore);
        // No bass
        float noBassScore = fl::max(0.0f, 1.0f - mBassToTotal / 0.15f);
        // Treble flatness: moderate-high (0.6-0.8)
        float flatnessScore = fl::max(0.0f, 1.0f - fl::abs(mTrebleFlatness - 0.72f) / 0.30f);
        // ZCF: hi-hat has very high zero crossings (~0.5-0.8)
        float zcfScore = fl::max(0.0f, (mZeroCrossingFactor - 0.35f) / 0.45f);
        zcfScore = fl::min(1.0f, zcfScore);

        mHiHatConfidence = 0.35f * trebleScore + 0.20f * noBassScore +
                           0.25f * flatnessScore + 0.20f * zcfScore;
    }

    // Tom: extreme bass dominance, NO click, very high subBassProxy, low ZCF
    {
        // Very high bass (0.85-0.97)
        float bassScore = fl::max(0.0f, (mBassToTotal - 0.80f) / 0.17f);
        bassScore = fl::min(1.0f, bassScore);
        // NO click: key discriminant from kick — very low click ratio (< 0.15)
        float noClickScore = fl::max(0.0f, 1.0f - mClickRatio / 0.30f);
        // No treble
        float noTrebleScore = fl::max(0.0f, 1.0f - mTrebleToTotal / 0.08f);
        // ZCF: tom has very low zero crossings (~0.05-0.15)
        float zcfScore = fl::max(0.0f, 1.0f - mZeroCrossingFactor / 0.25f);

        mTomConfidence = 0.30f * bassScore + 0.25f * noClickScore +
                         0.20f * noTrebleScore + 0.25f * zcfScore;
    }

    // Clamp all to [0, 1]
    mKickConfidence = fl::clamp(mKickConfidence, 0.0f, 1.0f);
    mSnareConfidence = fl::clamp(mSnareConfidence, 0.0f, 1.0f);
    mHiHatConfidence = fl::clamp(mHiHatConfidence, 0.0f, 1.0f);
    mTomConfidence = fl::clamp(mTomConfidence, 0.0f, 1.0f);
}

void PercussionDetector::applyCrossBandRejection() {
    // Winner-takes-all among competing types
    // Kick vs Tom: these share bass dominance. Use click ratio as discriminant.
    // If click ratio > 0.25, it's more kick-like; suppress tom
    // If click ratio < 0.15, it's more tom-like; suppress kick
    if (mBassToTotal > 0.45f) {
        if (mClickRatio > 0.25f) {
            // Click present → kick-like, suppress tom
            mTomConfidence *= 0.3f;
        } else if (mClickRatio < 0.15f) {
            // No click → tom-like, suppress kick
            mKickConfidence *= 0.3f;
        }
    }

    // HiHat vs Snare: both have treble. Discriminate by bass content.
    // If bass is very low (< 0.10), it's a hi-hat not a snare
    if (mTrebleToTotal > 0.50f && mBassToTotal < 0.15f) {
        mSnareConfidence *= 0.3f;
    }

    // Crash/broadband rejection: treble-dominant with low bass
    // Suppress snare for crash-like spectra (low bass, high treble, moderate midToTreble)
    if (mTrebleToTotal > 0.55f && mBassToTotal < 0.20f) {
        mSnareConfidence *= 0.3f;
        mKickConfidence *= 0.2f;
        mTomConfidence *= 0.2f;
    }

    // Noise rejection: white noise has very low treble flatness (< 0.50)
    // Real percussion (snare, hihat) has higher treble flatness (> 0.60)
    if (mTrebleFlatness < 0.50f && mTrebleToTotal > 0.40f) {
        mSnareConfidence *= 0.3f;
    }

    // Kick has high click ratio (> 2.0) which is very different from snare (~0.9)
    // If click ratio is very high, suppress snare (it's a kick)
    if (mClickRatio > 2.0f && mBassToTotal > 0.30f) {
        mSnareConfidence *= 0.3f;
    }
}

} // namespace fl
