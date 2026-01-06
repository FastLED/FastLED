#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/fx/audio/detectors/beat.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

/**
 * @brief Detects downbeats (first beat of each measure) in music
 *
 * The DownbeatDetector analyzes beat patterns to identify the first beat
 * of each musical measure. It detects metric groupings (time signatures)
 * and tracks measure position.
 *
 * Features:
 * - Downbeat detection with confidence
 * - Time signature detection (4/4, 3/4, 6/8, etc.)
 * - Beat numbering within measures
 * - Measure phase tracking (0-1 within measure)
 * - Adaptive meter detection
 *
 * Dependencies:
 * - Requires BeatDetector for rhythm analysis
 * - Uses FFT for accent detection
 * - Analyzes beat interval patterns
 */
class DownbeatDetector : public AudioDetector {
public:
    /**
     * @brief Construct with shared BeatDetector (recommended)
     */
    explicit DownbeatDetector(shared_ptr<BeatDetector> beatDetector);

    /**
     * @brief Construct with standalone BeatDetector
     */
    DownbeatDetector();

    ~DownbeatDetector() override;

    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return true; }
    bool needsFFTHistory() const override { return false; }
    const char* getName() const override { return "DownbeatDetector"; }
    void reset() override;

    // ----- Callbacks (multiple listeners supported) -----

    /** Fires on detected downbeat (first beat of measure) */
    function_list<void()> onDownbeat;

    /** Fires on each beat with beat number (1-based, downbeat = 1) */
    function_list<void(u8 beatNumber)> onMeasureBeat;

    /** Fires when time signature changes */
    function_list<void(u8 beatsPerMeasure)> onMeterChange;

    /** Fires with measure phase each frame (0-1 range) */
    function_list<void(float phase)> onMeasurePhase;

    // ----- State Access -----

    /** Returns true if downbeat was detected this frame */
    bool isDownbeat() const { return mDownbeatDetected; }

    /** Returns current beat number within measure (1-based, 1 = downbeat) */
    u8 getCurrentBeat() const { return mCurrentBeat; }

    /** Returns detected beats per measure (time signature numerator) */
    u8 getBeatsPerMeasure() const { return mBeatsPerMeasure; }

    /** Returns measure phase (0-1, 0 = downbeat) */
    float getMeasurePhase() const { return mMeasurePhase; }

    /** Returns downbeat detection confidence (0-1) */
    float getConfidence() const { return mConfidence; }

    // ----- Configuration -----

    /** Set minimum confidence for downbeat detection (default: 0.6) */
    void setConfidenceThreshold(float threshold) { mConfidenceThreshold = threshold; }

    /** Set accent detection threshold (default: 1.2) */
    void setAccentThreshold(float threshold) { mAccentThreshold = threshold; }

    /** Enable/disable automatic meter detection (default: true) */
    void setAutoMeterDetection(bool enable) { mAutoMeterDetection = enable; }

    /** Manually set time signature (disables auto-detection) */
    void setTimeSignature(u8 beatsPerMeasure);

    /** Share an external BeatDetector instance */
    void setBeatDetector(shared_ptr<BeatDetector> beatDetector);

private:
    // ----- BeatDetector Management -----
    shared_ptr<BeatDetector> mBeatDetector;
    bool mOwnsBeatDetector;  // True if we created our own BeatDetector

    // ----- State -----
    bool mDownbeatDetected;
    u8 mCurrentBeat;          // 1-based beat number (1 = downbeat)
    u8 mBeatsPerMeasure;      // Detected time signature
    float mMeasurePhase;      // 0-1 within measure
    float mConfidence;

    // ----- Configuration -----
    float mConfidenceThreshold;
    float mAccentThreshold;
    bool mAutoMeterDetection;
    bool mManualMeter;

    // ----- Beat Tracking -----
    u32 mLastDownbeatTime;
    u32 mLastBeatTime;
    u8 mBeatsSinceDownbeat;

    // ----- Accent Detection -----
    float mPreviousEnergy;
    vector<float> mBeatAccents;  // Recent beat accent strengths
    static constexpr size MAX_BEAT_HISTORY = 32;

    // ----- Meter Detection -----
    vector<u8> mMeterCandidates;  // Recent detected meters
    static constexpr size METER_HISTORY_SIZE = 8;

    // ----- Helper Methods -----
    void updateBeatDetector(shared_ptr<AudioContext> context);
    float calculateBeatAccent(const FFTBins& fft, float bassEnergy);
    bool detectDownbeat(u32 timestamp, float accent);
    void detectMeter();
    void updateMeasurePhase(u32 timestamp);
    u8 findMostCommonMeter() const;
};

} // namespace fl
