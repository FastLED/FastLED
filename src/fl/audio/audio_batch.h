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
    AudioBatch() FL_NO_EXCEPT = default;
    explicit AudioBatch(fl::span<const AudioFrame> frames,
                        audio::Processor *proc = nullptr)
        FL_NO_EXCEPT : mFrames(frames), mProc(proc) {}

    AudioBatch(const AudioBatch &) FL_NO_EXCEPT = delete;
    AudioBatch &operator=(const AudioBatch &) FL_NO_EXCEPT = delete;

    // --- Cheap: peak aggregates over AudioFrames (compute-once) ---
    float bass() const FL_NO_EXCEPT { ensurePeaks(); return mPeaks.bass; }
    float mid() const FL_NO_EXCEPT { ensurePeaks(); return mPeaks.mid; }
    float treble() const FL_NO_EXCEPT { ensurePeaks(); return mPeaks.treble; }
    float volume() const FL_NO_EXCEPT { ensurePeaks(); return mPeaks.volume; }
    bool beat() const FL_NO_EXCEPT { ensurePeaks(); return mPeaks.beat; }

    // --- Expensive: lazy snapshots from Processor (compute-once) ---
    const VibeLevels &vibe() const FL_NO_EXCEPT;
    const EqLevels &equalizer() const FL_NO_EXCEPT;
    const PercussionState &percussion() const FL_NO_EXCEPT;

    // --- Raw frame access ---
    fl::span<const AudioFrame> frames() const FL_NO_EXCEPT { return mFrames; }
    bool empty() const FL_NO_EXCEPT { return mFrames.empty(); }
    fl::size frameCount() const FL_NO_EXCEPT { return mFrames.size(); }
    bool hasProcessor() const FL_NO_EXCEPT { return mProc != nullptr; }

    // --- Range-based for over raw frames ---
    const AudioFrame *begin() const FL_NO_EXCEPT { return mFrames.begin(); }
    const AudioFrame *end() const FL_NO_EXCEPT { return mFrames.end(); }

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

    void ensurePeaks() const FL_NO_EXCEPT;
};

} // namespace fl
