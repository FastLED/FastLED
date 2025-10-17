# FastLED Audio Processing Architecture Design

**Version:** 2.0
**Date:** 2025-01-16
**Status:** Complete design specification

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Architecture Overview](#architecture-overview)
3. [Core Components](#core-components)
   - [AudioContext - Shared Computation State](#audiocontext---shared-computation-state)
   - [AudioDetector Base Class](#audiodetector-base-class)
   - [AudioProcessor - High-Level Facade](#audioprocessor---high-level-facade)
4. [Design Rationale](#design-rationale)
5. [Implementation Examples](#implementation-examples)
   - [VocalDetector](#example-vocaldetector-implementation)
   - [PercussionDetector](#example-percussiondetector-implementation)
6. [Usage Examples](#usage-examples)
7. [Complete Detector Catalog](#complete-detector-catalog)
8. [Implementation Tiers](#implementation-tiers)
9. [Testing Strategy](#testing-strategy)
10. [Performance Benchmarks](#performance-benchmarks)
11. [Migration Guide](#migration-guide)

---

## Executive Summary

This document presents a comprehensive architecture for FastLED's audio processing system that:

1. **Solves the FFT sharing problem** - Expensive computations (FFT, spectrum analysis) are computed once and shared between multiple detectors
2. **Enables lazy evaluation** - Detectors are only created when needed, computations only run when accessed
3. **Provides a simple API** - High-level `AudioProcessor` facade with callback-based events
4. **Remains extensible** - Easy to add new detector types and cached computations
5. **Manages resources efficiently** - Smart pointers handle lifetimes, no memory duplication

### Key Innovation: AudioContext Pattern

The **AudioContext** pattern is the cornerstone of this design:

```cpp
// Create lightweight context wrapping audio sample
AudioContext ctx(sample);

// Multiple detectors access same context
beatDetector.update(ctx);     // Computes FFT once
vocalDetector.update(ctx);    // Reuses cached FFT!
percussionDetector.update(ctx); // Reuses cached FFT!
```

This pattern achieves **optimal performance** (FFT computed once) with **minimal complexity** (simple context object).

---

## Architecture Overview

### Class Hierarchy

```
AudioProcessor (high-level facade)
    ├── owns: shared_ptr<AudioContext>
    ├── owns: lazy-created Audio Detectors
    └── delegates to: concrete detectors

AudioContext (shared computation state)
    ├── owns: AudioSample (value, ref-counted)
    ├── caches: FFTBins (lazy)
    ├── caches: FFT history ring buffer
    └── provides: shared access to expensive computations

AudioDetector (base class for all detectors)
    ├── owns: shared_ptr<AudioContext> (shared ownership)
    ├── implements: virtual void update()
    └── triggers: callbacks via fl::function

Concrete Detectors (BeatDetector, VocalDetector, etc.)
    ├── inherit from: AudioDetector
    ├── access: context->getFFT(), context->getHistory()
    └── fire: specific event callbacks
```

### Data Flow

```
Audio Input
    ↓
AudioSample (PCM data, pre-computed RMS/ZCF)
    ↓
AudioContext (lazy FFT, history tracking)
    ↓
Multiple Detectors (shared access to cached data)
    ↓
User Callbacks (beat events, vocal detection, etc.)
```

---

## Core Components

### AudioContext - Shared Computation State

The `AudioContext` class manages lazy computation and caching of expensive operations like FFT, spectrum analysis, and history tracking. It's designed to be shared between multiple detectors via `shared_ptr`.

#### Header Declaration

```cpp
// fl/audio_context.h
#pragma once

#include "fl/audio.h"
#include "fl/fft.h"
#include "fl/vector.h"
#include "fl/ptr.h"

namespace fl {

class AudioContext {
public:
    explicit AudioContext(const AudioSample& sample);
    ~AudioContext();

    // ----- Basic Sample Access -----
    const AudioSample& getSample() const { return mSample; }
    span<const int16_t> getPCM() const { return mSample.pcm(); }
    float getRMS() const { return mSample.rms(); }
    float getZCF() const { return mSample.zcf(); }
    uint32_t getTimestamp() const { return mSample.timestamp(); }

    // ----- Lazy FFT Computation (with caching) -----
    const FFTBins& getFFT(
        int bands = 16,
        float fmin = FFT_Args::DefaultMinFrequency(),
        float fmax = FFT_Args::DefaultMaxFrequency()
    );
    bool hasFFT() const { return mFFTComputed; }

    // ----- FFT History (for temporal analysis) -----
    const vector<FFTBins>& getFFTHistory(int depth = 4);
    bool hasFFTHistory() const { return mFFTHistoryDepth > 0; }
    const FFTBins* getHistoricalFFT(int framesBack) const;

    // ----- Update & Reset -----
    void setSample(const AudioSample& sample);
    void clearCache();

private:
    AudioSample mSample;
    mutable FFTBins mFFT;
    mutable bool mFFTComputed;
    mutable FFT_Args mFFTArgs;
    vector<FFTBins> mFFTHistory;
    int mFFTHistoryDepth;
    int mFFTHistoryIndex;
};

} // namespace fl
```

#### Implementation

```cpp
// fl/audio_context.cpp
#include "fl/audio_context.h"

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
        if (mFFT.size() != bands) {
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
    if (framesBack < 0 || framesBack >= (int)mFFTHistory.size()) {
        return nullptr;
    }
    int index = (mFFTHistoryIndex - 1 - framesBack + mFFTHistory.size()) % mFFTHistory.size();
    return &mFFTHistory[index];
}

void AudioContext::setSample(const AudioSample& sample) {
    // Save current FFT to history
    if (mFFTComputed && mFFTHistoryDepth > 0) {
        if ((int)mFFTHistory.size() < mFFTHistoryDepth) {
            mFFTHistory.push_back(mFFT);
            mFFTHistoryIndex = mFFTHistory.size();
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
```

---

### AudioDetector Base Class

Base class for all audio detectors providing a common interface.

```cpp
// fl/audio_detector.h
#pragma once

#include "fl/ptr.h"
#include "fl/function.h"

namespace fl {

class AudioContext;

class AudioDetector {
public:
    virtual ~AudioDetector() = default;

    virtual void update(shared_ptr<AudioContext> context) = 0;
    virtual bool needsFFT() const { return false; }
    virtual bool needsFFTHistory() const { return false; }
    virtual const char* getName() const = 0;
    virtual void reset() {}
};

} // namespace fl
```

---

### AudioProcessor - High-Level Facade

The `AudioProcessor` provides a simple, user-friendly API with lazy detector instantiation and callback-based events.

```cpp
// fl/audio_processor.h
#pragma once

#include "fl/audio.h"
#include "fl/audio_context.h"
#include "fl/audio_detector.h"
#include "fl/ptr.h"
#include "fl/function.h"

namespace fl {

class BeatDetector;
class VocalDetector;
class PercussionDetector;
class PitchDetector;
class SilenceDetector;

class AudioProcessor {
public:
    AudioProcessor();
    ~AudioProcessor();

    // ----- Main Update -----
    void update(const AudioSample& sample);

    // ----- Beat Detection Events -----
    void onBeat(function<void()> callback);
    void onBeatPhase(function<void(float phase)> callback);
    void onOnset(function<void(float strength)> callback);
    void onTempoChange(function<void(float bpm, float confidence)> callback);

    // ----- Frequency Band Events -----
    void onBass(function<void(float level)> callback);
    void onMid(function<void(float level)> callback);
    void onTreble(function<void(float level)> callback);

    // ----- Percussion Detection Events -----
    void onPercussion(function<void(const char* type)> callback);
    void onKick(function<void()> callback);
    void onSnare(function<void()> callback);
    void onHiHat(function<void()> callback);

    // ----- Vocal Detection Events -----
    void onVocal(function<void(bool active)> callback);
    void onVocalStart(function<void()> callback);
    void onVocalEnd(function<void()> callback);

    // ----- Pitch Detection Events -----
    void onNoteOn(function<void(uint8_t note, uint8_t velocity)> callback);
    void onNoteOff(function<void(uint8_t note)> callback);
    void onPitch(function<void(float hz)> callback);

    // ----- Silence Detection Events -----
    void onSilence(function<void(bool silent)> callback);
    void onSilenceStart(function<void()> callback);
    void onSilenceEnd(function<void()> callback);

    // ----- Energy/Level Events -----
    void onEnergy(function<void(float rms)> callback);
    void onPeak(function<void(float peak)> callback);

    // ----- State Access -----
    shared_ptr<AudioContext> getContext() const { return mContext; }
    const AudioSample& getSample() const;
    void reset();

private:
    shared_ptr<AudioContext> mContext;

    // Lazy detector storage
    shared_ptr<BeatDetector> mBeatDetector;
    shared_ptr<VocalDetector> mVocalDetector;
    shared_ptr<PercussionDetector> mPercussionDetector;
    shared_ptr<PitchDetector> mPitchDetector;
    shared_ptr<SilenceDetector> mSilenceDetector;

    // Lazy creation helpers
    shared_ptr<BeatDetector> getBeatDetector();
    shared_ptr<VocalDetector> getVocalDetector();
    shared_ptr<PercussionDetector> getPercussionDetector();
    shared_ptr<PitchDetector> getPitchDetector();
    shared_ptr<SilenceDetector> getSilenceDetector();
};

} // namespace fl
```

#### Implementation

```cpp
// fl/audio_processor.cpp
#include "fl/audio_processor.h"
#include "fx/audio/beat.h"
#include "fx/audio/vocal.h"
#include "fx/audio/percussion.h"

namespace fl {

AudioProcessor::AudioProcessor()
    : mContext(make_shared<AudioContext>(AudioSample()))
{}

AudioProcessor::~AudioProcessor() = default;

void AudioProcessor::update(const AudioSample& sample) {
    mContext->setSample(sample);

    if (mBeatDetector) mBeatDetector->update(mContext);
    if (mVocalDetector) mVocalDetector->update(mContext);
    if (mPercussionDetector) mPercussionDetector->update(mContext);
    if (mPitchDetector) mPitchDetector->update(mContext);
    if (mSilenceDetector) mSilenceDetector->update(mContext);
}

void AudioProcessor::onBeat(function<void()> callback) {
    auto detector = getBeatDetector();
    detector->onBeat = callback;
}

void AudioProcessor::onVocal(function<void(bool)> callback) {
    auto detector = getVocalDetector();
    detector->onVocalChange = callback;
}

void AudioProcessor::onPercussion(function<void(const char*)> callback) {
    auto detector = getPercussionDetector();
    detector->onPercussionHit = callback;
}

shared_ptr<BeatDetector> AudioProcessor::getBeatDetector() {
    if (!mBeatDetector) {
        mBeatDetector = make_shared<BeatDetector>();
    }
    return mBeatDetector;
}

shared_ptr<VocalDetector> AudioProcessor::getVocalDetector() {
    if (!mVocalDetector) {
        mVocalDetector = make_shared<VocalDetector>();
    }
    return mVocalDetector;
}

shared_ptr<PercussionDetector> AudioProcessor::getPercussionDetector() {
    if (!mPercussionDetector) {
        mPercussionDetector = make_shared<PercussionDetector>();
    }
    return mPercussionDetector;
}

void AudioProcessor::reset() {
    mContext->clearCache();
    if (mBeatDetector) mBeatDetector->reset();
    if (mVocalDetector) mVocalDetector->reset();
    if (mPercussionDetector) mPercussionDetector->reset();
    if (mPitchDetector) mPitchDetector->reset();
    if (mSilenceDetector) mSilenceDetector->reset();
}

} // namespace fl
```

---

## Design Rationale

### Problem Statement

Given multiple detectors that need FFT data:

```cpp
// PROBLEM: How many times is FFT computed?
AudioSample sample = audio.next();

BeatDetector beats;
VocalDetector vocal;
PercussionDetector percussion;

sample.fft(&fftBins1);
beats.update(sample, &fftBins1);

sample.fft(&fftBins2);  // RECOMPUTES FFT!
vocal.update(sample, &fftBins2);

sample.fft(&fftBins3);  // RECOMPUTES FFT AGAIN!
percussion.update(sample, &fftBins3);
```

**Current behavior**: FFT computed 3 times (wasteful!)

### Solution Comparison

| Feature | Cache in Sample | Manager | Context (Chosen) | Manual |
|---------|----------------|---------|------------------|--------|
| **FFT sharing** | ✅ Auto | ✅ Auto | ✅ Auto | ⚠️ Manual |
| **Memory** | High | Medium | Low | Low |
| **Lazy** | ❌ No | ✅ Yes | ✅ Yes | ✅ Yes |
| **Modular** | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes |
| **API simplicity** | ✅ Simple | ⚠️ Medium | ✅ Simple | ✅ Simple |
| **Extensible** | ❌ No | ✅ Yes | ✅ Yes | ⚠️ Limited |

### Why AudioContext Pattern?

**Chosen solution** because:

1. ✅ **Zero cost when not used** - Existing `AudioSample.fft()` API unchanged
2. ✅ **Optimal when needed** - FFT computed once, shared automatically
3. ✅ **Lightweight** - Stack-allocated, reference semantics
4. ✅ **Extensible** - Easy to add new cached computations
5. ✅ **Clear intent** - Context signals "I'm sharing computations"
6. ✅ **Backward compatible** - Old code keeps working

---

## Implementation Examples

### Example: VocalDetector Implementation

Detects human voice in audio using spectral characteristics.

```cpp
// fx/audio/vocal.h
#pragma once

#include "fl/audio_detector.h"
#include "fl/audio_context.h"
#include "fl/function.h"

namespace fl {

class VocalDetector : public AudioDetector {
public:
    VocalDetector();
    ~VocalDetector() override;

    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "VocalDetector"; }
    void reset() override;

    // Callbacks
    function<void(bool active)> onVocalChange;
    function<void()> onVocalStart;
    function<void()> onVocalEnd;

    // State access
    bool isVocal() const { return mVocalActive; }
    float getConfidence() const { return mConfidence; }
    void setThreshold(float threshold) { mThreshold = threshold; }

private:
    bool mVocalActive;
    bool mPreviousVocalActive;
    float mConfidence;
    float mThreshold;
    float mSpectralCentroid;
    float mSpectralRolloff;
    float mFormantRatio;

    float calculateSpectralCentroid(const FFTBins& fft);
    float calculateSpectralRolloff(const FFTBins& fft);
    float estimateFormantRatio(const FFTBins& fft);
    bool detectVocal(float centroid, float rolloff, float formantRatio);
};

} // namespace fl
```

```cpp
// fx/audio/vocal.cpp
#include "fx/audio/vocal.h"
#include "fl/audio_context.h"
#include "fl/math.h"

namespace fl {

VocalDetector::VocalDetector()
    : mVocalActive(false)
    , mPreviousVocalActive(false)
    , mConfidence(0.0f)
    , mThreshold(0.65f)
    , mSpectralCentroid(0.0f)
    , mSpectralRolloff(0.0f)
    , mFormantRatio(0.0f)
{}

VocalDetector::~VocalDetector() = default;

void VocalDetector::update(shared_ptr<AudioContext> context) {
    const FFTBins& fft = context->getFFT(16);

    mSpectralCentroid = calculateSpectralCentroid(fft);
    mSpectralRolloff = calculateSpectralRolloff(fft);
    mFormantRatio = estimateFormantRatio(fft);

    mVocalActive = detectVocal(mSpectralCentroid, mSpectralRolloff, mFormantRatio);

    if (mVocalActive != mPreviousVocalActive) {
        if (onVocalChange) onVocalChange(mVocalActive);
        if (mVocalActive && onVocalStart) onVocalStart();
        if (!mVocalActive && onVocalEnd) onVocalEnd();
        mPreviousVocalActive = mVocalActive;
    }
}

void VocalDetector::reset() {
    mVocalActive = false;
    mPreviousVocalActive = false;
    mConfidence = 0.0f;
}

float VocalDetector::calculateSpectralCentroid(const FFTBins& fft) {
    float weightedSum = 0.0f;
    float magnitudeSum = 0.0f;

    for (fl::size i = 0; i < fft.bins_raw.size(); i++) {
        float magnitude = fft.bins_raw[i];
        weightedSum += i * magnitude;
        magnitudeSum += magnitude;
    }

    return (magnitudeSum < 1e-6f) ? 0.0f : weightedSum / magnitudeSum;
}

float VocalDetector::calculateSpectralRolloff(const FFTBins& fft) {
    const float rolloffThreshold = 0.85f;
    float totalEnergy = 0.0f;

    for (fl::size i = 0; i < fft.bins_raw.size(); i++) {
        float magnitude = fft.bins_raw[i];
        totalEnergy += magnitude * magnitude;
    }

    float energyThreshold = totalEnergy * rolloffThreshold;
    float cumulativeEnergy = 0.0f;

    for (fl::size i = 0; i < fft.bins_raw.size(); i++) {
        float magnitude = fft.bins_raw[i];
        cumulativeEnergy += magnitude * magnitude;
        if (cumulativeEnergy >= energyThreshold) {
            return static_cast<float>(i) / fft.bins_raw.size();
        }
    }

    return 1.0f;
}

float VocalDetector::estimateFormantRatio(const FFTBins& fft) {
    if (fft.bins_raw.size() < 8) return 0.0f;

    // F1 range (bins 2-4)
    float f1Energy = 0.0f;
    for (int i = 2; i <= 4; i++) {
        f1Energy = fl_max(f1Energy, fft.bins_raw[i]);
    }

    // F2 range (bins 4-8)
    float f2Energy = 0.0f;
    for (int i = 4; i <= 7; i++) {
        f2Energy = fl_max(f2Energy, fft.bins_raw[i]);
    }

    return (f1Energy < 1e-6f) ? 0.0f : f2Energy / f1Energy;
}

bool VocalDetector::detectVocal(float centroid, float rolloff, float formantRatio) {
    float normalizedCentroid = centroid / 16.0f;

    bool centroidOk = (normalizedCentroid >= 0.3f && normalizedCentroid <= 0.7f);
    bool rolloffOk = (rolloff >= 0.5f && rolloff <= 0.8f);
    bool formantOk = (formantRatio >= 0.8f && formantRatio <= 2.0f);

    float centroidScore = 1.0f - fl_abs(normalizedCentroid - 0.5f) * 2.0f;
    float rolloffScore = 1.0f - fl_abs(rolloff - 0.65f) / 0.35f;
    float formantScore = (formantOk) ? 1.0f : 0.0f;

    mConfidence = (centroidScore + rolloffScore + formantScore) / 3.0f;

    return (centroidOk && rolloffOk && formantOk && mConfidence >= mThreshold);
}

} // namespace fl
```

---

### Example: PercussionDetector Implementation

Detects percussion hits (kick, snare, hi-hat) using frequency band energy and temporal patterns.

```cpp
// fx/audio/percussion.h
#pragma once

#include "fl/audio_detector.h"
#include "fl/audio_context.h"
#include "fl/function.h"

namespace fl {

class PercussionDetector : public AudioDetector {
public:
    PercussionDetector();
    ~PercussionDetector() override;

    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return true; }
    bool needsFFTHistory() const override { return true; }
    const char* getName() const override { return "PercussionDetector"; }
    void reset() override;

    // Callbacks
    function<void(const char* type)> onPercussionHit;
    function<void()> onKick;
    function<void()> onSnare;
    function<void()> onHiHat;
    function<void()> onTom;

    // Configuration
    void setKickThreshold(float threshold) { mKickThreshold = threshold; }
    void setSnareThreshold(float threshold) { mSnareThreshold = threshold; }
    void setHiHatThreshold(float threshold) { mHiHatThreshold = threshold; }

private:
    float mKickThreshold;
    float mSnareThreshold;
    float mHiHatThreshold;
    float mPrevBassEnergy;
    float mPrevMidEnergy;
    float mPrevTrebleEnergy;
    uint32_t mLastKickTime;
    uint32_t mLastSnareTime;
    uint32_t mLastHiHatTime;

    static constexpr uint32_t KICK_COOLDOWN_MS = 100;
    static constexpr uint32_t SNARE_COOLDOWN_MS = 80;
    static constexpr uint32_t HIHAT_COOLDOWN_MS = 50;

    float getBassEnergy(const FFTBins& fft);
    float getMidEnergy(const FFTBins& fft);
    float getTrebleEnergy(const FFTBins& fft);
    bool detectKick(float bassEnergy, float bassFlux, uint32_t timestamp);
    bool detectSnare(float midEnergy, float midFlux, uint32_t timestamp);
    bool detectHiHat(float trebleEnergy, float trebleFlux, uint32_t timestamp);
};

} // namespace fl
```

```cpp
// fx/audio/percussion.cpp
#include "fx/audio/percussion.h"
#include "fl/audio_context.h"
#include "fl/math.h"

namespace fl {

PercussionDetector::PercussionDetector()
    : mKickThreshold(0.7f)
    , mSnareThreshold(0.6f)
    , mHiHatThreshold(0.5f)
    , mPrevBassEnergy(0.0f)
    , mPrevMidEnergy(0.0f)
    , mPrevTrebleEnergy(0.0f)
    , mLastKickTime(0)
    , mLastSnareTime(0)
    , mLastHiHatTime(0)
{}

PercussionDetector::~PercussionDetector() = default;

void PercussionDetector::update(shared_ptr<AudioContext> context) {
    const FFTBins& fft = context->getFFT(16);
    uint32_t timestamp = context->getTimestamp();

    float bassEnergy = getBassEnergy(fft);
    float midEnergy = getMidEnergy(fft);
    float trebleEnergy = getTrebleEnergy(fft);

    float bassFlux = fl_max(0.0f, bassEnergy - mPrevBassEnergy);
    float midFlux = fl_max(0.0f, midEnergy - mPrevMidEnergy);
    float trebleFlux = fl_max(0.0f, trebleEnergy - mPrevTrebleEnergy);

    bool kickDetected = detectKick(bassEnergy, bassFlux, timestamp);
    bool snareDetected = detectSnare(midEnergy, midFlux, timestamp);
    bool hihatDetected = detectHiHat(trebleEnergy, trebleFlux, timestamp);

    if (kickDetected) {
        if (onKick) onKick();
        if (onPercussionHit) onPercussionHit("kick");
        mLastKickTime = timestamp;
    }

    if (snareDetected) {
        if (onSnare) onSnare();
        if (onPercussionHit) onPercussionHit("snare");
        mLastSnareTime = timestamp;
    }

    if (hihatDetected) {
        if (onHiHat) onHiHat();
        if (onPercussionHit) onPercussionHit("hihat");
        mLastHiHatTime = timestamp;
    }

    mPrevBassEnergy = bassEnergy;
    mPrevMidEnergy = midEnergy;
    mPrevTrebleEnergy = trebleEnergy;
}

void PercussionDetector::reset() {
    mPrevBassEnergy = 0.0f;
    mPrevMidEnergy = 0.0f;
    mPrevTrebleEnergy = 0.0f;
    mLastKickTime = 0;
    mLastSnareTime = 0;
    mLastHiHatTime = 0;
}

float PercussionDetector::getBassEnergy(const FFTBins& fft) {
    float energy = 0.0f;
    for (int i = 0; i < 3 && i < static_cast<int>(fft.bins_raw.size()); i++) {
        energy += fft.bins_raw[i];
    }
    return energy / 3.0f;
}

float PercussionDetector::getMidEnergy(const FFTBins& fft) {
    float energy = 0.0f;
    for (int i = 3; i < 8 && i < static_cast<int>(fft.bins_raw.size()); i++) {
        energy += fft.bins_raw[i];
    }
    return energy / 5.0f;
}

float PercussionDetector::getTrebleEnergy(const FFTBins& fft) {
    float energy = 0.0f;
    for (int i = 8; i < 16 && i < static_cast<int>(fft.bins_raw.size()); i++) {
        energy += fft.bins_raw[i];
    }
    return energy / 8.0f;
}

bool PercussionDetector::detectKick(float bassEnergy, float bassFlux, uint32_t timestamp) {
    if (timestamp - mLastKickTime < KICK_COOLDOWN_MS) return false;
    bool strongBass = (bassEnergy > mKickThreshold);
    bool strongOnset = (bassFlux > mKickThreshold * 0.5f);
    return strongBass && strongOnset;
}

bool PercussionDetector::detectSnare(float midEnergy, float midFlux, uint32_t timestamp) {
    if (timestamp - mLastSnareTime < SNARE_COOLDOWN_MS) return false;
    bool strongMid = (midEnergy > mSnareThreshold);
    bool strongOnset = (midFlux > mSnareThreshold * 0.5f);
    return strongMid && strongOnset;
}

bool PercussionDetector::detectHiHat(float trebleEnergy, float trebleFlux, uint32_t timestamp) {
    if (timestamp - mLastHiHatTime < HIHAT_COOLDOWN_MS) return false;
    bool strongTreble = (trebleEnergy > mHiHatThreshold);
    bool strongOnset = (trebleFlux > mHiHatThreshold * 0.4f);
    return strongTreble && strongOnset;
}

} // namespace fl
```

---

## Usage Examples

### Example 1: Simple Beat-Reactive Sketch

```cpp
#include <FastLED.h>
#include "fl/audio.h"
#include "fl/audio_processor.h"

#define NUM_LEDS 60
#define DATA_PIN 5
CRGB leds[NUM_LEDS];

UIAudio audio("Audio");
fl::AudioProcessor processor;

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

    processor.onBeat([]() {
        fill_solid(leds, NUM_LEDS, CRGB::White);
    });

    processor.onBeatPhase([](float phase) {
        uint8_t brightness = 255 * (1.0f - phase);
        FastLED.setBrightness(brightness);
    });
}

void loop() {
    while (fl::AudioSample sample = audio.next()) {
        processor.update(sample);
    }
    FastLED.show();
}
```

### Example 2: Multi-Detector Vocal & Percussion

```cpp
#include <FastLED.h>
#include "fl/audio.h"
#include "fl/audio_processor.h"

#define NUM_LEDS 100
CRGB leds[NUM_LEDS];

UIAudio audio("Audio");
fl::AudioProcessor processor;

bool vocalActive = false;
CRGB currentColor = CRGB::Blue;

void setup() {
    FastLED.addLeds<WS2812B, 5, GRB>(leds, NUM_LEDS);

    processor.onVocal([](bool active) {
        vocalActive = active;
        currentColor = active ? CRGB::Purple : CRGB::Blue;
    });

    processor.onKick([]() {
        fill_solid(leds, NUM_LEDS / 3, CRGB::Red);
    });

    processor.onSnare([]() {
        fill_solid(leds + NUM_LEDS / 3, NUM_LEDS / 3, CRGB::Yellow);
    });

    processor.onHiHat([]() {
        leds[random16(NUM_LEDS)] = CRGB::White;
    });
}

void loop() {
    fadeToBlackBy(leds, NUM_LEDS, 20);

    while (fl::AudioSample sample = audio.next()) {
        processor.update(sample);
    }

    if (vocalActive) {
        for (int i = NUM_LEDS * 2/3; i < NUM_LEDS; i++) {
            leds[i] = blend(leds[i], currentColor, 128);
        }
    }

    FastLED.show();
}
```

---

## Complete Detector Catalog

### Tier 1: Essential (MVP - Must Have)

**6 core detectors for 80% of use cases**

```cpp
Audio audio;                    // Input with AGC
Spectrum spectrum;              // Raw FFT
FrequencyBands bands;           // Bass/mid/treble abstraction
BeatDetector beats;             // Rhythmic pulse (most requested!)
EnergyAnalyzer energy;          // Overall loudness
TempoAnalyzer tempo;            // BPM tracking
```

### Tier 2: High Value

**6 important features that enhance usability**

```cpp
TransientDetector transients;   // Attack detection
NoteDetector notes;             // Musical notes
DownbeatDetector downbeat;      // Measure-level timing
DynamicsAnalyzer dynamics;      // Loudness trends
PitchDetector pitch;            // Pitch tracking
SilenceDetector silence;        // Auto-standby
```

### Tier 3: Differentiators

**7 unique features not found in competing systems**

```cpp
VocalDetector vocal;            // Voice detection (unique!)
ChordDetector chords;           // Chord recognition (unique!)
KeyDetector key;                // Key detection (unique!)
MoodAnalyzer mood;              // Mood detection (unique!)
PercussionDetector percussion;  // Drum-specific
BuildupDetector buildup;        // EDM buildups
DropDetector drop;              // EDM drops
```

### Full Detector List

See complete catalog of 100+ detector classes organized by category:

1. **Core Audio Capture & Processing**
2. **Frequency Domain Analysis** - Spectrum, FrequencyBands, MelSpectrum, Chroma, SpectralCentroid, SpectralFlux
3. **Temporal/Rhythm Detection** - Beat, Downbeat, Tempo, TimeSignature, RhythmPattern, Swing
4. **Transient & Dynamics** - Transient, Attack, Percussion, Kick/Snare/HiHat, Energy, Peak, Envelope
5. **Pitch & Melody** - Pitch, Note, Melody, PitchBend, Vibrato
6. **Harmonic & Tonal** - Chord, Key, Mode, Tonic, Harmonic, Consonance
7. **Vocal & Instrument** - Vocal, Instrumental, Speech, Drum/Bass/Guitar/Piano
8. **Musical Structure** - Section, Structure, Intro/Outro, Break, Buildup, Drop, Transition, Silence
9. **Genre & Mood** - Genre, Subgenre, Mood, Emotion, Valence, Arousal, Danceability
10. **Spatial & Stereo** - StereoWidth, Panning, SpatialMovement
11. **Complexity & Texture** - Complexity, Density, Texture, Polyphony
12. **Signal Quality** - NoiseFloor, Clipping, SignalQuality, DynamicRange
13. **Specialized** - Microtiming, Quantization, Groove, Novelty, Compression, Reverb

---

## Implementation Tiers

### Development Roadmap

**Phase 1: Tier 1 (6 detectors)** - Core MVP
- Audio input with AGC
- FFT/Spectrum
- FrequencyBands
- BeatDetector
- EnergyAnalyzer
- TempoAnalyzer

**Phase 2: Tier 2 (6 detectors)** - Polish and refinement
- TransientDetector
- NoteDetector
- DownbeatDetector
- DynamicsAnalyzer
- PitchDetector
- SilenceDetector

**Phase 3: Tier 3 (7 detectors)** - Unique differentiators
- VocalDetector
- ChordDetector
- KeyDetector
- MoodAnalyzer
- PercussionDetector
- BuildupDetector
- DropDetector

**Phase 4: Selective Tier 4/5** - Based on user demand

---

## Testing Strategy

### Unit Tests for AudioContext

```cpp
TEST(AudioContextTest, FFTComputedLazily) {
    AudioContext ctx(testSample);
    EXPECT_FALSE(ctx.hasFFT());

    const FFTBins& fft1 = ctx.getFFT(16);
    EXPECT_TRUE(ctx.hasFFT());

    const FFTBins& fft2 = ctx.getFFT(16);
    EXPECT_EQ(&fft1, &fft2);  // Same object!
}

TEST(AudioContextTest, FFTHistoryBuildsCorrectly) {
    AudioContext ctx(testSample);
    const auto& history = ctx.getFFTHistory(4);

    ctx.getFFT(16);
    ctx.setSample(sample2);
    EXPECT_EQ(ctx.getFFTHistory(4).size(), 1);

    // Fill history
    for (int i = 0; i < 3; i++) {
        ctx.getFFT(16);
        ctx.setSample(testSample);
    }

    EXPECT_EQ(ctx.getFFTHistory(4).size(), 4);
}
```

### Integration Tests for AudioProcessor

```cpp
TEST(AudioProcessorTest, LazyDetectorInstantiation) {
    AudioProcessor processor;
    EXPECT_EQ(processor.getContext().use_count(), 1);

    processor.onBeat([]() {});
    EXPECT_GT(processor.getContext().use_count(), 1);
}

TEST(AudioProcessorTest, MultipleDetectorsShareContext) {
    AudioProcessor processor;
    processor.onVocal([](bool) {});
    processor.onBeat([]() {});

    AudioSample sample = createSineWave(512, 0.1f);
    processor.update(sample);
    // Both detectors updated with shared context
}
```

---

## Performance Benchmarks

### Expected Performance

```
AudioContext::getFFT() first call:  ~2-5ms (depends on sample size)
AudioContext::getFFT() cached:      ~0.001ms (pointer return)
VocalDetector::update():            ~0.1-0.5ms
PercussionDetector::update():       ~0.1-0.3ms
AudioProcessor::update():           ~2-6ms (with 3-5 active detectors)

Memory footprint:
  AudioProcessor:          ~40 bytes (stack)
  AudioContext:            ~200 bytes (heap, shared)
  Per detector:            ~50-100 bytes (heap, lazy)
  FFT cache (16 bands):    ~128 bytes
  FFT history (4 frames):  ~512 bytes

Total (5 detectors):       ~1.2 KB
```

---

## Migration Guide

### From Existing Low-Level API

**Before:**
```cpp
fl::BeatDetectorConfig beatConfig;
fl::BeatDetector* beatDetector = new fl::BeatDetector(beatConfig);

beatDetector->onBeat = [](float conf, float bpm, float time) {
    Serial.println("Beat!");
};

void loop() {
    AudioSample sample = audio.next();
    static float floatBuffer[512];
    for (int i = 0; i < sample.size(); i++) {
        floatBuffer[i] = sample[i] / 32768.0f;
    }
    beatDetector->processFrame(floatBuffer, sample.size());
}
```

**After:**
```cpp
fl::AudioProcessor processor;

processor.onBeat([]() {
    Serial.println("Beat!");
});

void loop() {
    while (AudioSample sample = audio.next()) {
        processor.update(sample);
    }
}
```

### Coexistence Strategy

Both APIs can coexist:

```cpp
fl::AudioProcessor processor;  // High-level
fl::BeatDetector* customBeat = new fl::BeatDetector(customConfig);  // Low-level

void loop() {
    while (AudioSample sample = audio.next()) {
        processor.update(sample);  // High-level processing

        // Low-level processing (manual)
        static float floatBuffer[512];
        for (int i = 0; i < sample.size(); i++) {
            floatBuffer[i] = sample[i] / 32768.0f;
        }
        customBeat->processFrame(floatBuffer, sample.size());
    }
}
```

---

## Benefits of This Design

1. **Simple API** - Single `AudioProcessor` object, callback registration
2. **Lazy instantiation** - Only create detectors that are used
3. **Efficient sharing** - FFT computed once, shared via AudioContext
4. **Extensible** - Easy to add new detectors and cached computations
5. **Memory efficient** - shared_ptr manages lifetimes, no duplication
6. **Backward compatible** - Coexists with low-level detector APIs

---

**Document Version:** 2.0
**Date:** 2025-01-16
**Status:** Complete design specification with implementation examples and tests
