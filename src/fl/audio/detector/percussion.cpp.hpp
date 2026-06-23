// Percussion - Multi-feature spectral classification
// Part of FastLED Audio System v2.0 - Phase 3 (Differentiators)

#include "fl/audio/detector/percussion.h"
#include "fl/audio/audio_context.h"
#include "fl/math/math.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

Percussion::Percussion()
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
    , mKickThreshold(0.35f)
    , mSnareThreshold(0.30f)
    , mHiHatThreshold(0.30f)
    , mTomThreshold(0.30f)
    , mLastKickTime(0)
    , mLastSnareTime(0)
    , mLastHiHatTime(0)
    , mLastTomTime(0)
{}

Percussion::~Percussion() FL_NOEXCEPT = default;

void Percussion::update(shared_ptr<Context> context) {
    mRetainedFFT = context->getFFT16(fft::Mode::CQ_NAIVE);
    const fft::Bins& fft = *mRetainedFFT;
    u32 timestamp = context->getTimestamp();

    // Step 0: Get zero-crossing factor from raw audio sample
    mZeroCrossingFactor = context->getZCF();

    // Step 1: Compute spectral features from 16-bin fft::FFT
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

void Percussion::fireCallbacks() {
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

void Percussion::reset() {
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

void Percussion::computeFeatures(const fft::Bins& fft) {
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

    // Frequency-based band boundaries (range-agnostic).
    // Bass: fmin–300 Hz  (kick body, sub-bass)
    // Mid:  300–2000 Hz  (snare body, tom)
    // Treble: 2000+ Hz   (click transients, hi-hat, cymbals)
    // Click: 1500–4500 Hz (kick beater attack, snare crack)
    int bassCutBin = fl::max(1, fft.freqToBin(300.0f));
    int trebleCutBin = fl::max(bassCutBin + 1, fft.freqToBin(2000.0f));
    int clickLoBin = fl::max(bassCutBin, fft.freqToBin(1500.0f));
    int clickHiBin = fl::min(n - 1, fft.freqToBin(4500.0f));

    float bassSum = 0.0f;
    for (int i = 0; i <= bassCutBin; ++i) bassSum += bins[i];

    float midSum = 0.0f;
    for (int i = bassCutBin + 1; i < trebleCutBin; ++i) midSum += bins[i];

    float trebleSum = 0.0f;
    for (int i = trebleCutBin; i < n; ++i) trebleSum += bins[i];

    // Click band: 1500–4500 Hz (kick click, snare crack)
    float clickBandSum = 0.0f;
    for (int i = clickLoBin; i <= clickHiBin; ++i) clickBandSum += bins[i];

    // Upper-mid for click ratio denominator
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

    // Sub-bass proxy: lowest bin / first mid bin
    int firstMidBin = bassCutBin + 1;
    mSubBassProxy = (firstMidBin < n && bins[firstMidBin] > 1e-6f)
                        ? bins[0] / bins[firstMidBin]
                        : 0.0f;

    // Treble flatness: geometric_mean / arithmetic_mean of treble bins
    int trebleBinCount = n - trebleCutBin;
    float trebleArithMean = (trebleBinCount > 0) ? trebleSum / static_cast<float>(trebleBinCount) : 0.0f;
    if (trebleArithMean > 1e-6f) {
        float sumLog = 0.0f;
        int validCount = 0;
        for (int i = trebleCutBin; i < n; ++i) {
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

void Percussion::computeConfidences() {
    // Feature distributions with frequency-based bands (90-14080 Hz, 16 CQ bins):
    //
    // Type     | bassToTotal | trebleToTotal | clickRatio | trebleFlatness | midToTreble | subBassProxy
    // ---------|-------------|---------------|------------|----------------|-------------|-------------
    // Kick     |   0.30-0.45 |    0.30-0.50  |  1.0-2.0   |   0.9-1.0      |   0.5-0.9   |   0.5-3
    // Snare    |   0.10-0.25 |    0.55-0.75  |  1.5-2.5   |   0.9-1.0      |   0.2-0.5   |   0.0-0.2
    // HiHat    |   0.03-0.10 |    0.70-0.90  |  2.5-5.0   |   0.9-1.0      |   0.1-0.2   |   0.2-1.5
    // Tom      |   0.25-0.45 |    0.30-0.50  |  0.8-1.5   |   0.9-1.0      |   0.5-1.0   |   0.0-0.3

    // Kick: bass-dominant with high sub-bass proxy (distinguishes from tom)
    // Key discriminants from tom: subBassProxy (kick > 0.5, tom < 0.3)
    {
        // Bass score: peaks at 0.35 (kick range 0.25-0.50)
        float bassScore = fl::max(0.0f, 1.0f - fl::abs(mBassToTotal - 0.35f) / 0.20f);
        // Sub-bass proxy: kick has notable sub-bass energy (> 0.5)
        float subBassScore = fl::min(1.0f, mSubBassProxy / 2.0f);
        // Moderate treble (0.30-0.50 — higher than tom due to click)
        float trebleScore = fl::max(0.0f, 1.0f - fl::abs(mTrebleToTotal - 0.40f) / 0.20f);
        // ZCF: kick has low-moderate zero crossings (~0.1-0.4)
        float zcfScore = fl::max(0.0f, 1.0f - fl::abs(mZeroCrossingFactor - 0.25f) / 0.25f);

        mKickConfidence = 0.25f * bassScore + 0.35f * subBassScore +
                          0.20f * trebleScore + 0.20f * zcfScore;
    }

    // Snare: low-moderate bass + high treble from noise rattles
    {
        // Bass score: low bass (0.10-0.25) — lower than kick/tom
        float bassScore = fl::max(0.0f, 1.0f - fl::abs(mBassToTotal - 0.15f) / 0.15f);
        // Treble presence: snare has high treble (0.55-0.75)
        float trebleScore = fl::max(0.0f, 1.0f - fl::abs(mTrebleToTotal - 0.65f) / 0.20f);
        // Mid/Treble ratio: snare ~0.2-0.5 (moderate)
        float midTrebleScore = fl::max(0.0f, 1.0f - fl::abs(mMidToTreble - 0.30f) / 0.25f);
        // Very low sub-bass (< 0.2 — no body resonance)
        float noSubBassScore = fl::max(0.0f, 1.0f - mSubBassProxy / 0.5f);
        // ZCF: moderate (noise has many zero crossings)
        float zcfScore = fl::max(0.0f, (mZeroCrossingFactor - 0.20f) / 0.40f);
        zcfScore = fl::min(1.0f, zcfScore);

        mSnareConfidence = 0.20f * bassScore + 0.25f * trebleScore +
                           0.15f * midTrebleScore + 0.20f * noSubBassScore +
                           0.20f * zcfScore;
    }

    // HiHat: treble-dominant with high ZCF
    {
        // Treble dominance: hi-hat is mostly treble (0.70-0.90)
        float trebleScore = fl::max(0.0f, (mTrebleToTotal - 0.60f) / 0.30f);
        trebleScore = fl::min(1.0f, trebleScore);
        // No bass
        float noBassScore = fl::max(0.0f, 1.0f - mBassToTotal / 0.15f);
        // Very high click ratio (> 2.5 — all energy in click band)
        float highClickScore = fl::max(0.0f, (mClickRatio - 2.0f) / 3.0f);
        highClickScore = fl::min(1.0f, highClickScore);
        // ZCF: hi-hat has very high zero crossings (~0.5-0.8)
        float zcfScore = fl::max(0.0f, (mZeroCrossingFactor - 0.35f) / 0.45f);
        zcfScore = fl::min(1.0f, zcfScore);

        mHiHatConfidence = 0.30f * trebleScore + 0.20f * noBassScore +
                           0.25f * highClickScore + 0.25f * zcfScore;
    }

    // Tom: bass with NO sub-bass, low mid-to-treble discriminates from kick
    {
        // Moderate bass (0.25-0.45) — similar range to kick
        float bassScore = fl::max(0.0f, 1.0f - fl::abs(mBassToTotal - 0.35f) / 0.20f);
        // NO sub-bass: key discriminant from kick (tom < 0.3, kick > 0.5)
        float noSubBassScore = fl::max(0.0f, 1.0f - mSubBassProxy / 0.5f);
        // High mid-to-treble ratio (tom body resonance in mid)
        float midTrebleScore = fl::max(0.0f, (mMidToTreble - 0.50f) / 0.50f);
        midTrebleScore = fl::min(1.0f, midTrebleScore);
        // ZCF: tom has very low zero crossings (~0.05-0.15)
        float zcfScore = fl::max(0.0f, 1.0f - mZeroCrossingFactor / 0.25f);

        mTomConfidence = 0.25f * bassScore + 0.30f * noSubBassScore +
                         0.20f * midTrebleScore + 0.25f * zcfScore;
    }

    // Clamp all to [0, 1]
    mKickConfidence = fl::clamp(mKickConfidence, 0.0f, 1.0f);
    mSnareConfidence = fl::clamp(mSnareConfidence, 0.0f, 1.0f);
    mHiHatConfidence = fl::clamp(mHiHatConfidence, 0.0f, 1.0f);
    mTomConfidence = fl::clamp(mTomConfidence, 0.0f, 1.0f);
}

void Percussion::applyCrossBandRejection() {
    // Winner-takes-all among competing types.
    // With frequency-based bands (90-14080 Hz), kick and tom have similar
    // bass/treble ratios. Use sub-bass proxy as primary discriminant.

    // Kick vs Tom: both have moderate bass. Sub-bass proxy separates them.
    // Kick has sub-bass body resonance (subBassProxy > 0.5), tom doesn't.
    if (mBassToTotal > 0.20f) {
        if (mSubBassProxy > 0.5f) {
            // Sub-bass present → kick-like, suppress tom
            mTomConfidence *= 0.3f;
        } else if (mSubBassProxy < 0.3f) {
            // No sub-bass → tom-like, suppress kick
            mKickConfidence *= 0.3f;
        }
    }

    // HiHat vs Snare: both have high treble. Discriminate by bass content.
    if (mTrebleToTotal > 0.60f && mBassToTotal < 0.12f) {
        mSnareConfidence *= 0.3f;
    }

    // Crash/broadband rejection: very high treble with no bass
    if (mTrebleToTotal > 0.70f && mBassToTotal < 0.10f) {
        mSnareConfidence *= 0.3f;
        mKickConfidence *= 0.2f;
        mTomConfidence *= 0.2f;
    }

    // Noise rejection: white noise has very low treble flatness (< 0.50)
    if (mTrebleFlatness < 0.50f && mTrebleToTotal > 0.40f) {
        mSnareConfidence *= 0.3f;
    }

    // Very high click ratio + high treble → hi-hat, not snare
    if (mClickRatio > 3.0f && mTrebleToTotal > 0.60f) {
        mSnareConfidence *= 0.3f;
    }
}

} // namespace detector
} // namespace audio
} // namespace fl
