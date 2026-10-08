#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"
#include "fl/stl/deque.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

/**
 * @brief Detects downbeats (first beat of each measure) in music
 *
 * The Downbeat analyzes beat patterns to identify the first beat
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
 * - Requires Beat for rhythm analysis
 * - Uses FFT for accent detection
 * - Analyzes beat interval patterns
 */
class Downbeat : public Detector {
public:
    /**
     * @brief Construct with shared Beat (recommended)
     */
    explicit Downbeat(shared_ptr<Beat> beatDetector) FL_NO_EXCEPT;

    /**
     * @brief Construct with standalone Beat
     */
    Downbeat() FL_NO_EXCEPT;

    ~Downbeat() FL_NO_EXCEPT override;

    void update(shared_ptr<Context> context) FL_NO_EXCEPT override;
    void fireCallbacks() FL_NO_EXCEPT override;
    bool needsFFT() const FL_NO_EXCEPT override { return true; }
    bool needsFFTHistory() const FL_NO_EXCEPT override { return false; }
    const char* getName() const FL_NO_EXCEPT override { return "Downbeat"; }
    void reset() FL_NO_EXCEPT override;

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
    bool isDownbeat() const FL_NO_EXCEPT { return mDownbeatDetected; }

    /** Returns current beat number within measure (1-based, 1 = downbeat) */
    u8 getCurrentBeat() const FL_NO_EXCEPT { return mCurrentBeat; }

    /** Returns detected beats per measure (time signature numerator) */
    u8 getBeatsPerMeasure() const FL_NO_EXCEPT { return mBeatsPerMeasure; }

    /** Returns measure phase (0-1, 0 = downbeat) */
    float getMeasurePhase() const FL_NO_EXCEPT { return mMeasurePhase; }

    /** Returns downbeat detection confidence (0-1) */
    float getConfidence() const FL_NO_EXCEPT { return mConfidence; }

    // ----- Configuration -----

    /** Set minimum confidence for downbeat detection (default: 0.6) */
    void setConfidenceThreshold(float threshold) FL_NO_EXCEPT { mConfidenceThreshold = threshold; }

    /** Set accent detection threshold (default: 1.2) */
    void setAccentThreshold(float threshold) FL_NO_EXCEPT { mAccentThreshold = threshold; }

    /** Enable/disable automatic meter detection (default: true) */
    void setAutoMeterDetection(bool enable) FL_NO_EXCEPT { mAutoMeterDetection = enable; }

    /** Manually set time signature (disables auto-detection) */
    void setTimeSignature(u8 beatsPerMeasure) FL_NO_EXCEPT;

    /** Share an external Beat instance */
    void setBeatDetector(shared_ptr<Beat> beatDetector) FL_NO_EXCEPT;

private:
    // ----- Beat Management -----
    shared_ptr<Beat> mBeatDetector;
    bool mOwnsBeatDetector;  // True if we created our own Beat

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
    deque<float> mBeatAccents;  // Recent beat accent strengths
    static constexpr size MAX_BEAT_HISTORY = 32;

    // ----- Meter Detection -----
    deque<u8> mMeterCandidates;  // Recent detected meters
    static constexpr size METER_HISTORY_SIZE = 8;

    // ----- Pending Callback Flags -----
    bool mFireDownbeat = false;
    bool mFireMeasureBeat = false;
    u8 mPendingBeatNumber = 0;
    bool mFireMeterChange = false;
    u8 mPendingMeter = 0;

    shared_ptr<const fft::Bins> mRetainedFFT;

    // ----- Helper Methods -----
    void updateBeatDetector(shared_ptr<Context> context) FL_NO_EXCEPT;
    float calculateBeatAccent(const fft::Bins& fft, float bassEnergy) FL_NO_EXCEPT;
    bool detectDownbeat(u32 timestamp, float accent) FL_NO_EXCEPT;
    void detectMeter() FL_NO_EXCEPT;
    void updateMeasurePhase(u32 timestamp) FL_NO_EXCEPT;
    u8 findMostCommonMeter() const FL_NO_EXCEPT;
};

} // namespace detector
} // namespace audio
} // namespace fl
