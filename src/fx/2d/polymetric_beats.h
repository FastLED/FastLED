/*  FastLED - Polymetric Beat Visualization
    ---------------------------------------------------------
    Audio-reactive particle system for polymetric rhythm detection.
    Combines beat detection, polymetric analysis, and particle effects.

    Generic algorithm supporting arbitrary N/M overlays (e.g., 7/8 over 4/4).
    See PolymetricProfiles namespace for preset configurations.

    License: MIT (same spirit as FastLED)
*/

#pragma once

#include "FastLED.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "fx/fx2d.h"
#include "fx/audio/beat_detector.h"
#include "fx/particles/rhythm_particles.h"
#include "fl/xymap.h"

namespace fl {

FASTLED_SMART_PTR(PolymetricBeats);

/// @brief Configuration for PolymetricBeats effect
struct PolymetricBeatsConfig {
    // Beat detection
    BeatDetectorConfig beat_cfg;

    // Particle system
    RhythmParticlesConfig particle_cfg;

    // Audio processing
    int sample_rate_hz = 44100;         ///< Audio sample rate
    int hop_size = 512;                 ///< FFT hop size

    // Rendering
    uint8_t background_fade = 250;      ///< Background fade amount (0-255, higher = slower fade)
    bool clear_on_beat = false;         ///< Clear screen on beat

    /// @brief Constructor with minimal defaults
    /// @details Use PolymetricProfiles::Tipper() or similar for preset configurations
    PolymetricBeatsConfig() {
        // Minimal defaults - users should use profiles for specific styles
        beat_cfg.odf_type = OnsetDetectionFunction::MultiBand;
        beat_cfg.adaptive_whitening = true;
        beat_cfg.polymetric.enable = false;  // Disabled by default, enable via profile

        particle_cfg.max_particles = 1000;
    }
};

/// @brief Polymetric beat visualization effect
/// @details Integrates BeatDetector and RhythmParticles for real-time music visualization
///          Supports arbitrary polymetric overlays (N/M over K bars)
class PolymetricBeats : public Fx2d {
public:
    using Params = PolymetricBeatsConfig;

    /// @brief Construct PolymetricBeats effect
    /// @param xyMap XY mapping for LED grid
    /// @param cfg Configuration (use PolymetricProfiles for presets)
    explicit PolymetricBeats(const XYMap& xyMap, const PolymetricBeatsConfig& cfg = PolymetricBeatsConfig());

    /// @brief Main rendering method
    /// @param context Drawing context
    void draw(DrawContext context) override;

    /// @brief Get effect name
    /// @return Effect name
    fl::string fxName() const override { return "PolymetricBeats"; }

    // ---- Audio Input ----

    /// @brief Process audio frame
    /// @param samples Audio samples (normalized float, -1.0 to +1.0)
    /// @param n Number of samples
    void processAudio(const float* samples, int n);

    // ---- Configuration Access ----

    /// @brief Get beat detector
    /// @return Reference to BeatDetector
    BeatDetector& getBeatDetector() { return _beatDetector; }

    /// @brief Get particle system
    /// @return Reference to RhythmParticles
    RhythmParticles& getParticles() { return _particles; }

    /// @brief Get current configuration
    /// @return Current configuration
    const PolymetricBeatsConfig& config() const { return _cfg; }

    /// @brief Update configuration
    /// @param cfg New configuration
    void setConfig(const PolymetricBeatsConfig& cfg);

    /// @brief Set background fade amount
    /// @param fade Fade amount (0-255, higher = slower fade)
    void setBackgroundFade(uint8_t fade) { _cfg.background_fade = fade; }

    /// @brief Enable/disable clear on beat
    /// @param enable True to clear screen on beat
    void setClearOnBeat(bool enable) { _cfg.clear_on_beat = enable; }

    // ---- Stats ----

    /// @brief Get current tempo estimate
    /// @return Tempo estimate
    TempoEstimate getTempo() const { return _beatDetector.getTempo(); }

    /// @brief Get active particle count
    /// @return Number of active particles
    int getActiveParticleCount() const { return _particles.getActiveParticleCount(); }

    /// @brief Get current 4/4 phase
    /// @return Phase (0.0-1.0)
    float getPhase4_4() const { return _beatDetector.getPhase4_4(); }

    /// @brief Get current overlay phase
    /// @return Phase (0.0-1.0)
    float getPhaseOverlay() const { return _beatDetector.getPhase7_8(); }

private:
    PolymetricBeatsConfig _cfg;

    // Core components
    BeatDetector _beatDetector;
    RhythmParticles _particles;

    // Frame tracking
    uint32_t _lastDrawTime;

    // Clear on beat state
    bool _shouldClear;

    // Callback wiring
    void wireCallbacks();
};

// ---------- Configuration Profiles ----------

/// @brief Preset configurations for different musical styles
namespace PolymetricProfiles {

/// @brief Tipper-style broken beat EDM (7/8 over 4/4)
/// @details Optimized for Tipper's production style:
///          - 7/8 overlay pattern over 2 bars of 4/4
///          - Moderate swing amount (12%)
///          - Particle configs tuned for kick/snare/hat separation
inline PolymetricBeatsConfig Tipper() {
    PolymetricBeatsConfig cfg;

    // Beat detection - multi-band with adaptive whitening
    cfg.beat_cfg.odf_type = OnsetDetectionFunction::MultiBand;
    cfg.beat_cfg.adaptive_whitening = true;

    // Polymetric analysis - 7/8 over 4/4
    cfg.beat_cfg.polymetric.enable = true;
    cfg.beat_cfg.polymetric.overlay_numerator = 7;
    cfg.beat_cfg.polymetric.overlay_denominator = 8;
    cfg.beat_cfg.polymetric.overlay_bars = 2;
    cfg.beat_cfg.polymetric.swing_amount = 0.12f;

    // Particle system - optimized for broken beat
    cfg.particle_cfg.max_particles = 1000;
    cfg.particle_cfg.radial_gravity = -0.2f;  // Slight repulsion from center
    cfg.particle_cfg.curl_strength = 0.7f;
    cfg.particle_cfg.kick_duck_amount = 0.35f;
    cfg.particle_cfg.bloom_threshold = 100;
    cfg.particle_cfg.bloom_strength = 0.5f;

    return cfg;
}

// Future profiles can be added here:
// inline PolymetricBeatsConfig Aphex() { ... }  // 5/4 + triplets for Aphex Twin
// inline PolymetricBeatsConfig Amon() { ... }   // Complex polyrhythms for Amon Tobin
// inline PolymetricBeatsConfig Meshuggah() { ... }  // Metal polyrhythms

} // namespace PolymetricProfiles

} // namespace fl

#endif // SKETCH_HAS_LOTS_OF_MEMORY
