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
 * @brief Multi-band accent information for backbeat detection
 */
struct MultibandAccent {
    float bass;   // Bass band accent strength (0-1)
    float mid;    // Mid band accent strength (0-1) - critical for snare
    float high;   // High band accent strength (0-1)
    float total;  // Weighted combination of all bands
};

/**
 * @brief Detects backbeats (beats 2 and 4 in 4/4 time) in music
 *
 * The Backbeat identifies backbeat patterns using multi-band frequency
 * analysis with emphasis on mid-range frequencies where snare drums are prominent.
 * It complements the Downbeat and is crucial for rock, pop, funk, and
 * many other genres where the snare drum emphasizes the backbeat.
 *
 * Features:
 * - Multi-band accent detection (bass, mid, high)
 * - Adaptive threshold learning
 * - Spectral profile matching
 * - Works standalone or with Downbeat
 * - Confidence scoring
 * - Pattern consistency checking
 *
 * Dependencies:
 * - Requires Beat for rhythm analysis
 * - Optional Downbeat for accurate measure position
 * - Uses FFT for frequency analysis
 */
class Backbeat : public Detector {
public:
    /**
     * @brief Construct with shared Beat
     */
    explicit Backbeat(shared_ptr<Beat> beatDetector) FL_NO_EXCEPT;

    /**
     * @brief Construct with shared Beat and Downbeat
     */
    explicit Backbeat(shared_ptr<Beat> beatDetector,
                              shared_ptr<Downbeat> downbeatDetector) FL_NO_EXCEPT;

    /**
     * @brief Construct with standalone Beat
     */
    Backbeat() FL_NO_EXCEPT;

    ~Backbeat() FL_NO_EXCEPT override;

    void update(shared_ptr<Context> context) FL_NO_EXCEPT override;
    void fireCallbacks() FL_NO_EXCEPT override;
    bool needsFFT() const FL_NO_EXCEPT override { return true; }
    bool needsFFTHistory() const FL_NO_EXCEPT override { return false; }
    const char* getName() const FL_NO_EXCEPT override { return "Backbeat"; }
    void reset() FL_NO_EXCEPT override;

    // ----- Callbacks (multiple listeners supported) -----

    /** Fires on detected backbeat (beats 2, 4) with beat number, confidence, and strength */
    function_list<void(u8 beatNumber, float confidence, float strength)> onBackbeat;

    // ----- State Access -----

    /** Returns true if backbeat was detected this frame */
    bool isBackbeat() const FL_NO_EXCEPT { return mBackbeatDetected; }

    /** Returns the beat number of the last detected backbeat (1-based) */
    u8 getLastBackbeatNumber() const FL_NO_EXCEPT { return mLastBackbeatNumber; }

    /** Returns backbeat detection confidence (0-1) */
    float getConfidence() const FL_NO_EXCEPT { return mConfidence; }

    /** Returns current backbeat accent strength (0-1+) */
    float getStrength() const FL_NO_EXCEPT { return mCurrentStrength; }

    /** Returns ratio of backbeat to downbeat energy (0-2+) */
    float getBackbeatRatio() const FL_NO_EXCEPT { return mBackbeatRatio; }

    // ----- Configuration -----

    /** Set minimum confidence for backbeat detection (default: 0.6) */
    void setConfidenceThreshold(float threshold) FL_NO_EXCEPT { mConfidenceThreshold = threshold; }

    /** Set bass accent threshold (default: 1.2) */
    void setBassThreshold(float threshold) FL_NO_EXCEPT { mBassThreshold = threshold; }

    /** Set mid accent threshold (default: 1.3) - critical for snare */
    void setMidThreshold(float threshold) FL_NO_EXCEPT { mMidThreshold = threshold; }

    /** Set high accent threshold (default: 1.1) */
    void setHighThreshold(float threshold) FL_NO_EXCEPT { mHighThreshold = threshold; }

    /** Set which beats are backbeats using bitmask (bit 0=beat 1, bit 1=beat 2, etc.) */
    void setBackbeatExpectedBeats(u8 beatMask) FL_NO_EXCEPT { mBackbeatMask = beatMask; }

    /** Enable/disable adaptive threshold learning (default: true) */
    void setAdaptive(bool enable) FL_NO_EXCEPT { mAdaptive = enable; }

    /** Share an external Beat instance */
    void setBeatDetector(shared_ptr<Beat> beatDetector) FL_NO_EXCEPT;

    /** Share an external Downbeat instance */
    void setDownbeatDetector(shared_ptr<Downbeat> downbeatDetector) FL_NO_EXCEPT;

private:
    // ----- Detector Dependencies -----
    shared_ptr<Beat> mBeatDetector;
    shared_ptr<Downbeat> mDownbeatDetector;
    bool mOwnsBeatDetector;
    bool mOwnsDownbeatDetector;

    // ----- State -----
    bool mBackbeatDetected;
    u8 mLastBackbeatNumber;
    float mConfidence;
    float mCurrentStrength;
    float mBackbeatRatio;

    // ----- Configuration -----
    float mConfidenceThreshold;
    float mBassThreshold;
    float mMidThreshold;
    float mHighThreshold;
    u8 mBackbeatMask;  // Bitmask: which beats are backbeats
    bool mAdaptive;

    // ----- Beat Tracking -----
    u8 mCurrentBeat;
    u8 mBeatsPerMeasure;
    bool mPreviousWasBeat;

    // ----- Accent History -----
    MultibandAccent mPreviousAccent;
    deque<float> mBackbeatAccents;      // Accent strengths on backbeats
    deque<float> mNonBackbeatAccents;   // Accent strengths on non-backbeats
    static constexpr size MAX_ACCENT_HISTORY = 16;

    // ----- Adaptive Thresholds -----
    float mBackbeatMean;
    float mNonBackbeatMean;
    float mAdaptiveThreshold;

    // ----- Spectral Profile Learning -----
    vector<float> mBackbeatSpectralProfile;  // Average spectrum of backbeats
    static constexpr size SPECTRAL_PROFILE_SIZE = 16;
    float mProfileAlpha;  // EMA smoothing factor for profile updates

    shared_ptr<const fft::Bins> mRetainedFFT;

    // ----- Helper Methods -----
    void updateBeatDetector(shared_ptr<Context> context) FL_NO_EXCEPT;
    void updateBeatPosition() FL_NO_EXCEPT;
    MultibandAccent calculateMultibandAccent(const fft::Bins& fft) FL_NO_EXCEPT;
    float detectBackbeatAccent(const MultibandAccent& accent) FL_NO_EXCEPT;
    bool isBackbeatPosition() const FL_NO_EXCEPT;
    bool detectBackbeat(float accentStrength, const fft::Bins& fft) FL_NO_EXCEPT;
    void updateAdaptiveThresholds() FL_NO_EXCEPT;
    void updateBackbeatProfile(const fft::Bins& fft) FL_NO_EXCEPT;
    float calculatePatternConfidence(const fft::Bins& fft) FL_NO_EXCEPT;
};

} // namespace detector
} // namespace audio
} // namespace fl
