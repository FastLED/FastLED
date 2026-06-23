#pragma once

#include "fl/stl/mutex.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/audio/audio_frame.h"
#include "fl/stl/noexcept.h"

namespace fl {

namespace audio {
class Processor;
} // namespace audio

// ---------------------------------------------------------------------------
// Lightweight snapshot structs — owned copies, no dangling references.
// Live in fl:: namespace so effects don't need audio internals.
// ---------------------------------------------------------------------------

/// Snapshot of self-normalizing MilkDrop-style vibe levels.
struct VibeLevels {
    // Self-normalizing relative levels (~1.0 = average for current song)
    float bass = 1.0f;
    float mid = 1.0f;
    float treb = 1.0f;
    float vol = 1.0f; // (bass + mid + treb) / 3

    // Spike detection (true when energy is rising)
    bool bassSpike = false;
    bool midSpike = false;
    bool trebSpike = false;
};

/// Snapshot of 16-bin normalized spectrum.
struct EqLevels {
    static constexpr int kNumBins = 16;
    float bins[kNumBins] = {};
    float bass = 0;
    float mid = 0;
    float treble = 0;
    float volume = 0;
    float dominantFreqHz = 0;
    bool isSilence = false;
};

/// Snapshot of percussion detection state.
struct PercussionState {
    bool kick = false;
    bool snare = false;
    bool hihat = false;
    bool tom = false;
};

// ---------------------------------------------------------------------------
// AudioBatch — lazy proxy over a batch of AudioFrames + optional Processor.
//
// Created on the stack once per draw() cycle and shared (via const pointer)
// across both compositor layers. Cheap accessors (bass/mid/treble/volume/beat)
// aggregate over AudioFrames. Expensive accessors (vibe/equalizer/percussion)
// lazily snapshot from the Processor on first call.
//
// All accessors are const and mutex-guarded — safe to call from either layer.
// When no Processor is wired, expensive accessors return default-constructed
// (zero/neutral) snapshots.
// ---------------------------------------------------------------------------
class AudioBatch {
  public:
    AudioBatch() FL_NOEXCEPT = default;
    explicit AudioBatch(fl::span<const AudioFrame> frames,
                        audio::Processor *proc = nullptr)
        : mFrames(frames), mProc(proc) {}

    AudioBatch(const AudioBatch &) FL_NOEXCEPT = delete;
    AudioBatch &operator=(const AudioBatch &) FL_NOEXCEPT = delete;

    // --- Cheap: peak aggregates over AudioFrames (compute-once) ---
    float bass() const { ensurePeaks(); return mPeaks.bass; }
    float mid() const { ensurePeaks(); return mPeaks.mid; }
    float treble() const { ensurePeaks(); return mPeaks.treble; }
    float volume() const { ensurePeaks(); return mPeaks.volume; }
    bool beat() const { ensurePeaks(); return mPeaks.beat; }

    // --- Expensive: lazy snapshots from Processor (compute-once) ---
    const VibeLevels &vibe() const;
    const EqLevels &equalizer() const;
    const PercussionState &percussion() const;

    // --- Raw frame access ---
    fl::span<const AudioFrame> frames() const { return mFrames; }
    bool empty() const { return mFrames.empty(); }
    fl::size frameCount() const { return mFrames.size(); }
    bool hasProcessor() const { return mProc != nullptr; }

    // --- Range-based for over raw frames ---
    const AudioFrame *begin() const { return mFrames.begin(); }
    const AudioFrame *end() const { return mFrames.end(); }

  private:
    fl::span<const AudioFrame> mFrames;
    audio::Processor *mProc = nullptr; // non-owning, null = no audio

    mutable fl::mutex mMutex;

    // Cheap peak aggregate
    mutable bool mPeaksComputed = false;
    mutable AudioFrame mPeaks;

    // Expensive lazy snapshots
    mutable bool mVibeComputed = false;
    mutable VibeLevels mVibe;

    mutable bool mEqComputed = false;
    mutable EqLevels mEq;

    mutable bool mPercComputed = false;
    mutable PercussionState mPerc;

    void ensurePeaks() const;
};

} // namespace fl
