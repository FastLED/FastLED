/*  FastLED - Polymetric Rhythm Analysis
    ---------------------------------------------------------
    Analyzes polymetric overlay patterns (e.g., 7/8 over 4/4)
    for complex EDM rhythm detection.

    License: MIT (same spirit as FastLED)
*/

#pragma once

#include "FastLED.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "fl/stdint.h"
#include "fl/function.h"

namespace fl {

// ---------- Configuration ----------

/// @brief Configuration for polymetric analysis
struct PolymetricConfig {
    bool enable = false;             ///< Enable polymetric analysis
    int overlay_numerator = 7;       ///< Overlay meter numerator (7 for 7/8)
    int overlay_denominator = 8;     ///< Overlay meter denominator
    int overlay_bars = 2;            ///< Overlay cycle length in bars (7/8 over 2 bars of 4/4)
    float swing_amount = 0.12f;      ///< Swing amount 0.0-0.25 (0=straight, 0.25=hard swing)
    float humanize_ms = 4.0f;        ///< Micro-timing jitter ±ms
    bool enable_tuplet_detection = true; ///< Detect triplet/quintuplet fills
};

/// @brief Subdivision types for rhythmic events
enum class SubdivisionType : uint8_t {
    Quarter = 0,     ///< Quarter note (beat)
    Eighth,          ///< Eighth note
    Sixteenth,       ///< Sixteenth note
    Triplet,         ///< Triplet subdivision
    Quintuplet       ///< Quintuplet subdivision
};

// ---------- Polymetric Analyzer ----------

/// @brief Analyzes polymetric rhythm patterns
/// @details Tracks multiple simultaneous meters (e.g., 7/8 over 4/4)
///          and detects micro-timing deviations like swing and tuplets
class PolymetricAnalyzer {
public:
    /// @brief Construct polymetric analyzer
    /// @param cfg Configuration
    explicit PolymetricAnalyzer(const PolymetricConfig& cfg);

    /// @brief Update with beat event from tempo tracker
    /// @param bpm Current tempo in BPM
    /// @param timestamp_ms Current timestamp in milliseconds
    void onBeat(float bpm, float timestamp_ms);

    /// @brief Update on each audio frame
    /// @param timestamp_ms Current timestamp in milliseconds
    void update(float timestamp_ms);

    /// @brief Get current 4/4 phase (0.0-1.0)
    /// @return Phase within 4/4 bar
    float getPhase4_4() const { return _phase4_4; }

    /// @brief Get current 7/8 overlay phase (0.0-1.0)
    /// @return Phase within overlay cycle
    float getPhase7_8() const { return _phaseOverlay; }

    /// @brief Get current 16th note phase (0.0-1.0)
    /// @return Phase within 16th note subdivision
    float getPhase16th() const { return _phase16th; }

    /// @brief Get swing offset for current subdivision
    /// @return Swing offset in range [-swing_amount, +swing_amount]
    float getSwingOffset() const;

    /// @brief Check if currently in a fill section
    /// @return True if fill is active
    bool isInFill() const { return _inFill; }

    /// @brief Get fill density (0.0-1.0)
    /// @return Fill density estimate
    float getFillDensity() const { return _fillDensity; }

    /// @brief Reset internal state
    void reset();

    /// @brief Get current configuration
    const PolymetricConfig& config() const { return _cfg; }

    /// @brief Update configuration
    void setConfig(const PolymetricConfig& cfg);

    /// @brief Callback for polymetric beat events
    /// @param phase4_4 Phase in 4/4 meter (0.0-1.0)
    /// @param phase7_8 Phase in overlay meter (0.0-1.0)
    function<void(float phase4_4, float phase7_8)> onPolymetricBeat;

    /// @brief Callback for subdivision events
    /// @param subdivision Subdivision type
    /// @param swing_offset Swing offset value
    function<void(SubdivisionType subdivision, float swing_offset)> onSubdivision;

    /// @brief Callback for fill detection
    /// @param starting True if fill is starting, false if ending
    /// @param density Fill density (0.0-1.0)
    function<void(bool starting, float density)> onFill;

private:
    PolymetricConfig _cfg;

    // Phase tracking
    float _phase4_4;         ///< Current phase in 4/4 meter (0.0-1.0)
    float _phaseOverlay;     ///< Current phase in overlay meter (0.0-1.0)
    float _phase16th;        ///< Current phase for 16th note subdivision (0.0-1.0)

    // Tempo tracking
    float _currentBPM;       ///< Current tempo in BPM
    float _lastBeatTime;     ///< Last beat timestamp in ms
    float _beatPeriodMs;     ///< Period between beats in ms

    // Fill detection
    bool _inFill;            ///< Currently in a fill
    float _fillDensity;      ///< Estimated fill density
    float _fillStartTime;    ///< Fill start timestamp

    // Subdivision tracking
    float _lastPhase16th;    ///< Last 16th note phase for edge detection

    // Internal methods
    void updatePhases(float timestamp_ms);
    void detectSubdivisions(float timestamp_ms);
    void detectFills(float timestamp_ms);
    float calculateSwingOffset(float phase) const;
};

} // namespace fl

#endif // SKETCH_HAS_LOTS_OF_MEMORY
